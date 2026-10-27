#!/usr/bin/env python3
"""Check C++ table recovery against independent literal data-flow inspection.

Tracks immediate word provenance and parallel stores in the initializer. This
is not a DSP interpreter: opaque controls are not executed, hardware loops are
not run and the result is not claimed to be a complete initialized RAM image.
Only the explicitly authored coefficient/position literals are compared.
"""
import argparse
import json
from pathlib import Path
import random
import struct
import subprocess
import tempfile
from check_firmware import chunks, decode_program


def literal_writes(words):
    pointers = [None] * 8
    registers = dict(A=None, B=None, D=None)
    memory = dict(X={}, Y={}, E={})
    for pc, word in enumerate(words):
        if pc < 8:
            continue  # The reset vector branches directly to instruction 8.
        if word >> 30 == 3:
            first, second = word >> 27 & 7, word >> 24 & 7
            if first in (2, 3):
                pointers[first] = word >> 12 & 127
            if second in (0, 1, 4, 5, 6, 7):
                pointers[second] = word & 4095
        elif word >> 27 == 14:
            assert word >> 17 & 31 == 21
            registers['D'] = word & 65535
            if word >> 16 & 1:
                registers['A'] = word & 65535
        elif word >> 30 == 0:
            source, destination = word >> 12 & 7, word >> 1 & 7
            if destination in (2, 3, 4, 5, 6, 7):
                pointer = {2: 2, 3: 3, 4: 4, 5: 5, 6: 0, 7: 1}[destination]
                space = {2: 'Y', 3: 'Y', 4: 'E', 5: 'E', 6: 'X', 7: 'X'}[destination]
                value = registers.get({0: 'D', 2: 'A', 3: 'B'}.get(source))
                if pointers[pointer] is not None:
                    assert value is not None, (pc, word)
                    memory[space][pointers[pointer]] = value
            operation, target = word >> 17 & 31, word >> 15 & 3
            if operation == 19:
                registers['D'] = 0
                if target in (2, 3):
                    registers['AB'[target - 2]] = 0
            else:
                assert operation == 0
            used = set()
            if destination in (2, 3, 4, 5, 6, 7):
                used.add({2: 2, 3: 3, 4: 4, 5: 5, 6: 0, 7: 1}[destination])
            left, right = word >> 24 & 7, word >> 27 & 3
            if left in (0, 1, 2, 5, 6, 7):
                used.add({0: 0, 1: 1, 2: 2, 5: 5, 6: 6, 7: 7}[left])
            used.add({0: 0, 1: 4, 2: 2, 3: 3}[right])
            for pointer in used:
                shift = {0: 4, 1: 4, 2: 6, 3: 6, 4: 8, 5: 8, 6: 8, 7: 8}[pointer]
                if pointers[pointer] is not None:
                    pointers[pointer] += (0, 1, 0, -1)[word >> shift & 3]
        elif word >> 26 == 24:
            source, destination, address = word >> 22 & 15, word >> 1 & 7, word >> 4 & 4095
            if destination in (2, 4, 6):
                space = {2: 'Y', 4: 'E', 6: 'X'}[destination]
                value = registers.get({10: 'A', 12: 'D', 14: 'B'}.get(source))
                assert value is not None
                memory[space][address] = value
        elif word >> 27 == 13:
            # Opaque register controls invalidate arithmetic provenance. The
            # later table stores use freshly loaded immediates and clear B.
            registers = dict(A=None, B=None, D=None)
        elif word >> 29 != 4:
            raise ValueError(f'unhandled literal-provenance instruction {word:08x} at {pc:03x}')
    return memory


def signed(value):
    return value if value < 32768 else value - 65536


def functions(data):
    result = []
    for tag, value in chunks(data[12:]):
        if tag == b'LIST' and value[:4] == b'code':
            result.append({kind.decode(): code for kind, code in chunks(value[4:]) if kind in (b'init', b'main')})
    return result


def compare(data, result):
    original = functions(data)
    assert len(result) == len(original) == 4
    for row, function, rate, count, delay in zip(result, original, (44100, 22050, 11025, 8000), (49, 19, 17, 11), (24, 9, 8, 5)):
        memory = literal_writes(decode_program(function['init'])[0])
        assert row['rate'] == rate and row['direct_delay_samples'] == delay
        for name, address in [('left_filter', 25), ('right_filter', 25 + count)]:
            assert row[name] == dict(address=address, taps=[signed(memory['X'][address + i]) for i in range(count)])
        expected = []
        for position in range(33):
            words = [signed(memory['E'][position * 4 + lane]) for lane in range(4)]
            expected.append([words[1], words[2], words[0], words[3]])
        assert row['positions'] == expected
    assert all(row['positions'] == result[0]['positions'] for row in result)


def replace_instruction(data, program, address, replacement):
    offset = data.index(program)
    result = bytearray(data)
    word = struct.pack('<I', replacement)
    mapping = (5, 4, 7, 6, 1, 0, 3, 2)
    encoded = bytes(sum(((byte >> bit) & 1) << mapping[bit] for bit in range(8)) for byte in word)
    result[offset + address * 4:offset + address * 4 + 4] = encoded
    checksum = sum(result[offset:offset + len(program) - 4]) & 65535
    struct.pack_into('<H', result, offset + len(program) - 4, checksum)
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('firmware', type=Path)
    parser.add_argument('--compare-builtins', action='store_true', help='also require the built-in banks to match this firmware exactly')
    parser.add_argument('--tables', type=Path, default=Path('build/qsound_tables'))
    args = parser.parse_args()
    data = args.firmware.read_bytes()
    command = str(args.tables.resolve())
    compare(data, json.loads(subprocess.check_output([command, str(args.firmware.resolve())])))
    if args.compare_builtins:
        compare(data, json.loads(subprocess.check_output([command])))
        print('Built-in coefficients match the external firmware in all four banks.')
    randomizer = random.Random(0x435350)
    valid = rejected = 0
    with tempfile.TemporaryDirectory() as temporary:
        path = Path(temporary) / 'modified.csp'
        for function in functions(data):
            words = decode_program(function['init'])[0]
            # Change actual coefficient literals, retaining all instruction
            # fields and regenerating the encoded program checksum.
            first_filter = words.index(0xf8600019)
            for unused in range(8):
                value = randomizer.randrange(65536)
                changed = replace_instruction(data, function['init'], first_filter + 1, 0x742b0000 | value)
                path.write_bytes(changed)
                compare(changed, json.loads(subprocess.check_output([command, str(path)])))
                valid += 1
            for replacement in (0x1400201e, 0x74000000, 0x88000000):
                changed = replace_instruction(data, function['init'], first_filter + 2, replacement)
                path.write_bytes(changed)
                process = subprocess.run([command, str(path)], capture_output=True)
                assert process.returncode != 0 and b'unsupported QSound spatial-table instruction layout' in process.stderr
                rejected += 1
    print(f'All eight FIR banks and 4 x 33 position rows match independent literal data-flow; {valid} changed coefficients accepted and {rejected} unsupported layouts rejected. No DSP arithmetic or audio output checked.')


if __name__ == '__main__':
    main()
