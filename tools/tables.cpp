#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <print>
#include <vector>
#include "qsound/firmware.h"
#include "qsound/upgrade.h"
#include "qsound/spatial_tables.h"

namespace {

void print_filter(qsound::filter_coefficients const &filter) {
  std::print("{{\"address\":{},\"taps\":[", filter.address);
  bool first{true};
  for(auto const coefficient : filter.taps) {
    std::print("{}{}", first ? "" : ",", coefficient);
    first = false;
  }
  std::print("]}}");
}

} // anonymous namespace

auto main(int const argc, char const *const *argv)->int try {
  if(argc != 1 && argc != 2) {
    std::println(std::cerr, "Usage: qsound_tables [FIRMWARE] (default: built-in coefficients)");
    return EXIT_FAILURE;
  }
  std::vector<qsound::firmware_function> firmware;
  if(argc == 2) {
    std::ifstream file{argv[1], std::ios::binary};
    if(!file) throw std::runtime_error{"cannot open firmware file"};
    std::vector<uint8_t> const bytes{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    firmware = qsound::decode_firmware_asset(bytes);
  }
  std::print("[");
  bool first_function{true};
  for(unsigned int const rate : {44'100u, 22'050u, 11'025u, 8'000u}) {
    auto const tables{firmware.empty() ? qsound::make_spatial_tables(rate)
      : qsound::recover_spatial_tables(qsound::select_qsound_function(firmware, rate))};
    std::print("{}{{\"rate\":{},\"direct_delay_samples\":{},\"left_filter\":", first_function ? "" : ",", rate, tables.direct_delay_samples);
    print_filter(tables.left_filter);
    std::print(",\"right_filter\":");
    print_filter(tables.right_filter);
    std::print(",\"positions\":[");
    bool first_position{true};
    for(auto const &position : tables.positions) {
      std::print("{}[{},{},{},{}]", first_position ? "" : ",", position.direct_left, position.direct_right, position.filtered_left, position.filtered_right);
      first_position = false;
    }
    std::print("]}}");
    first_function = false;
  }
  std::println("]");
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::println(std::cerr, "Table recovery failed: {}", error.what());
  return EXIT_FAILURE;
}
