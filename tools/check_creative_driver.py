#!/usr/bin/env python3
"""Trace original Creative CSP.SYS parameter writes with supplied SB16 replies.

Requires Unicorn and either a pinned Creative upgrade ZIP or its CSP.SYS.
Executes the driver's allocation and parameter API; does not emulate the DSP.
No original driver bytes are bundled. Hardware status/echo replies, port
configuration, handle seed and delay calibration are explicit test inputs.
"""
import argparse
import io
import hashlib
import json
from pathlib import Path
import struct
import zipfile

from fetch_firmware import ARCHIVE_SHA256, MEMBER_SHA256, first_dcl_member, verified

DRIVER_SHA256 = '43e1267997574ab3a36bc6453a98453cd10676e3caed64ab6e2a9c7ccfc2fa91'
DRIVERS = {
    DRIVER_SHA256: dict(name='1994 MZ driver', size=10873, header=32, entry=0x1ce,
                       ports=0x603, seed=0x49e, delay=0x488, busy=0x6e),
    'af1e6d52cecc49cbe5e884f6a706c6dd7c74b3fc0ab34558993dc6af65e17c5d':
        dict(name='1993 flat driver', size=6599, header=0, entry=0x218,
             ports=0x44c, seed=0x464, delay=0x458, busy=None),
}
ARCHIVES = {
    ARCHIVE_SHA256: dict(member='MAIN.PVL', sha256=MEMBER_SHA256, start=0x5ac9, end=0x74a2),
    'a3ac99806573ad9e51964788fbc8f9a54947df85a1d1f6d2c6b3aa248fafbb38':
        dict(member='INSTALL.001', sha256='97bbe40e66beb4ba619ab52543240239af60a9b8742e68323cf46263f91d342f',
             start=0x8e21, end=0x9f48),
}
IMAGE_BASE = 0x10000
CALLER_STACK = 0x2ff00
RETURN_ADDRESS = 0x70000
WRITE_PORT = 0x22c
READ_PORT = 0x22a
STATUS_PORT = 0x22e
HANDLE = 0x42


def driver_bytes(args):
    if args.archive:
        data = args.archive.read_bytes()
        descriptor = ARCHIVES.get(hashlib.sha256(data).hexdigest())
        if descriptor is None:
            raise ValueError('unrecognised Creative upgrade archive SHA-256')
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            member = verified(archive.read(descriptor['member']), descriptor['sha256'], 'Creative member')
        # Pinned PVM directory bounds: extract only the bounded DCL stream.
        driver = first_dcl_member(member[descriptor['start']:descriptor['end']], maximum_size=10873)
    else:
        driver = args.driver.read_bytes()
    digest = hashlib.sha256(driver).hexdigest()
    descriptor = DRIVERS.get(digest)
    if descriptor is None or len(driver) != descriptor['size']:
        raise ValueError('unrecognised Creative CSP.SYS SHA-256/size')
    if descriptor['header']:
        if driver[:2] != b'MZ' or struct.unpack_from('<H', driver, 6)[0] != 0:
            raise ValueError('expected the relocation-free original MZ driver')
        assert struct.unpack_from('<H', driver, 8)[0] * 16 == descriptor['header']
    return driver[descriptor['header']:], descriptor, digest


class Driver:
    def __init__(self, image, descriptor):
        from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_INSN
        import unicorn.x86_const as registers
        self.registers = registers
        self.descriptor = descriptor
        self.cpu = Uc(UC_ARCH_X86, UC_MODE_16)
        self.cpu.mem_map(0, 0x100000)
        self.cpu.mem_write(IMAGE_BASE, image)
        # Supply the post-initialisation port configuration, not DOS probing.
        for index, port in enumerate((READ_PORT, WRITE_PORT, STATUS_PORT)):
            address = descriptor['ports'] + index * 2
            self.cpu.mem_write(IMAGE_BASE + address, struct.pack('<H', port))
        self.cpu.mem_write(IMAGE_BASE + descriptor['seed'], bytes([HANDLE]))
        self.cpu.mem_write(IMAGE_BASE + descriptor['delay'], bytes(4))  # zero delay-loop calibration
        self.snapshot = bytes(self.cpu.mem_read(IMAGE_BASE, 0x10000))
        self.writes = []
        self.reads = []
        self.echo_delta = 0
        self.cpu.hook_add(UC_HOOK_INSN, self.write, None, 1, 0, registers.UC_X86_INS_OUT)
        self.cpu.hook_add(UC_HOOK_INSN, self.read, None, 1, 0, registers.UC_X86_INS_IN)

    def write(self, cpu, port, size, value, user):
        assert port == WRITE_PORT and size == 1, (port, size, value)
        self.writes.append(value)

    def read(self, cpu, port, size, user):
        assert size == 1
        self.reads.append(port)
        if port == WRITE_PORT:
            return 0  # DSP accepts a command byte
        if port == STATUS_PORT:
            return 0x80  # response available, if this path uses polling
        if port == READ_PORT:
            assert len(self.writes) == 4
            return (self.writes[2] + self.echo_delta) & 255
        raise AssertionError(f'unexpected original-driver port read: {port:#x}')

    def call(self, function, handle=HANDLE, parameter=0, operand=0, echo_delta=0, busy=False):
        self.writes.clear()
        self.reads.clear()
        self.echo_delta = echo_delta
        self.cpu.mem_write(IMAGE_BASE, self.snapshot)
        if busy:
            assert self.descriptor['busy'] is not None
            self.cpu.mem_write(IMAGE_BASE + self.descriptor['busy'], b'\1')
        self.cpu.mem_write(CALLER_STACK, struct.pack('<HHIIII',
            0, RETURN_ADDRESS >> 4, function, handle, parameter, operand))
        r = self.registers
        for register, value in ((r.UC_X86_REG_CS, IMAGE_BASE >> 4),
                                (r.UC_X86_REG_DS, IMAGE_BASE >> 4),
                                (r.UC_X86_REG_ES, IMAGE_BASE >> 4),
                                (r.UC_X86_REG_SS, 0x2000), (r.UC_X86_REG_SP, 0xff00),
                                (r.UC_X86_REG_EFLAGS, 2)):
            self.cpu.reg_write(register, value)
        self.cpu.emu_start(IMAGE_BASE + self.descriptor['entry'], RETURN_ADDRESS, count=20000)
        pc = self.cpu.reg_read(r.UC_X86_REG_CS) * 16 + self.cpu.reg_read(r.UC_X86_REG_IP)
        assert pc == RETURN_ADDRESS, f'original driver did not return: {pc:#x}'
        return (self.cpu.reg_read(r.UC_X86_REG_DX) << 16) | self.cpu.reg_read(r.UC_X86_REG_AX)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--archive', type=Path, help='original pinned CSPU.ZIP or CSP_UPG.ZIP')
    source.add_argument('--driver', type=Path, help='original pinned CSP.SYS')
    parser.add_argument('--report', type=Path, help='optional local JSON manifest')
    args = parser.parse_args()
    image, descriptor, digest = driver_bytes(args)
    driver = Driver(image, descriptor)
    assert driver.call(0x10000, parameter=0x30000000) == 0
    assert bytes(driver.cpu.mem_read(0x30000, 4)) == struct.pack('<I', HANDLE)
    assert not driver.writes
    driver.snapshot = bytes(driver.cpu.mem_read(IMAGE_BASE, 0x10000))
    cases = 0
    for command in (0, 1, 2, 3, 0xe0):
        for value in range(256):
            assert driver.call(0x80000, parameter=command, operand=value) == 0, (command, value)
            assert driver.writes == [5, value, command, 3], (command, value, driver.writes)
            assert driver.reads == [WRITE_PORT] * 4 + [READ_PORT], driver.reads
            cases += 1
    # Boundary and driver-failure cases, with original return statuses.
    failures = [dict(operand=256), dict(operand=0x10000), dict(parameter=0x80),
                dict(handle=HANDLE + 1), dict(handle=0x10000 | HANDLE),
                dict(echo_delta=1)]
    statuses = [8, 8, 8, 5, 5, 1]
    if descriptor['busy'] is not None:
        failures.append(dict(busy=True))
        statuses.append(1)
    for inputs, expected in zip(failures, statuses):
        assert driver.call(0x80000, **inputs) == expected, inputs
        assert bool(driver.writes) == ('echo_delta' in inputs), inputs
    # Command arguments are otherwise narrowed to their low byte; only the
    # exact 32-bit argument 0x80 receives the special rejection above.
    assert driver.call(0x80000, parameter=0x10080, operand=33) == 0
    assert driver.writes == [5, 33, 0x80, 3]
    manifest = dict(driver_sha256=digest, driver=descriptor['name'], allocation_cases=1,
                    parameter_cases=cases, failure_cases=len(failures), narrowing_cases=1,
                    scope='Original x86 driver execution with explicit supplied SB16 status/echo responses; no DSP execution.',
                    parameter_byte_transform='None: all 256 values pass unchanged for commands 00, 01, 02, 03 and e0.',
                    position_33='Passed unchanged to the DSP; no driver-side special mapping.')
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(manifest, indent=2) + '\n')
    print(f"Original CSP.SYS ({descriptor['name']}): {cases} unchanged parameter writes, allocation, {len(failures)} failure cases and command narrowing verified.")
    print('Position 33 and larger sweep positions are passed unchanged. Supplied hardware replies; no DSP execution.')


if __name__ == '__main__':
    main()
