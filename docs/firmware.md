# Firmware acquisition and format

Run `python3 tools/fetch_firmware.py` from the repository root. It downloads Creative's archived `CSPU.ZIP`, opens `MAIN.PVL`, and decodes only its first PKWARE DCL member. No installer, emulator, external extractor or game assets are required. Existing correct firmware is verified; damaged copies are replaced atomically. Downloads retry transient failures.

The original firmware is written to ignored `firmware/WFM0001A.CSP`. It is not part of this project's source distribution. You can instead pass your own file to the tools.

| Input | SHA-256 |
| --- | --- |
| CSPU.ZIP | `2583d1e6f8cbbc9b273be578a82d696d02280ac9e36c664988ca245e65682995` |
| MAIN.PVL | `748b8e44c9971e8faeb835dca6141465e0f30d035690e6677e9f53d5da67f544` |
| WFM0001A.CSP (11,760 bytes) | `7ffd5f1b246cbda604d1dff17b052cab0eb367d5a09a3827ae02b14b6e23bdb8` |

The RIFF `INFO` records identify 5 August 1993, engineer Joe Ng, and QSound, fading and VU-meter effects.

| Function | Hz | Setup instructions | Processing instructions |
| --- | ---: | ---: | ---: |
| 1 | 44100 | 451 | 351 |
| 2 | 22050 | 331 | 351 |
| 3 | 11025 | 323 | 351 |
| 4 | 8000 | 299 | 351 |

The loader validates all parent and child RIFF bounds, even-byte chunk padding, function ordering, unique numbers, program capacities and checksums before decoding instructions. It reads integers explicitly as little-endian. Unknown metadata chunks are skipped within their enclosing list. The program checksum is the low 16 bits of the sum of its scrambled instruction bytes; the final word is a separate program identifier. All eight programs in the reference firmware validate.

`qsound_inspect firmware/WFM0001A.CSP --instructions` prints the decoded program words and the fields described by the ST18930/31 manual. It labels unrecognised formats as extensions, not as no-ops or guessed transfers. Inspection is not execution or proof of audio correctness.

The extraction tool's bounded binary-literal DCL decoder was checked against Mark Adler's independently compiled `blast` implementation on the original archive. Its offline tests also use the published DCL example, each input truncation and output-limit failures.
