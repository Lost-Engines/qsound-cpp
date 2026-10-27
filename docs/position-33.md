# TFX position 33: evidence and a practical interpretation

For an approximate integration, **clamp position 33 to the rightmost recovered position, 32**. That is a reasonable compatibility choice, not a verified reconstruction of the DSP's response. The strict default still rejects it; `position_policy::clamp_to_endpoints` now makes this compatibility choice explicit at construction. Nothing in this investigation silently changes TFX playback.

## What is established

- The gain table contains 33 positions, indexed 0–32. Position 0 is the leftmost virtual source, 16 is central and 32 is rightmost.
- The full TFX table has 106 descriptors. Sixty-one use static position 33, including two empty-name descriptors; 59 of the 103 named descriptors use it. Nineteen named descriptors use static position 0. `tracking` uses 16; `lookslik` uses 0.
- Executing the original TFX sound-request path for **all 106 descriptors** copies each authored effect definition unchanged into the shared pan state. There is no request-side clamp or special mapping for 33. The native queue preparation and the six-byte publication routine execute, rather than being substituted by a Python copy.
- The previously checked START callback publishes the integer position unchanged. Both original Creative DOS drivers then pass all byte values unchanged to the DSP.
- The 103 named effect definitions are identical across all eight retained engine inventories, including the earlier floppy/CD builds, PC Plus demo and cinematic-preview engine. Position 33 is not peculiar to a damaged copy or the September 1994 install. This comparison concerns the effect words, not complete behavioural equivalence between engines.

## What a literal memory read would do

The first gain-update block at DSP indices `0a8`–`0b4` consists of a shift, a sign guard, an E1 pointer load and four sequential reads. It contains no explicit comparison with 33, clamp, modulo operation or bypass branch for that value. Under the ordinary `position * 4` interpretation, position 33 reads E-RAM words 132–135.

Those are no longer positioning coefficients. The initializer's final loop writes 133 to E132 as a delay-ring head, then clears the following history words. Its three later literal stores initialize other ring heads. Following the related core's documented loop count and the linear-address interpretation gives the same cold row in all four rate variants:

| Table-order field | Conditional cold value |
| --- | ---: |
| Filtered left | 133 |
| Direct left | 0 |
| Direct right | 0 |
| Filtered right | 0 |

In the current Q15 signal-path model this would give a very faint filtered left signal and no direct path. For the 8 kHz bank, its steady low-frequency gain would be about −49 dB. Once audio has passed through the histories, the values could depend on previous samples and the ring cursor. It would not define a stable extra spatial position.

This remains a **conditional calculation**, not an executed ST18932 trace. The newer core's formatting, circular-access and memory-interface details have not been fully established. It is evidence against inventing an ordinary extra positioning-table row; it does not prove the card actually reads those four words in this way.

## Why endpoint clamping is the best simple guess

33 is immediately beyond the valid right endpoint, and the sample table repeatedly chooses 0 and 33 for different groups of speech. An off-by-one right-end request is a simpler explanation than an undocumented centre or dry-output sentinel for which no handling has been found. Mapping 33 to 32 preserves that likely left/right distinction and gives deterministic, bounded playback.

Other interpretations have weaker support:

| Hypothesis | Problem |
| --- | --- |
| Centre position | No special mapping was found; numerically 33 is adjacent to the right endpoint. |
| Dry mono bypass | The inspected gain-copy block contains no branch that selects it for 33. |
| Wrap 33 to 0 | Would abruptly move the source from the right endpoint to the left; no modulo-33 operation was found. |
| Literal scratch-RAM gains | Could reflect an original bug, but depends on unresolved addressing and runtime history. A fixed four-gain substitute would not reproduce it. |

Moving effects are a separate complication: the original controller can produce values larger than 33 and can wrap its unsigned phase. A compatibility integration should state its general out-of-range policy rather than fixing only one voice value and leaving other effects capable of failing. It should also keep the verified wrapping controller separate from the chosen audio-position policy.

## Reproduce

```sh
python3 tools/investigate_positions.py --output research/position-investigation.json
```

This optional research check requires Unicorn, the original September 1994 install and the neighbouring TFX C++ checkout's `tools/native_reference.py`. Paths can be supplied with `--research`, `--tfx-tools` and `--firmware`. The engine-inventory comparison additionally reads the retained versions named in `tfx-reverse/analysis/engine-builds.json`, checking their hashes before comparing data.

The host execution supplies a short synthetic sample buffer, idle-stream state, loaded-resource metadata and ready Sound Blaster status. It does not synthesize or validate audio, reproduce interrupts or run driver installation. The cold-RAM calculation pattern-checks the actual initializer tail and position-copy block while keeping the instruction-format assumptions explicit.
