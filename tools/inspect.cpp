#include <fstream>
#include <iostream>
#include <iterator>
#include <print>
#include <string_view>
#include <vector>
#include "qsound/firmware.h"
#include "qsound/instruction.h"

namespace {

void inspect_program(qsound::program const &program, std::string_view const name) {
  std::println("{}: {} instructions, checksum {:04x}, identifier {:04x}", name, program.size, program.checksum, program.identifier);
  for(unsigned int i{0}; i != program.size; ++i) {
    std::println("  {:03x} {:08x} {}", i, program.instructions[i], qsound::describe_instruction(program.instructions[i]));
  }
}

} // anonymous namespace

auto main(int const argc, char const *const *argv)->int {
  if(argc != 2 && argc != 3) {
    std::println(std::cerr, "Usage: qsound_inspect FILE [--instructions]");
    return 2;
  }
  bool const instructions{argc == 3 && std::string_view{argv[2]} == "--instructions"};
  if(argc == 3 && !instructions) {
    std::println(std::cerr, "Unknown option: {}", argv[2]);
    return 2;
  }
  try {
    std::ifstream file{argv[1], std::ios::binary};
    if(!file) throw std::runtime_error{"cannot open firmware file"};
    std::vector<std::uint8_t> const bytes{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    auto const firmware{qsound::decode_firmware(bytes)};
    for(auto const &function : firmware) {
      auto const &description{function.description};
      std::println("Function {}: codec {:04x}, direction {:04x}, widths {:04x}, channels {:04x}, rates {:04x}",
        description.number, description.codec, description.directions, description.sample_widths, description.channels, description.sample_rates);
      std::println("  {} initialisers, {} processing instructions", function.initialisers.size(), function.processing.size);
      if(!instructions) continue;
      for(auto const &program : function.initialisers) inspect_program(program, "init");
      inspect_program(function.processing, "main");
    }
    return 0;
  } catch(qsound::format_error const &error) {
    std::println(std::cerr, "Firmware error at byte {}: {}", error.offset, error.what());
  } catch(std::exception const &error) {
    std::println(std::cerr, "Error: {}", error.what());
  }
  return 1;
}
