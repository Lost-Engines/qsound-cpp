#!/usr/bin/env python3
"""Check TFX effect-word transport and the linear-address position-33 hypothesis.

Optional research tool: requires Unicorn, the TFX C++ native_reference helper
and original TFX 2.05 files. Executing original sound requests verifies host
transport, not DSP arithmetic. The cold-RAM calculation is explicitly conditional
on the documented related-core loop and linear-address interpretations.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys

from check_firmware import decode_program
from check_spatial_tables import functions

ROOT = Path(__file__).resolve().parents[1]
SAMPLE_TABLE = 0x9860
DESCRIPTOR_BYTES = 26
DESCRIPTOR_COUNT = 106
DATA_FILE = 0xedaa4
POSITION_WORDS = 4
LAST_POSITION = 32


def descriptors(executable):
    for index in range(DESCRIPTOR_COUNT):
        offset = SAMPLE_TABLE + index * DESCRIPTOR_BYTES
        name, rate, cooldown, effect = struct.unpack_from('<IHHI', executable, DATA_FILE + offset)
        text = executable[DATA_FILE + name:DATA_FILE + name + 16].split(b'\0')[0].decode('ascii')
        yield dict(index=index, offset=offset, name=text, rate=rate,
                   words=list(struct.unpack_from('<HhH', executable, DATA_FILE + effect)))


def native_transport(research, native_tools):
    sys.path.insert(0, str(native_tools.resolve()))
    from native_reference import NativeReference
    from unicorn import UC_HOOK_INSN
    from unicorn import x86_const as registers
    image = NativeReference(research / 'installs/cd_1994_09/TFX/TFX.DAT')
    image.cpu.mem_map(0x300000, 0x2000)
    image.cpu.mem_map(0x91000, 0xf000)
    image.write(0x25cc8, '<I', 0x300000)
    image.write(0x20c70, '<I', 0x2000)
    image.write(0x20c74, '<I', 0x98000)
    # Supplied Sound Blaster readiness; original command construction executes.
    image.cpu.hook_add(UC_HOOK_INSN, lambda cpu, port, size, user: 0,
                      None, 1, 0, registers.UC_X86_INS_IN)
    image.cpu.hook_add(UC_HOOK_INSN, lambda cpu, port, size, value, user: None,
                      None, 1, 0, registers.UC_X86_INS_OUT)
    rows = list(descriptors(image.executable))
    for row in rows:
        at = row['offset']
        # Make each descriptor playable with a synthetic 16-byte low-memory
        # sample. Preserve the authored name/rate/cooldown/effect definition;
        # only loaded-sample fields and its runtime cooldown counter change.
        image.write(at + 12, '<h', 0)
        image.write(at + 14, '<IIHH', 16, 0x92000, 0, 1)
        image.cpu.mem_write(0x300000, bytes(512))
        image.cpu.mem_write(0x300002, struct.pack('<H', 1))  # previous stream idle
        image.cpu.mem_write(0x300016, struct.pack('<I', 0x98000))
        image.cpu.mem_write(0x98000, bytes(6))
        image.write(0xdfa8, '<B', 255)
        image.write(0xac85, '<B', 1)
        image.call(0xdcfa4, stack=(0, image.data + at, 0))
        actual = bytes(image.cpu.mem_read(0x98000, 6))
        assert actual == struct.pack('<HhH', *row['words']), (row, actual.hex())
    return rows


def cold_linear_reads(firmware):
    results = []
    for rate, function in zip((44100, 22050, 11025, 8000), functions(firmware)):
        initialiser = decode_program(function['init'])[0]
        processing = list(decode_program(function['main'])[0])
        tail = initialiser[-12:]
        assert tail[0] == 0xfc600084 and tail[1] == 0x742b0085
        assert tail[2] & 0xffffff00 == 0xca600000 and tail[3] == 0x04272108
        assert tail[10] == 0x68830000 and tail[11] >> 29 == 4
        # One parallel store / clear operation, repeated low-byte+1 times:
        # first store writes the previous accumulator (133), then zeros.
        count = (tail[2] & 255) + 1
        memory = {132 + index: 133 if index == 0 else 0 for index in range(count)}
        for index in (4, 6, 8):
            assert tail[index] & 0xffff0000 == 0x742b0000
            assert tail[index + 1] & 0xffff000f == 0x62800008
            memory[(tail[index + 1] >> 4) & 4095] = tail[index] & 65535
        # No clamp/comparison against 33 occurs in the first gain-copy block.
        assert processing[0xa8:0xab] == [0x78250020, 0x8a802d40, 0x4400000b]
        assert all(processing[index] == 0x05290100 for index in (0xab, 0xad, 0xaf, 0xb1))
        assert processing[0xb3:0xb5] == [0x6380002c, 0xa8080000]
        words = [memory[(LAST_POSITION + 1) * POSITION_WORDS + lane] for lane in range(POSITION_WORDS)]
        assert words == [133, 0, 0, 0]
        results.append(dict(rate=rate, position=33, address=132,
                            conditional_authored_order=words,
                            direct_left=words[1], direct_right=words[2],
                            filtered_left=words[0], filtered_right=words[3],
                            caveat='Cold state under the related-core loop and linear-address hypotheses, not an executed DSP result.'))
    assert len(results) == 4
    return results


def compare_builds(research, reference):
    path = research / 'analysis/engine-builds.json'
    if not path.exists():
        return []
    expected = {row['name']: row['words'] for row in reference if row['name']}
    results = []
    for build in json.loads(path.read_text())['builds']:
        source = research / build['source']
        data = source.read_bytes()
        assert hashlib.sha256(data).hexdigest() == build['sha256'], source
        base = build['sections'][2]['file']
        base = int(base, 16) if isinstance(base, str) else base
        actual = {row['name']: list(struct.unpack_from('<HhH', data, base + int(row['effect_pointer'], 16)))
                  for row in build['sample_descriptors']}
        assert actual == expected, build['name']
        results.append(dict(build=build['name'], executable_sha256=build['sha256'],
                            named_definitions=len(actual), identical_effect_words=True))
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--research', type=Path, default=ROOT.parent / 'tfx-reverse')
    parser.add_argument('--tfx-tools', type=Path, default=ROOT.parent / 'tfx-cpp/tools')
    parser.add_argument('--firmware', type=Path, default=ROOT / 'firmware/WFM0001A.CSP')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    rows = native_transport(args.research, args.tfx_tools)
    readings = cold_linear_reads(args.firmware.read_bytes())
    builds = compare_builds(args.research, rows)
    phases = Counter(row['words'][0] >> 8 for row in rows if not row['words'][2])
    assert phases[33] == 61 and phases[0] == 19
    report = dict(scope='Original host request execution; conditional DSP address analysis, not hardware audio verification.',
                  definitions=rows, static_position_counts=dict(sorted(phases.items())),
                  cold_linear_read_hypothesis=readings, compared_builds=builds,
                  practical_recommendation='Explicit endpoint clamp to 32 for position 33; a compatibility hypothesis, not verified original hardware behaviour.')
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f'{len(rows)} original sample requests publish their effect words unchanged, including {phases[33]} position-33 records.')
    print(f'All {len(builds)} retained engine inventories agree on the {len([row for row in rows if row["name"]])} named effect definitions.')
    print('All four initialisers give conditional cold linear row 33 = [133, 0, 0, 0]; this is not DSP execution.')


if __name__ == '__main__':
    main()
