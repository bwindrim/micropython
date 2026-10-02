# `gamepad` MicroPython user C module

This Pico W user C module runs Bluepad32 on the CYW43 Bluetooth controller.
The firmware enables Classic Bluetooth and BLE while excluding Wi-Fi, lwIP,
the network backend and MicroPython's Bluetooth backend.

```python
import gamepad, time

gamepad.start()
while True:
    gamepad.poll()
    connected, buttons, dpad, axis_x, axis_y, battery = gamepad.read()
    if connected:
        print(buttons, dpad, axis_x, axis_y, battery)
    time.sleep_ms(5)
```

`dpad` is a bitmask using `DPAD_UP`, `DPAD_DOWN`, `DPAD_LEFT`, and
`DPAD_RIGHT`. `buttons` combines Bluepad32's main buttons in bits 0..15
and miscellaneous buttons in bits 16..31. Battery is a percentage; zero also
represents an unavailable battery report. `read()` returns the latest snapshot,
and `poll()` services Bluetooth in the foreground. After `start()`, the firmware
also polls from MicroPython event handling, including sleeps and an idle REPL,
with waits capped at 5 ms. Long native calls or CPU loops without event handling
still require explicit polling. All servicing runs on core 0 outside interrupts.

`info()` returns the latest device name, six address bytes, vendor/product IDs,
transport (0=unspecified by Bluepad32, 1=Classic, 2=BLE), ready flag and parsed gamepad report count.
Identity is retained after disconnection; `ready` becomes false.

`status()` reports 0 before start, 1 during HCI initialization, 2 while scanning,
3 after discovery, and 4 after controller setup is ready. Disconnect returns to
status 2 and clears the snapshot. `started()` and `connected()` return
booleans. `clear()` clears the Python snapshot; it does not disconnect Bluetooth.
`diagnose()` reads the last watchdog checkpoint, not a history of HCI events.

Build the Pico W firmware with:

```sh
cmake -S ports/rp2 -B ports/rp2/build-RPI_PICO_W \
  -DMICROPY_BOARD=RPI_PICO_W \
  -DMICROPY_GAMEPAD_BLUEPAD32=ON \
  -DUSER_C_MODULES="$PWD/usermods/gamepad/micropython.cmake" \
  -DMICROPY_GAMEPAD_TRACE=OFF
cmake --build ports/rp2/build-RPI_PICO_W -j4
```

Flash `ports/rp2/build-RPI_PICO_W/firmware.uf2`. The gamepad option must be
supplied during initial CMake configuration so SDK driver selection occurs
before MicroPython's user-module discovery. Bluetooth pairing storage uses
the last two flash sectors, outside the writable filesystem.

The existing BTstack checkout requires the buffer-release backport in
[patches/btstack-synchronous-gap-buffer.patch](patches/btstack-synchronous-gap-buffer.patch).
It is already applied in this workspace. The SDK firmware loader also uses
static scratch buffers in this integration to avoid its wrapped malloc path.
For startup evidence, source revisions and remaining diagnostics, see
[BASELINE.md](BASELINE.md). Startup and foreground polling produced controller connections and input reports.
Those captures did not include device identity, so they do not yet establish
reliable Zero 2 operation. The event-loop firmware was flashed and tested without explicit polling:
`info()` identified `8BitDo Zero 2 gamepad`, A produced mask 2 and release 0,
and left produced `axis_x=-512`. Power-cycling the Zero 2 cleared the snapshot,
reconnected the same device and restored button input. After a Pico reset,
the still-powered Zero 2 did not reconnect during the observation interval;
power-cycling it in the same mode restored connection and input. Run
[monitor.py](monitor.py) for an ongoing identity/status/input monitor.
Tests 5 and 7 only check startup; they exit upon scanning.
`network`, `bluetooth` and `socket` imports are excluded.
