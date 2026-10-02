import gamepad, time

gamepad.start()

last = -1
for _ in range(2000):
    gamepad.poll()
    s = gamepad.status()
    if s != last:
        print("status:", s, "diag:", hex(gamepad.diagnose()))
        last = s
    time.sleep_ms(5)

print("final:", gamepad.status(), gamepad.read())
