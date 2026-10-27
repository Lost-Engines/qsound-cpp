#pragma once

#include "firmware.h"

namespace qsound {

inline unsigned int constexpr maximum_upgrade_firmware_bytes{65536};

// MAIN.PVL's first binary-literal DCL member is the original QSound container.
// Unpack on the loading thread; no ZIP/DOS/Python dependency is needed at runtime.
std::vector<std::uint8_t> unpack_upgrade_firmware(std::span<std::uint8_t const> bytes,
  unsigned int maximum_bytes = maximum_upgrade_firmware_bytes);
std::vector<firmware_function> decode_firmware_asset(std::span<std::uint8_t const> bytes);

} // namespace qsound
