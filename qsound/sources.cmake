# Compile the runtime sources directly into your application's existing target.
set(QSOUND_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
set(QSOUND_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/default_tables.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/position_sweep.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/spatial_model.cpp"
)

# Optional firmware extraction and instruction inspection for development.
set(QSOUND_RESEARCH_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/firmware.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/upgrade.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/instruction.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/spatial_tables.cpp"
)
