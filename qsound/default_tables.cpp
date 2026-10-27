#include "spatial_tables.h"
#include <stdexcept>
#include "coefficients.h"

namespace qsound {
namespace {

template<size_t TapCount>
spatial_tables make_bank(coefficients::filter_bank<TapCount> const &bank) {
  // Original X-RAM addresses are retained as inspection metadata only.
  unsigned int constexpr coefficient_ram_start{25};
  return {
    .left_filter{
      .address{coefficient_ram_start},
      .taps{bank.left.begin(), bank.left.end()},
    },
    .right_filter{
      .address{coefficient_ram_start + static_cast<unsigned int>(TapCount)},
      .taps{bank.right.begin(), bank.right.end()},
    },
    .direct_delay_samples{bank.direct_delay_samples},
    .positions{coefficients::positions},
  };
}

} // anonymous namespace

spatial_tables make_spatial_tables(unsigned int const sample_rate) {
  switch(sample_rate) {
  case 8000: return make_bank(coefficients::bank_8000);
  case 11025: return make_bank(coefficients::bank_11025);
  case 22050: return make_bank(coefficients::bank_22050);
  case 44100: return make_bank(coefficients::bank_44100);
  default: throw std::invalid_argument{"QSound coefficient banks support 8000, 11025, 22050 or 44100 Hz"};
  }
}

} // namespace qsound
