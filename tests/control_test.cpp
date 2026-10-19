#include <cassert>
#include <cstdlib>
#include "qsound/position_sweep.h"

auto main()->int {
  qsound::position_sweep sweep{{
    .position{0xfffe},
    .increment{3},
    .moving{true},
  }};
  assert(!sweep.tick() && "the initial FF cache suppresses an FF position");
  assert(sweep.value().position == 1 && "phase arithmetic wraps at 16 bits");
  assert(sweep.tick() == 0 && "the published position precedes phase advancement");
  assert(!sweep.tick() && "unchanged integer positions do not generate commands");
  sweep.set({
    .position{0},
    .increment{-1},
    .moving{true},
  });
  assert(!sweep.tick() && "changing a sample preserves the shared position cache");
  assert(sweep.value().position == 65535 && "negative increments wrap downwards");
  assert(sweep.tick() == 255 && "wrapped positions are not clamped to the UI's 0..32 range");
  sweep.set({
    .position{0x1000},
    .increment{123},
    .moving{false},
  });
  assert(sweep.tick() == 16 && "a stationary sample still publishes its position");
  assert(sweep.value().position == 0x1000 && "stationary sweeps ignore the increment");
  assert(!sweep.tick() && "a stationary sample does not repeatedly publish");
  return EXIT_SUCCESS;
}
