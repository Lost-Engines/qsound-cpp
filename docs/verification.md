# Verification status

The checked implementation includes firmware decoding, instruction inspection, TFX pan-sweep control, spatial-table recovery and an analytical audio model. **Audio output from the model is not verified ST18932 output.** The [remaining execution questions](blocker.md) prevent claiming that equivalence.

## Checks completed

- GCC 16.2, C++26: Debug and Release builds, with warnings enabled.
- All four offline CTests pass in Debug, Release and an AddressSanitizer/UndefinedBehaviorSanitizer build. The final sanitizer run enabled leak detection and halted on undefined behaviour.
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

`tools/check_spatial_tables.py` compares all eight FIR banks and 132 gain literals in each of the four functions against independent Python literal data-flow. Thirty-two coefficient mutations are accepted and twelve unsupported layout mutations rejected, with checksums regenerated. Both original QSound releases pass. `tools/check_tfx_control.py` additionally executes 10,000 original OSL PIT interrupts for the inspected shared-timer configuration. These establish table extraction and timer scheduling; neither is DSP arithmetic verification.

`tools/check_spatial_model.py` compares the C++ audio model against independent scalar equations: 48 cases / 27,936 stereo frames across four coefficient banks and four arithmetic policies, plus malformed PCM and unsupported-position rejection. All pass in Debug, Release and the sanitizer build. Asset-free model tests cover cancellation, delay, tap ordering and reset. Local FLAC auditions compare the four policies for two original TFX samples. These checks establish the stated analytical model, not the unresolved DSP arithmetic or physical-card behaviour. See [model scope](spatial-model.md).
