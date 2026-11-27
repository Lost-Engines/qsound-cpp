#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <boost/program_options.hpp>
#include "qsound/firmware.h"
#include "qsound/instruction.h"
#include "qsound/upgrade.h"

namespace {

void inspect_program(qsound::program const &program, std::string_view const name) {
  std::println("{}: {} instructions, checksum {:04x}, identifier {:04x}", name, program.size, program.checksum, program.identifier);
  for(unsigned int i{0}; i != program.size; ++i) {
    std::println("  {:03x} {:08x} {}", i, program.instructions[i], qsound::describe_instruction(program.instructions[i]));
  }
}

} // anonymous namespace

auto main(int const argc, char const *const *argv)->int try {
  boost::program_options::options_description options{"Options"};
  options.add_options()
    ("help,h", "Show usage")
    ("firmware", boost::program_options::value<std::string>()->required()->value_name("FILE"), "Firmware file in CSP or PVL format")
    ("instructions", "List decoded instructions")
    ("extensions", "List instructions outside the documented format families");
  boost::program_options::positional_options_description positional;
  positional.add("firmware", 1);
  int constexpr option_style{boost::program_options::command_line_style::default_style & ~boost::program_options::command_line_style::allow_guessing};
  boost::program_options::variables_map arguments;
  boost::program_options::store(boost::program_options::command_line_parser(argc, argv)
    .options(options).positional(positional).style(option_style).run(), arguments);
  if(arguments.contains("help")) {
    std::cout << "Usage: qsound_inspect FILE [--instructions|--extensions]\n" << options;
    return EXIT_SUCCESS;
  }
  boost::program_options::notify(arguments);
  bool const instructions{arguments.contains("instructions")};
  bool const extensions{arguments.contains("extensions")};
  if(instructions && extensions) throw boost::program_options::error{"--instructions and --extensions are mutually exclusive"};

  std::ifstream file{arguments["firmware"].as<std::string>(), std::ios::binary};
  if(!file) throw std::runtime_error{"cannot open firmware file"};
  std::vector<uint8_t> const bytes{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
  auto const firmware{qsound::decode_firmware_asset(bytes)};
  std::map<uint32_t, unsigned int> extension_counts;
  for(auto const &function : firmware) {
    auto const &description{function.description};
    std::println("Function {}: codec {:04x}, direction {:04x}, widths {:04x}, channels {:04x}, rates {:04x}",
      description.number, description.codec, description.directions, description.sample_widths, description.channels, description.sample_rates);
    std::println("  {} initialisers, {} processing instructions", function.initialisers.size(), function.processing.size);
    if(extensions) {
      auto const report{[&](qsound::program const &program, std::string_view const name){
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
  return EXIT_SUCCESS;
} catch(boost::program_options::error const &error) {
  std::println(std::cerr, "Error: {}\nUsage: qsound_inspect FILE [--instructions|--extensions]", error.what());
  return 2;
} catch(qsound::format_error const &error) {
  std::println(std::cerr, "Firmware error at byte {}: {}", error.offset, error.what());
  return EXIT_FAILURE;
} catch(std::exception const &error) {
  std::println(std::cerr, "Error: {}", error.what());
  return EXIT_FAILURE;
}
