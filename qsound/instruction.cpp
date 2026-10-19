#include "instruction.h"
#include <format>

namespace qsound {

instruction_format classify_instruction(std::uint32_t const word) noexcept {
  if((word >> 30) == 0) return instruction_format::indirect_calculation;
  if((word >> 29) == 2) return instruction_format::direct_calculation;
  if((word >> 27) == 14) return instruction_format::immediate_calculation;
  if((word >> 27) == 15) return instruction_format::shift;
  if((word >> 29) == 4) return instruction_format::immediate_branch;
  if((word >> 28) == 10) return instruction_format::computed_branch;
  if((word >> 26) == 24) return instruction_format::register_transfer;
  if((word >> 30) == 3) return instruction_format::initialise;
  return instruction_format::undocumented;
}

std::string_view format_name(instruction_format const format) noexcept {
  switch(format) {
  case instruction_format::indirect_calculation: return "OPIN";
  case instruction_format::direct_calculation: return "OPDI";
  case instruction_format::immediate_calculation: return "OPIM";
  case instruction_format::shift: return "SHIFT";
  case instruction_format::immediate_branch: return "BRI";
  case instruction_format::computed_branch: return "BRC";
  case instruction_format::register_transfer: return "SVR";
  case instruction_format::initialise: return "INI";
  case instruction_format::undocumented: return "EXTENSION";
  }
  return "INVALID";
}

std::string describe_instruction(std::uint32_t const word) {
  auto const format{classify_instruction(word)};
  auto const name{format_name(format)};
  switch(format) {
  case instruction_format::indirect_calculation:
    return std::format("{} lsrc={} rsrc={} alu={:02x} dst={} zsrc={} zdst={} m={} n={} inc={}/{}/{}",
      name, (word >> 24) & 7, (word >> 27) & 3, (word >> 17) & 31, (word >> 15) & 3,
      (word >> 12) & 7, (word >> 1) & 7, (word >> 11) & 1, (word >> 10) & 1,
      (word >> 4) & 3, (word >> 6) & 3, (word >> 8) & 3);
  case instruction_format::direct_calculation:
    return std::format("{} lsrc={} address={:03x} rsrc={} alu={:02x} dst={} zdst={}",
      name, (word >> 24) & 7, (word >> 4) & 0xfff, (word >> 27) & 3,
      (word >> 17) & 31, (word >> 16) & 1, word & 15);
  case instruction_format::immediate_calculation:
    return std::format("{} lsrc={} immediate={:04x} alu={:02x} dst={}",
      name, (word >> 24) & 7, word & 0xffff, (word >> 17) & 31, (word >> 16) & 1);
  case instruction_format::shift:
    return std::format("{} lsrc={} address={:03x} operation={} count={} dst={}",
      name, (word >> 24) & 7, (word >> 4) & 0xfff, (word >> 21) & 3, (word >> 17) & 15, (word >> 16) & 1);
  case instruction_format::immediate_branch:
    return std::format("{} address={:04x} ras={} condition={} true={} save={}",
      name, (word >> 6) & 0xffff, (word >> 28) & 1, (word >> 23) & 15, (word >> 27) & 1, (word >> 22) & 1);
  case instruction_format::computed_branch:
    return std::format("{} source={} condition={} true={} rti={} save={}",
      name, (word >> 12) & 7, (word >> 23) & 15, (word >> 27) & 1, (word >> 19) & 1, (word >> 22) & 1);
  case instruction_format::register_transfer:
    return std::format("{} source={} address={:03x} dst={} zt={}",
      name, (word >> 22) & 15, (word >> 4) & 0xfff, (word >> 1) & 7, word & 1);
  case instruction_format::initialise:
    return std::format("{} jdst={} j={:02x} kdst={} k={:03x} mode={} saturation={} address_order={}",
      name, (word >> 27) & 7, (word >> 12) & 255, (word >> 24) & 7, word & 0xfff,
      (word >> 22) & 3, (word >> 21) & 1, (word >> 20) & 1);
  case instruction_format::undocumented:
    return std::format("{} word={:08x} (not described by ST18930/31 formats)", name, word);
  }
  return "INVALID";
}

} // namespace qsound
