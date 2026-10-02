/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef MICROPY_INCLUDED_RP2_CYW43_CONFIGPORT_H
#define MICROPY_INCLUDED_RP2_CYW43_CONFIGPORT_H

#if MICROPY_PY_NETWORK_CYW43
#include "extmod/cyw43_config_common.h"
#else
// Pico SDK's CYW43 async-context implementation provides the scheduling and
// delay hooks for the Bluetooth-only image.  Avoid MicroPython's Wi-Fi/lwIP
// configuration, which defines incompatible versions of those hooks.
#include "pico.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "py/runtime.h"

#define CYW43_NETUTILS                  (0)
#define CYW43_PRINTF(...)               (void)0
#define CYW43_EPERM                     (-PICO_ERROR_NOT_PERMITTED)
#define CYW43_EIO                       (-PICO_ERROR_IO)
#define CYW43_EINVAL                    (-PICO_ERROR_INVALID_ARG)
#define CYW43_ETIMEDOUT                 (-PICO_ERROR_TIMEOUT)
#define CYW43_HOST_NAME                 "PicoW"
#define CYW43_ARRAY_SIZE(a)             count_of(a)

typedef uint cyw43_hal_pin_obj_t;

static inline uint32_t cyw43_hal_ticks_us(void) {
    return time_us_32();
}

static inline int cyw43_hal_pin_read(cyw43_hal_pin_obj_t pin) {
    return gpio_get(pin);
}
static inline void cyw43_hal_pin_low(cyw43_hal_pin_obj_t pin) {
    gpio_put(pin, false);
}
static inline void cyw43_hal_pin_high(cyw43_hal_pin_obj_t pin) {
    gpio_put(pin, true);
}

#define CYW43_HAL_PIN_MODE_INPUT        GPIO_IN
#define CYW43_HAL_PIN_MODE_OUTPUT       GPIO_OUT
#define CYW43_HAL_PIN_PULL_NONE         0
#define CYW43_HAL_PIN_PULL_UP           1
#define CYW43_HAL_PIN_PULL_DOWN         2
static inline void cyw43_hal_pin_config(cyw43_hal_pin_obj_t pin, uint32_t mode, uint32_t pull, __unused uint32_t alt) {
    assert((mode == CYW43_HAL_PIN_MODE_INPUT || mode == CYW43_HAL_PIN_MODE_OUTPUT) && alt == 0);
    gpio_set_dir(pin, mode);
    gpio_set_pulls(pin, pull == CYW43_HAL_PIN_PULL_UP, pull == CYW43_HAL_PIN_PULL_DOWN);
}

#define CYW43_HAL_MAC_WLAN0             0
void cyw43_hal_get_mac(int idx, uint8_t buf[6]);
void cyw43_hal_generate_laa_mac(int idx, uint8_t buf[6]);

// Implemented by Pico SDK's CYW43 async-context driver.
void cyw43_thread_enter(void);
void cyw43_thread_exit(void);
#define CYW43_THREAD_ENTER cyw43_thread_enter();
#define CYW43_THREAD_EXIT cyw43_thread_exit();
#define CYW43_THREAD_LOCK_CHECK
void cyw43_await_background_or_timeout_us(uint32_t timeout_us);
#define CYW43_SDPCM_SEND_COMMON_WAIT cyw43_await_background_or_timeout_us(1000);
#define CYW43_DO_IOCTL_WAIT cyw43_await_background_or_timeout_us(1000);
void cyw43_delay_us(uint32_t us);
void cyw43_delay_ms(uint32_t ms);
void cyw43_schedule_internal_poll_dispatch(void (*func)(void));

#ifndef cyw43_malloc
#define cyw43_malloc(nmemb) m_tracked_calloc(nmemb, 1)
#endif
#ifndef cyw43_free
#define cyw43_free m_tracked_free
#endif
#endif

#define CYW43_INCLUDE_LEGACY_F1_OVERFLOW_WORKAROUND_VARIABLES (1)
#define CYW43_WIFI_NVRAM_INCLUDE_FILE   "wifi_nvram_43439.h"
#define CYW43_SLEEP_MAX                 (10) // Unclear why rp2 port overrides the default here
#define CYW43_USE_OTP_MAC               (1)

static inline bool cyw43_poll_is_pending(void) {
    #if MICROPY_PY_NETWORK_CYW43
    return pendsv_is_pending(PENDSV_DISPATCH_CYW43);
    #else
    // The Bluetooth-only image uses Pico SDK's async-context loop rather
    // than MicroPython's CYW43 PendSV dispatcher.
    return false;
    #endif
}

static inline void cyw43_yield(void) {
    if (!cyw43_poll_is_pending()) {
        best_effort_wfe_or_timeout(make_timeout_time_ms(1));
    }
}

#if MICROPY_PY_NETWORK_CYW43
#define CYW43_POST_POLL_HOOK            cyw43_post_poll_hook();
#else
#define CYW43_POST_POLL_HOOK
#endif

// set in SDK board header
#define CYW43_NUM_GPIOS                 CYW43_WL_GPIO_COUNT

#if CYW43_PIN_WL_DYNAMIC

// Dynamic pins can be changed at runtime before initialising the CYW43

typedef enum cyw43_pin_index_t {
    CYW43_PIN_INDEX_WL_REG_ON,
    CYW43_PIN_INDEX_WL_DATA_OUT,
    CYW43_PIN_INDEX_WL_DATA_IN,
    CYW43_PIN_INDEX_WL_HOST_WAKE,
    CYW43_PIN_INDEX_WL_CLOCK,
    CYW43_PIN_INDEX_WL_CS,
    CYW43_PIN_INDEX_WL_COUNT // last
} cyw43_pin_index_t;

// Function to retrieve a cyw43 dynamic pin
uint cyw43_get_pin_wl(cyw43_pin_index_t pin_id);

#define CYW43_PIN_WL_REG_ON cyw43_get_pin_wl(CYW43_PIN_INDEX_WL_REG_ON)
#define CYW43_PIN_WL_DATA_OUT cyw43_get_pin_wl(CYW43_PIN_INDEX_WL_DATA_OUT)
#define CYW43_PIN_WL_DATA_IN cyw43_get_pin_wl(CYW43_PIN_INDEX_WL_DATA_IN)
#define CYW43_PIN_WL_HOST_WAKE cyw43_get_pin_wl(CYW43_PIN_INDEX_WL_HOST_WAKE)
#define CYW43_PIN_WL_CLOCK cyw43_get_pin_wl(CYW43_PIN_INDEX_WL_CLOCK)
#define CYW43_PIN_WL_CS cyw43_get_pin_wl(CYW43_PIN_INDEX_WL_CS)

#else

#define CYW43_PIN_WL_REG_ON             CYW43_DEFAULT_PIN_WL_REG_ON
#define CYW43_PIN_WL_DATA_OUT           CYW43_DEFAULT_PIN_WL_DATA_OUT
#define CYW43_PIN_WL_DATA_IN            CYW43_DEFAULT_PIN_WL_DATA_IN
#define CYW43_PIN_WL_HOST_WAKE          CYW43_DEFAULT_PIN_WL_HOST_WAKE
#define CYW43_PIN_WL_CLOCK              CYW43_DEFAULT_PIN_WL_CLOCK
#define CYW43_PIN_WL_CS                 CYW43_DEFAULT_PIN_WL_CS

#endif

#if MICROPY_PY_NETWORK_CYW43
#define CYW43_SDPCM_SEND_COMMON_WAIT \
    if (get_core_num() == 0) { \
        cyw43_yield(); \
    } \

#define CYW43_DO_IOCTL_WAIT \
    if (get_core_num() == 0) { \
        cyw43_yield(); \
    } \

#endif

// Bluetooth requires dynamic memory allocation to load its firmware (the allocation
// call is made from pico-sdk).  This allocation is always done at thread-level, not
// from an IRQ, so is safe to delegate to the MicroPython GC heap.
#ifndef cyw43_malloc
#define cyw43_malloc(nmemb) m_tracked_calloc(nmemb, 1)
#endif
#ifndef cyw43_free
#define cyw43_free m_tracked_free
#endif

#endif // MICROPY_INCLUDED_RP2_CYW43_CONFIGPORT_H
