# Built-in coefficients

Normal application use and offline rendering require no firmware file. The editable numerical defaults live in [`qsound/coefficients.h`](../qsound/coefficients.h). `make_spatial_tables(rate)` returns an owned copy ready for `spatial_model`; its implementation is in [`qsound/default_tables.cpp`](../qsound/default_tables.cpp).

## What is included

| Bank | Left filter taps | Right filter taps | Direct delay, samples |
| ---: | ---: | ---: | ---: |
| 8000 | 11 | 11 | 5 |
| 11025 | 17 | 17 | 8 |
| 22050 | 19 | 19 | 9 |
| 44100 | 49 | 49 | 24 |

The four banks share 33 positioning rows, each containing direct-left, direct-right, filtered-left and filtered-right gains. In total there are 192 filter coefficients and 132 gains: **648 bytes of signed 16-bit numerical values**, plus four delay counts. The original instruction programs and firmware containers are not embedded.

These values were extracted from Creative's `WFM0001A.CSP`, SHA-256 `7ffd5f1b246cbda604d1dff17b052cab0eb367d5a09a3827ae02b14b6e23bdb8`. [Firmware provenance](firmware.md) records the original upgrade archives. The header retains that source hash and the authored tap order. Built-in and externally recovered tables produce identical output under the same arithmetic policies; this does not settle the remaining original-DSP arithmetic questions.

## Customise a processor

Create a private copy, make the changes you want, then transfer it to the processor:

```cpp
auto tables{qsound::make_spatial_tables(8000)};
// Example modification: reduce this filter tap's contribution.
tables.left_filter.taps[0] /= 2;
qsound::spatial_model processor{
  std::move(tables),
  {
    .rounding{qsound::rounding_model::floor},
    .accumulation{qsound::accumulation_model::saturate_each_mac},
  },
};
```

Include `"qsound/spatial_model.h"` and `<utility>` for this example. The copy's `left_filter.taps`, `right_filter.taps`, `direct_delay_samples` and `positions` are all editable. Each factory call returns independent storage; changing a copy does not change other instances or the defaults. The processor owns its tables after construction. Replacing an active processor to change its coefficients also replaces its histories.

Gains and taps are signed Q15 values: a value represents its integer divided by 32,768. FIR taps are stored **oldest to newest**; the last coefficient multiplies the current sample. Source positions retain the four named gain fields. The selected arithmetic policy controls saturation when a modification increases a signal beyond the represented range.

To change defaults for your whole build, edit the named `bank_8000`, `bank_11025`, `bank_22050`, `bank_44100` and shared `positions` definitions in `coefficients.h`, then rebuild. The `filter_coefficients::address` field is original X-RAM inspection metadata and is not used by the audio processor. Normal bounds validation still applies to custom filter and delay lengths.

## Reproduce or investigate the extraction

The firmware fetcher, parser and comparison tools remain available for development. From the repository root, after building the tools:

```sh
python3 tools/fetch_firmware.py
python3 tools/check_spatial_tables.py firmware/WFM0001A.CSP --compare-builtins
mkdir -p research
python3 tools/export_coefficients.py firmware/WFM0001A.CSP --output research/reference-coefficients.h
diff -u qsound/coefficients.h research/reference-coefficients.h
```

The independent table checker compares every built-in tap, gain and delay against the original, and separately exercises coefficient mutations and unsupported program layouts. Intentional changes to the built-in defaults will naturally differ from the original reference. The exporter emits an editable header, validates the four-bank/shared-position layout, and records the input asset hash. Use `--tables` to select a different inspection executable. It writes to standard output unless `--output` is supplied.

`qsound_tables` with no arguments prints the built-in banks. Giving it a CSP or PVL path prints externally recovered banks instead. `render_spatial_model.py --firmware FILE` and `qsound_model_probe ... --firmware FILE` use the external tables for comparison; normal invocation uses the built-in defaults. A render manifest records the selected table source and a hash of the actual selected coefficient data, including custom defaults.

Applications that need the optional firmware APIs can add `${QSOUND_RESEARCH_SOURCES}` alongside `${QSOUND_SOURCES}`. The normal source list compiles only the table factory, PCM processor and position controller.
