# `gamepad` MicroPython user C module

This module defines the Python-facing gamepad API and a fixed-size state bridge
for the later Bluepad32 integration. It does **not** initialize Bluetooth yet.

```python
import gamepad

gamepad.start()
connected, buttons, dpad, axis_x, axis_y, battery = gamepad.read()
```

`dpad` is a bitmask using `DPAD_UP`, `DPAD_DOWN`, `DPAD_LEFT`, and
`DPAD_RIGHT`. `buttons` is a transport-defined 32-bit bitmask; the Bluepad32
mapping is added in the next integration step.

Build the Pico W firmware with:

```sh
cd ports/rp2
make BOARD=RPI_PICO_W clean
make BOARD=RPI_PICO_W USER_C_MODULES=../../usermods/gamepad/micropython.cmake
```

Before Bluepad32 is wired in, `gamepad.read()` returns a disconnected, zeroed
state.
