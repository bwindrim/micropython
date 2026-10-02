import gamepad, time
gamepad.start()

while gamepad.status() < 2:
    gamepad.poll()
    time.sleep_ms(5)

print("status:", gamepad.status())
