// Bluepad32's Pico W platform configuration. This is intentionally minimal:
// MicroPython provides the main application and Bluepad32 owns the controller
// host logic added in the following integration step.
#ifndef MICROPY_GAMEPAD_SDKCONFIG_H
#define MICROPY_GAMEPAD_SDKCONFIG_H

#define CONFIG_BLUEPAD32_MAX_DEVICES 1
#define CONFIG_BLUEPAD32_MAX_ALLOWLIST 1
#define CONFIG_BLUEPAD32_GAP_SECURITY 1
#define CONFIG_BLUEPAD32_ENABLE_BLE_BY_DEFAULT 1
#define CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#define CONFIG_TARGET_PICO_W
#if MICROPY_GAMEPAD_TRACE
#define CONFIG_BLUEPAD32_LOG_LEVEL 2
#else
#define CONFIG_BLUEPAD32_LOG_LEVEL 1
#endif

#endif // MICROPY_GAMEPAD_SDKCONFIG_H
