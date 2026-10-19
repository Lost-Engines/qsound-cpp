#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "spatial_tables.h"

namespace qsound {

enum class rounding_model {
  floor,
  nearest,
};

enum class accumulation_model {
  saturate_each_mac,
  saturate_output,
};

struct arithmetic_model {
  rounding_model rounding;
  accumulation_model accumulation;
};

struct pcm_inputs {
  std::int16_t left{};
  std::int16_t right{};
};

struct stereo_sample {
  std::int16_t left{};
  std::int16_t right{};
};

// Analytical port of the recovered two-input spatial path. Arithmetic policies
// are explicit hypotheses, not verified ST18932 or physical-card behaviour.
class spatial_model {
  struct output_history {
    std::vector<std::int16_t> direct;
    std::vector<std::int16_t> filtered;
    unsigned int direct_cursor{};
    unsigned int filter_cursor{};
  };

  spatial_tables tables;
  arithmetic_model arithmetic;
  position_gains left_gains{};
  position_gains right_gains{};
  output_history left_history;
  output_history right_history;

  std::int64_t accumulate(std::int64_t previous, std::int64_t product) const noexcept;
  std::int16_t quantize(std::int64_t value) const noexcept;
  std::int16_t mix(pcm_inputs input, std::int16_t left_gain, std::int16_t right_gain) const noexcept;
  std::int16_t output(std::int16_t direct, std::int16_t filtered, output_history &history,
    filter_coefficients const &filter) noexcept;

public:
  spatial_model(spatial_tables tables, arithmetic_model arithmetic);
  void set_positions(unsigned int left, unsigned int right);
  stereo_sample process(pcm_inputs input) noexcept;
  void reset() noexcept;
};

} // namespace qsound
