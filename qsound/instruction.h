#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace qsound {

// Formats documented in the ST18930/31 manual. ST18932 extensions are kept
// distinct; a recognised family must not be mistaken for verified semantics.
enum class instruction_format {
  indirect_calculation,
  direct_calculation,
  immediate_calculation,
  shift,
  immediate_branch,
  computed_branch,
  register_transfer,
  initialise,
  undocumented,
};

instruction_format classify_instruction(std::uint32_t word) noexcept;
std::string_view format_name(instruction_format format) noexcept;
std::string describe_instruction(std::uint32_t word);

} // namespace qsound
