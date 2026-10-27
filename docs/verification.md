# Verification

The component has offline regression tests, independent firmware/audio-model comparisons and original x86 control-code traces. These establish different things:

| Area | Evidence | Limit |
| --- | --- | --- |
| Firmware decoding and spatial tables | Independent readers, original files, checksums and mutations. | Reading instructions and constants does not execute the DSP. |
| TFX movement and Creative driver parameters | Original x86 routines executed with explicit DOS/hardware responses. | The DSP itself is not part of these traces. |
| Audio processor | Independent scalar equations and synthetic signal tests. | Agreement establishes the implemented model, not original-card equivalence. |
| Original hardware audio | No reference capture is available. | Exact DSP arithmetic, addressing and out-of-table positioning remain unresolved. |

TFX C++ uses the processor with floor rounding, saturation after each multiply/accumulate and explicit endpoint clamping. Those choices are documented hypotheses; see [audio-model scope](spatial-model.md) and [unresolved DSP semantics](dsp-semantics.md).

## Offline regression tests

Run from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug -j
ctest --test-dir build -C Debug --output-on-failure
```

The C++ tests use included coefficients and synthetic data. Independent scalar audio comparisons and the Python fetch-decoder test are enabled when Python is available. None requires network access, original firmware, a game installation or an emulator.

| Test | Coverage |
| --- | --- |
| `firmware.formats_and_bounds` | Instruction permutation, known encoded word, endian conversion, checksums, capacity limits, malformed/truncated RIFF data, unknown chunks and rate selection. |
| `firmware.upgrade_dcl` | Published overlapping-copy DCL example, native unpacking, truncation, malformed streams and extraction limits. |
| `firmware.fetch_decoder` | Independent Python extraction, truncation, malformed headers/back-references, output limits and hash rejection. |
| `model.arithmetic_and_history` | Cancellation, delay, tap ordering, reset, strict positioning and every byte value under the explicit clamp policy. |
| `model.reference_equations` | 48 cases / 27,936 frames across all built-in banks and arithmetic policies, checked against independent Python equations. |
| `control.position_sweep` | Fractional phase, signed increments, wrapping and publication-cache behaviour. |

Recorded Linux checks use GCC 16.2 with C++26: Debug, Release and AddressSanitizer/UndefinedBehaviorSanitizer builds pass all six tests. For sanitizers, configure a separate build directory with `-fsanitize=address,undefined` in both `CMAKE_CXX_FLAGS` and `CMAKE_EXE_LINKER_FLAGS`. Assertions used by tests remain enabled in release configurations.

## Firmware and audio-model comparisons

Built-in model comparisons run in ordinary CTest. The following optional extraction checks additionally require external firmware; they use Python's standard library and the built tools. They do not require a TFX installation or Unicorn:

```sh
python3 tools/fetch_firmware.py
python3 tools/check_firmware.py firmware/WFM0001A.CSP
python3 tools/check_spatial_tables.py firmware/WFM0001A.CSP --compare-builtins
python3 tools/check_spatial_model.py firmware/WFM0001A.CSP
```

Use each script's `--help` for executable-path overrides when using a different build directory. On generators that put executables under a configuration directory, supply those paths explicitly, for example `--inspect build/Release/qsound_inspect.exe` to the firmware checker.

Recorded results:

- All four firmware functions, eight setup/processing programs, 2,808 decoded instructions and program checksums agree with an independent parser and bit-by-bit permutation. The two archived QSound containers have identical corresponding decoded instructions despite different metadata.
- All eight FIR banks and all 132 position-gain words in each function agree with independent Python literal data-flow. Thirty-two coefficient mutations are recovered and twelve unsupported layout mutations are rejected after regenerating checksums.
- Forty-eight audio cases, totalling 27,936 stereo frames across four banks and four arithmetic policies, agree with independent scalar equations. Inputs cover impulses, noise, clipping and tails; malformed PCM and unsupported positions are rejected.
- Native `MAIN.PVL` extraction reproduces all tables in the independently fetched CSP. Both asset forms pass through the same RIFF and program-checksum validation.
- The fetch script has been exercised against the archived download, verifying archive/member/output hashes and reuse of correct existing output. This network check is separate from offline CTest.
- Additional Creative A-law, mu-law, ADPCM, IMA ADPCM and FastSpeech containers pass structural decoding. Those codecs are not implemented by the audio processor.

The [FLAC audition tool](usage.md#render-wav-input-to-flac) compares arithmetic choices on recordings. [Listening notes](spatial-model.md#offline-auditions) describe the measured differences in original TFX samples; the recordings and renders are external research inputs.

## Original-code comparisons

These are research tools, not installation or build requirements. They need external originals, and some default to the author's neighbouring TFX research checkout. Override those locations for your own environment.

| Tool | Additional inputs | Established result |
| --- | --- | --- |
| `tools/check_tfx_control.py` | Unicorn; September 1994 TFX install (`--install`), firmware (`--firmware`), built controller probe (`--probe`). | 725 cases / 9,056 original sweep callbacks, all 106 sample descriptors, all four firmware uploads, startup commands and ignored driver-error replies. A further 10,000 original PIT interrupts establish the inspected callback cadence. |
| `tools/check_creative_driver.py` | Unicorn; pinned Creative upgrade ZIP (`--archive`) or extracted `CSP.SYS` (`--driver`). | Both driver APIs: 2,560 parameter calls over the full byte range, allocation, command narrowing and 13 failure cases. |
| `tools/investigate_positions.py` | Unicorn; TFX research tree (`--research`) with retained original installs/inventory, plus TFX C++'s `tools/native_reference.py` and its dependencies (`--tfx-tools`). | All 106 sound requests preserve authored effect words. Eight engine inventories agree for their 103 named samples. Initializer inspection constrains a conditional position-33 memory read. |

The controller and driver traces supply DOS I/O, sample metadata and explicit hardware status replies as described in [TFX control](tfx-control.md), [Creative driver](creative-driver.md) and [position 33](position-33.md). They execute original x86 instructions; they do not execute the firmware or capture card audio.

## Documentation examples

The usage guide's runtime CMake inclusion and C++ snippets are checked without firmware or research sources across all four banks. Optional CSP and PVL extraction checks compare every built-in coefficient and delay. Whole-buffer and split-buffer output agreed, and the documented tail flush reached silence. The inspection, raw-PCM, controller and FLAC-render commands were also exercised. These checks validate the documented integration path.

The Windows PCM probe was cross-built with MinGW GCC 16 and run under Wine. A 65,792-byte stream containing every byte value, including CR, LF and Ctrl-Z, produced output identical to the native Linux tool; a partial frame was rejected. This checks binary stream transport and the compiled processor, not physical-card fidelity. Renderer executable discovery and missing-input/tool diagnostics were also checked.
