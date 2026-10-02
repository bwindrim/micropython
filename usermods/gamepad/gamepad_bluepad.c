// Bluepad32 platform adapter for the MicroPython gamepad module.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "pico.h"

#include "hardware/watchdog.h"

// Provided by the linked Pico SDK CYW43 architecture target.
int cyw43_arch_init(void);
void cyw43_arch_poll(void);

#include "uni.h"

#include "gamepad_bridge.h"

#if MICROPY_GAMEPAD_TRACE
#include "hci_dump.h"
#include "py/mpprint.h"

static void gamepad_trace_packet(uint8_t type, uint8_t in, uint8_t *packet, uint16_t len) {
    if (type != HCI_COMMAND_DATA_PACKET &&
        (type != HCI_EVENT_PACKET || len == 0 ||
         (packet[0] != HCI_EVENT_COMMAND_COMPLETE && packet[0] != HCI_EVENT_COMMAND_STATUS && packet[0] != BTSTACK_EVENT_STATE))) {
        return;
    }
    mp_printf(MP_PYTHON_PRINTER, "HCI %s", in ? "<" : ">");
    for (uint16_t i = 0; i < len; i++) {
        mp_printf(MP_PYTHON_PRINTER, " %02x", packet[i]);
    }
    mp_printf(MP_PYTHON_PRINTER, "\n");
}

static void gamepad_trace_message(int level, const char *format, va_list args) {
    (void)level;
    mp_vprintf(MP_PYTHON_PRINTER, format, args);
    mp_printf(MP_PYTHON_PRINTER, "\n");
}

static const hci_dump_t gamepad_trace = {
    .log_packet = gamepad_trace_packet,
    .log_message = gamepad_trace_message,
};
#endif

static bool gamepad_bluepad_started;
static bool gamepad_bluepad_polling;
static gamepad_info_t gamepad_device_info;
static volatile uint8_t gamepad_bluepad_status;

// Bluepad32 invokes this callback unconditionally for custom platforms.
// MicroPython needs no platform-specific setup at this point.
static void gamepad_platform_init(int argc, const char **argv) {
    (void)argc;
    (void)argv;
}

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
    gamepad_bluepad_status = 2; // HCI ready; beginning discovery
    uni_bt_start_scanning_and_autoconnect_unsafe();
}

static uni_error_t gamepad_on_device_discovered(bd_addr_t addr, const char *name, uint16_t cod, uint8_t rssi) {
    (void)addr;
    (void)name;
    (void)cod;
    (void)rssi;
    gamepad_bluepad_status = 3; // acceptable controller discovered
    return UNI_ERROR_SUCCESS;
}

static void gamepad_on_device_connected(uni_hid_device_t *device) {
    (void)device;
    // A transport connection is not yet a usable gamepad. Wait for parser
    // setup to finish before reporting connected to Python.
}

static void gamepad_on_device_disconnected(uni_hid_device_t *device) {
    (void)device;
    gamepad_device_info.ready = false;
    gamepad_bridge_clear();
    gamepad_bluepad_status = 2; // discovery remains enabled after disconnect
}

static uni_error_t gamepad_on_device_ready(uni_hid_device_t *device) {
    // Some parsers set the controller class only on the first input report.
    memcpy(gamepad_device_info.name, device->name, sizeof(device->name));
    gamepad_device_info.name[sizeof(gamepad_device_info.name) - 1] = 0;
    memcpy(gamepad_device_info.address, device->conn.btaddr, 6);
    gamepad_device_info.vendor_id = device->vendor_id;
    gamepad_device_info.product_id = device->product_id;
    gamepad_device_info.transport = device->conn.protocol;
    gamepad_device_info.ready = true;
    gamepad_device_info.reports = 0;
    gamepad_bluepad_status = 4;
    gamepad_bridge_set_connected(true);
    return UNI_ERROR_SUCCESS;
}

static void gamepad_on_controller_data(uni_hid_device_t *device, uni_controller_t *controller) {
    (void)device;
    if (controller->klass != UNI_CONTROLLER_CLASS_GAMEPAD) {
        return;
    }

    gamepad_device_info.reports++;
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

// Bluepad32 calls this unconditionally for scanning changes and the system
// button. The module exposes reports directly and needs no extra action.
static void gamepad_on_oob_event(uni_platform_oob_event_t event, void *data) {
    (void)event;
    (void)data;
}

static struct uni_platform gamepad_platform = {
    .name = "MicroPython gamepad",
    .init = gamepad_platform_init,
    .on_init_complete = gamepad_on_init_complete,
    .on_device_discovered = gamepad_on_device_discovered,
    .on_device_connected = gamepad_on_device_connected,
    .on_device_disconnected = gamepad_on_device_disconnected,
    .on_device_ready = gamepad_on_device_ready,
    .on_controller_data = gamepad_on_controller_data,
    .on_oob_event = gamepad_on_oob_event,
};

int gamepad_bluepad_start(void) {
    if (gamepad_bluepad_started) {
        return 0;
    }

    watchdog_hw->scratch[0] = 0x47500011;
    // Pico SDK owns the controller firmware loader and BTstack async loop.
    int err = cyw43_arch_init();
    if (err != 0) {
        return err;
    }

#if MICROPY_GAMEPAD_TRACE
    hci_dump_init(&gamepad_trace);
#endif
    uni_platform_set_custom(&gamepad_platform);
    err = uni_init(0, NULL);
    if (err != UNI_ERROR_SUCCESS) {
        return err;
    }

    gamepad_bluepad_started = true;
    gamepad_bluepad_status = 1; // Bluepad32 initialized; awaiting HCI ready
    watchdog_hw->scratch[0] = 0x47500020; // complete through Bluepad32
    return 0;
}

uint8_t gamepad_bluepad_status_get(void) {
    return gamepad_bluepad_status;
}

void gamepad_bluepad_poll(void) {
    if (gamepad_bluepad_started && !gamepad_bluepad_polling && get_core_num() == 0 && !__get_current_exception()) {
        gamepad_bluepad_polling = true;
        // The SDK async workers service CYW43, BTstack data sources, callbacks
        // and timers. This is the polling step used by its blocking run loop.
        cyw43_arch_poll();
        gamepad_bluepad_polling = false;
    }
}

int gamepad_bluepad_wait_ms(int timeout_ms) {
    if (gamepad_bluepad_started && get_core_num() == 0 && (timeout_ms < 0 || timeout_ms > 5)) {
        return 5;
    }
    return timeout_ms;
}

void gamepad_bluepad_info_get(gamepad_info_t *info) {
    *info = gamepad_device_info;
}
