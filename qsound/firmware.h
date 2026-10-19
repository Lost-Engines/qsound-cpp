#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace qsound {

inline unsigned int constexpr program_capacity{512};

// The on-disk program has scrambled instruction bytes, then a byte-sum and ID.
// Decode on the loading thread; no firmware parsing belongs in an audio callback.
struct program {
  std::array<std::uint32_t, program_capacity> instructions{};
  unsigned int size{0};
  std::uint16_t checksum{0};
  std::uint16_t identifier{0};
};

struct function_description {
  std::uint16_t number{0};
  std::uint16_t codec{0};
  std::uint16_t directions{0};
  std::uint16_t sample_widths{0};
  std::uint16_t channels{0};
  std::uint16_t sample_rates{0};
};

struct firmware_function {
  function_description description;
  std::vector<program> initialisers;
  program processing;
};

class format_error : public std::runtime_error {
public:
  std::size_t const offset;
  format_error(std::size_t offset, char const *message);
};

// Creative's permutation is an involution. This is a byte transformation,
// independent of host endianness and separate from program checksum validation.
constexpr std::uint8_t unscramble_byte(std::uint8_t const value) noexcept {
  return static_cast<std::uint8_t>(((value & 0b1010'0000u) >> 5)
    | ((value & 0b0101'0000u) >> 3)
    | ((value & 0b0000'1010u) << 3)
    | ((value & 0b0000'0101u) << 5));
}

program decode_program(std::span<std::uint8_t const> bytes);

// Accepts Creative RIFF/CSP containers, including multiple functions and init
// blocks. Unknown chunks are skipped within their declared parent's bounds.
std::vector<firmware_function> decode_firmware(std::span<std::uint8_t const> bytes);
firmware_function const &select_qsound_function(std::span<firmware_function const> functions, unsigned int sample_rate);

} // namespace qsound
