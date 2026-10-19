# Include this file, add ${QSOUND_SOURCES} to your existing target, and add
# ${QSOUND_INCLUDE_DIR} to that target's include directories. No library target.
set(QSOUND_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
set(QSOUND_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/firmware.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/instruction.cpp"
)
