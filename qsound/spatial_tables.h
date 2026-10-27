#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace qsound {

inline unsigned int constexpr spatial_position_count{33};

struct filter_coefficients {
  unsigned int address{};
  std::vector<std::int16_t> taps;
};

struct position_gains {
  std::int16_t direct_left{};
  std::int16_t direct_right{};
  std::int16_t filtered_left{};
  std::int16_t filtered_right{};
};

struct spatial_tables {
  filter_coefficients left_filter;
  filter_coefficients right_filter;
  unsigned int direct_delay_samples{};
  std::array<position_gains, spatial_position_count> positions{};
};

// Make an owned, editable copy of the built-in bank. No firmware is required.
spatial_tables make_spatial_tables(unsigned int sample_rate);

struct firmware_function;

// Optional research API: also compile QSOUND_RESEARCH_SOURCES to use this.
// Recover explicit literals from the reference QSound program layout.
// This does not execute initialization, control instructions or DSP arithmetic.
spatial_tables recover_spatial_tables(firmware_function const &function);

} // namespace qsound
