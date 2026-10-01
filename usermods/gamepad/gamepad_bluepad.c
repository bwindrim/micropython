// Bluepad32 platform adapter for the MicroPython gamepad module.

#include <stdbool.h>
#include <stdint.h>

#include "extmod/modbluetooth.h"
#include "uni.h"

#include "gamepad_bridge.h"

static bool gamepad_bluepad_started;

static int16_t gamepad_axis_to_i16(int32_t axis) {
    if (axis > INT16_MAX) {
        return INT16_MAX;
    }
    if (axis < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)axis;
}

static uint8_t gamepad_dpad_from_bluepad(uint8_t dpad) {
    uint8_t result = 0;
    if (dpad & DPAD_UP) {
        result |= GAMEPAD_DPAD_UP;
    }
    if (dpad & DPAD_DOWN) {
        result |= GAMEPAD_DPAD_DOWN;
    }
    if (dpad & DPAD_LEFT) {
        result |= GAMEPAD_DPAD_LEFT;
    }
    if (dpad & DPAD_RIGHT) {
        result |= GAMEPAD_DPAD_RIGHT;
    }
    return result;
}

static uint8_t gamepad_battery_percent(uint8_t battery) {
    if (battery == UNI_CONTROLLER_BATTERY_NOT_AVAILABLE) {
        return 0;
    }
    return (uint8_t)(((uint16_t)battery * 100 + 127) / 255);
}

static void gamepad_on_init_complete(void) {
    // This callback runs in BTstack's event-loop context, so calling the
    // Bluepad32 "unsafe" scanning helper is correct here.
    uni_bt_start_scanning_and_autoconnect_unsafe();
}

static void gamepad_on_device_connected(uni_hid_device_t *device) {
    (void)device;
    gamepad_bridge_set_connected(true);
}

static void gamepad_on_device_disconnected(uni_hid_device_t *device) {
    (void)device;
    gamepad_bridge_clear();
}

static uni_error_t gamepad_on_device_ready(uni_hid_device_t *device) {
    (void)device;
    return UNI_ERROR_SUCCESS;
}

static void gamepad_on_controller_data(uni_hid_device_t *device, uni_controller_t *controller) {
    (void)device;
    if (controller->klass != UNI_CONTROLLER_CLASS_GAMEPAD) {
        return;
    }

    const uni_gamepad_t *pad = &controller->gamepad;
    // Main and miscellaneous buttons occupy separate Bluepad32 fields.
    const uint32_t buttons = (uint32_t)pad->buttons | ((uint32_t)pad->misc_buttons << 16);
    gamepad_bridge_update(
        buttons,
        gamepad_dpad_from_bluepad(pad->dpad),
        gamepad_axis_to_i16(pad->axis_x),
        gamepad_axis_to_i16(pad->axis_y),
        gamepad_battery_percent(controller->battery));
}

static struct uni_platform gamepad_platform = {
    .name = "MicroPython gamepad",
    .on_init_complete = gamepad_on_init_complete,
    .on_device_connected = gamepad_on_device_connected,
    .on_device_disconnected = gamepad_on_device_disconnected,
    .on_device_ready = gamepad_on_device_ready,
    .on_controller_data = gamepad_on_controller_data,
};

int gamepad_bluepad_start(void) {
    if (gamepad_bluepad_started) {
        return 0;
    }

    // Reuse MicroPython's CYW43 transport and scheduler-backed BTstack run
    // loop.  It must be initialised before Bluepad32 registers Classic HID
    // host services and powers on the controller.
    int err = mp_bluetooth_init();
    if (err != 0) {
        return -err;
    }

    uni_platform_set_custom(&gamepad_platform);
    err = uni_init(0, NULL);
    if (err != UNI_ERROR_SUCCESS) {
        return err;
    }

    gamepad_bluepad_started = true;
    return 0;
}
