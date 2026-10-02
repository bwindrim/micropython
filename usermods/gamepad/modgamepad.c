// MicroPython-facing state bridge for the Bluepad32 controller host.

#include <stdbool.h>
#include <stdint.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "hardware/watchdog.h"

#include "gamepad_bridge.h"

int gamepad_bluepad_start(void);
void gamepad_bluepad_poll(void);
uint8_t gamepad_bluepad_status_get(void);

typedef struct {
    volatile bool connected;
    volatile uint32_t buttons;
    volatile uint8_t dpad;
    volatile int16_t axis_x;
    volatile int16_t axis_y;
    volatile uint8_t battery_percent;
    volatile bool start_requested;
} gamepad_state_t;

static gamepad_state_t gamepad_state;

void gamepad_bridge_set_connected(bool connected) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    gamepad_state.connected = connected;
    MICROPY_END_ATOMIC_SECTION(atomic_state);
}

void gamepad_bridge_update(uint32_t buttons, uint8_t dpad, int16_t axis_x,
    int16_t axis_y, uint8_t battery_percent) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    gamepad_state.buttons = buttons;
    gamepad_state.dpad = dpad;
    gamepad_state.axis_x = axis_x;
    gamepad_state.axis_y = axis_y;
    gamepad_state.battery_percent = battery_percent;
    MICROPY_END_ATOMIC_SECTION(atomic_state);
}

void gamepad_bridge_clear(void) {
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    gamepad_state.connected = false;
    gamepad_state.buttons = 0;
    gamepad_state.dpad = 0;
    gamepad_state.axis_x = 0;
    gamepad_state.axis_y = 0;
    gamepad_state.battery_percent = 0;
    MICROPY_END_ATOMIC_SECTION(atomic_state);
}

static mp_obj_t gamepad_start(void) {
    if (!gamepad_state.start_requested) {
        // Retained across a watchdog reset: proves that the Python entry
        // point itself was reached before handing off to the C host.
        watchdog_hw->scratch[0] = 0x47500010;
        int err = gamepad_bluepad_start();
        if (err != 0) {
            mp_raise_OSError(err);
        }
        gamepad_state.start_requested = true;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(gamepad_start_obj, gamepad_start);

// The Pico SDK's single-core CYW43 architecture is deliberately polled from
// normal MicroPython execution.  This avoids sharing a background IRQ with
// MicroPython's USB/event machinery.
static mp_obj_t gamepad_poll(void) {
    gamepad_bluepad_poll();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(gamepad_poll_obj, gamepad_poll);

static mp_obj_t gamepad_started(void) {
    return mp_obj_new_bool(gamepad_state.start_requested);
}
static MP_DEFINE_CONST_FUN_OBJ_0(gamepad_started_obj, gamepad_started);

// 1=Bluepad32 initialized, 2=scanning, 3=controller discovered, 4=connected.
static mp_obj_t gamepad_status(void) {
    return mp_obj_new_int_from_uint(gamepad_bluepad_status_get());
}
static MP_DEFINE_CONST_FUN_OBJ_0(gamepad_status_obj, gamepad_status);

// Returns the last CYW43 startup checkpoint, retained by the watchdog across
// a reset.  This is intentionally a low-level diagnostic API.
static mp_obj_t gamepad_diagnose(void) {
    return mp_obj_new_int_from_uint(watchdog_hw->scratch[0]);
}
static MP_DEFINE_CONST_FUN_OBJ_0(gamepad_diagnose_obj, gamepad_diagnose);

static mp_obj_t gamepad_connected(void) {
    return mp_obj_new_bool(gamepad_state.connected);
}
static MP_DEFINE_CONST_FUN_OBJ_0(gamepad_connected_obj, gamepad_connected);

// Return (connected, buttons, dpad, axis_x, axis_y, battery_percent).
// D-pad bits use DPAD_UP, DPAD_DOWN, DPAD_LEFT, and DPAD_RIGHT.
static mp_obj_t gamepad_read(void) {
    bool connected;
    uint32_t buttons;
    uint8_t dpad;
    int16_t axis_x;
    int16_t axis_y;
    uint8_t battery_percent;

    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    connected = gamepad_state.connected;
    buttons = gamepad_state.buttons;
    dpad = gamepad_state.dpad;
    axis_x = gamepad_state.axis_x;
    axis_y = gamepad_state.axis_y;
    battery_percent = gamepad_state.battery_percent;
    MICROPY_END_ATOMIC_SECTION(atomic_state);

    mp_obj_t values[6] = {
        mp_obj_new_bool(connected),
        mp_obj_new_int_from_uint(buttons),
        mp_obj_new_int_from_uint(dpad),
        mp_obj_new_int(axis_x),
        mp_obj_new_int(axis_y),
        mp_obj_new_int_from_uint(battery_percent),
    };
    return mp_obj_new_tuple(MP_ARRAY_SIZE(values), values);
}
static MP_DEFINE_CONST_FUN_OBJ_0(gamepad_read_obj, gamepad_read);

static mp_obj_t gamepad_clear(void) {
    gamepad_bridge_clear();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(gamepad_clear_obj, gamepad_clear);

static const mp_rom_map_elem_t gamepad_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_gamepad) },
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&gamepad_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_poll), MP_ROM_PTR(&gamepad_poll_obj) },
    { MP_ROM_QSTR(MP_QSTR_started), MP_ROM_PTR(&gamepad_started_obj) },
    { MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&gamepad_status_obj) },
    { MP_ROM_QSTR(MP_QSTR_diagnose), MP_ROM_PTR(&gamepad_diagnose_obj) },
    { MP_ROM_QSTR(MP_QSTR_connected), MP_ROM_PTR(&gamepad_connected_obj) },
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&gamepad_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear), MP_ROM_PTR(&gamepad_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_DPAD_UP), MP_ROM_INT(GAMEPAD_DPAD_UP) },
    { MP_ROM_QSTR(MP_QSTR_DPAD_DOWN), MP_ROM_INT(GAMEPAD_DPAD_DOWN) },
    { MP_ROM_QSTR(MP_QSTR_DPAD_LEFT), MP_ROM_INT(GAMEPAD_DPAD_LEFT) },
    { MP_ROM_QSTR(MP_QSTR_DPAD_RIGHT), MP_ROM_INT(GAMEPAD_DPAD_RIGHT) },
};
static MP_DEFINE_CONST_DICT(gamepad_module_globals, gamepad_module_globals_table);

const mp_obj_module_t gamepad_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&gamepad_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_gamepad, gamepad_user_cmodule);
