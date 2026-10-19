#!/usr/bin/env python3
"""Offline format tests: no firmware redistribution and no network access."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("fetch_firmware", Path(__file__).resolve().parents[1] / "tools/fetch_firmware.py")
fetch_firmware = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetch_firmware)


class DecoderTest(unittest.TestCase):
    def test_published_dcl_example(self):
        # Ben Rudiak-Gould's public DCL example, corrected by Mark Adler.
        data = bytes.fromhex("00 04 82 24 25 8f 80 7f")
        self.assertEqual(fetch_firmware.first_dcl_member(data), b"AIAIAIAIAIAIA")

    def test_each_truncation(self):
        data = bytes.fromhex("00 04 82 24 25 8f 80 7f")
        for length in range(len(data)):
            with self.subTest(length=length), self.assertRaises(ValueError):
                fetch_firmware.first_dcl_member(data[:length])

    def test_output_bound(self):
        data = bytes.fromhex("00 04 82 24 25 8f 80 7f")
        with self.assertRaisesRegex(ValueError, "limit"):
            fetch_firmware.first_dcl_member(data, maximum_size=12)
        self.assertEqual(len(fetch_firmware.first_dcl_member(data, maximum_size=13)), 13)

    def test_headers_and_back_reference(self):
        for data in (b"\x01\x04", b"\x00\x03", b"\x00\x07", b"\x00\x04\x01\x00\x00"):
            with self.subTest(data=data), self.assertRaises(ValueError):
                fetch_firmware.first_dcl_member(data)

    def test_archive_hash_failure(self):
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            fetch_firmware.verified(b"not an archive", fetch_firmware.ARCHIVE_SHA256, "archive")


if __name__ == "__main__":
    unittest.main()
