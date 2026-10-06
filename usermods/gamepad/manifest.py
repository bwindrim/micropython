# Keep the RP2 filesystem boot scripts and PIO helpers. Do not inherit the
# Pico W board manifest, which also bundles networking and MicroPython BLE.
freeze("$(PORT_DIR)/modules", ("_boot.py", "_boot_fat.py", "rp2.py"))
include("$(MPY_DIR)/extmod/asyncio")
