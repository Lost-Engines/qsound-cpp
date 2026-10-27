# Research acknowledgements

The first-party C++ implementation and Python tools in this repository were written for this project. No pre-existing DSP emulator has been incorporated.

- mkarcher's published ASP/CSP research supplies the instruction-byte permutation and program layout facts; see the linked discussion in [research sources](docs/research.md).
- SGS-Thomson's ST18930/31 and ST18933 documentation supplies the documented processor format and architecture information.
- The ALSA CSP loader and cspctl documentation provide independent host-protocol and firmware-container references.
- Mark Adler's zlib `contrib/blast` documents the PKWARE DCL format and its coding tables. An independently compiled copy was used locally to check extraction. The Python fetcher and native C++ upgrade loader use purpose-limited decoders written for this project; blast is not a build or runtime dependency. The published example used by the tests was described by Ben Rudiak-Gould and corrected by Mark Adler.

Original Creative firmware, manuals and archived distributions are external research inputs with their own copyright and licence terms. They are not included in the project's source distribution.
