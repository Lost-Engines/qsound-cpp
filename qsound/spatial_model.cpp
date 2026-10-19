#include "spatial_model.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace qsound {
namespace {

unsigned int constexpr accumulator_word_shift{16};
std::int64_t constexpr word_alignment{std::int64_t{1} << accumulator_word_shift};
std::int64_t constexpr product_alignment{2};

auto saturate_accumulator(std::int64_t const value)->std::int64_t {
  return std::clamp(value, static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
    static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()));
}

} // anonymous namespace

spatial_model::spatial_model(spatial_tables tables, arithmetic_model const arithmetic)
  : tables{std::move(tables)},
    arithmetic{arithmetic} {
  /// Allocate bounded histories once; processing subsequently performs no allocation
  unsigned int constexpr maximum_history{program_capacity};
  if(this->tables.left_filter.taps.empty() || this->tables.right_filter.taps.empty()
    || this->tables.left_filter.taps.size() > maximum_history || this->tables.right_filter.taps.size() > maximum_history
    || this->tables.direct_delay_samples >= maximum_history) {
    throw std::invalid_argument{"spatial model requires bounded nonempty filter histories"};
  }
  left_history.direct.resize(this->tables.direct_delay_samples + 1);
  right_history.direct.resize(this->tables.direct_delay_samples + 1);
  left_history.filtered.resize(this->tables.left_filter.taps.size());
  right_history.filtered.resize(this->tables.right_filter.taps.size());
  unsigned int constexpr centre_position{spatial_position_count / 2};
  set_positions(centre_position, centre_position);
}

void spatial_model::set_positions(unsigned int const left, unsigned int const right) {
  /// Preserve unsupported positions as an explicit unresolved case, without clamping
  if(left >= spatial_position_count || right >= spatial_position_count) {
    throw std::out_of_range{"spatial model only supports recovered table positions 0..32; TFX position 33 remains unresolved"};
  }
  left_gains = tables.positions[left];
  right_gains = tables.positions[right];
}

std::int64_t spatial_model::accumulate(std::int64_t const previous, std::int64_t const product) const noexcept {
  auto const sum{previous + product};
  return arithmetic.accumulation == accumulation_model::saturate_each_mac ? saturate_accumulator(sum) : sum;
}

std::int16_t spatial_model::quantize(std::int64_t const value) const noexcept {
  /// Compare the two rounding hypotheses with defined signed shifts and clipping
  auto const bounded{saturate_accumulator(value)};
  auto const rounded{bounded + (arithmetic.rounding == rounding_model::nearest ? word_alignment / 2 : 0)};
  auto const word{rounded >> accumulator_word_shift};
  return static_cast<std::int16_t>(std::clamp(word, static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::min()),
    static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::max())));
}

std::int16_t spatial_model::mix(pcm_inputs const input, std::int16_t const left_gain, std::int16_t const right_gain) const noexcept {
  auto const left{static_cast<std::int64_t>(input.left) * left_gain * product_alignment};
  auto const right{static_cast<std::int64_t>(input.right) * right_gain * product_alignment};
  return quantize(accumulate(accumulate(0, left), right));
}

std::int16_t spatial_model::output(std::int16_t const direct, std::int16_t const filtered,
  output_history &history, filter_coefficients const &filter) noexcept {
  /// Follow write/advance/read order: literal tap zero sees the oldest history word
  history.direct[history.direct_cursor] = direct;
  if(++history.direct_cursor == history.direct.size()) history.direct_cursor = 0;
  auto const delayed{history.direct[history.direct_cursor]};
  history.filtered[history.filter_cursor] = filtered;
  if(++history.filter_cursor == history.filtered.size()) history.filter_cursor = 0;
  std::int64_t accumulator{static_cast<std::int64_t>(delayed) * word_alignment};
  auto cursor{history.filter_cursor};
  for(auto const coefficient : filter.taps) {
    auto const product{static_cast<std::int64_t>(history.filtered[cursor]) * coefficient * product_alignment};
    accumulator = accumulate(accumulator, product);
    if(++cursor == history.filtered.size()) cursor = 0;
  }
  return quantize(accumulator);
}

stereo_sample spatial_model::process(pcm_inputs const input) noexcept {
  auto const direct_left{mix(input, left_gains.direct_left, right_gains.direct_left)};
  auto const direct_right{mix(input, left_gains.direct_right, right_gains.direct_right)};
  auto const filtered_left{mix(input, left_gains.filtered_left, right_gains.filtered_left)};
  auto const filtered_right{mix(input, left_gains.filtered_right, right_gains.filtered_right)};
  return {
    .left{output(direct_left, filtered_left, left_history, tables.left_filter)},
    .right{output(direct_right, filtered_right, right_history, tables.right_filter)},
  };
}

void spatial_model::reset() noexcept {
  /// Clear delay histories while retaining the selected source positions
  auto const clear{[](output_history &history) {
    std::ranges::fill(history.direct, 0);
    std::ranges::fill(history.filtered, 0);
    history.direct_cursor = history.filter_cursor = 0;
  }};
  clear(left_history);
  clear(right_history);
}

} // namespace qsound
