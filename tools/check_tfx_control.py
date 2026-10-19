#!/usr/bin/env python3
"""Compare pan sweeps with original TFX 2.05 and trace Q2.COM's driver calls.

Requires Unicorn and an external original installation/Creative firmware. DOS
file I/O and the Creative driver boundary are supplied explicitly. Successful
stub replies allow observing host control, not claiming DSP/audio execution.
The original instructions are not patched and no original bytes are output.
"""
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess

from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_CODE, UC_HOOK_INTR, UC_HOOK_INSN
from unicorn import x86_const as r
from check_firmware import chunks

Q2_SHA256 = '24d626ce3a436296cd4cbfeb4123c8eb0b26765cb73f6678346512fb064edbbf'
START_SHA256 = 'c5a60c7791c971a872aaed1e19ef2638aab0b21eb1e039907b8d64eb8f8980d6'
TFX_SHA256 = '2cc41a39fa95a2c9fd883c4220d7bdd9cca00c543c89b019eb270763f59d1d9a'


def original(path, expected):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError(f'{path}: requires the September 1994 TFX 2.05 reference')
    return data


def q2_trace(raw, firmware, rate_mask=1, failing_call=None):
    cpu = Uc(UC_ARCH_X86, UC_MODE_16)
    cpu.mem_map(0, 0x200000)
    cpu.mem_write(0x10100, raw)
    for register, value in ((r.UC_X86_REG_CS, 0x1000), (r.UC_X86_REG_DS, 0x1000),
                            (r.UC_X86_REG_ES, 0x1000), (r.UC_X86_REG_SS, 0x1000),
                            (r.UC_X86_REG_SP, 0xff00), (r.UC_X86_REG_FLAGS, 0x202)):
        cpu.reg_write(register, value)
    cpu.mem_write(0x13a88, struct.pack('<H', 0x5000))
    cpu.mem_write(0x15000, struct.pack('<2H', 0x5010, 0))
    cpu.mem_write(0x15010, b'SOUND=C:\\SB16\0')
    cpu.mem_write(0x15100, struct.pack('<3H', 0x5120, 0x5130, 0))
    cpu.mem_write(0x15120, b'Q2.COM\0')
    cpu.mem_write(0x15130, b'32768\0')  # Caller-selected low-memory interface address.
    cpu.mem_write(0x1ff00, struct.pack('<4H', 0xf000, 2, 0x5100, 0))
    cpu.mem_write(0x90000, b'\xcb')  # Supplied Creative driver returns with RETF.
    opened, calls = [], []
    file_position = 0

    def carry(error=False):
        cpu.reg_write(r.UC_X86_REG_FLAGS, (cpu.reg_read(r.UC_X86_REG_FLAGS) & ~1) | int(error))

    def far_pointer(value):
        return (value >> 16) * 16 + (value & 65535)

    def interrupt(current, number, unused):
        nonlocal file_position
        ax = current.reg_read(r.UC_X86_REG_AX)
        ds = current.reg_read(r.UC_X86_REG_DS) * 16
        dx = current.reg_read(r.UC_X86_REG_DX)
        cx = current.reg_read(r.UC_X86_REG_CX)
        bx = current.reg_read(r.UC_X86_REG_BX)
        assert number == 0x21
        carry()
        if ax >> 8 == 0x3d:
            name = bytes(current.mem_read(ds + dx, 256)).split(b'\0')[0].decode('ascii')
            opened.append(name)
            if name == 'CSPXXXX0':
                current.reg_write(r.UC_X86_REG_AX, 6)
            elif name == 'C:\\SB16\\CSP\\WFM0001a.CSP':
                file_position = 0
                current.reg_write(r.UC_X86_REG_AX, 5)
            else:
                current.reg_write(r.UC_X86_REG_AX, 2)
                carry(True)
        elif ax == 0x4402:
            assert bx == 6 and cx == 4
            current.mem_write(ds + dx, struct.pack('<HH', 0, 0x9000))
            current.reg_write(r.UC_X86_REG_AX, 4)
        elif ax >> 8 == 0x3e:
            assert bx in (5, 6)
            current.reg_write(r.UC_X86_REG_AX, 0)
        elif ax >> 8 == 0x3f:
            assert bx == 5
            data = firmware[file_position:file_position + cx]
            current.mem_write(ds + dx, data)
            file_position += len(data)
            current.reg_write(r.UC_X86_REG_AX, len(data))
        elif ax >> 8 == 0x42:
            assert bx == 5 and ax & 255 in (0, 1, 2)
            displacement = struct.unpack('<i', struct.pack('<HH', dx, cx))[0]
            file_position = displacement + (0, file_position, len(firmware))[ax & 255]
            assert 0 <= file_position <= len(firmware)
            current.reg_write(r.UC_X86_REG_AX, file_position & 65535)
            current.reg_write(r.UC_X86_REG_DX, file_position >> 16)
        else:
            raise RuntimeError(f'undeclared DOS boundary {ax:04x}')

    def driver(current, address, size, unused):
        stack = current.reg_read(r.UC_X86_REG_SP) + current.reg_read(r.UC_X86_REG_SS) * 16
        command, handle, first, second = struct.unpack('<4I', current.mem_read(stack + 4, 16))
        call = dict(command=command, handle=handle, first=first, second=second)
        if command in (0x30000, 0xa0000):
            data = bytes(current.mem_read(far_pointer(first), second))
            call.update(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
        assert command in (0x10000, 0x30000, 0x40000, 0x80000, 0xa0000)
        calls.append(call)
        failure = failing_call == len(calls)
        if command == 0x10000 and not failure:
            current.mem_write(far_pointer(first), struct.pack('<I', 0x1234))
        current.reg_write(r.UC_X86_REG_AX, 0x45 if failure else 0)
        current.reg_write(r.UC_X86_REG_DX, 0)

    # Main's actual masks are all 1. Other rates are explicit caller-data tests.
    if rate_mask != 1:
        cpu.hook_add(UC_HOOK_CODE, lambda c, p, n, u: c.mem_write(0x13ad0, struct.pack('<H', rate_mask)),
                     begin=0x1070d, end=0x1070d)
    cpu.hook_add(UC_HOOK_INTR, interrupt)
    cpu.hook_add(UC_HOOK_CODE, driver, begin=0x90000, end=0x90000)
    cpu.emu_start(0x10962, 0x1f000, count=3_000_000)
    assert cpu.reg_read(r.UC_X86_REG_CS) == 0x1000 and cpu.reg_read(r.UC_X86_REG_IP) == 0xf000
    interface = struct.unpack('<3I', cpu.mem_read(0x8000, 12))
    return dict(rate_mask=rate_mask, failing_call=failing_call, calls=calls,
                interface=list(interface), opened=opened)


class OriginalSweep:
    def __init__(self, launcher):
        self.cpu = cpu = Uc(UC_ARCH_X86, UC_MODE_16)
        cpu.mem_map(0, 0x100000)
        cpu.mem_write(0x10000, launcher[0x200:])
        # This wrapper uses CS-relative data. Unrelated MZ startup is not run.
        cpu.mem_write(0x90000, b'\xcb')
        cpu.mem_write(0x90010, b'\xcf')
        cpu.mem_write(0x108ce, struct.pack('<HH', 16, 0x9000))
        cpu.mem_write(0x108d8, struct.pack('<I', 0x1234))
        cpu.mem_write(0x108df, struct.pack('<HH', 0, 0x9000))
        self.calls = []
        self.status = 0
        cpu.hook_add(UC_HOOK_CODE, self.driver, begin=0x90000, end=0x90000)

    def driver(self, cpu, address, size, unused):
        stack = cpu.reg_read(r.UC_X86_REG_SP) + cpu.reg_read(r.UC_X86_REG_SS) * 16
        command, handle, first, second = struct.unpack('<4I', cpu.mem_read(stack + 4, 16))
        assert command == 0x80000 and handle == 0x1234
        self.calls.append([first, second])
        cpu.reg_write(r.UC_X86_REG_AX, self.status)
        cpu.reg_write(r.UC_X86_REG_DX, 0)

    def run(self, phase, increment, moving, cached, ticks, status=0):
        cpu = self.cpu
        self.status = status
        cpu.mem_write(0x1089e, struct.pack('<3H', phase, increment & 65535, moving))
        cpu.mem_write(0x108a4, bytes([cached, 0]))
        results = []
        for unused in range(ticks):
            for register, value in ((r.UC_X86_REG_CS, 0x1000), (r.UC_X86_REG_DS, 0x7000),
                                    (r.UC_X86_REG_ES, 0x7000), (r.UC_X86_REG_SS, 0x7000),
                                    (r.UC_X86_REG_SP, 0xff00), (r.UC_X86_REG_FLAGS, 0x202)):
                cpu.reg_write(register, value)
            cpu.mem_write(0x7ff00, struct.pack('<HHH', 0xf000, 0x1000, 0x202))
            self.calls.clear()
            cpu.emu_start(0x108a6, 0x1f000, count=10_000)
            assert cpu.reg_read(r.UC_X86_REG_CS) == 0x1000 and cpu.reg_read(r.UC_X86_REG_IP) == 0xf000
            phase, = struct.unpack('<H', cpu.mem_read(0x1089e, 2))
            cached = cpu.mem_read(0x108a4, 1)[0]
            if self.calls:
                assert self.calls == [[0xe0, 1], [0, cached], [2, 0]]
            results.append([phase, cached, cached if self.calls else -1])
        return results



def original_timer_schedule(launcher, interrupts=10_000):
    """Run the original OSL PIT setup and inactive-player IRQ chaining path."""
    cpu = Uc(UC_ARCH_X86, UC_MODE_16)
    cpu.mem_map(0, 0x100000)
    cpu.mem_write(0x10000, launcher[0x200:])
    master_segment = 0x1353  # Original MZ segment 0353, loaded at 1000.
    master = master_segment * 16
    cpu.mem_write(0x90000, b'\xcf')  # Supplied previous BIOS handler: IRET.
    cpu.mem_write(master + 0x1e3b, struct.pack('<HH', 0, 0x9000))
    cpu.mem_write(master + 0x1d09, bytes(2))  # Explicit inactive OSL player.
    cpu.mem_write(master + 0x1de4, bytes(1))
    writes, callbacks = [], []
    current_interrupt = 0
    cpu.hook_add(UC_HOOK_INSN,
                 lambda c, port, size, value, u: writes.append([port, value]),
                 None, 1, 0, r.UC_X86_INS_OUT)
    cpu.hook_add(UC_HOOK_CODE, lambda c, pc, size, u: callbacks.append(current_interrupt),
                 begin=0x90000, end=0x90000)

    def call(address, irq=False):
        for register, value in ((r.UC_X86_REG_CS, master_segment), (r.UC_X86_REG_DS, 0x7000),
                                (r.UC_X86_REG_ES, 0x7000), (r.UC_X86_REG_SS, 0x7000),
                                (r.UC_X86_REG_SP, 0xff00), (r.UC_X86_REG_FLAGS, 0x202)):
            cpu.reg_write(register, value)
        frame = struct.pack('<HHH', 0xf000, master_segment, 0x202) if irq else struct.pack('<H', 0xf000)
        cpu.mem_write(0x7ff00, frame)
        cpu.emu_start(master + address, master + 0xf000, count=10_000)
        assert cpu.reg_read(r.UC_X86_REG_CS) == master_segment and cpu.reg_read(r.UC_X86_REG_IP) == 0xf000

    call(0x1e61)
    assert writes == [[0x43, 0x36], [0x40, 0x55], [0x40, 0x55]]
    phase, modulus, step = struct.unpack('<3H', cpu.mem_read(master + 0x1d0b, 6))
    assert (phase, modulus, step) == (0, 137, 50)
    expected = []
    for current_interrupt in range(interrupts):
        phase -= step
        if phase < 0:
            phase += modulus
            expected.append(current_interrupt)
        call(0x1e03, irq=True)
    assert callbacks == expected
    return dict(pit_divisor=0x5555, phase_step=step, phase_modulus=modulus,
                interrupts=interrupts, chained_callbacks=len(callbacks), first_callbacks=callbacks[:32])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--install', type=Path, default=Path('../tfx-reverse/installs/cd_1994_09/TFX'))
    parser.add_argument('--firmware', type=Path, default=Path('firmware/WFM0001A.CSP'))
    parser.add_argument('--probe', type=Path, default=Path('build/qsound_control_probe'))
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    q2 = original(args.install / 'Q2.COM', Q2_SHA256)
    launcher = original(args.install / 'START.EXE', START_SHA256)
    engine = original(args.install / 'TFX.DAT', TFX_SHA256)
    firmware = args.firmware.read_bytes()
    programs = []
    for tag, value in chunks(firmware[12:]):
        if tag == b'LIST' and value[:4] == b'code':
            programs.append({kind.decode(): code for kind, code in chunks(value[4:]) if kind in (b'init', b'main')})
    assert len(programs) == 4
    traces = []
    for rate_mask, program in zip((1, 2, 4, 8), reversed(programs)):
        trace = q2_trace(q2, firmware, rate_mask)
        assert trace['interface'] == [0x62951413, 0x90000000, 0x1234]
        assert [c['command'] for c in trace['calls']] == [0x10000, 0xa0000, 0x30000, 0x40000, 0x80000, 0x80000, 0x80000]
        for call, kind in zip(trace['calls'][1:3], ('init', 'main')):
            assert call['bytes'] == len(program[kind]) and call['sha256'] == hashlib.sha256(program[kind]).hexdigest()
        assert [[c['first'], c['second']] for c in trace['calls'][-3:]] == [[0xe0, 1], [0, 255], [1, 255]]
        traces.append(trace)
    failures = []
    for failed in range(1, 8):
        trace = q2_trace(q2, firmware, failing_call=failed)
        # Q2 checks allocation and the three initial parameter calls' statuses;
        # it still publishes its interface after errors in the other stages.
        assert bool(trace['interface'][0]) == (failed != 1)
        failures.append(dict(call=failed, published=bool(trace['interface'][0])))

    groups = defaultdict(list)
    for index in range(106):
        name, rate, delay, effect = struct.unpack_from('<IHHI', engine, 0xedaa4 + 0x9860 + index * 26)
        start = 0xedaa4 + name
        title = engine[start:engine.index(0, start)].decode('ascii')
        parameters = struct.unpack_from('<3h', engine, 0xedaa4 + effect) if effect else (4096, 0, 0)
        groups[parameters].append(dict(index=index, name=title, rate=rate))
    assert len(groups) == 9 and sum(map(len, groups.values())) == 106
    cases = [(p & 65535, step, moving & 65535, 255, 256) for p, step, moving in groups]
    cases += [(phase, step, moving, cached, 8)
              for phase in (0, 255, 256, 0x7fff, 0x8000, 0xff00, 65535)
              for step in (-32768, -1024, -1, 0, 1, 1024, 32767)
              for moving in (0, 1, 16, 65535) for cached in (0, 16, 255)]
    randomizer = random.Random(0x514f554e)
    cases += [(randomizer.randrange(65536), randomizer.randrange(-32768, 32768),
               randomizer.randrange(65536), randomizer.randrange(256), 16) for unused in range(128)]
    reference = OriginalSweep(launcher)
    expected = [row for case in cases for row in reference.run(*case)]
    request = ''.join(' '.join(map(str, case)) + '\n' for case in cases)
    output = subprocess.check_output([str(args.probe.resolve())], input=request, text=True)
    actual = [list(map(int, row.split())) for row in output.splitlines()]
    assert actual == expected, 'C++ sweep differs from original START.EXE'
    for case in cases[:9]:
        assert reference.run(*case, status=0x45) == reference.run(*case)
    timer = original_timer_schedule(launcher)
    manifest = dict(timer=timer, q2_sha256=Q2_SHA256, launcher_sha256=START_SHA256,
                    firmware_sha256=hashlib.sha256(firmware).hexdigest(), loader_traces=traces,
                    ignored_driver_failures=failures, sweep_cases=len(cases), sweep_ticks=len(expected),
                    effects=[dict(parameters=list(parameters), samples=samples) for parameters, samples in groups.items()],
                    scope='Original host control execution only; supplied driver replies, no CSP/DSP audio execution.')
    if args.output:
        args.output.write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Q2 uploads and startup commands agree for all four firmware rates; {len(cases)} sweep cases / {len(expected)} ticks agree with original START.EXE; 106 sample definitions grouped; 10,000 original PIT IRQs checked. No QSound audio execution claimed.')


if __name__ == '__main__':
    main()
