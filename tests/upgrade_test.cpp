#include <array>
#include <cassert>
#include <cstdlib>
#include <string>
#include "qsound/upgrade.h"

auto main()->int {
  // Public binary-literal DCL example: an overlapping backward copy.
  std::array<std::uint8_t, 8> constexpr compressed{0x00, 0x04, 0x82, 0x24, 0x25, 0x8f, 0x80, 0x7f};
  auto const output{qsound::unpack_upgrade_firmware(compressed)};
  assert(std::string(output.begin(), output.end()) == "AIAIAIAIAIAIA");
  for(unsigned int length{}; length != compressed.size(); ++length) {
    bool rejected{};
    try {
      static_cast<void>(qsound::unpack_upgrade_firmware(std::span{compressed}.first(length)));
    } catch(std::exception const &) {
      rejected = true;
    }
    assert(rejected);
  }
  bool rejected{};
  try {
    static_cast<void>(qsound::unpack_upgrade_firmware(compressed, 12));
  } catch(std::exception const &) {
    rejected = true;
  }
  assert(rejected && qsound::unpack_upgrade_firmware(compressed, 13) == output);
  for(auto const header : {std::array<std::uint8_t, 2>{1, 4}, {0, 3}, {0, 7}}) {
    rejected = false;
    try {
      static_cast<void>(qsound::unpack_upgrade_firmware(header));
    } catch(std::exception const &) {
      rejected = true;
    }
    assert(rejected);
  }
  return EXIT_SUCCESS;
}
