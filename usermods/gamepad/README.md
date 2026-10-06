# `gamepad` MicroPython user C module

This Pico W user C module runs Bluepad32 on the CYW43 Bluetooth controller.
The firmware enables Classic Bluetooth and BLE while excluding Wi-Fi, lwIP,
the network backend and MicroPython's Bluetooth backend. The frozen manifest
retains RP2's filesystem boot scripts and PIO helpers so the flash filesystem
mounts normally and remains accessible to Thonny and VS Code.
It also includes the standard MicroPython `asyncio` package and its `uasyncio`
alias, including tasks, events, locks and timeouts. TCP connection and server
helpers require sockets, which remain excluded from this Bluetooth-only build.

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
transport (0=unknown, 1=Classic, 2=BLE), ready flag and parsed gamepad report count.
Identity is retained after disconnection; `ready` becomes false.

`status()` reports 0 before start, 1 during HCI initialization, 2 while scanning,
3 after discovery, and 4 after controller setup is ready. Disconnect returns to
status 2 and clears the snapshot. `started()` and `connected()` return
booleans. `clear()` clears the Python snapshot; it does not disconnect Bluetooth.
`diagnose()` reads the last watchdog checkpoint, not a history of HCI events.

Prepare dependencies from a fresh checkout, then build the Pico W firmware:

```sh
git clone --branch GamePad https://github.com/bwindrim/micropython.git
cd micropython
python3 usermods/gamepad/setup_dependencies.py
```

The setup script initialises only the dependencies used by this build, including
the pinned Bluepad32 submodule, and applies the tracked compatibility patches.
It is safe to rerun. `python3 usermods/gamepad/setup_dependencies.py --check`
verifies the revisions and patches without fetching or changing files. Existing
revision mismatches or conflicting edits stop setup rather than discarding work.
Patched submodules will intentionally appear dirty; their changes are preserved
as patch files in this parent repository. Do not commit those changes locally
inside a submodule and update its pointer unless that commit is published in a
repository available to other users.

Build with:

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

Back up existing Pico files before first flashing this configuration. Bluetooth
storage reduces the filesystem from 848 KiB (212 blocks) to 840 KiB (210 blocks).
An existing filesystem with the old geometry cannot mount; the RP2 boot script
formats a new filesystem, after which the backed-up files must be restored.

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
reconnected the same device and restored button input. The current firmware remembers the last working Classic device's address and
name in Bluetooth flash storage and retries it every five seconds when no
controller is ready. Bluepad32 still handles connection timeout/cleanup, so a
failed attempt can take up to 20 seconds before another attempt gets a slot.
This applies to Classic Bluetooth; BLE continues using normal Bluepad32 scanning.
The tested 8BitDo E4:17:D8 address family uses PIN 0000 for legacy incoming
pairing, and transient connection failures preserve saved link keys. Run
[monitor.py](monitor.py) for an ongoing identity/status/input monitor.
Tests 5 and 7 only check startup; they exit upon scanning.
`network`, `bluetooth` and `socket` imports are excluded.

Automatic recovery after an abrupt Pico reset is not established. A controlled
test started with an identified, reporting Zero 2, reset the Pico immediately,
and observed five failed paging attempts over 90 seconds (HCI error 0x04).
Power-cycling the Zero 2 still restores connection. `info()` also exposes
`saved_peer`, `reconnect_attempts`, `reconnect_state` and `connection_error`
(last nonzero connection/authentication status) for diagnosing retries.

A shorter link-supervision timeout and a clean disconnect before software reset
were tested and removed because neither restored recovery. The clean-disconnect
test confirmed the controller turned off; a powered-off Zero 2 cannot be
reconnected remotely. The firmware does not intercept `machine.reset()`.
