# QSound C++

Reconstruction of Creative Sound Blaster ASP/CSP QSound processing for modern C++ programs, initially for [TFX C++](https://github.com/Lost-Engines/tfx-cpp).

The intended integration is source inclusion: include the `qsound/` headers and add the corresponding `.cpp` files to the application's CMake target. There is no installed library, package manager, audio-device dependency, or DOS runtime.

Development is in progress. No audio-processing implementation is claimed yet. Original Creative firmware is external input and is not included in this repository. See [research notes](docs/research.md) for the evidence and remaining questions.
