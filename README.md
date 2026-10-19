# QSound C++

Reconstruction of Creative Sound Blaster ASP/CSP QSound processing for modern C++ programs, initially for [TFX C++](https://github.com/Lost-Engines/tfx-cpp).

The intended integration is source inclusion: include the `qsound/` headers and add the corresponding `.cpp` files to the application's CMake target. There is no installed library, package manager, audio-device dependency, or DOS runtime.

**DSP execution is blocked on missing ST18932 instruction semantics; this is currently a firmware loader and research toolkit, not a working audio processor.** See the [exact blocker and ways to unblock it](docs/blocker.md). Original Creative firmware is external input and is not included in this repository. See [research notes](docs/research.md) for the evidence and remaining questions.

## Build and inspect

```sh
python3 tools/fetch_firmware.py
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
build/qsound_inspect firmware/WFM0001A.CSP --instructions
```

A C++26 compiler is required. Python is used only by the optional firmware download and offline extraction tests. The C++ component depends only on the standard library. See [firmware provenance and validation](docs/firmware.md).

For source inclusion in another project:

```cmake
include(path/to/qsound-cpp/qsound/sources.cmake)
target_sources(your_program PRIVATE ${QSOUND_SOURCES})
target_include_directories(your_program PRIVATE ${QSOUND_INCLUDE_DIR})
target_compile_features(your_program PRIVATE cxx_std_26)
```

For a compact report of instructions outside the older documented formats:

```sh
build/qsound_inspect firmware/WFM0001A.CSP --extensions
python3 tools/check_firmware.py firmware/WFM0001A.CSP
```

[Verification scope and results](docs/verification.md) distinguish file decoding from the DSP/audio work that remains.
