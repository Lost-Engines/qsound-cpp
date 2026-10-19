#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>
#include "qsound/firmware.h"
#include "qsound/instruction.h"

namespace {

using bytes = std::vector<std::uint8_t>;

void require(bool const condition, char const *message) {
  if(!condition) throw std::runtime_error{message};
}

void word(bytes &target, unsigned int const value, unsigned int const size = 2) {
  for(unsigned int byte{0}; byte != size; ++byte) target.push_back(static_cast<std::uint8_t>(value >> (byte * 8)));
}

void tag(bytes &target, std::string_view const text) {
  target.insert(target.end(), text.begin(), text.end());
}

void chunk(bytes &target, std::string_view const name, std::span<std::uint8_t const> const payload) {
  tag(target, name);
  word(target, static_cast<unsigned int>(payload.size()), 4);
  target.insert(target.end(), payload.begin(), payload.end());
  if(payload.size() & 1u) target.push_back(0);
}

bytes program(std::span<std::uint32_t const> const instructions) {
  bytes result;
  for(auto const instruction : instructions) {
    for(unsigned int byte{0}; byte != 4; ++byte) {
      result.push_back(qsound::unscramble_byte(static_cast<std::uint8_t>(instruction >> (byte * 8))));
    }
  }
  unsigned int checksum{0};
  for(auto const value : result) checksum += value;
  word(result, checksum);
  word(result, 0x1234);
  return result;
}

bytes firmware() {
  bytes description;
  for(auto const value : {1u, 1u, 2u, 3u, 3u, 8u}) word(description, value);
  bytes function;
  tag(function, "func");
  chunk(function, "desc", description);
  chunk(function, "odd!", std::array<std::uint8_t, 1>{37});
  bytes code;
  tag(code, "code");
  chunk(code, "init", program(std::array{0x8800'0200u}));
  chunk(code, "main", program(std::array{0x742b'1234u, 0xa808'0000u}));
  bytes payload;
  tag(payload, "CSP ");
  chunk(payload, "LIST", function);
  chunk(payload, "LIST", code);
  bytes result;
  chunk(result, "RIFF", payload);
  return result;
}

template<typename F>
void rejects(F const &function) {
  try {
    function();
  } catch(std::invalid_argument const&) {
    return;
  } catch(qsound::format_error const&) {
    return;
  }
  throw std::runtime_error{"malformed firmware was accepted"};
}

} // anonymous namespace

auto main()->int {
  try {
    for(unsigned int value{0}; value != 256; ++value) {
      auto const byte{static_cast<std::uint8_t>(value)};
      require(qsound::unscramble_byte(qsound::unscramble_byte(byte)) == byte, "instruction permutation is not reversible");
    }
    require(qsound::unscramble_byte(1) == 0x20 && qsound::unscramble_byte(0x80) == 4, "instruction bit positions changed");
    // Captured encoded entry instruction, independently decoded as BRI $0008.
    auto const entry{qsound::decode_program(std::array<std::uint8_t, 8>{0, 0x10, 0, 0x44, 0x54, 0, 0x34, 0x12})};
    require(entry.instructions[0] == 0x8800'0200u && entry.size == 1, "known instruction did not decode correctly");
    auto const file{firmware()};
    auto const functions{qsound::decode_firmware(file)};
    auto const &selected{qsound::select_qsound_function(functions, 44'100)};
    require(selected.description.number == 1 && selected.initialisers.size() == 1, "function metadata changed");
    require(selected.processing.size == 2 && selected.processing.instructions[0] == 0x742b'1234u, "instruction decoding changed");
    require(selected.processing.identifier == 0x1234, "program identifier changed");
    rejects([&] { qsound::select_qsound_function(functions, 48'000); });
    rejects([&] { qsound::select_qsound_function(functions, 22'050); });
    auto duplicate{functions};
    duplicate.push_back(selected);
    rejects([&] { qsound::select_qsound_function(duplicate, 44'100); });
    for(std::size_t size{0}; size != file.size(); ++size) {
      rejects([&] { qsound::decode_firmware(std::span{file}.first(size)); });
    }
    for(unsigned int byte{0}; byte != 4; ++byte) {
      auto broken{file};
      broken[4 + byte] ^= 0xff;
      rejects([&] { qsound::decode_firmware(broken); });
    }
    // Keep the enclosing RIFF length valid while damaging a child's length.
    auto short_list{file};
    short_list[16] = 0xff;
    rejects([&] { qsound::decode_firmware(short_list); });
    auto duplicate_function{file};
    duplicate_function.insert(duplicate_function.end(), file.begin() + 12, file.end());
    auto const duplicated_size{static_cast<std::uint32_t>(duplicate_function.size() - 8)};
    for(unsigned int byte{0}; byte != 4; ++byte) duplicate_function[4 + byte] = static_cast<std::uint8_t>(duplicated_size >> (byte * 8));
    rejects([&] { qsound::decode_firmware(duplicate_function); });
    auto wrong_type{file};
    wrong_type[8] = 'W';
    rejects([&] { qsound::decode_firmware(wrong_type); });
    auto broken{file};
    broken.back() ^= 1;
    // Program IDs are not part of the checksum. Corrupt an instruction instead.
    broken[file.size() - 12] ^= 1;
    rejects([&] { qsound::decode_firmware(broken); });
    std::array<std::uint32_t, qsound::program_capacity + 1> too_large{};
    rejects([&] { qsound::decode_program(program(too_large)); });
    auto const largest{qsound::decode_program(program(std::span{too_large}.first(qsound::program_capacity)))};
    require(largest.size == qsound::program_capacity, "maximum-sized valid program was rejected");
    rejects([&] { qsound::decode_program(std::array<std::uint8_t, 9>{}); });
    require(qsound::classify_instruction(0x8800'0200) == qsound::instruction_format::immediate_branch, "branch classification changed");
    require(qsound::classify_instruction(0x688e'f0ff) == qsound::instruction_format::undocumented, "ST18932 extension was silently treated as ST18930");
    std::cout << "Firmware loading, endian conversion, checksum, rate selection, unknown chunks and truncation checks passed\n";
    return EXIT_SUCCESS;
  } catch(std::exception const &error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
