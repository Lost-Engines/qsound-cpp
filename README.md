# QSound C++

Creative Sound Blaster ASP/CSP [QSound](https://en.wikipedia.org/wiki/QSound) spatial processing for modern C++ programs. Feed two signed 16-bit PCM sources in and receive stereo output, using filters and positioning tables recovered from Creative's original firmware. Used by [TFX C++](https://github.com/Lost-Engines/tfx-cpp).

The component provides 33 source positions, four firmware filter banks, persistent filter histories and an optional TFX movement controller. Editable coefficient banks are included; no firmware file or download is required. Audio processing allocates no memory and performs no I/O. It targets Creative's CT1748 implementation of QSound; the Capcom arcade implementation uses different hardware.

**The processor is usable now, with an explicit accuracy limit:** its signal path is reconstructed, but exact arithmetic and out-of-range position behaviour have not been verified against original hardware. Applications select those policies explicitly. See [verification](docs/verification.md) for what has been established.

## Use in your application

A **C++26 compiler** and the standard library are required. Keep this repository in your source tree and compile its sources into your existing target:

```cmake
include(path/to/qsound-cpp/qsound/sources.cmake)
target_sources(your_program PRIVATE ${QSOUND_SOURCES})
target_include_directories(your_program PRIVATE ${QSOUND_INCLUDE_DIR})
target_compile_features(your_program PRIVATE cxx_std_26)
```

The **[usage guide](docs/usage.md)** covers firmware loading, C++ examples, input formats, positioning, sample rates, streaming, threading and error handling. Your application supplies its mixer and audio device. Integration uses ordinary headers and sources; there is no separate library installation.

## Try it on a recording

From the repository root, with CMake 3.30+, a C++26 compiler, Python 3 and the `flac` command-line encoder:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
python3 tools/render_spatial_model.py sample.wav --output research/renders/sample
```

Supply your own uncompressed 8-bit or 16-bit PCM WAV. The renderer writes four maximum-compression stereo FLACs comparing the supported arithmetic choices, plus a manifest. Its default position is centre (16); use `--position 0` or `--position 32` for the endpoints. It defaults to TFX's 8 kHz filter bank and retains the WAV's playback rate; [rate selection and tool options](docs/usage.md#render-wav-input-to-flac) explain how to change that.

The [built-in coefficients](docs/coefficients.md) are named numerical tables you can edit or copy and customise per processor. The firmware fetcher and extraction tools remain available for optional research and verification; ordinary users do not need them.

## Documentation and contributions

- [Usage guide](docs/usage.md) — integrate and run the component.
- [Built-in coefficients](docs/coefficients.md) — customise the defaults or reproduce their extraction.
- [Documentation index](docs/README.md) — firmware details, verification and reverse-engineering findings.
- [Contributing](CONTRIBUTING.md) — build checks, reproducible reports and useful research contributions.

[Acknowledgements](NOTICE.md) record the prior research and format references. Original firmware and other external research inputs retain their own terms.
