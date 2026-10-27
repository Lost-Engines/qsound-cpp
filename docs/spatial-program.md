# Spatial tables and signal-path reconstruction

The C++ `recover_spatial_tables` function recovers the explicitly authored constants from the original Creative QSound firmware. It recognizes the reference program layout and rejects unsupported layouts. It does **not** execute the DSP, ignore unknown instructions to produce audio, or claim to reconstruct a complete power-on RAM image.

## Recovered tables

| Firmware rate | Taps per output | Direct-path delay in samples | Coefficient X-RAM starts |
| --- | ---: | ---: | --- |
| 44,100 Hz | 49 | 24 | 25, 74 |
| 22,050 Hz | 19 | 9 | 25, 44 |
| 11,025 Hz | 17 | 8 | 25, 42 |
| 8,000 Hz | 11 | 5 | 25, 36 |

The two coefficient banks are different and asymmetric. Their dominant coefficients are negative, consistent with cross-channel cancellation. Their signed coefficient sums are respectively:

| Rate | First bank | Second bank |
| --- | ---: | ---: |
| 44,100 | -27,700 | -26,098 |
| 22,050 | -28,864 | -28,136 |
| 11,025 | -26,786 | -24,045 |
| 8,000 | -27,442 | -24,273 |

The external-memory position table has 132 words, arranged as **33 rows of four gains**. All four rate variants author the same position table. Its two halves are filled by literal writes through an increasing E0 pointer and a decreasing E1 pointer. The decoder preserves the slight asymmetries in the authored constants.

Named gain ordering follows the processing program's four mix buses, described as left/right by output order. The original table order is `filtered_left`, `direct_left`, `direct_right`, `filtered_right`; the tool's JSON emits `direct_left`, `direct_right`, `filtered_left`, `filtered_right`. Representative rows are:

| Position | Direct left | Direct right | Filtered left | Filtered right |
| --- | ---: | ---: | ---: | ---: |
| 0 | 32,766 | 0 | 0 | 32,766 |
| 16 | 18,220 | 18,220 | 3,288 | 3,288 |
| 32 | 0 | 32,766 | 32,766 | 0 |

These are original signed integer literals, not replacement coefficients generated from a generic stereo effect. Q15 scaling is a natural interpretation of their sizes and the multiply/accumulate path; exact multiplier formatting and rounding remain separate questions.

## How the processing loop uses them

The 351-word processing programs share a control-flow layout while their pointer bounds, FIR lengths and coefficient starts vary with rate.

- Instructions `040`–`050` collect four input lanes into Y-RAM. The control path selects either an ordinary pass-through or spatial processing.
- Instructions `060`–`071` form four mix buses. The two-input path repeats a five-instruction body; the four-input path repeats a seven-instruction body. The four buses occupy Y words 4–7. Input coefficients are read from the matrix following the delay/filter histories.
- Instructions `072`–`08b` maintain the first direct-delay ring and FIR history, perform the first convolution, and write the first output at E address `804`.
- Instructions `08c`–`0a5` do the corresponding work for the second output at `805`.
- Instructions `0a8`–`0db` use the four position words in X-RAM to copy four gains per input from the position table into the mix matrix. For example, the first input's four table words are stored at `base+4`, `base`, `base+8`, `base+12`, which establishes the gain ordering above.
- Instructions `0dc`–`0fc` maintain output level extrema. Instructions from `0fd` handle host commands.

For the first output, a direct bus passes through a ring whose first data word is E `085` and whose final data address is programmed by instruction `073`. Each sample is written, the pointer advances, and the next oldest sample is read. Hence the delay is the ring capacity minus one, giving the sample counts above. The FIR ring is also written before the read cursor advances. On that interpretation the coefficient stream walks **oldest to newest** history; a conventional FIR interface with tap zero multiplying the current sample must reverse the recovered coefficient array. This ordering follows the program's ring operations, but the new control instructions governing pointer formatting/circular access have not been independently executed.

This yields a functional signal-path model: each output combines a delayed direct mix and a filtered cancellation mix. It substantially narrows a future implementation; a general emulator of all ST18932 features would not be necessary for a port of this particular algorithm.

## Open questions that must remain explicit

- Exact 32-bit product scaling, accumulator formatting, saturation and rounding.
- The new register/control operations around circular-buffer access.
- The precise activation and rate-switch behaviour of the hardware sample interface.
- **TFX's position 33:** it is present in many speech definitions, but lies beyond the 33-row table's last index, 32. Under a direct `position*4` interpretation it reaches the first delay-ring control word. The current evidence does not justify silently clamping it, treating it as a normal centre position, or assuming that unintended RAM access is definitely the hardware behaviour.

TFX selects the 8 kHz firmware even though its PCM samples use various playback clocks. A faithful integration should retain that selection unless further original-code evidence establishes a later program switch.

## Reproduce and validation scope

```sh
build/qsound_tables firmware/WFM0001A.CSP > research/spatial-tables.json
python3 tools/check_spatial_tables.py firmware/WFM0001A.CSP
```

The checker uses a separate Python literal-provenance analysis rather than the C++ pattern reader. It compares all eight FIR banks and all 132 position words in all four functions. It also changes coefficient literals and regenerates their encoded checksums to verify that the reader follows external firmware data, and checks rejection of unsupported instruction layouts. Those checks establish extraction, not execution or audio fidelity. The recovered numerical tables are also included as editable defaults in `qsound/coefficients.h`. `--compare-builtins` checks them against the external reference; original firmware programs remain external research inputs.

The [original Creative driver traces](creative-driver.md) rule out driver-side clamping or a special mapping for TFX position 33. Its parameter byte is passed unchanged to the DSP. That narrows the question without establishing the DSP address/format behaviour.

[Further position-33 analysis](position-33.md) checks original request-side transport, identical definitions across retained builds, and the initializer tail. Under a cold linear-address interpretation the would-be row is `[133, 0, 0, 0]` in authored order, consisting of a delay head and cleared history. Endpoint clamping to 32 is recommended only as an explicit compatibility choice.
