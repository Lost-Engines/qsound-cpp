# Usage guide

This component spatialises two signed 16-bit PCM inputs into stereo output, using the original Creative ASP/CSP QSound firmware's filters, gains and delays. Applications include its sources directly and supply their own asset loading, sound decoding, mixer, sample clock and audio device. The optional TFX position-sweep controller can reproduce that game's source movement independently of audio processing.

The signal path is an analytical reconstruction with explicit arithmetic choices. Firmware decoding and the TFX controller have original-data/code comparisons; the generated audio has not been verified against an executing ST18932 or a physical card. See [verification](verification.md) for those boundaries and [the audio model](spatial-model.md) for the equations.

## Contents

- [Include the component](#include-the-component)
- [Obtain and load firmware](#obtain-and-load-firmware)
- [Construct a processor and render buffers](#construct-a-processor-and-render-buffers)
- [PCM inputs and output](#pcm-inputs-and-output)
- [Position sources](#position-sources)
- [Select arithmetic](#select-arithmetic)
- [Sample rates and scheduling](#sample-rates-and-scheduling)
- [Histories, tails and lifetime](#histories-tails-and-lifetime)
- [Animate positions with the TFX controller](#animate-positions-with-the-tfx-controller)
- [Handle failures](#handle-failures)
- [Build and use the command-line tools](#build-and-use-the-command-line-tools)
- [Public API reference](#public-api-reference)

## Include the component

Keep a checkout or vendored copy in your source tree and add its sources to your application's existing target:

```cmake
include(path/to/qsound-cpp/qsound/sources.cmake)
target_sources(your_program PRIVATE ${QSOUND_SOURCES})
target_include_directories(your_program PRIVATE ${QSOUND_INCLUDE_DIR})
target_compile_features(your_program PRIVATE cxx_std_26)
```

`your_program` must already exist. Include public headers as `"qsound/spatial_model.h"`, for example. The include directory is the parent of `qsound/`.

A C++26 compiler and the standard library are sufficient for the component. There is no library installation, package discovery, audio backend or DOS driver to initialise. Include the sources once per consuming target. Building this repository's inspection tools separately is optional; it requires CMake 3.30 or later.

## Obtain and load firmware

The coefficient tables come from external original firmware. They are recovered at startup rather than replaced with approximations in the source.

From the repository root, this optional standard-library-only Python script downloads Creative's archived upgrade and extracts the firmware:

```sh
python3 tools/fetch_firmware.py
```

The default output is `firmware/WFM0001A.CSP`. Correct existing output is verified and reused; missing or damaged output is replaced. Downloads retry transient failures and verify pinned hashes. For an existing upgrade archive or a different destination:

```sh
python3 tools/fetch_firmware.py --archive /path/to/CSPU.ZIP --output /path/to/WFM0001A.CSP
```

The C++ entry point `decode_firmware_asset(bytes)` accepts either:

| Bytes supplied by the application | Handling |
| --- | --- |
| An original RIFF/CSP firmware file, such as `WFM0001A.CSP` | Validate and decode the container and programs. |
| The `MAIN.PVL` member extracted from Creative's upgrade ZIP | Unpack its first bounded binary-literal PKWARE DCL stream in C++, then validate and decode the resulting CSP. |

Pass the file's contents as `std::span<uint8_t const>`. The component does not open paths, search directories or fetch data. It does not accept `CSPU.ZIP` itself: extract `MAIN.PVL` using your application's existing ZIP tooling first. Using that member allows native runtime loading without Python. The original firmware retains its own terms; [firmware provenance](firmware.md) records the reference files and hashes.

Use `decode_firmware(bytes)` instead when you specifically require an already unpacked CSP. Both loaders return decoded functions; select a QSound playback function with `select_qsound_function(functions, firmware_rate)`, then pass it to `recover_spatial_tables`.

The selector accepts exactly **8000, 11025, 22050 or 44100**. It rejects absent or ambiguous matches. Table recovery requires the recognised QSound program layout; accepting a CSP container does not imply every codec or firmware revision can be rendered.

## Construct a processor and render buffers

The following is complete application-side helper code. `make_processor` and `process_mono` are example helpers, not additional functions supplied by the component:

```cpp
#include <cstdint>
#include <ranges>
#include <span>
#include <stdexcept>
#include <utility>
#include "qsound/spatial_model.h"
#include "qsound/upgrade.h"

auto make_processor(std::span<uint8_t const> const firmware_bytes,
  unsigned int const firmware_rate)->qsound::spatial_model {
  auto const functions{qsound::decode_firmware_asset(firmware_bytes)};
  auto const &function{qsound::select_qsound_function(functions, firmware_rate)};
  auto tables{qsound::recover_spatial_tables(function)};
  return qsound::spatial_model{
    std::move(tables),
    {
      .rounding{qsound::rounding_model::floor},
      .accumulation{qsound::accumulation_model::saturate_each_mac},
    },
    qsound::position_policy::clamp_to_endpoints,
  };
}

void process_mono(qsound::spatial_model &processor,
  std::span<int16_t const> const input,
  std::span<qsound::stereo_sample> const output) {
  if(input.size() != output.size()) {
    throw std::invalid_argument{"one stereo output frame is required per mono input sample"};
  }
  for(auto const &[sample, frame] : std::views::zip(input, output)) {
    frame = processor.process({
      .left{sample},
      .right{},
    });
  }
}
```

Load the firmware and call `make_processor` once during setup. Pass the returned processor by reference to `process_mono` for each successive decoded buffer, using an equally sized, preallocated output buffer. Its initial source positions are both **16**, the centre. Set other positions with `processor.set_positions(left, right)` before processing or at the relevant sample boundary.

This example explicitly selects the same arithmetic and endpoint compatibility choices as TFX C++. To enforce the recovered 0–32 position range, pass `position_policy::reject` instead, or omit the constructor's third argument. The choices are explained below.

All firmware parsing, table recovery and history allocation happen during setup. The returned processor owns the recovered tables and histories; the firmware bytes and decoded functions can be released afterwards.

## PCM inputs and output

`process(pcm_inputs)` consumes **one sample frame** and returns one `stereo_sample`. Both structures contain signed 16-bit `left` and `right` members.

The input member names identify two independent source lanes. They do not fix those sources to the left and right output speakers. Each source has its own selected position and can contribute to both output channels. A mono source normally occupies `.left`, with `.right` zero as above. For two sources, or the two channels of a decoded stereo stream:

```cpp
auto const output{processor.process({
  .left{first_source_sample},
  .right{second_source_sample},
})};
```

Those inputs must share a sample clock. Supply zero for an inactive source. For a mono source, copying it into both lanes would mix two sources and change the result; supply it once.

Samples use the full signed 16-bit range, **-32768 to 32767**. Convert unsigned 8-bit PCM by subtracting 128 and multiplying by 256:

```cpp
auto unsigned_pcm_to_signed(uint8_t const sample)->int16_t {
  return static_cast<int16_t>((static_cast<int>(sample) - 128) * 256);
}
```

For a floating-point mixer, divide each output component by `32768.0f`. If your input is floating point, convert it to bounded, finite signed 16-bit PCM before calling the processor. There are no float or compressed-audio overloads. Decode WAV, FLAC, MP3 or other formats in the application.

The C++ API takes numeric samples, independent of file byte order. It does not specify a packed file representation for `stereo_sample`; write channel values explicitly when serialising. Interleaving, resampling, mixing with music, volume control and final mixer headroom belong to the caller. One instance provides two input lanes. The application manages voices and any additional channels.

## Position sources

```cpp
processor.set_positions(0, 32);
```

This places the first input at the leftmost authored position and the second at the rightmost. The firmware provides **33 positions, numbered 0–32**, with **16 at the centre**. Positions are discrete; the API has no floating-point angle, normalised coordinate or automatic interpolation.

An endpoint is a virtual source position, not a request to silence the opposite speaker. QSound uses filtered cancellation signals in both outputs; even position 0 can produce right-channel audio. The intended spatial effect uses stereo speakers. Headphones can sound unusually wide or phasey because the cancellation signals reach the ears differently.

| Constructor policy | `set_positions` behaviour |
| --- | --- |
| `position_policy::reject` (default) | Accept 0–32; throw `std::out_of_range` if either input exceeds 32. Neither position changes on failure. |
| `position_policy::clamp_to_endpoints` | Accept unsigned inputs and map every value above 32 to 32, independently for each source. |

The arguments are unsigned. Validate signed coordinates before converting them; the clamp policy does not turn negative coordinates into the left endpoint. Nor does it wrap values modulo 33.

TFX sends position 33 for many speech samples and can publish larger bytes from wrapping sweeps. Explicit endpoint clamping makes those calls usable with the recovered table, while preserving the original controller's wrapping. The original DSP's interpretation of those values remains unresolved; this is a compatibility choice rather than recovered hardware behaviour. See [position 33](position-33.md).

Changing positions updates the gains without clearing audio histories. It does not automatically move or interpolate a source between successive calls.

## Select arithmetic

The constructor requires an `arithmetic_model`; neither field has an implicit default. Select one value from each row:

| Field | Choice | Meaning |
| --- | --- | --- |
| `rounding` | `rounding_model::floor` | Take the signed high word, rounding down. |
| `rounding` | `rounding_model::nearest` | Add half a word before conversion; ties round towards positive infinity. |
| `accumulation` | `accumulation_model::saturate_each_mac` | Saturate each multiply/accumulate result to signed 32-bit. |
| `accumulation` | `accumulation_model::saturate_output` | Retain a wider accumulator and saturate when converting to a PCM word. |

Both the input-mix and final filter stages quantise to signed 16-bit samples. These choices describe the implemented equations; the four combinations are hypotheses, not four confirmed card modes. **TFX C++ chooses `floor` and `saturate_each_mac`.** See [the analytical model](spatial-model.md) for the assumptions and measured differences between auditions.

Arithmetic and position policy are selected at construction. To change them, construct another processor; doing so starts with empty histories and centre positions.

## Sample rates and scheduling

Selecting a firmware bank chooses its coefficients and delay lengths:

| Firmware rate | Taps in each output filter | Direct delay in input samples |
| ---: | ---: | ---: |
| 8000 | 11 | 5 |
| 11025 | 17 | 8 |
| 22050 | 19 | 9 |
| 44100 | 49 | 24 |

The rate is not an audio-device setting and the processor performs no resampling. For a new application, choose the bank matching the intended QSound processing rate, process one frame per tick of that sample clock, and resample the resulting stereo stream if your output device uses another rate. There is no 48000 Hz firmware bank.

For original-game reproduction, bank selection and actual playback timing can differ. TFX loads the **8000** bank but its Sound Blaster DAC time constants determine when samples advance. TFX C++ processes at those sample boundaries, then holds each stereo result until the next boundary when feeding its 48 kHz mixer. Running the filter repeatedly on the same held input at every host output frame would change its delays and response.

Audio callbacks can contain any number of frames. Process them in order and retain the same processor across callback boundaries. Frame rate, callback buffer size and position-controller tick rate are separate clocks.

## Histories, tails and lifetime

Keep a persistent processor for each independent two-source processing path. Constructing or resetting it for every buffer loses the filter and delay history. Pass it by reference; copying a processor also copies its tables and current histories.

`process` is `noexcept`, allocates nothing, performs no I/O and takes no locks. `set_positions` also allocates nothing, but can throw with the strict policy. `reset` is `noexcept` and clears both filters, both direct-delay histories and their cursors while retaining the selected positions, arithmetic and policy. It cuts any pending tail immediately.

To keep a tail after a source ends, continue calling `process({})` with silent inputs and keep the returned stereo frames. A conservative flush length for this implementation is:

```cpp
auto const tail_frames{std::max({
  tables.left_filter.taps.size(),
  tables.right_filter.taps.size(),
  static_cast<size_t>(tables.direct_delay_samples),
}) + 1};
```

Include `<algorithm>` and `<cstddef>` for this expression. Calculate and retain the count **before** moving `tables` into the processor. Feed those frames at the same processing rate as the preceding audio. The offline renderer uses this flush length too. A tail on one lane can be retained while new audio begins on the other; the output histories belong to the combined processing path.

The processor provides no internal synchronisation. Own it on the audio/rendering thread, or arrange external synchronisation so `process`, position updates and `reset` cannot race. Deliver control changes to that thread at the intended sample boundaries. Prepare firmware, tables and output buffers before audio starts; the example buffer helper also checks that input and output lengths agree.

## Animate positions with the TFX controller

Applications that already calculate source positions can call `set_positions` directly. The optional `position_sweep` class reproduces the small wrapping controller used by TFX, without tying it to the PCM loop or a platform timer.

```cpp
#include "qsound/position_sweep.h"

qsound::position_sweep sweep{{
  .position{16 * 256},
  .increment{-128},
  .moving{true},
}};

// Call once for each position-controller event, not for every PCM frame.
if(auto const position{sweep.tick()}) {
  processor.set_positions(*position, 16);
}
```

Here the first source starts at 16 and moves by -0.5 positions per controller event. This example expects a processor constructed with the explicit clamp policy if the sweep is allowed to leave 0–32. Stop or replace the sweep yourself if you want bounded motion instead.

`sweep_parameters` has three fields:

| Field | Meaning |
| --- | --- |
| `position` | Unsigned 16-bit 8.8 phase. Its upper byte is the published integer position. |
| `increment` | Signed 16-bit 8.8 phase change per enabled tick. |
| `moving` | Whether each tick adds the increment. The original third effect word only enables movement; it is not a duration. |

Each `tick()` first publishes the current upper byte **if it differs from the cached byte**, then advances the phase if movement is enabled. Addition wraps at 16 bits. The return type is `std::optional<uint8_t>`: a missing value means no position write is needed, not that motion stopped. A returned value of zero is still an engaged optional and is handled by the example's `if`.

The default phase is zero, movement is disabled and the previous-position cache is `0xff`, so the first default tick publishes zero. The constructor's optional second argument overrides that cached byte. Starting at phase `0xff00` with the default cache skips the initial write because the bytes already match.

`set(parameters)` replaces the sweep parameters while preserving the cached byte. `value()` returns the current parameters by const reference; after a tick its phase has already advanced. `last_position()` returns the last cached raw byte, before any processor clamping. Neither changing a sweep nor ticking it clears audio histories.

The controller has no timer or time units. The inspected TFX timer configuration invokes it about **19.9 times per second**, with an irregular interrupt cadence. A faithful integration must schedule the original events, including timer configuration changes, rather than call `tick()` once per graphics frame or host audio callback. Other applications choose their own movement clock. See [the original TFX controller and timer analysis](tfx-control.md).

## Handle failures

Decode firmware and construct the processor during setup, where failures can be reported to the user or a fallback renderer selected:

| Failure | Exception / handling |
| --- | --- |
| Invalid CSP structure, bounds, program size or checksum; truncated DCL stream | `qsound::format_error`, derived from `std::runtime_error`, with an `offset` member. |
| Unsupported/invalid DCL coding or extraction limit | `std::invalid_argument`. |
| Unsupported, missing or ambiguous firmware rate; unrecognised spatial-table layout | `std::invalid_argument`. |
| Empty or oversized filter histories, or an oversized direct delay | `std::invalid_argument` from construction. |
| Position outside 0–32 with strict policy | `std::out_of_range` from `set_positions`; existing positions are preserved. |

`format_error::offset` identifies a byte in the stream being decoded. When CSP decoding follows PVL unpacking, CSP offsets refer to the unpacked container. Loading and construction use standard allocations and may also throw standard allocation exceptions. The component does not silently disable processing or choose another firmware asset after a failure; fallback and user messages belong to the application.

## Build and use the command-line tools

Run these commands from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The C++ tests use synthetic data and do not need the original firmware. An additional offline decoder test runs when Python is available. Research comparisons against original executables have separate dependencies and external inputs; see [verification](verification.md).

### Inspect firmware and recovered tables

```sh
build/qsound_inspect firmware/WFM0001A.CSP
build/qsound_inspect firmware/WFM0001A.CSP --instructions
build/qsound_inspect firmware/WFM0001A.CSP --extensions
build/qsound_tables firmware/WFM0001A.CSP
```

The inspector prints metadata and optionally decoded instruction words or only the unrecognised instruction formats. The tables tool writes JSON containing all four banks, filters, delays and position gains. Each JSON position row is ordered `direct_left`, `direct_right`, `filtered_left`, `filtered_right`. Both tools also accept a raw `MAIN.PVL` path. Instruction inspection does not execute the DSP.

### Render WAV input to FLAC

With Python and the `flac` command-line encoder available:

```sh
python3 tools/render_spatial_model.py sample.wav --output research/renders/sample \
  --firmware firmware/WFM0001A.CSP --firmware-rate 8000 \
  --position 0 --right-position 16
```

The renderer accepts uncompressed mono/stereo **unsigned 8-bit or signed 16-bit PCM WAV**. Mono occupies the first input lane; stereo supplies both. It preserves the WAV playback rate, appends a silent filter tail and produces four maximum-compression FLACs, one per arithmetic combination, plus `model-renders.json` with hashes and comparisons. It does not play audio.

Defaults are firmware `firmware/WFM0001A.CSP`, bank 8000, and position 16 for each input. `--output` is required. `--position` controls the first input, `--right-position` the second, both limited to 0–32. `--firmware` also accepts `MAIN.PVL`. `--probe` and `--tables` override the default executables `build/qsound_model_probe` and `build/qsound_tables`. Selecting a bank does not resample the WAV: for rate-matched processing, supply a WAV at that bank's rate; for TFX auditions, preserve its original sample timing instead.

### Process a raw PCM stream

```sh
build/qsound_model_probe firmware/WFM0001A.CSP 8000 0 16 floor each \
  < input-stereo-s16le.pcm > output-stereo-s16le.pcm
```

Arguments are `FIRMWARE RATE LEFT_POSITION RIGHT_POSITION floor|nearest each|output`. Firmware can be CSP or PVL. Input and output are headerless, interleaved **left then right, signed 16-bit little-endian**, four bytes per frame. For mono input, interleave a silent second lane first. The tool neither resamples nor appends a tail; append zero frames yourself if required. It uses strict 0–32 positions, so it cannot directly audition the clamp policy. Invalid arguments or incomplete input frames produce a nonzero exit status and a diagnostic on stderr. The probe assumes binary standard streams. Its current implementation does not explicitly select Windows binary mode; this limitation also affects the Python renderer that invokes it.

`qsound_control_probe` is a controller regression utility. Each whitespace-separated input row contains `phase increment movement_word cached_position tick_count` as decimal integers. Each requested tick produces `phase cached_position changed_position`, with `-1` for no publication. The printed phase is after advancement. It reads no audio or firmware.

## Public API reference

All symbols below are in namespace `qsound`. Headers are self-contained; include the one declaring the feature you use.

| Header | Types and operations |
| --- | --- |
| [`qsound/upgrade.h`](../qsound/upgrade.h) | `decode_firmware_asset(bytes)` handles CSP or upgrade PVL. `unpack_upgrade_firmware(bytes, maximum_bytes)` returns the first unpacked DCL member; the default output limit is `maximum_upgrade_firmware_bytes` (65,536). |
| [`qsound/firmware.h`](../qsound/firmware.h) | `decode_firmware(bytes)`, `select_qsound_function(functions, rate)`, `decode_program(bytes)`, `unscramble_byte(value)`; `program`, `function_description`, `firmware_function`, `format_error`, `program_capacity` (512 instructions). |
| [`qsound/spatial_tables.h`](../qsound/spatial_tables.h) | `recover_spatial_tables(function)`; `spatial_tables`, `filter_coefficients`, `position_gains`, `spatial_position_count` (33). |
| [`qsound/spatial_model.h`](../qsound/spatial_model.h) | `spatial_model(tables, arithmetic, positions = position_policy::reject)`; `set_positions(left, right)`, `process(input)`, `reset()`; `pcm_inputs`, `stereo_sample` and the arithmetic/position policy types. |
| [`qsound/position_sweep.h`](../qsound/position_sweep.h) | `position_sweep(parameters = {}, previous_position = 0xff)`; `set(parameters)`, `tick()`, `value()`, `last_position()`; `sweep_parameters`, `position_phase`. |
| [`qsound/instruction.h`](../qsound/instruction.h) | `classify_instruction(word)`, `format_name(format)`, `describe_instruction(word)`, `instruction_format` for research/inspection. These recognise documented older instruction formats and identify unknown extensions; they do not execute firmware. |

`select_qsound_function` returns a reference into the decoded functions; retain that vector until table recovery finishes. `recover_spatial_tables` returns independently owned data. A model takes its tables by value: move them when transferring ownership, or pass an lvalue to copy them for a separate processor. Recovered FIR taps are stored in authored **oldest-to-newest** order, with the last tap multiplying the current sample. Preserve that ordering when inspecting or exporting them. In a decoded `program`, only `size` instruction words are populated; the array capacity is larger.

For further detail, see [firmware format](firmware.md), [signal-path analysis](spatial-program.md), [arithmetic and auditions](spatial-model.md), [TFX controller](tfx-control.md), [endpoint compatibility](position-33.md) and [verification](verification.md).
