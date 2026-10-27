#pragma once

#include <cstdint>
#include <optional>

namespace qsound {

using position_phase = std::uint16_t;

struct sweep_parameters {
  position_phase position{};                                                    // unsigned 8.8 position phase
  std::int16_t increment{};                                                     // signed 8.8 phase change per callback
  bool moving{};
};

class position_sweep {
  sweep_parameters parameters;
  std::uint8_t previous_position{0xff};

public:
  explicit position_sweep(sweep_parameters parameters = {}, std::uint8_t previous_position = 0xff);
  void set(sweep_parameters parameters);
  std::optional<std::uint8_t> tick();
  sweep_parameters const &value() const noexcept;
  std::uint8_t last_position() const noexcept;
};

} // namespace qsound
