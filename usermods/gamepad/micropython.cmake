# Bluepad32/BTstack is added in the next integration step. This module keeps
# the MicroPython-facing API and the C-to-Python state bridge transport-neutral.
add_library(usermod_gamepad INTERFACE)

target_sources(usermod_gamepad INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modgamepad.c
)

target_include_directories(usermod_gamepad INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_gamepad)
