# Bluepad32 is kept as a source dependency so that its controller parsers are
# compiled for the same Pico SDK and BTstack instance as MicroPython.
set(GAMEPAD_BLUEPAD32_ROOT "${CMAKE_CURRENT_LIST_DIR}/lib/bluepad32")

if(NOT EXISTS "${GAMEPAD_BLUEPAD32_ROOT}/src/components/bluepad32/CMakeLists.txt")
    message(FATAL_ERROR "Bluepad32 source dependency is missing; initialise usermods/gamepad/lib/bluepad32")
endif()

# Do not add Bluepad32's CMake directory as a subdirectory: MicroPython's user
# module discovery would then recurse into all Pico SDK interface libraries.
# Instead compile its Pico W source set directly into the user-module target.
set(GAMEPAD_BLUEPAD32_SRC "${GAMEPAD_BLUEPAD32_ROOT}/src/components/bluepad32")
set(GAMEPAD_BLUEPAD32_SOURCES
    bt/uni_bt.c bt/uni_bt_allowlist.c bt/uni_bt_bredr.c bt/uni_bt_conn.c
    bt/uni_bt_hci_cmd.c bt/uni_bt_le.c bt/uni_bt_sdp.c bt/uni_bt_service.c
    bt/uni_bt_setup.c
    controller/uni_balance_board.c controller/uni_controller.c
    controller/uni_controller_type.c controller/uni_gamepad.c
    controller/uni_keyboard.c controller/uni_mouse.c
    parser/uni_hid_parser.c parser/uni_hid_parser_8bitdo.c
    parser/uni_hid_parser_android.c parser/uni_hid_parser_atari.c
    parser/uni_hid_parser_ds3.c parser/uni_hid_parser_ds4.c
    parser/uni_hid_parser_ds5.c parser/uni_hid_parser_generic.c
    parser/uni_hid_parser_icade.c parser/uni_hid_parser_keyboard.c
    parser/uni_hid_parser_mouse.c parser/uni_hid_parser_nimbus.c
    parser/uni_hid_parser_ouya.c parser/uni_hid_parser_psmove.c
    parser/uni_hid_parser_smarttvremote.c parser/uni_hid_parser_stadia.c
    parser/uni_hid_parser_steam.c parser/uni_hid_parser_switch.c
    parser/uni_hid_parser_wii.c parser/uni_hid_parser_xboxone.c
    platform/uni_platform.c arch/uni_console_pico.c arch/uni_system_pico.c
    arch/uni_log_pico.c arch/uni_property_pico.c uni_circular_buffer.c
    uni_hid_device.c uni_init.c uni_joystick.c uni_log.c uni_property.c
    uni_utils.c uni_version.c uni_virtual_device.c
)
list(TRANSFORM GAMEPAD_BLUEPAD32_SOURCES PREPEND "${GAMEPAD_BLUEPAD32_SRC}/")

add_library(usermod_gamepad INTERFACE)

target_sources(usermod_gamepad INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modgamepad.c
    ${GAMEPAD_BLUEPAD32_SOURCES}
)

# sdkconfig.h supplies Bluepad32's compile-time platform selection. The
# btstack include directory is already supplied by MicroPython's RP2 port.
target_include_directories(usermod_gamepad INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    "${GAMEPAD_BLUEPAD32_ROOT}/src/components/bluepad32/include"
)

target_include_directories(usermod_gamepad INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    "${GAMEPAD_BLUEPAD32_SRC}/include"
)

# QSTR preprocessing is driven from MicroPython's `usermod` target rather
# than from `usermod_gamepad`; expose the embedded BTstack headers there too.
# Bluepad32's Pico property backend uses the flash-bank TLV implementation
# that is supplied by the RP2 port's existing BTstack build.
target_include_directories(usermod INTERFACE
    "${CMAKE_SOURCE_DIR}/../../lib/btstack/platform/embedded"
)

# The RP2 port consumes this flag after it creates the firmware target and then
# links the Pico SDK's Classic BTstack library. Linking it here would make the
# user-module source collector walk Pico SDK interface libraries as user code.
set(MICROPY_GAMEPAD_BLUEPAD32 ON)

target_link_libraries(usermod INTERFACE usermod_gamepad)
