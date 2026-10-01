#ifndef MICROPY_INCLUDED_USERMOD_GAMEPAD_BRIDGE_H
#define MICROPY_INCLUDED_USERMOD_GAMEPAD_BRIDGE_H

// C-side bridge used by the Bluetooth transport. These functions are safe to
// call from the Bluepad32/BTstack event-loop context; Python reads a snapshot
// through gamepad.read().

#include <stdbool.h>
#include <stdint.h>

enum {
    GAMEPAD_DPAD_UP = 1 << 0,
    GAMEPAD_DPAD_DOWN = 1 << 1,
    GAMEPAD_DPAD_LEFT = 1 << 2,
    GAMEPAD_DPAD_RIGHT = 1 << 3,
};

void gamepad_bridge_set_connected(bool connected);
void gamepad_bridge_update(uint32_t buttons, uint8_t dpad, int16_t axis_x,
    int16_t axis_y, uint8_t battery_percent);
void gamepad_bridge_clear(void);

#endif // MICROPY_INCLUDED_USERMOD_GAMEPAD_BRIDGE_H
