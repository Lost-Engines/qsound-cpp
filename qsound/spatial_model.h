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

enum class position_policy {
  reject,
  clamp_to_endpoints,
};

struct arithmetic_model {
  rounding_model rounding;
  accumulation_model accumulation;
};

struct pcm_inputs {
  int16_t left{0};
  int16_t right{0};
};

struct stereo_sample {
  int16_t left{0};
  int16_t right{0};
};

// Analytical port of the recovered two-input spatial path. Arithmetic policies
// are explicit hypotheses, not verified ST18932 or physical-card behaviour.
class spatial_model {
private:
  struct output_history {
    std::vector<int16_t> direct;
    std::vector<int16_t> filtered;
    unsigned int direct_cursor{0};
    unsigned int filter_cursor{0};
  };

  spatial_tables tables;
  arithmetic_model arithmetic;
  position_policy positions;
  position_gains left_gains{};
  position_gains right_gains{};
  output_history left_history;
  output_history right_history;

  int64_t accumulate(int64_t previous, int64_t product) const noexcept;
  int16_t quantize(int64_t value) const noexcept;
  int16_t mix(pcm_inputs input, int16_t left_gain, int16_t right_gain) const noexcept;
  int16_t output(int16_t direct, int16_t filtered, output_history &history,
                 filter_coefficients const &filter) noexcept;

public:
  spatial_model(spatial_tables tables, arithmetic_model arithmetic, position_policy positions = position_policy::reject);
  void set_positions(unsigned int left, unsigned int right);
  stereo_sample process(pcm_inputs input) noexcept;
  void reset() noexcept;
};

} // namespace qsound
