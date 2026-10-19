#include "position_sweep.h"

namespace qsound {

position_sweep::position_sweep(sweep_parameters const parameters, std::uint8_t const previous_position)
  : parameters{parameters},
    previous_position{previous_position} {
}

void position_sweep::set(sweep_parameters const next) {
  /// Replace the sample's sweep without resetting the shared position cache
  parameters = next;
}

std::optional<std::uint8_t> position_sweep::tick() {
  /// Publish the current integer position before advancing the wrapping phase
  auto const position{static_cast<std::uint8_t>(parameters.position >> 8)};
  std::optional<std::uint8_t> changed;
  if(position != previous_position) {
    previous_position = position;
    changed = position;
  }
  if(parameters.moving) {
    parameters.position = static_cast<position_phase>(
      parameters.position + parameters.increment);
  }
  return changed;
}

sweep_parameters const &position_sweep::value() const noexcept {
  return parameters;
}

std::uint8_t position_sweep::last_position() const noexcept {
  return previous_position;
}

} // namespace qsound
