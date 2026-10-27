#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <print>
#include <string_view>
#include <vector>
#include "qsound/firmware.h"
#include "qsound/upgrade.h"
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
    std::println(std::cerr, "Usage: qsound_inspect FILE [--instructions|--extensions]");
    return 2;
  }
  bool const instructions{argc == 3 && std::string_view{argv[2]} == "--instructions"};
  bool const extensions{argc == 3 && std::string_view{argv[2]} == "--extensions"};
  if(argc == 3 && !instructions && !extensions) {
    std::println(std::cerr, "Unknown option: {}", argv[2]);
    return 2;
  }
  try {
    std::ifstream file{argv[1], std::ios::binary};
    if(!file) throw std::runtime_error{"cannot open firmware file"};
    std::vector<std::uint8_t> const bytes{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    auto const firmware{qsound::decode_firmware_asset(bytes)};
    std::map<std::uint32_t, unsigned int> extension_counts;
    for(auto const &function : firmware) {
      auto const &description{function.description};
      std::println("Function {}: codec {:04x}, direction {:04x}, widths {:04x}, channels {:04x}, rates {:04x}",
        description.number, description.codec, description.directions, description.sample_widths, description.channels, description.sample_rates);
      std::println("  {} initialisers, {} processing instructions", function.initialisers.size(), function.processing.size);
      if(extensions) {
        auto const report{[&](qsound::program const &program, std::string_view const name) {
          for(unsigned int address{0}; address != program.size; ++address) {
            auto const word{program.instructions[address]};
            if(qsound::classify_instruction(word) != qsound::instruction_format::undocumented) continue;
            ++extension_counts[word];
            std::println("  {} address {:03x}: {:08x}", name, address, word);
          }
        }};
        for(auto const &program : function.initialisers) report(program, "init");
        report(function.processing, "main");
      }
      if(!instructions) continue;
      for(auto const &program : function.initialisers) inspect_program(program, "init");
      inspect_program(function.processing, "main");
    }
    if(extensions) {
      std::println("{} unique words outside the documented ST18930 format families:", extension_counts.size());
      for(auto const &[word, count] : extension_counts) std::println("  {:08x}: {} occurrences", word, count);
      std::println("Recognising the other formats does not establish their ST18932 execution semantics.");
    }
    return 0;
  } catch(qsound::format_error const &error) {
    std::println(std::cerr, "Firmware error at byte {}: {}", error.offset, error.what());
  } catch(std::exception const &error) {
    std::println(std::cerr, "Error: {}", error.what());
  }
  return 1;
}
