#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "spatial_tables.h"

namespace qsound::coefficients {

// Editable numerical parameters recovered from Creative QSound firmware.
// Taps are in authored oldest-to-newest order; see docs/coefficients.md.
// Source asset SHA-256: 7ffd5f1b246cbda604d1dff17b052cab0eb367d5a09a3827ae02b14b6e23bdb8
// Reproduce with tools/export_coefficients.py; no firmware is needed at runtime.

template<size_t TapCount>
struct filter_bank {
  std::array<int16_t, TapCount> left;
  std::array<int16_t, TapCount> right;
  unsigned int direct_delay_samples;
};

inline constexpr std::array<position_gains, spatial_position_count> positions{
  position_gains{
    .direct_left{32766},
    .direct_right{0},
    .filtered_left{0},
    .filtered_right{32766},
  },
  position_gains{
    .direct_left{32766},
    .direct_right{0},
    .filtered_left{0},
    .filtered_right{29884},
  },
  position_gains{
    .direct_left{32766},
    .direct_right{0},
    .filtered_left{0},
    .filtered_right{26568},
  },
  position_gains{
    .direct_left{32766},
    .direct_right{0},
    .filtered_left{0},
    .filtered_right{23364},
  },
  position_gains{
    .direct_left{32766},
    .direct_right{0},
    .filtered_left{0},
    .filtered_right{19964},
  },
  position_gains{
    .direct_left{32766},
    .direct_right{0},
    .filtered_left{0},
    .filtered_right{16704},
  },
  position_gains{
    .direct_left{32766},
    .direct_right{0},
    .filtered_left{0},
    .filtered_right{13268},
  },
  position_gains{
    .direct_left{31130},
    .direct_right{0},
    .filtered_left{0},
    .filtered_right{10634},
  },
  position_gains{
    .direct_left{29492},
    .direct_right{1638},
    .filtered_left{0},
    .filtered_right{7192},
  },
  position_gains{
    .direct_left{27870},
    .direct_right{4932},
    .filtered_left{0},
    .filtered_right{6666},
  },
  position_gains{
    .direct_left{26378},
    .direct_right{6718},
    .filtered_left{0},
    .filtered_right{6404},
  },
  position_gains{
    .direct_left{24592},
    .direct_right{8208},
    .filtered_left{0},
    .filtered_right{6084},
  },
  position_gains{
    .direct_left{23102},
    .direct_right{9994},
    .filtered_left{0},
    .filtered_right{5632},
  },
  position_gains{
    .direct_left{21464},
    .direct_right{11632},
    .filtered_left{0},
    .filtered_right{5080},
  },
  position_gains{
    .direct_left{21464},
    .direct_right{14910},
    .filtered_left{0},
    .filtered_right{4420},
  },
  position_gains{
    .direct_left{18220},
    .direct_right{14942},
    .filtered_left{0},
    .filtered_right{3762},
  },
  position_gains{
    .direct_left{18220},
    .direct_right{18220},
    .filtered_left{3288},
    .filtered_right{3288},
  },
  position_gains{
    .direct_left{15204},
    .direct_right{18514},
    .filtered_left{3850},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{14648},
    .direct_right{21200},
    .filtered_left{4320},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{11636},
    .direct_right{21466},
    .filtered_left{5198},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{9994},
    .direct_right{23102},
    .filtered_left{5632},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{8208},
    .direct_right{24592},
    .filtered_left{6084},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{6718},
    .direct_right{26378},
    .filtered_left{6404},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{4916},
    .direct_right{27852},
    .filtered_left{6514},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{1638},
    .direct_right{29492},
    .filtered_left{7192},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{0},
    .direct_right{31130},
    .filtered_left{10634},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{0},
    .direct_right{32766},
    .filtered_left{13268},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{0},
    .direct_right{32766},
    .filtered_left{16704},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{0},
    .direct_right{32766},
    .filtered_left{19964},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{0},
    .direct_right{32766},
    .filtered_left{23364},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{0},
    .direct_right{32766},
    .filtered_left{26568},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{0},
    .direct_right{32766},
    .filtered_left{29884},
    .filtered_right{0},
  },
  position_gains{
    .direct_left{0},
    .direct_right{32766},
    .filtered_left{32766},
    .filtered_right{0},
  },
};

inline constexpr filter_bank<11> bank_8000{
  .left{
    333, 1598, -262, -4195, -22108, -4649, 800, -174,
    1833, -553, -65,
  },
  .right{
    1804, 1089, -689, -2659, -22102, -4633, -176, 388,
    2135, 511, 59,
  },
  .direct_delay_samples{5},
};

inline constexpr filter_bank<17> bank_11025{
  .left{
    591, 1177, -324, 2132, -1093, -1780, -9679, -18499,
    682, -1303, 381, 597, 921, 364, -304, -414,
    -235,
  },
  .right{
    1039, 1348, 684, 591, -1647, -48, -13948, -14736,
    1954, -3232, 1983, -340, 2230, 216, 435, -463,
    -111,
  },
  .direct_delay_samples{8},
};

inline constexpr filter_bank<19> bank_22050{
  .left{
    871, -303, -1333, -1204, -1282, -4210, -8952, -10112,
    -4033, 289, 742, -608, -888, 176, 564, 344,
    100, 435, 540,
  },
  .right{
    -117, -250, -889, -536, -498, -4072, -9742, -10114,
    -2835, 357, -505, -1163, -388, 274, 283, 356,
    466, 553, 684,
  },
  .direct_delay_samples{9},
};

inline constexpr filter_bank<49> bank_44100{
  .left{
    30, 103, 194, 294, 378, 414, 355, 177,
    -71, -340, -576, -709, -736, -702, -707, -1037,
    -1914, -3185, -4549, -5478, -5023, -4199, -1599, -438,
    107, 408, 274, -127, -346, -351, -283, -137,
    33, 156, 241, 263, 206, 135, 103, 124,
    191, 250, 248, 191, 102, 20, -30, -64,
    -96,
  },
  .right{
    284, 237, 162, 90, 49, 39, 32, -10,
    -115, -270, -412, -452, -345, -185, -234, -792,
    -1971, -3507, -4804, -6108, -4664, -3185, -1523, -299,
    200, 99, -248, -519, -566, -417, -187, 15,
    135, 172, 162, 151, 167, 206, 245, 270,
    283, 301, 327, 342, 323, 256, 154, 47,
    -33,
  },
  .direct_delay_samples{24},
};

} // namespace qsound::coefficients
