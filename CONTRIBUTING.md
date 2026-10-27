# Contributing

Improvements to integration, documentation, tests and the recovered signal path are welcome. Application integration is documented in the [usage guide](docs/usage.md); [research findings](docs/README.md#reverse-engineering-findings) explain the evidence behind the implementation.

## Build and check

Use CMake 3.30+ and a C++26 compiler:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug -j
ctest --test-dir build -C Debug --output-on-failure
```

These tests run offline and need no original firmware or game files. Python 3 enables the independent scalar audio comparisons and fetch-decoder test. For changes to firmware extraction or audio equations, also run the relevant [independent comparisons](docs/verification.md#firmware-and-audio-model-comparisons) with externally obtained firmware.

Follow the [style guide](style-guide.md) for touched code. Keep integration through `qsound/sources.cmake`, and keep allocation, file access and device handling outside the audio processing loop. Add focused regression coverage when changing behaviour.

## Reports and changes

For build failures, include the compiler, CMake version, platform, command and diagnostic. For audio differences, include the component revision or modified coefficient data, any external firmware SHA-256, the selected bank, input sample rate and format, source positions, arithmetic policies and whether the position controller is active. A short synthetic input or a recording you can share makes a comparison reproducible.

State which result is expected and what establishes it: original-code execution, a hardware capture, a published format or an application requirement. The analytical model's tests establish its equations; they do not establish original-card equivalence. Keep compatibility policies explicit when the hardware behaviour is unknown.

The recovered numerical defaults are included and editable; record intentional changes with their effects. Original firmware programs, game data and manuals remain external inputs. Keep tests based on synthetic fixtures where possible, and record hashes and acquisition details for optional original-data comparisons.

## Useful research

The main remaining evidence gaps are exact DSP arithmetic, circular addressing and the hardware interpretation of positions beyond 32. Original-card impulse responses, controlled position sweeps or authoritative ST18932 instruction documentation would help distinguish the current hypotheses. Record input samples, clock rates, firmware, controls and capture setup alongside any measurements. See [unresolved DSP semantics](docs/dsp-semantics.md) for the specific questions.
