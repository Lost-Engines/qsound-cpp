#include "firmware.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace qsound {
namespace {

std::uint16_t little_word(std::span<std::uint8_t const> const bytes) {
  return static_cast<std::uint16_t>(bytes[0] | (static_cast<unsigned int>(bytes[1]) << 8));
}

std::uint32_t little_dword(std::span<std::uint8_t const> const bytes) {
  return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8)
    | (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

struct chunk {
  std::string_view tag;
  std::span<std::uint8_t const> data;
  std::size_t offset;
};

class chunks {
  std::span<std::uint8_t const> bytes;
  std::size_t offset;
public:
  chunks(std::span<std::uint8_t const> bytes, std::size_t offset);
  std::optional<chunk> next();
};

chunks::chunks(std::span<std::uint8_t const> const bytes, std::size_t const offset)
  : bytes{bytes},
    offset{offset} {
}

std::optional<chunk> chunks::next() {
  if(bytes.empty()) return std::nullopt;
  if(bytes.size() < 8) throw format_error{offset, "truncated RIFF chunk header"};
  std::size_t const size{little_dword(bytes.subspan(4))};
  std::size_t const padding{size & 1u};
  if(size > bytes.size() - 8 || padding > bytes.size() - 8 - size) {
    throw format_error{offset + 4, "RIFF chunk exceeds its parent"};
  }
  chunk const result{
    .tag{reinterpret_cast<char const*>(bytes.data()), 4},
    .data{bytes.subspan(8, size)},
    .offset{offset + 8},
  };
  bytes = bytes.subspan(8 + size + padding);
  offset += 8 + size + padding;
  return result;
}

bool starts_with(std::span<std::uint8_t const> const bytes, std::string_view const value) {
  return bytes.size() >= value.size() && std::ranges::equal(bytes.first(value.size()), value);
}

program read_program(chunk const &source) {
  try {
    return decode_program(source.data);
  } catch(format_error const &error) {
    throw format_error{source.offset + error.offset, error.what()};
  }
}

} // anonymous namespace

format_error::format_error(std::size_t const offset, char const *message)
  : std::runtime_error{message},
    offset{offset} {
}

program decode_program(std::span<std::uint8_t const> const bytes) {
  unsigned int constexpr trailer_size{4};
  unsigned int constexpr instruction_size{4};
  if(bytes.size() < instruction_size + trailer_size) throw format_error{0, "program has no complete instruction"};
  if((bytes.size() - trailer_size) % instruction_size != 0) throw format_error{0, "program is not a whole number of 32-bit instructions"};
  if(bytes.size() > program_capacity * instruction_size + trailer_size) throw format_error{0, "program exceeds the CSP's instruction RAM"};
  auto const encoded{bytes.first(bytes.size() - trailer_size)};
  unsigned int sum{0};
  for(auto const value : encoded) sum += value;
  std::uint16_t const checksum{little_word(bytes.last(trailer_size))};
  if(static_cast<std::uint16_t>(sum) != checksum) throw format_error{encoded.size(), "program checksum mismatch"};
  program result{
    .instructions{},
    .size{static_cast<unsigned int>(encoded.size() / instruction_size)},
    .checksum{checksum},
    .identifier{little_word(bytes.last(2))},
  };
  for(unsigned int i{0}; i != result.size; ++i) {
    std::uint32_t word{0};
    for(unsigned int byte{0}; byte != instruction_size; ++byte) {
      word |= static_cast<std::uint32_t>(unscramble_byte(encoded[i * instruction_size + byte])) << (byte * 8);
    }
    result.instructions[i] = word;
  }
  return result;
}

std::vector<firmware_function> decode_firmware(std::span<std::uint8_t const> const bytes) {
  std::size_t constexpr maximum_file_size{4 * 1024 * 1024};
  if(bytes.size() > maximum_file_size) throw format_error{0, "firmware container exceeds the loading limit"};
  if(bytes.size() < 12 || !starts_with(bytes, "RIFF")) throw format_error{0, "expected RIFF firmware"};
  if(little_dword(bytes.subspan(4)) != bytes.size() - 8) throw format_error{4, "RIFF length does not match the file"};
  if(!starts_with(bytes.subspan(8), "CSP ")) throw format_error{8, "RIFF file is not Creative CSP firmware"};

  std::vector<firmware_function> functions;
  std::optional<function_description> pending;
  chunks lists{bytes.subspan(12), 12};
  while(auto const item{lists.next()}) {
    if(item->tag != "LIST") continue;
    if(item->data.size() < 4) throw format_error{item->offset, "LIST has no type"};
    if(starts_with(item->data, "func")) {
      if(pending) throw format_error{item->offset, "function has no code list"};
      chunks fields{item->data.subspan(4), item->offset + 4};
      while(auto const field{fields.next()}) {
        if(field->tag != "desc") continue;
        if(pending) throw format_error{field->offset, "duplicate function description"};
        if(field->data.size() != 12) throw format_error{field->offset, "invalid function description size"};
        pending = function_description{
          .number{little_word(field->data)},
          .codec{little_word(field->data.subspan(2))},
          .directions{little_word(field->data.subspan(4))},
          .sample_widths{little_word(field->data.subspan(6))},
          .channels{little_word(field->data.subspan(8))},
          .sample_rates{little_word(field->data.subspan(10))},
        };
      }
      if(!pending) throw format_error{item->offset, "function list has no description"};
      if(std::ranges::any_of(functions, [&](auto const &function) { return function.description.number == pending->number; })) {
        throw format_error{item->offset, "duplicate function number"};
      }
    } else if(starts_with(item->data, "code")) {
      if(!pending) throw format_error{item->offset, "code list has no preceding description"};
      firmware_function function{
        .description{*pending},
        .initialisers{},
        .processing{},
      };
      bool found_main{false};
      chunks fields{item->data.subspan(4), item->offset + 4};
      while(auto const field{fields.next()}) {
        if(field->tag == "init") {
          if(found_main) throw format_error{field->offset, "initialiser follows the processing program"};
          function.initialisers.push_back(read_program(*field));
        } else if(field->tag == "main") {
          if(found_main) throw format_error{field->offset, "duplicate processing program"};
          function.processing = read_program(*field);
          found_main = true;
        }
      }
      if(!found_main) throw format_error{item->offset, "code list has no processing program"};
      functions.push_back(std::move(function));
      pending.reset();
    }
  }
  if(pending) throw format_error{bytes.size(), "last function has no code list"};
  if(functions.empty()) throw format_error{12, "firmware has no executable functions"};
  return functions;
}

firmware_function const &select_qsound_function(std::span<firmware_function const> const functions, unsigned int const sample_rate) {
  unsigned int rate_flag{0};
  switch(sample_rate) {
  case 8'000: rate_flag = 1; break;
  case 11'025: rate_flag = 2; break;
  case 22'050: rate_flag = 4; break;
  case 44'100: rate_flag = 8; break;
  default: throw std::invalid_argument{"QSound firmware supports 8000, 11025, 22050 or 44100 Hz"};
  }
  firmware_function const *selected{nullptr};
  for(auto const &function : functions) {
    auto const &description{function.description};
    if(description.codec != 1 || !(description.directions & 2u) || !(description.sample_rates & rate_flag)) continue;
    if(selected) throw std::invalid_argument{"ambiguous QSound sample-rate selection"};
    selected = &function;
  }
  if(!selected) throw std::invalid_argument{"no playback QSound function for the requested rate"};
  return *selected;
}

} // namespace qsound
