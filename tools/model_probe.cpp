#include <array>
#include <bit>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <print>
#include <string_view>
#include <utility>
#include <vector>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif
#include "qsound/firmware.h"
#include "qsound/spatial_model.h"
#include "qsound/upgrade.h"

namespace {

auto number(std::string_view const text)->unsigned int {
  unsigned int result{};
  auto const parsed{std::from_chars(text.data(), text.data() + text.size(), result)};
  if(parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) throw std::invalid_argument{"invalid unsigned argument"};
  return result;
}

auto sample(char const low, char const high)->std::int16_t {
  auto const bits{static_cast<std::uint16_t>(static_cast<unsigned char>(low)
    | (static_cast<unsigned int>(static_cast<unsigned char>(high)) << 8))};
  return std::bit_cast<std::int16_t>(bits);
}

} // anonymous namespace

auto main(int const argc, char const *const *argv)->int try {
  if(argc != 7) {
    std::println(std::cerr, "Usage: qsound_model_probe FIRMWARE RATE LEFT_POSITION RIGHT_POSITION floor|nearest each|output");
    std::println(std::cerr, "Input/output: raw stereo signed 16-bit little endian. Analytical model; not verified DSP output.");
    return EXIT_FAILURE;
  }
#if defined(_WIN32)
  // PCM bytes include CR, LF and Ctrl-Z; the CRT must not translate them.
  if(_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1) {
    throw std::runtime_error{"cannot enable binary PCM streams"};
  }
#endif
  std::string_view const rounding{argv[5]};
  std::string_view const accumulation{argv[6]};
  if((rounding != "floor" && rounding != "nearest") || (accumulation != "each" && accumulation != "output")) {
    throw std::invalid_argument{"an explicit supported arithmetic hypothesis is required"};
  }
  qsound::arithmetic_model const arithmetic{
    .rounding{rounding == "floor" ? qsound::rounding_model::floor : qsound::rounding_model::nearest},
    .accumulation{accumulation == "each" ? qsound::accumulation_model::saturate_each_mac : qsound::accumulation_model::saturate_output},
  };
  std::ifstream file{argv[1], std::ios::binary};
  if(!file) throw std::runtime_error{"cannot open firmware file"};
  std::vector<std::uint8_t> const bytes{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
  auto const firmware{qsound::decode_firmware_asset(bytes)};
  auto tables{qsound::recover_spatial_tables(qsound::select_qsound_function(firmware, number(argv[2])))};
  qsound::spatial_model model{std::move(tables), arithmetic};
  model.set_positions(number(argv[3]), number(argv[4]));
  std::array<char, 4> frame{};
  while(std::cin.read(frame.data(), frame.size())) {
    auto const output{model.process({
      .left{sample(frame[0], frame[1])},
      .right{sample(frame[2], frame[3])},
    })};
    auto const left{static_cast<std::uint16_t>(output.left)};
    auto const right{static_cast<std::uint16_t>(output.right)};
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
