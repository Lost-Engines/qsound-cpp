cmake_minimum_required(VERSION 3.30)
project(qsound_cpp LANGUAGES CXX)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
include(CTest)
include(qsound/sources.cmake)

function(configure_qsound_tool target)
  target_compile_features(${target} PRIVATE cxx_std_26)
  target_include_directories(${target} PRIVATE "${QSOUND_INCLUDE_DIR}")
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # MinGW's <print> terminal support is supplied by libstdc++exp.
    target_link_libraries(${target} PRIVATE stdc++exp)
  endif()
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
  )
endfunction()

add_executable(qsound_inspect tools/inspect.cpp ${QSOUND_SOURCES} ${QSOUND_RESEARCH_SOURCES})
configure_qsound_tool(qsound_inspect)

add_executable(qsound_model_probe tools/model_probe.cpp ${QSOUND_SOURCES} ${QSOUND_RESEARCH_SOURCES})
configure_qsound_tool(qsound_model_probe)

add_executable(qsound_tables tools/tables.cpp ${QSOUND_SOURCES} ${QSOUND_RESEARCH_SOURCES})
configure_qsound_tool(qsound_tables)

add_executable(qsound_control_probe tools/control_probe.cpp ${QSOUND_SOURCES})
configure_qsound_tool(qsound_control_probe)

if(BUILD_TESTING)
  function(configure_qsound_test target)
    configure_qsound_tool(${target})
    target_compile_options(${target} PRIVATE
      $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-UNDEBUG>
      $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    )
  endfunction()

  add_executable(qsound_upgrade_test tests/upgrade_test.cpp ${QSOUND_SOURCES} ${QSOUND_RESEARCH_SOURCES})
  configure_qsound_test(qsound_upgrade_test)
  add_test(NAME firmware.upgrade_dcl COMMAND qsound_upgrade_test)
  add_executable(qsound_model_test tests/model_test.cpp ${QSOUND_SOURCES})
  configure_qsound_test(qsound_model_test)
  add_test(NAME model.arithmetic_and_history COMMAND qsound_model_test)
  add_executable(qsound_control_test tests/control_test.cpp ${QSOUND_SOURCES})
  configure_qsound_test(qsound_control_test)
  add_test(NAME control.position_sweep COMMAND qsound_control_test)
  add_executable(qsound_firmware_test tests/firmware_test.cpp ${QSOUND_SOURCES} ${QSOUND_RESEARCH_SOURCES})
  configure_qsound_test(qsound_firmware_test)
  add_test(NAME firmware.formats_and_bounds COMMAND qsound_firmware_test)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_FOUND)
    add_test(NAME model.reference_equations COMMAND "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_spatial_model.py"
      --probe "$<TARGET_FILE:qsound_model_probe>" --tables "$<TARGET_FILE:qsound_tables>"
    )
    add_test(NAME firmware.fetch_decoder COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/fetch_firmware_test.py")
  endif()
endif()
