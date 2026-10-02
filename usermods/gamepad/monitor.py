"""Show Bluetooth state, device identity and button changes until Ctrl-C."""
import gamepad
import time

gamepad.start()
last = None
last_identity = None
while True:
    gamepad.poll()
    info = gamepad.info()
    identity = (info["name"], info["address"], info["vendor_id"],
                info["product_id"], info["transport"], info["ready"])
    if identity != last_identity:
        print("DEVICE", info)
        last_identity = identity
    current = (gamepad.status(), gamepad.read())
    if current != last:
        print("STATE", current, "reports", info["reports"])
        last = current
    time.sleep_ms(5)
