import time

print("Imported time module")

import gamepad

print("Imported gamepad module")

print("before start")
gamepad.start()
print("after start")

while True:
    print("connected =", gamepad.connected(), "report =", gamepad.read())
    time.sleep(1)
