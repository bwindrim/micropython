import gamepad
import time

gamepad.start()

last = None
while True:
    gamepad.poll()
    state = gamepad.read()
    if state != last:
        print(state)
        last = state
    time.sleep_ms(5)