import gamepad, time

gamepad.start()
for _ in range(2000):
    gamepad.poll()
    if gamepad.status() >= 2:
        break
    time.sleep_ms(5)

print(gamepad.status(), hex(gamepad.diagnose()))