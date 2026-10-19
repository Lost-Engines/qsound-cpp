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
  return EXIT_SUCCESS;
}
