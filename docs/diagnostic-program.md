# Embedded CSP diagnostic program

The [Soundblaster DSP project discussion, replies 124–129](https://www.vogons.org/viewtopic.php?t=48732&start=120) adds two useful research inputs: a detailed host command protocol, and identification of a diagnostic CSP program embedded in the SB16 controller ROM. This is separate from the QSound firmware distributed in Creative's upgrade packages.

## Source and extraction

The [CT1741 firmware archive](https://github.com/S95Sedan/CT1741_DSP/tree/c64cff9a7730a6071f5166872c55b9a91640ac08) contains the original controller ROM and an annotated 8052 disassembly. The archive is not an ST18932 disassembler or emulator. Creative's firmware bytes remain research inputs and are not included in this repository.

Inspected input at that revision:

| Item | Value |
| --- | --- |
| ROM file | `firmware/v413-8k_e22e9001.bin` |
| ROM size | 8,192 bytes |
| ROM SHA-256 | `6cff63bc296f69fe153133fb1c528b3f3c5094d63dce149689c6432d8b8b7aaa` |
| Embedded program offset | `0x13a1` |
| Embedded program length | `0x3bc` = 956 bytes |
| Instructions | 238 32-bit words |
| Checksum | `0xfbe1` |
| Program identifier | `0x5555` |
| Extracted program SHA-256 | `9257db074641d7cd02c954a823177f1d98ee68ad5ed39910460fa317dd80d20e` |

The controller loader initialises a counter to `0x3bb`; this is not being treated as a verified transfer length. The 956-byte extraction follows the annotated `asp_code` array boundary and has the same encoded-byte checksum and identifier trailer format accepted by `qsound::decode_program`. Summing its 952 instruction bytes modulo 65,536 produces `0xfbe1`; the remaining bytes are `e1 fb 55 55`. The controller loader itself has not been executed or verified here.

The existing C++ inspector was also run against the extracted program in a temporary, synthetic RIFF wrapper. The wrapper only provided the container expected by that tool; its function descriptor was artificial, not metadata recovered from the ROM. All addresses below are instruction indices after byte unscrambling. No instructions were executed.

## What it contributes

In reply 129, mkarcher identifies the program's functions as RAM testing, block transfers, buffer negation, volume-controlled test output and ADC-to-DAC loopback. These are his reverse-engineering findings, not behaviours independently verified on hardware here. They offer much simpler candidate execution tests than the complete QSound filter chain.

The diagnostic has nine instructions outside the documented ST18930 format families, comprising six distinct words:

| Instruction addresses | Decoded word |
| --- | --- |
| `006` | `6821d1ff` |
| `007` | `688e70ff` |
| `022` | `688f70ff` |
| `06b`, `07a`, `088` | `688f40ff` |
| `08f`, `094` | `688f80ff` |
| `0e7` | `68600002` |

Its first two extension words differ from QSound's startup words `6821d9ff` and `688ef0ff` by `0x800` and `0x8000`, respectively. That gives concrete comparison points for recovering control-register fields. The bit differences alone do not identify the registers or establish what those bits control.

Reply 124 also documents host interface details relevant to a future executor: command `05` writes a low-byte/high-byte input word and signals the CSP; command `03` reads an output byte and signals the CSP; command `02` encodes a function request with high byte `f2`; commands `0b`/`0c` use firmware-defined block-transfer protocols. Commands `0e`/`0f` access the controller's X-Bus, while `f9`/`fa` access its internal RAM. These help model host interaction, not the DSP's arithmetic.

## Remaining limitation

This is a useful additional analysis target, not an independent execution oracle. We still need to establish the extension instructions and ST18932 arithmetic/control semantics described in [the blocker](blocker.md). The diagnostic's simpler algorithms can constrain interpretations and later exercise an executor; matching those functions alone would not prove QSound's rounding, saturation or filtering correct.
