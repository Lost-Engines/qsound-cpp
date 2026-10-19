#include "spatial_tables.h"
#include <bit>
#include <optional>
#include <stdexcept>
#include <utility>

namespace qsound {
namespace {

auto signed_word(std::uint32_t const value)->std::int16_t {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

void require(bool const condition) {
  if(!condition) throw std::invalid_argument{"unsupported QSound spatial-table instruction layout"};
}

auto filter_blocks(program const &initialiser)->std::array<filter_coefficients, 2> {
  /// Read consecutive immediate-to-X0 literal pairs without simulating the DSP
  std::array<filter_coefficients, 2> filters{};
  unsigned int found{};
  unsigned int constexpr first_coefficient_address{25};
  for(unsigned int pc{8}; pc + 2 < initialiser.size; ++pc) {
    auto const word{initialiser.instructions[pc]};
    if((word & 0xfffff000u) != 0xf8600000u || (word & 0xfffu) < first_coefficient_address) continue;
    if((initialiser.instructions[pc + 1] & 0xffff0000u) != 0x742b0000u || initialiser.instructions[pc + 2] != 0x1400201cu) continue;
    require(found < filters.size());
    auto &filter{filters[found++]};
    filter.address = word & 0xfffu;
    ++pc;
    while(pc + 1 < initialiser.size && (initialiser.instructions[pc] & 0xffff0000u) == 0x742b0000u
      && initialiser.instructions[pc + 1] == 0x1400201cu) {
      filter.taps.push_back(signed_word(initialiser.instructions[pc]));
      pc += 2;
    }
    --pc;
  }
  require(found == filters.size() && filters[0].address == first_coefficient_address);
  require(filters[1].address == filters[0].address + filters[0].taps.size());
  require(!filters[0].taps.empty() && filters[0].taps.size() == filters[1].taps.size());
  return filters;
}

auto gain_table(program const &initialiser)->std::array<position_gains, spatial_position_count> {
  /// Reconstruct the two opposing literal-write streams forming the position table
  unsigned int constexpr words_per_position{4};
  unsigned int constexpr table_words{spatial_position_count * words_per_position};
  unsigned int constexpr reverse_start{table_words - 1};
  unsigned int pc{8};
  bool zero_b{};
  while(pc + 1 < initialiser.size && !(initialiser.instructions[pc] == 0xfc600000u
    && initialiser.instructions[pc + 1] == (0xfd600000u | reverse_start))) {
    auto const word{initialiser.instructions[pc++]};
    if((word >> 30) == 0 && ((word >> 15) & 3u) == 3) {
      zero_b = ((word >> 17) & 31u) == 19;
    }
    require((word >> 29) != 4 && (word >> 28) != 10);
  }
  require(pc + 1 < initialiser.size && zero_b);
  pc += 2;
  std::array<std::optional<std::int16_t>, table_words> words{};
  int forward{};
  int backward{static_cast<int>(reverse_start)};
  std::optional<std::int16_t> accumulator;
  std::optional<std::int16_t> result;
  while(pc < initialiser.size) {
    auto const word{initialiser.instructions[pc++]};
    if((word & 0xfffe0000u) == 0x742a0000u) {
      result = signed_word(word);
      if(word & 0x10000u) accumulator = result;
      continue;
    }
    if(word == 0xfc600084u) break;                                                // next section initializes the delay rings
    auto const source{(word >> 12) & 7u};
    auto const destination{(word >> 1) & 7u};
    require((word & 0xffffc0ffu) == 0x04000008u || (word & 0xffffc0ffu) == 0x0400000au);
    require((destination == 4 && ((word >> 8) & 3u) == 1) || (destination == 5 && ((word >> 8) & 3u) == 3));
    std::optional<std::int16_t> value;
    if(source == 0) value = result;
    else if(source == 2) value = accumulator;
    else if(source == 3) value = 0;
    require(value.has_value());
    auto const address{destination == 4 ? forward++ : backward--};
    require(address >= 0 && address < static_cast<int>(table_words) && !words[static_cast<unsigned int>(address)]);
    words[static_cast<unsigned int>(address)] = *value;
  }
  std::array<position_gains, spatial_position_count> positions{};
  for(unsigned int position{0}; position != spatial_position_count; ++position) {
    auto const index{position * words_per_position};
    for(unsigned int lane{0}; lane != words_per_position; ++lane) require(words[index + lane].has_value());
    positions[position] = {
      .direct_left{*words[index + 1]},
      .direct_right{*words[index + 2]},
      .filtered_left{*words[index]},
      .filtered_right{*words[index + 3]},
    };
  }
  return positions;
}

} // anonymous namespace

spatial_tables recover_spatial_tables(firmware_function const &function) {
  /// Decode the known literal layout, rejecting changes requiring further analysis
  require(function.description.codec == 1 && function.initialisers.size() == 1);
  auto const &initialiser{function.initialisers.front()};
  auto const &processing{function.processing};
  require(initialiser.size <= program_capacity && processing.size <= program_capacity);
  require(processing.size == 351 && initialiser.size >= 8);
  auto filters{filter_blocks(initialiser)};
  unsigned int constexpr left_ring_setup{0x72};
  unsigned int constexpr left_filter_setup{0x82};
  unsigned int constexpr right_filter_setup{0x9c};
  require(processing.instructions[left_ring_setup] == 0xfc600084u);
  require((processing.instructions[left_ring_setup + 1] & 0xfffff000u) == 0xfd600000u);
  require(processing.instructions[left_filter_setup] == (0xf8600000u | filters[0].address));
  require(processing.instructions[right_filter_setup] == (0xf8600000u | filters[1].address));
  require((processing.instructions[left_filter_setup + 1] & 0xffffff00u) == 0xca602000u);
  require((processing.instructions[left_filter_setup + 1] & 255u) + 3 == filters[0].taps.size());
  auto const ring_end{processing.instructions[left_ring_setup + 1] & 0xfffu};
  unsigned int constexpr first_delay_sample{133};
  require(ring_end >= first_delay_sample);
  return {
    .left_filter{std::move(filters[0])},
    .right_filter{std::move(filters[1])},
    .direct_delay_samples{ring_end - first_delay_sample},
    .positions{gain_table(initialiser)},
  };
}

} // namespace qsound
