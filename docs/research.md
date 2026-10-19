# Creative ASP/CSP research

## Scope

This project targets Creative's CT1748 ASP/CSP implementation of QSound, not Capcom's DSP16A implementation. A generic stereo widening approximation would not establish faithful TFX output. The aim is a bounded, deterministic, reusable C++26 implementation with no allocations or I/O in audio processing.

## Primary references

- [mkarcher's hardware and firmware research, 2022–2023](https://www.vogons.org/viewtopic.php?t=86739): ST18932 identity, instruction-byte permutation, program layout, RAM organisation, firmware patching and QSound processing observations. This is prior research, not code incorporated from that project.
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
