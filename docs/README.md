# Documentation

Start with the [usage guide](usage.md) to integrate the component or render a recording. The research documents explain the evidence behind the implementation and are optional background for application developers.

## Using and developing the component

| Document | Contents |
| --- | --- |
| [Usage](usage.md) | Source inclusion, built-in banks, C++ examples, audio formats, positioning, streaming, timing and tools. |
| [Coefficients](coefficients.md) | Editable built-in tables, per-processor customisation and reproducible extraction. |
| [Firmware](firmware.md) | Download provenance, reference hashes, Creative container structure and validation. |
| [Verification](verification.md) | Offline tests, independent comparisons, original-code traces, dependencies and accuracy limits. |
| [Contributing](../CONTRIBUTING.md) | Build checks and what to include in reports or proposed changes. |

## Reverse-engineering findings

| Document | Contents |
| --- | --- |
| [Research sources](research.md) | Primary references, related hardware and scope of the investigation. |
| [Spatial program](spatial-program.md) | Recovered coefficients, gains, delays and the inferred signal path. |
| [Audio model](spatial-model.md) | Implemented equations, arithmetic choices and listening comparisons. |
| [TFX controller](tfx-control.md) | Original loader, sample effect words, wrapping positions and timer scheduling. |
| [Position 33](position-33.md) | Evidence for out-of-table inputs and the explicit endpoint compatibility choice. |
| [Creative driver](creative-driver.md) | Original DOS driver traces and unchanged parameter transmission. |
| [Diagnostic program](diagnostic-program.md) | A simpler CSP program embedded in the SB16 controller ROM. |
| [Unresolved DSP semantics](dsp-semantics.md) | Missing instruction and arithmetic details, and evidence that could resolve them. |

The ordinary build and tests do not require DOS executables, a TFX checkout or original sound hardware. Some optional research scripts need external originals and emulation helpers, described in [verification](verification.md#original-code-comparisons).
