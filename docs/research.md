# Creative ASP/CSP research

## Scope

This project targets Creative's CT1748 ASP/CSP implementation of QSound, not Capcom's DSP16A implementation. A generic stereo widening approximation would not establish faithful TFX output. The aim is a bounded, deterministic, reusable C++26 implementation with no allocations or I/O in audio processing.

## Primary references

- [mkarcher's hardware and firmware research, 2022–2023](https://www.vogons.org/viewtopic.php?t=86739): ST18932 identity, instruction-byte permutation, program layout, RAM organisation, firmware patching and QSound processing observations. This is prior research, not code incorporated from that project.
- [SB16 controller research, replies 124–129](https://www.vogons.org/viewtopic.php?t=48732&start=120): host command semantics and identification of an embedded diagnostic CSP program. That program was extracted and checksum-validated; see [diagnostic findings](diagnostic-program.md).
- [1989 SGS Modem Databook and Applications](https://bitsavers.informatik.uni-stuttgart.de/components/sgs/_dataBooks/1989_SGS_Modem_Databook_and_Applications.pdf), ST18930 datasheet starting at printed page 554: closely related processor with instruction encodings. Extra ST18932 capabilities must be established separately.
- [ALSA CSP loader/control source](https://github.com/torvalds/linux/blob/master/sound/isa/sb/sb16_csp.c) and [cspctl manual](https://github.com/alsa-project/alsa-tools/blob/master/sb16_csp/cspctl.1): Creative RIFF containers, rate selection and the host control protocol. These operate physical hardware; they are not a DSP emulator.
- [Creative CSP upgrade distribution](https://files.mpoli.fi/unpacked/hardware/sound/clabs/csp_upg.zip/): original firmware and DOS/Windows software to inspect locally, not redistribute as project source.
- [TFX dependency investigation](https://github.com/Lost-Engines/tfx-cpp/blob/master/tools/check_qsound_dependencies.py): the original `Q2.COM` expects `%SOUND%\CSP\WFM0001?.CSP` and the `CSPXXXX0` resident DOS device. It publishes an interface only after successful setup.

## Current implementation

The component decodes original firmware, recovers its spatial tables, processes stereo PCM and reproduces TFX's position controller. TFX C++ uses this source directly. Arithmetic choices and endpoint compatibility are explicit application policies.

There is no general ST18932 instruction executor. The [remaining instruction and arithmetic questions](dsp-semantics.md) limit claims of original-card equivalence, while [verification](verification.md) records the independent data/model comparisons and original x86 traces supporting the implementation.

## Additional source checks

The [SB16 chip-identification discussion](https://www.vogons.org/viewtopic.php?t=87824) corroborates the distinction between the CT1741 controller and CT1748 ASP/CSP, and links back to the existing CSP research. It does not publish extra instruction definitions.

The [SDK discussion](https://www.vogons.org/viewtopic.php?t=37435) led to the original [Developer Kit for Sound Blaster Series, second edition](http://www.dcee.net/Files/Programm/Sound/ctsbk2.zip). Its nested `SBK2.EXE` archive contains 206 entries, including `INCLUDE/CTMMSYS.H`. The original header was recovered and inspected, not incorporated into project source. It defines CSP device queries, open/close, firmware download and state control; the state values are inactive `0`, standby `1` and active `2`. The Creative Programmer's Guide chapter 6 and Library Reference chapter 3 describe these interfaces. The inspected header has no QSound API and does not define ST18932 instruction encodings. These are host driver interfaces, not DSP execution semantics.

For provenance, the downloaded outer SDK archive has SHA-256 `14261354702f0221e95d73f0c2e5d7dccbba620992a28d9da82c9497137e997d`; the extracted `CTMMSYS.H` has SHA-256 `90140857ef8e5e3c5d56693437a5ad2ad9fd63ef623973a1bf095df442172644`.

That SDK thread also speculates that ST18932 belongs to ST7/STM8. The [ST Assembler-Linker manual, UM0144 revision 4](https://www.st.com/resource/en/user_manual/cd00056470-st-assemblerlinker-stmicroelectronics.pdf#page=45), provides the actual ST18932 reference: page 45, table 16 describes OBSEND's ST S-record 4 output format as four bytes per address, with ST18932 program space as an example. Section 6.2.7 explains the word-oriented output formats. This documents a generic object formatter; it does not establish shared instruction sets or supply a DSP assembler's instruction table.

The [Datasheet Archive query for ST18932-CORE](https://www.datasheetarchive.com/?q=ST18932-CORE) returned unrelated Cortex-M part highlights when checked. Searching for [ST18932 alone](https://www.datasheetarchive.com/?q=ST18932) produced contextual hits for ST18933, modem chips and the ST7/STM8 tool manuals. The relevant ST18933 material is already covered above; the ST7 tool hits are explained by the output-format reference. No ST18932 core specification was recovered from these results.

## Findings by topic

- [TFX host control](tfx-control.md): loader setup, effect definitions, unsigned wrapping and original timer scheduling.
- [Spatial program](spatial-program.md): FIR banks, position gains, direct delays and the recovered two-input signal path.
- [Audio model](spatial-model.md): implemented equations, explicit arithmetic choices and comparisons on original sample recordings.
- [Creative driver](creative-driver.md): both original far APIs transmit the full parameter byte unchanged, including position 33.
- [Position 33](position-33.md): request-side transport, definitions shared by retained TFX builds, conditional memory interpretation and endpoint compatibility policy.
- [Diagnostic program](diagnostic-program.md): a simpler CSP program embedded in the SB16 controller ROM.

The companion A-law, mu-law and IMA firmware programs share startup/control extensions and add variable shifts, normalisation operations and further control words. Their containers decode correctly, but recognising their codec formats does not establish the missing instruction semantics.
