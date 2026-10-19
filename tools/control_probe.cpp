#include <cstdlib>
#include <iostream>
#include <print>
#include "qsound/position_sweep.h"

auto main()->int {
  unsigned int phase{}, moving{}, cached{}, ticks{};
  int increment{};
  while(std::cin >> phase >> increment >> moving >> cached >> ticks) {
    if(phase > 65535 || increment < -32768 || increment > 32767 || moving > 65535 || cached > 255 || ticks > 100'000) return EXIT_FAILURE;
    qsound::position_sweep sweep{
      {
        .position{static_cast<qsound::position_phase>(phase)},
        .increment{static_cast<std::int16_t>(increment)},
        .moving{moving != 0},
      },
      static_cast<std::uint8_t>(cached),
    };
    for(unsigned int tick{0}; tick != ticks; ++tick) {
      auto const changed{sweep.tick()};
      std::println("{} {} {}", sweep.value().position, sweep.last_position(), changed ? static_cast<int>(*changed) : -1);
    }
  }
  return std::cin.eof() ? EXIT_SUCCESS : EXIT_FAILURE;
}
