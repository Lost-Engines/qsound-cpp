cmake_minimum_required(VERSION 3.30)
project(qsound_cpp LANGUAGES CXX)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
include(CTest)
include(qsound/sources.cmake)

function(configure_qsound_tool target)
  target_compile_features(${target} PRIVATE cxx_std_26)
  target_include_directories(${target} PRIVATE "${QSOUND_INCLUDE_DIR}")
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
  )
endfunction()

add_executable(qsound_inspect tools/inspect.cpp ${QSOUND_SOURCES})
configure_qsound_tool(qsound_inspect)

add_executable(qsound_control_probe tools/control_probe.cpp ${QSOUND_SOURCES})
configure_qsound_tool(qsound_control_probe)

if(BUILD_TESTING)
  add_executable(qsound_control_test tests/control_test.cpp ${QSOUND_SOURCES})
  configure_qsound_tool(qsound_control_test)
  target_compile_options(qsound_control_test PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-UNDEBUG>)
  add_test(NAME control.position_sweep COMMAND qsound_control_test)
  add_executable(qsound_firmware_test tests/firmware_test.cpp ${QSOUND_SOURCES})
  configure_qsound_tool(qsound_firmware_test)
  add_test(NAME firmware.formats_and_bounds COMMAND qsound_firmware_test)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_FOUND)
    add_test(NAME firmware.fetch_decoder COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/fetch_firmware_test.py")
  endif()
endif()
