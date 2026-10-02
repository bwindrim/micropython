import time
import gamepad

gamepad.start()

while True:
    gamepad.poll()
    print(gamepad.read())
    time.sleep_ms(100)