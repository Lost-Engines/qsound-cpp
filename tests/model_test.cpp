#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#include <vector>
#include "qsound/spatial_model.h"

namespace {

auto tables(unsigned int const delay, std::vector<std::int16_t> taps, qsound::position_gains const gains)->qsound::spatial_tables {
  qsound::spatial_tables result{
    .left_filter{
      .address{},
      .taps{taps},
    },
    .right_filter{
      .address{},
      .taps{std::move(taps)},
    },
    .direct_delay_samples{delay},
    .positions{},
  };
  result.positions.fill(gains);
  return result;
}

} // anonymous namespace

auto main()->int {
  for(unsigned int const rate : {8000u, 11025u, 22050u, 44100u}) {
    auto changed{qsound::make_spatial_tables(rate)};
    auto const original{changed};
    changed.left_filter.taps[0] ^= 1;
    changed.positions[16].direct_left ^= 1;
    changed.direct_delay_samples = 0;
    auto const fresh{qsound::make_spatial_tables(rate)};
    assert(fresh.left_filter.taps == original.left_filter.taps);
    assert(fresh.positions[16].direct_left == original.positions[16].direct_left);
    assert(fresh.direct_delay_samples == original.direct_delay_samples);
    assert(changed.left_filter.taps != fresh.left_filter.taps && "editable tables must have independent ownership");
  }
  bool unsupported_rate{};
  try {
    qsound::make_spatial_tables(48000);
  } catch(std::invalid_argument const &) {
    unsupported_rate = true;
  }
  assert(unsupported_rate && "bank selection must not silently approximate a rate");
  for(auto const rounding : {qsound::rounding_model::floor, qsound::rounding_model::nearest}) {
    for(auto const accumulation : {qsound::accumulation_model::saturate_each_mac, qsound::accumulation_model::saturate_output}) {
      qsound::arithmetic_model const arithmetic{
        .rounding{rounding},
        .accumulation{accumulation},
      };
      qsound::spatial_model cancellation{tables(0, {-32768}, {
        .direct_left{32767},
        .direct_right{},
        .filtered_left{32767},
        .filtered_right{},
      }), arithmetic};
      for(std::int16_t const input : std::array<std::int16_t, 6>{-32768, -2000, 0, 1, 2000, 32767}) {
        auto const result{cancellation.process({.left{input}, .right{}})};
        assert(result.left == 0 && result.right == 0 && "unity negative FIR cancels the identical direct bus");
      }
      qsound::spatial_model delayed{tables(2, {0}, {
        .direct_left{32767},
        .direct_right{},
        .filtered_left{},
        .filtered_right{},
      }), arithmetic};
      assert(delayed.process({.left{16384}, .right{}}).left == 0);
      assert(delayed.process({}).left == 0);
      assert(delayed.process({}).left == (rounding == qsound::rounding_model::floor ? 16383 : 16384));
      delayed.reset();
      assert(delayed.process({}).left == 0 && "reset clears retained samples");
      qsound::spatial_model ordered{tables(0, {0, -32768}, {
        .direct_left{},
        .direct_right{},
        .filtered_left{32767},
        .filtered_right{},
      }), arithmetic};
      assert(ordered.process({.left{16384}, .right{}}).left == (rounding == qsound::rounding_model::floor ? -16383 : -16384));
      assert(ordered.process({}).left == 0 && "the last authored tap multiplies the current sample");
      bool rejected{};
      try {
        ordered.set_positions(33, 16);
      } catch(std::out_of_range const &) {
        rejected = true;
      }
      assert(rejected && "unresolved TFX position 33 must not be silently approximated");
    }
  }
  auto endpoint_tables{tables(0, {0}, {})};
  for(unsigned int position{}; position != qsound::spatial_position_count; ++position) {
    endpoint_tables.positions[position] = {
      .direct_left{static_cast<std::int16_t>(position * 512)},
      .direct_right{static_cast<std::int16_t>(16384 - position * 256)},
      .filtered_left{},
      .filtered_right{},
    };
  }
  qsound::arithmetic_model const arithmetic{
    .rounding{qsound::rounding_model::floor},
    .accumulation{qsound::accumulation_model::saturate_each_mac},
  };
  qsound::spatial_model clamped{endpoint_tables, arithmetic, qsound::position_policy::clamp_to_endpoints};
  qsound::spatial_model explicit_endpoint{std::move(endpoint_tables), arithmetic};
  for(unsigned int position{}; position != 256; ++position) {
    clamped.set_positions(position, 255 - position);
    explicit_endpoint.set_positions(std::min(position, 32u), std::min(255 - position, 32u));
    qsound::pcm_inputs const input{
      .left{12345},
      .right{-23456},
    };
    auto const actual{clamped.process(input)};
    auto const expected{explicit_endpoint.process(input)};
    assert(actual.left == expected.left && actual.right == expected.right);
  }
  return EXIT_SUCCESS;
}
