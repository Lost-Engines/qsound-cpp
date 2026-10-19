# TFX QSound host control

The original September 1994 TFX 2.05 `Q2.COM`, `START.EXE` and sample definitions have been inspected and the controller routines executed unchanged under Unicorn. `tools/check_tfx_control.py` reproduces these comparisons using external originals. It supplies DOS file I/O and stub Creative-driver replies; the firmware is not executed and this does not verify any audio output.

## Firmware selection and startup

`Q2.COM` is a loader. Its main routine sets the sample-width, channel and sample-rate masks to `1`, selecting eight-bit mono and the **8 kHz** QSound function. Other sample rates in the firmware are supported by its parser; the reference tool separately exercises them by changing the sample-rate mask in caller data, without changing instructions.

The real initialization code uploads the exact selected `init` program, then the matching `main` program. All four rates were checked against independently parsed RIFF chunks, including complete byte counts and SHA-256 values. It allocates a driver handle and calls the driver with these command identifiers:

| Command | Use observed in Q2 |
| --- | --- |
| `00010000` | Allocate a handle through the output pointer |
| `000a0000` | Upload the selected initializer |
| `00030000` | Upload the selected processing program |
| `00040000` | Start processing |
| `00080000` | Set a codec parameter |

Startup parameter writes are `e0=01`, `00=ff`, `01=ff`. The published low-memory interface is a 12-byte record: marker `62951413`, driver far entry point, device handle. The program's decimal command-line argument is a low-memory byte offset, not a segment. `START.EXE` uses the interrupt-vector-table area immediately after its chosen game interrupt vector for this interface.

Q2 checks handle-allocation failure, but publishes its interface even when later driver operations report errors. The sweep callback also ignores parameter-write failures. These quirks are documented, not proposed as error-handling requirements for a new application.

## The three sample effect words

`TFX.DAT` routine at file offset `0xdd9f4` copies six bytes to the callback's state. `START.EXE` at file offset `0xaa6` interprets them as:

1. Unsigned **8.8 position phase**; the upper byte is the position sent to the processor.
2. Signed **8.8 position increment** per shared timer callback.
3. A nonzero **movement-enable word**. Values such as `16`, `18` and `48` are not durations or distinct movement modes in this routine.

The callback first compares the current integer position against a shared cached byte. If it changed, the callback writes `e0=01`, `00=position`, `02=00`. It then adds the increment if movement is enabled, wrapping at 16 bits. Positions are not clamped. A new sample replaces the three sweep words without resetting the shared cache, initially `ff`.

The callback is chained through the OSL timer handler. Its invocation rate belongs to that timer's state; it is not one update per PCM sample, rendered frame or arbitrary host audio callback. `position_sweep::tick()` deliberately takes one original callback event and performs no wall-clock scheduling.

All 106 sample descriptors use nine parameter combinations. Examples:

| Sample | Initial phase | Increment | Movement word |
| --- | ---: | ---: | ---: |
| `tracking` | `1000` (position 16) | 0 | 0 |
| `fx16` | `2000` (position 32) | -227 | 16 |
| `fx20` | `0e00` (position 14) | 1024 | 1 |
| `aft1b` | `0000` (position 0) | 455 | 18 |
| `fly1b` | `2000` (position 32) | -113 | 48 |

Many speech descriptors use position **33**, beyond the ALSA control's normal 0..32 positioning range. That value must be preserved while investigating its firmware interpretation. This is evidence of the value being sent, not a claim about where the resulting sound is perceived.

## Reproduce

```sh
cmake --build build -j
python3 tools/check_tfx_control.py --output research/tfx-control.json
```

The Python tool requires Unicorn and original data files at `../tfx-reverse/installs/cd_1994_09/TFX` by default. `--install`, `--firmware` and `--probe` override paths. It checks 725 cases, covering all nine authored sweeps, boundaries, signed increments, cache reuse and deterministic random inputs: **9,056 original callback executions agree with the C++ controller**, including the exact three-command sequence when an update occurs. Additional executions confirm ignored driver-error replies. No hardware, DSP arithmetic or QSound filter output is represented by those counts.
