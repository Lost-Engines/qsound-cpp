#!/usr/bin/env python3
"""Independently compare all original program words with the C++ inspector.

Requires external Creative firmware; no reference firmware bytes are committed.
The separate parser and bit-by-bit permutation do not import the C++ loader or
the fetching implementation. This checks decoding, not processor execution.
"""
import argparse
import hashlib
from pathlib import Path
import re
import struct
import subprocess


def chunks(data):
    offset = 0
    while offset < len(data):
        if len(data) - offset < 8:
            raise ValueError("short reference chunk")
        tag, size = struct.unpack_from("<4sI", data, offset)
        start = offset + 8
        end = start + size
        if end > len(data):
            raise ValueError("reference chunk exceeds parent")
        yield tag, data[start:end]
        offset = end + (size & 1)
    if offset != len(data):
        raise ValueError("missing reference padding")


def decode_program(data):
    encoded = data[:-4]
    checksum, identifier = struct.unpack("<HH", data[-4:])
    if sum(encoded) & 0xffff != checksum:
        raise ValueError("reference checksum mismatch")
    # Input bit i maps to output bit j; derived directly from the published
    # permutation rather than calling the production four-mask expression.
    # The firmware permutation swaps the bit pairs 0/5, 1/4, 2/7 and 3/6.
    mapping = (5, 4, 7, 6, 1, 0, 3, 2)
    decoded = bytes(sum(((byte >> source) & 1) << destination
                        for source, destination in enumerate(mapping)) for byte in encoded)
    words = struct.unpack("<" + "I" * (len(decoded) // 4), decoded)
    return words, checksum, identifier


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path)
    parser.add_argument("--inspect", type=Path, default=Path("build/qsound_inspect"))
    args = parser.parse_args()
    data = args.firmware.read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"CSP " or struct.unpack_from("<I", data, 4)[0] != len(data) - 8:
        raise ValueError("invalid reference container")
    programs = []
    for tag, value in chunks(data[12:]):
        if tag == b"LIST" and value[:4] == b"code":
            for kind, code in chunks(value[4:]):
                if kind in (b"init", b"main"):
                    words, checksum, identifier = decode_program(code)
                    programs.append((kind.decode(), words, checksum, identifier))
    output = subprocess.check_output([str(args.inspect.resolve()), str(args.firmware.resolve()), "--instructions"], text=True)
    actual = []
    for line in output.splitlines():
        header = re.fullmatch(r"(init|main): (\d+) instructions, checksum ([0-9a-f]{4}), identifier ([0-9a-f]{4})", line)
        if header:
            kind, count, checksum, identifier = header.groups()
            actual.append((kind, [], int(count), int(checksum, 16), int(identifier, 16)))
        instruction = re.match(r"  ([0-9a-f]{3}) ([0-9a-f]{8}) ", line)
        if instruction:
            address, word = map(lambda text: int(text, 16), instruction.groups())
            if not actual or address != len(actual[-1][1]):
                raise ValueError("C++ program listing has an unexpected instruction address")
            actual[-1][1].append(word)
    if len(actual) != len(programs):
        raise ValueError("different reference and C++ program counts")
    for reference, rendered in zip(programs, actual):
        kind, words, checksum, identifier = reference
        actual_kind, actual_words, actual_count, actual_checksum, actual_identifier = rendered
        if (kind, tuple(words), len(words), checksum, identifier) != (actual_kind, tuple(actual_words), actual_count, actual_checksum, actual_identifier):
            raise ValueError("reference and C++ program words or metadata differ")
    print(f"Verified {len(programs)} programs and {sum(len(p[1]) for p in programs)} instructions; firmware SHA-256 {hashlib.sha256(data).hexdigest()}. No processor execution or audio equivalence is claimed.")


if __name__ == "__main__":
    main()
