#!/usr/bin/env python3
"""Fetch Creative's archived CSP upgrade and extract its QSound firmware.

Only Python's standard library is required. The original firmware remains a
separate, ignored asset; downloading it does not change its original licence.
PKWARE DCL coding tables follow the public format documented by Mark Adler's
blast (https://github.com/madler/zlib/tree/master/contrib/blast). The decoder
below is a purpose-limited implementation for this pinned binary-literal file.
"""
import argparse
import hashlib
import io
from pathlib import Path
import time
import urllib.request
import zipfile

URL = "https://files.mpoli.fi/hardware/SOUND/CLABS/CSPU.ZIP"
ARCHIVE_SHA256 = "2583d1e6f8cbbc9b273be578a82d696d02280ac9e36c664988ca245e65682995"
MEMBER_SHA256 = "748b8e44c9971e8faeb835dca6141465e0f30d035690e6677e9f53d5da67f544"
FIRMWARE_SHA256 = "7ffd5f1b246cbda604d1dff17b052cab0eb367d5a09a3827ae02b14b6e23bdb8"
FIRMWARE_SIZE = 11760
MAXIMUM_DOWNLOAD = 2 * 1024 * 1024


def verified(data, expected, label):
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError(f"{label}: SHA-256 mismatch")
    return data


class Bits:
    def __init__(self, data):
        self.data = data
        self.position = 0

    def read(self, count):
        if self.position + count > len(self.data) * 8:
            raise ValueError("truncated DCL stream")
        result = 0
        for shift in range(count):
            result |= ((self.data[self.position // 8] >> (self.position % 8)) & 1) << shift
            self.position += 1
        return result


def huffman_table(repeats):
    lengths = [item & 15 for item in repeats for _ in range((item >> 4) + 1)]
    table = {}
    code = 0
    for width in range(1, 14):
        for symbol, length in enumerate(lengths):
            if length == width:
                table[width, code] = symbol
                code += 1
        code <<= 1
    return table


LENGTH_CODES = huffman_table((2, 35, 36, 53, 38, 23))
DISTANCE_CODES = huffman_table((2, 20, 53, 230, 247, 151, 248))
LENGTH_BASES = (3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264)
LENGTH_EXTRAS = (0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8)


def symbol(bits, table):
    code = 0
    for width in range(1, 14):
        code = (code << 1) | (bits.read(1) ^ 1)
        value = table.get((width, code))
        if value is not None:
            return value
    raise ValueError("invalid DCL Huffman code")


def first_dcl_member(data, maximum_size=FIRMWARE_SIZE):
    """Decode one bounded binary-literal stream; never unpack arbitrary paths."""
    bits = Bits(data)
    if bits.read(8) != 0:
        raise ValueError("expected binary-literal DCL stream")
    dictionary = bits.read(8)
    if dictionary not in (4, 5, 6):
        raise ValueError("invalid DCL dictionary size")
    output = bytearray()
    while True:
        if not bits.read(1):
            if len(output) == maximum_size:
                raise ValueError("DCL output exceeds the extraction limit")
            output.append(bits.read(8))
            continue
        code = symbol(bits, LENGTH_CODES)
        length = LENGTH_BASES[code] + bits.read(LENGTH_EXTRAS[code])
        if length == 519:
            return bytes(output)
        extra = 2 if length == 2 else dictionary
        distance = (symbol(bits, DISTANCE_CODES) << extra) + bits.read(extra) + 1
        if distance > len(output):
            raise ValueError("DCL back-reference precedes the output")
        if length > maximum_size - len(output):
            raise ValueError("DCL output exceeds the extraction limit")
        for _ in range(length):
            output.append(output[-distance])


def fetch():
    for attempt in range(5):
        try:
            request = urllib.request.Request(URL, headers={"User-Agent": "Lost Engines QSound CPP"})
            with urllib.request.urlopen(request, timeout=30) as response:
                data = response.read(MAXIMUM_DOWNLOAD + 1)
            if len(data) > MAXIMUM_DOWNLOAD:
                raise ValueError("download exceeds the expected archive limit")
            return verified(data, ARCHIVE_SHA256, "Creative upgrade archive")
        except (OSError, ValueError):
            if attempt == 4:
                raise
            time.sleep(min(2 ** attempt, 8))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, help="use an already downloaded CSPU.ZIP")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "firmware/WFM0001A.CSP")
    args = parser.parse_args()
    if args.output.is_file() and hashlib.sha256(args.output.read_bytes()).hexdigest() == FIRMWARE_SHA256:
        print(f"Verified {args.output}")
        return
    if args.archive:
        if args.archive.stat().st_size > MAXIMUM_DOWNLOAD:
            raise ValueError("archive exceeds the extraction limit")
        data = verified(args.archive.read_bytes(), ARCHIVE_SHA256, "Creative upgrade archive")
    else:
        data = fetch()
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        member = archive.getinfo("MAIN.PVL")
        if member.file_size != 809073:
            raise ValueError("unexpected Creative MAIN.PVL size")
        compressed = verified(archive.read(member), MEMBER_SHA256, "Creative MAIN.PVL")
    firmware = verified(first_dcl_member(compressed), FIRMWARE_SHA256, "QSound firmware")
    if len(firmware) != FIRMWARE_SIZE:
        raise ValueError("unexpected QSound firmware size")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_name(args.output.name + ".partial")
    try:
        temporary.write_bytes(firmware)
        temporary.replace(args.output)
    finally:
        temporary.unlink(missing_ok=True)
    print(f"Extracted and verified {args.output} ({len(firmware)} bytes)")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, zipfile.BadZipFile, KeyError) as error:
        raise SystemExit(f"Firmware fetch failed: {error}") from error
