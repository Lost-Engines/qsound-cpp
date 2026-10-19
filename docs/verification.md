# Verification status

The checked implementation is firmware decoding, instruction inspection and TFX pan-sweep control. **There is no QSound DSP execution or audio output yet.** The [ST18932 research blocker](blocker.md) prevents claiming either.

## Checks completed

- GCC 16.2, C++26: Debug and Release builds, with warnings enabled.
- Both offline CTests pass in Debug, Release and an AddressSanitizer/UndefinedBehaviorSanitizer build. The final sanitizer run enabled leak detection and halted on undefined behaviour.
- Firmware fetch performed over the network, verified its archive/member/output SHA-256 values, and verified the existing output on a second run.
- The reference QSound file: all four functions, all eight setup/processing programs, all 2,808 decoded instruction words and all program checksums agree with a separate Python reader and bit-by-bit permutation. The original archived 1993 upgrade version passes the same comparison.
- The two recovered QSound containers have identical decoded instructions in each corresponding program.
- The C++ loader also accepts the newer upgrade's A-law, mu-law, Creative ADPCM, IMA ADPCM and two FastSpeech containers. This is container/program validation only; none of those codecs is emulated.

Offline tests cover instruction-byte permutation, a known encoded branch word, endian conversion, checksum rejection, program capacity boundaries, duplicate functions, unsupported/ambiguous rates, unknown odd-sized chunks, invalid child sizes, wrong RIFF type and every truncation of a synthetic container. Fetch-tool tests cover a published DCL example, every truncation, malformed headers/back-references, output limits and hash failure.

## Reproduce

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
python3 tools/fetch_firmware.py
python3 tools/check_firmware.py firmware/WFM0001A.CSP
build/qsound_inspect firmware/WFM0001A.CSP --extensions
```

For sanitizers, configure another in-tree build with `-fsanitize=address,undefined` in `CMAKE_CXX_FLAGS` and `CMAKE_EXE_LINKER_FLAGS`, then run CTest and the independent firmware check against that build's inspector.

These checks do not establish DSP arithmetic, sample timing, impulse responses, speaker positioning or equality with an original card. No TFX audio path has been changed.

Original TFX host control is additionally checked by `tools/check_tfx_control.py`: 725 cases / 9,056 sweep callbacks, all 106 sample descriptors, all four selected setup/processing uploads, startup parameter sequences and explicit driver-failure cases. DOS I/O and Creative driver replies are supplied; original x86 control instructions execute. This establishes the C++ sweep and host protocol, not QSound audio. See [the control comparison scope](tfx-control.md).
