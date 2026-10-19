# Creative ASP/CSP research

## Scope

This project targets Creative's CT1748 ASP/CSP implementation of QSound, not Capcom's DSP16A implementation. A generic stereo widening approximation would not establish faithful TFX output. The aim is a bounded, deterministic, reusable C++26 implementation with no allocations or I/O in audio processing.

## Primary references

- [mkarcher's hardware and firmware research, 2022–2023](https://www.vogons.org/viewtopic.php?t=86739): ST18932 identity, instruction-byte permutation, program layout, RAM organisation, firmware patching and QSound processing observations. This is prior research, not code incorporated from that project.
- [SB16 controller research, replies 124–129](https://www.vogons.org/viewtopic.php?t=48732&start=120): host command semantics and identification of an embedded diagnostic CSP program. That program has now been extracted and checksum-validated; see [diagnostic findings](diagnostic-program.md).
- [1989 SGS Modem Databook and Applications](https://bitsavers.informatik.uni-stuttgart.de/components/sgs/_dataBooks/1989_SGS_Modem_Databook_and_Applications.pdf), ST18930 datasheet starting at printed page 554: closely related processor with instruction encodings. Extra ST18932 capabilities must be established separately.
- [ALSA CSP loader/control source](https://github.com/torvalds/linux/blob/master/sound/isa/sb/sb16_csp.c) and [cspctl manual](https://github.com/alsa-project/alsa-tools/blob/master/sb16_csp/cspctl.1): Creative RIFF containers, rate selection and the host control protocol. These operate physical hardware; they are not a DSP emulator.
- [Creative CSP upgrade distribution](https://files.mpoli.fi/unpacked/hardware/sound/clabs/csp_upg.zip/): original firmware and DOS/Windows software to inspect locally, not redistribute as project source.
- [TFX dependency investigation](https://github.com/Lost-Engines/tfx-cpp/blob/master/tools/check_qsound_dependencies.py): the original `Q2.COM` expects `%SOUND%\CSP\WFM0001?.CSP` and the `CSPXXXX0` resident DOS device. It publishes an interface only after successful setup.

## Implementation stages

1. Obtain and inventory original firmware; validate its container and instruction decoding.
2. Establish instruction semantics and implement the executable subset with explicit unsupported-instruction errors.
3. Run setup and processing programs with a small PCM/control API.
4. Compare deterministic test signals and original TFX control calls, then document integration and remaining hardware verification limits.

Tests must distinguish format validation, instruction checks, original-code comparisons, and real-hardware audio comparisons. Passing one does not imply the others.

## Current result

Stage 1 is complete: the original firmware has been acquired, inventoried and checked against an independent decoder. Stage 2 reached a [specific instruction-semantics blocker](blocker.md). There is no DSP executor or PCM-processing implementation yet. The reusable C++ code is firmware loading and instruction-field inspection.

The separate ST18933 datasheet was also located and inspected. It confirms that using the older ST18930 execution model unchanged would be incorrect; the precise ST18932 specification it references has not been located.

## Additional source checks

The [SB16 chip-identification discussion](https://www.vogons.org/viewtopic.php?t=87824) corroborates the distinction between the CT1741 controller and CT1748 ASP/CSP, and links back to the existing CSP research. It does not publish extra instruction definitions.

The [SDK discussion](https://www.vogons.org/viewtopic.php?t=37435) led to the original [Developer Kit for Sound Blaster Series, second edition](http://www.dcee.net/Files/Programm/Sound/ctsbk2.zip). Its nested `SBK2.EXE` archive contains 206 entries, including `INCLUDE/CTMMSYS.H`. The original header was recovered and inspected, not incorporated into project source. It defines CSP device queries, open/close, firmware download and state control; the state values are inactive `0`, standby `1` and active `2`. The Creative Programmer's Guide chapter 6 and Library Reference chapter 3 describe these interfaces. The inspected header has no QSound API and does not define ST18932 instruction encodings. These are host driver interfaces, not DSP execution semantics.

For provenance, the downloaded outer SDK archive has SHA-256 `14261354702f0221e95d73f0c2e5d7dccbba620992a28d9da82c9497137e997d`; the extracted `CTMMSYS.H` has SHA-256 `90140857ef8e5e3c5d56693437a5ad2ad9fd63ef623973a1bf095df442172644`.

That SDK thread also speculates that ST18932 belongs to ST7/STM8. The [ST Assembler-Linker manual, UM0144 revision 4](https://www.st.com/resource/en/user_manual/cd00056470-st-assemblerlinker-stmicroelectronics.pdf#page=45), provides the actual ST18932 reference: page 45, table 16 describes OBSEND's ST S-record 4 output format as four bytes per address, with ST18932 program space as an example. Section 6.2.7 explains the word-oriented output formats. This documents a generic object formatter; it does not establish shared instruction sets or supply a DSP assembler's instruction table.

The supplied [Datasheet Archive query for ST18932-CORE](https://www.datasheetarchive.com/?q=ST18932-CORE) returned unrelated Cortex-M part highlights when checked. Searching for [ST18932 alone](https://www.datasheetarchive.com/?q=ST18932) produced contextual hits for ST18933, modem chips and the ST7/STM8 tool manuals. The relevant ST18933 material is already covered above; the ST7 tool hits are explained by the output-format reference. No ST18932 core specification was recovered from these results.

## TFX control reconstruction

The original loader and timer-controlled pan sweeps have now been traced through `Q2.COM`, `START.EXE` and all 106 sample definitions. The reusable C++ `position_sweep` agrees with 9,056 original callback executions; all four firmware selections and complete upload bytes are checked at the Creative driver boundary. This extends the implementation beyond firmware loading, without claiming DSP audio execution. See [TFX control findings](tfx-control.md).
