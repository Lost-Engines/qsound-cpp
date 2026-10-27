# Analytical audio model

`qsound::spatial_model` turns the recovered firmware tables into stereo PCM. It is a port of the inferred two-input signal path, with explicit arithmetic hypotheses. It is **not an ST18932 instruction emulator or verified physical-card output**. TFX C++ uses this processor with the explicit policies described below.

## What it preserves

The model receives two FIR banks, 33 positioning rows and a direct-path delay. `make_spatial_tables(rate)` supplies editable copies of the included original numerical coefficients. Optional research code can recover the same data from an external firmware function; see [coefficient provenance](coefficients.md). Each input contributes to four buses: direct left/right and filtered left/right. Each output combines a delayed direct bus with its FIR cancellation bus. The histories retain their state when positions change; `reset()` clears the histories without changing positions.

The recovered ring operations suggest writing each new sample and then reading oldest to newest. The model follows that interpretation: the last authored coefficient multiplies the current sample. The [program analysis](spatial-program.md) records the instruction locations and the unresolved circular-access controls.

Construction allocates bounded histories once. Processing and position changes allocate nothing and perform no I/O. The component remains ordinary source included in the application's target.

## Arithmetic hypotheses

The caller must choose both policies; there is no implicit authenticity setting:

- `rounding_model::floor` takes the signed high word. `nearest` adds half a word before taking it, with ties towards positive infinity.
- `accumulation_model::saturate_each_mac` clips each multiply/accumulate result to signed 32-bit. `saturate_output` retains a wider intermediate and clips when converting to a PCM word.

Both assume signed Q15 gains and coefficients, a fractional 16-by-16 product aligned into 32 bits, and 16-bit storage between the mix and FIR stages. Those choices are consistent with the recovered constants and related core documentation, but are not established by an execution trace from this DSP. The four combinations measure only these particular uncertainties; they do not exhaust every possible core interpretation.

The default `position_policy::reject` accepts recovered positions 0–32 and rejects unresolved inputs. A caller can explicitly choose `position_policy::clamp_to_endpoints` as the constructor’s third argument. This maps every value above 32 to the right endpoint, including TFX’s speech position 33 and larger wrapped sweep bytes. It leaves `position_sweep` and its verified unsigned wrapping unchanged. The mapping is a compatibility policy; actual DSP behaviour outside the table remains unresolved. It does not implement the four-input path, host command interpreter, VU meters or hardware sample interface.

## Use

See the [complete usage guide](usage.md) for firmware loading, compilable integration helpers, PCM formats, source positioning, streaming and timing.

```cpp
qsound::spatial_model processor{qsound::make_spatial_tables(8000), {
  .rounding{qsound::rounding_model::floor},
  .accumulation{qsound::accumulation_model::saturate_each_mac},
}};
processor.set_positions(16, 16);
auto const output{processor.process({
  .left{sample},
  .right{},
})};
```

`sample` is signed 16-bit PCM. A mono source goes in one input with the other zero. The 8 kHz argument chooses the firmware bank that Q2 loads; it does not resample input or promise that the hardware clock always ran at 8 kHz.

## Offline auditions

```sh
python3 tools/render_spatial_model.py sample.wav --output research/renders/sample
```

This accepts uncompressed mono/stereo 8-bit or 16-bit PCM WAV, retains its playback rate, appends a filter tail and writes four maximum-compression FLACs. `--position` and `--right-position` select static source positions. `--bank-rate` selects the coefficient bank separately. The manifest records input and coefficient hashes, the optional firmware hash, policy choices and differences from floor/per-MAC saturation. It requires the `flac` executable and the built `qsound_model_probe` and `qsound_tables` tools. It never plays audio automatically.

Local auditions were made from TFX's `tracking` announcement at position 16 and `fx4` at position 0, using the 8 kHz bank and their original WAV playback rates. The two saturation policies gave identical output for these inputs. Rounding changed the results by at most two and one PCM least-significant bits respectively. That is useful for bounding these two choices on these examples; it is not evidence that all remaining semantic questions are inaudible. The original recordings, firmware and generated FLACs remain ignored research inputs.

A further audition uses `lookslik` ("looks like the lizard"), 1.4944 seconds at 7,500 Hz. Its authored position is 0, the leftmost virtual source. A centred position-16 comparison was also rendered. QSound's endpoint uses both speaker channels: the opposite output carries filtered cancellation audio, not silence. The intended externalisation is for a suitable stereo speaker arrangement; headphones deliver the cancellation signals directly to the ears and can sound unusually wide or phasey.

Unlike the previous clips, the position-0 `lookslik` audition distinguishes the saturation models: 66 interleaved samples differ by up to 552 PCM least-significant bits between floor/per-MAC and floor/output saturation. They produce 37 and 94 rail samples respectively. At position 16 the saturation policies agree, and rounding differs by at most two PCM units. These observations are input-dependent arithmetic comparisons, not a choice established from original hardware.

[The position-33 follow-up](position-33.md) gives a practical endpoint-clamp recommendation and the evidence supporting it. The compatibility choice is now available explicitly; strict rejection remains the default.

## Validation

```sh
ctest --test-dir build --output-on-failure
python3 tools/check_spatial_model.py
```

The asset-free C++ test checks cancellation, delay, coefficient ordering, reset and rejection of unsupported positions. A separate scalar Python implementation compares 48 cases across all four banks and all four arithmetic policies, totalling 27,936 stereo frames, including impulses, random values, clipping inputs and tails. It also checks malformed PCM input rejection. Debug, Release and AddressSanitizer/UndefinedBehaviorSanitizer builds pass these checks. These are tests of the stated equations, not a comparison with an executing original DSP.

TFX integration uses floor rounding and saturation after each MAC, the same explicit hypotheses as the supplied primary auditions. Neither that choice nor the endpoint policy upgrades the model to verified ST18932 execution.
