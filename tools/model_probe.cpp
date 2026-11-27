#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif // defined(_WIN32)
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include "qsound/firmware.h"
#include "qsound/spatial_model.h"
#include "qsound/upgrade.h"

namespace {

auto nonnegative_integer(std::string const &text, std::string_view const name)->unsigned int {
  int value{0};
  if(!boost::conversion::try_lexical_convert(text, value) || value < 0) {
    throw std::invalid_argument{std::string{name} + " must be a non-negative integer"};
  }
  return static_cast<unsigned int>(value);
}

auto sample(char const low, char const high)->int16_t {
  auto const bits{static_cast<uint16_t>(static_cast<unsigned char>(low)
    | (static_cast<unsigned int>(static_cast<unsigned char>(high)) << 8))};
  return std::bit_cast<int16_t>(bits);
}

} // anonymous namespace

auto main(int const argc, char const *const *argv)->int try {
  boost::program_options::options_description options{"Options"};
  options.add_options()
    ("help,h", "Show usage")
    ("firmware", boost::program_options::value<std::string>()->value_name("FILE"), "Read tables from a CSP or PVL file (default: built-in coefficients)");
  boost::program_options::options_description command_line;
  command_line.add(options).add_options()("parameters", boost::program_options::value<std::vector<std::string>>(), "Model parameters");
  int constexpr parameter_count{5};
  boost::program_options::positional_options_description positional;
  positional.add("parameters", parameter_count + 1);
  int constexpr option_style{boost::program_options::command_line_style::default_style & ~boost::program_options::command_line_style::allow_guessing};
  boost::program_options::variables_map arguments;
  boost::program_options::store(boost::program_options::command_line_parser(argc, argv)
    .options(command_line).positional(positional).style(option_style).run(), arguments);
  if(arguments.contains("help")) {
    std::cout << "Usage: qsound_model_probe RATE LEFT_POSITION RIGHT_POSITION floor|nearest each|output [--firmware FILE]\n"
              << "       qsound_model_probe FIRMWARE RATE LEFT_POSITION RIGHT_POSITION floor|nearest each|output\n"
              << "Reads and writes raw stereo signed 16-bit little-endian PCM using the analytical model.\n"
              << options;
    return EXIT_SUCCESS;
  }
  boost::program_options::notify(arguments);

  // The legacy syntax has one extra positional parameter before the rate.
  std::span<std::string const> parameters;
  if(arguments.contains("parameters")) parameters = arguments["parameters"].as<std::vector<std::string>>();
  auto firmware_path{arguments.contains("firmware") ? &arguments["firmware"].as<std::string>() : nullptr};
  if(parameters.size() == parameter_count + 1) {
    if(firmware_path) throw std::invalid_argument{"firmware specified twice"};
    firmware_path = &parameters.front();
    parameters = parameters.subspan(1);
  }
  if(parameters.size() != parameter_count) throw std::invalid_argument{"expected RATE LEFT_POSITION RIGHT_POSITION floor|nearest each|output"};
  auto const rate{nonnegative_integer(parameters[0], "RATE")};
  auto const left_position{nonnegative_integer(parameters[1], "LEFT_POSITION")};
  auto const right_position{nonnegative_integer(parameters[2], "RIGHT_POSITION")};
  std::string_view const rounding{parameters[3]};
  std::string_view const accumulation{parameters[4]};
#if defined(_WIN32)
  // PCM bytes include CR, LF and Ctrl-Z; the CRT must not translate them.
  if(_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1) {
    throw std::runtime_error{"cannot enable binary PCM streams"};
  }
#endif // defined(_WIN32)
  if((rounding != "floor" && rounding != "nearest") || (accumulation != "each" && accumulation != "output")) {
    throw std::invalid_argument{"an explicit supported arithmetic hypothesis is required"};
  }
  qsound::arithmetic_model const arithmetic{
    .rounding{rounding == "floor" ? qsound::rounding_model::floor : qsound::rounding_model::nearest},
    .accumulation{accumulation == "each" ? qsound::accumulation_model::saturate_each_mac : qsound::accumulation_model::saturate_output},
  };
  auto tables{[&]{
    if(!firmware_path) return qsound::make_spatial_tables(rate);
    std::ifstream file{*firmware_path, std::ios::binary};
    if(!file) throw std::runtime_error{"cannot open firmware file"};
    std::vector<uint8_t> const bytes{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    auto const firmware{qsound::decode_firmware_asset(bytes)};
    return qsound::recover_spatial_tables(qsound::select_qsound_function(firmware, rate));
  }()};
  qsound::spatial_model model{std::move(tables), arithmetic};
  model.set_positions(left_position, right_position);
  std::array<char, 4> frame{};
  while(std::cin.read(frame.data(), frame.size())) {
    auto const output{model.process({
      .left{sample(frame[0], frame[1])},
      .right{sample(frame[2], frame[3])},
    })};
    auto const left{static_cast<uint16_t>(output.left)};
    auto const right{static_cast<uint16_t>(output.right)};
    frame = {
      static_cast<char>(left & 255),
      static_cast<char>(left >> 8),
      static_cast<char>(right & 255),
      static_cast<char>(right >> 8),
    };
    std::cout.write(frame.data(), frame.size());
  }
  if(std::cin.gcount() != 0 || !std::cin.eof() || !std::cout) throw std::runtime_error{"incomplete PCM frame or output failure"};
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::println(std::cerr, "Spatial model failed: {}", error.what());
  return EXIT_FAILURE;
}
