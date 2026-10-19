# DSP execution is blocked on ST18932 semantics

The repository currently implements firmware loading and inspection, not QSound audio processing. It is not ready to enable QSound in TFX. There is deliberately no bypass or approximate stereo filter presented as the original effect.

## Reproducer

```sh
python3 tools/fetch_firmware.py
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
build/qsound_inspect firmware/WFM0001A.CSP --extensions
```

All four initialisers and all four processing programs start with decoded word `88000200`, whose ST18930 immediate-branch fields select address `0008`. At that location they all contain `6821d9ff`, followed by `688ef0ff` at `0009`. Neither matches any format specified by the available ST18930/31 opcode tables (datasheet pages 31–38). This is static instruction inspection, not a trace from an executing DSP.

The reference contains these eight distinct words outside those documented formats:

| Decoded word | Occurrences across all eight programs |
| --- | ---: |
| `6600000c` | 4 |
| `6821d9ff` | 8 |
| `68600400` | 16 |
| `68604400` | 16 |
| `68800030` | 8 |
| `68803030` | 4 |
| `68830000` | 8 |
| `688ef0ff` | 8 |

They cannot simply be skipped: some are in startup, others surround the filter calculations and the command-handling paths. A recognised instruction prefix also does not establish all of its ST18932 semantics.

## Why the related manual is insufficient

The [ST18933 datasheet](https://www.datasheetarchive.com/datasheet/ST18933/STMicroelectronics?version=1), February 1993, describes its ST18932 core on page 6: a 32-bit ALU and shifter, four 32-bit accumulators, signed/unsigned multiplication, a two-level return stack and additional addressing facilities. These differ materially from the older ST18930/31. Its programming examples use extra controls such as `rini csta form,nocpr`, but do not give their binary encodings. Those examples establish that more controls exist, not how to execute the Creative firmware.

The ST18933 datasheet refers to **ST18932 CORE & EMULATION CHIP SPECIFICATION** and technical manual **ST18932TM/2**. Neither was located during this investigation. The ST18940 material in the 1989 modem databook likewise refers detailed instruction questions to a separate programming manual.

[mkarcher's 2022 research](https://www.vogons.org/viewtopic.php?t=86739) establishes that enough of this core can be recovered to write a disassembler and patch firmware on real hardware. The linked discussion does not publish the complete disassembler or instruction notes; its June 2023 update says the documentation/tooling was not a high priority. Existing ALSA code establishes how to load programs and send parameters, not the DSP arithmetic.

## What would unblock execution

Any of these would materially advance the work:

1. The ST18932 specification, technical manual or original assembler/simulator tools.
2. mkarcher's disassembler and notes, especially the extra instruction families, accumulator formatting and control registers.
3. Access to a working CT1748-equipped SB16/AWE32 for controlled upload/readback tests. That would let us distinguish competing instruction interpretations and capture impulse responses. Hardware sound recordings alone would help audio verification, but not necessarily identify every internal instruction effect.

No message has been sent to the researcher. No real-hardware audio comparison is claimed. The original instructions and available manuals alone do not presently establish those missing behaviours, and a guessed implementation would not provide a faithful reference.

## Material retained locally

Ignored `research/` holds the two Creative upgrade archives, the 1989 modem databook, ST18933 datasheet, the published research discussion and ALSA source consulted. `firmware/` holds the fetched reference QSound container. These are research inputs, not redistributed project source. The two archived QSound versions inspected have byte-identical decoded instructions in all eight programs; their container metadata differs. Using the older firmware does not avoid the missing instruction families.
