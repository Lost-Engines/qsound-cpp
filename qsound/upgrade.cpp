#include "upgrade.h"

#include <algorithm>
#include <array>

namespace qsound {
namespace {

class bit_reader {
  std::span<std::uint8_t const> input;
  std::size_t byte{};
  unsigned int bit{};

public:
  explicit bit_reader(std::span<std::uint8_t const> const input) : input{input} {}

  auto read(unsigned int const count)->unsigned int {
    unsigned int result{};
    for(unsigned int shift{}; shift != count; ++shift) {
      if(byte == input.size()) throw format_error{byte, "truncated DCL firmware stream"};
      result |= ((input[byte] >> bit) & 1u) << shift;
      if(++bit == 8) {
        bit = 0;
        ++byte;
      }
    }
    return result;
  }
};

struct huffman_table {
  std::array<unsigned int, 14> counts{};
  std::array<unsigned int, 64> symbols{};
};

template<std::size_t Count>
constexpr auto make_table(std::array<unsigned int, Count> const &packed)->huffman_table {
  std::array<unsigned int, 64> lengths{};
  unsigned int size{};
  huffman_table result;
  for(auto const item : packed) {
    auto const length{item & 15u};
    for(unsigned int repeat{}; repeat != (item >> 4) + 1; ++repeat) lengths.at(size++) = length;
  }
  unsigned int output{};
  for(unsigned int width{1}; width != result.counts.size(); ++width) {
    for(unsigned int symbol{}; symbol != size; ++symbol) {
      if(lengths[symbol] == width) {
        result.symbols[output++] = symbol;
        ++result.counts[width];
      }
    }
  }
  return result;
}

// Binary-literal PKWARE DCL format tables, also used by our independent Python
// fetcher. Format reference: zlib/contrib/blast, Mark Adler's public description.
auto constexpr length_codes{make_table(std::array{2u, 35u, 36u, 53u, 38u, 23u})};
auto constexpr distance_codes{make_table(std::array{2u, 20u, 53u, 230u, 247u, 151u, 248u})};
std::array<unsigned int, 16> constexpr length_bases{3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264};
std::array<unsigned int, 16> constexpr length_extras{0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8};

auto symbol(bit_reader &bits, huffman_table const &table)->unsigned int {
  unsigned int code{}, first{}, index{};
  for(unsigned int width{1}; width != table.counts.size(); ++width) {
    code = (code << 1) | (bits.read(1) ^ 1u);
    auto const count{table.counts[width]};
    if(code >= first && code - first < count) return table.symbols[index + code - first];
    index += count;
    first = (first + count) << 1;
  }
  throw std::invalid_argument{"invalid DCL firmware Huffman code"};
}

} // anonymous namespace

std::vector<std::uint8_t> unpack_upgrade_firmware(std::span<std::uint8_t const> const bytes, unsigned int const maximum_bytes) {
  bit_reader bits{bytes};
  if(bits.read(8) != 0) throw std::invalid_argument{"expected binary-literal DCL firmware stream"};
  auto const dictionary{bits.read(8)};
  if(dictionary < 4 || dictionary > 6) throw std::invalid_argument{"invalid DCL firmware dictionary size"};
  std::vector<std::uint8_t> output;
  output.reserve(std::min(maximum_bytes, maximum_upgrade_firmware_bytes));
  for(;;) {
    if(!bits.read(1)) {
      if(output.size() == maximum_bytes) throw std::invalid_argument{"DCL firmware output exceeds extraction limit"};
      output.push_back(static_cast<std::uint8_t>(bits.read(8)));
      continue;
    }
    auto const code{symbol(bits, length_codes)};
    auto const length{length_bases[code] + bits.read(length_extras[code])};
    unsigned int constexpr end_marker{519};
    if(length == end_marker) return output;
    auto const extra{length == 2 ? 2u : dictionary};
    auto const distance{(symbol(bits, distance_codes) << extra) + bits.read(extra) + 1};
    if(distance > output.size()) throw std::invalid_argument{"DCL firmware back-reference precedes output"};
    if(length > maximum_bytes - output.size()) throw std::invalid_argument{"DCL firmware output exceeds extraction limit"};
    for(unsigned int index{}; index != length; ++index) output.push_back(output[output.size() - distance]);
  }
}

std::vector<firmware_function> decode_firmware_asset(std::span<std::uint8_t const> const bytes) {
  if(bytes.size() >= 4 && bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F') return decode_firmware(bytes);
  return decode_firmware(unpack_upgrade_firmware(bytes));
}

} // namespace qsound
