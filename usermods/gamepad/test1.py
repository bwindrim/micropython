import gamepad
from time import sleep_ms

print("Starting Bluepad32 scan...")
gamepad.start()
print("Started:", gamepad.started())

last = None

while True:
    state = gamepad.read()
    if state != last:
        connected, buttons, dpad, axis_x, axis_y, battery = state
        print(
            "connected=%s buttons=0x%08x dpad=0x%02x "
            "x=%d y=%d battery=%d%%"
            % (connected, buttons, dpad, axis_x, axis_y, battery)
        )
        last = state
    sleep_ms(50)
    