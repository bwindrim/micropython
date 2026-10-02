# Dependency patches

These patches are already applied in this workspace. They preserve the
existing dependency revisions; the newer SDK-pinned BTstack is only an isolated
baseline comparison. Each patch applies relative to the dependency root:

| Patch | Dependency root | Purpose |
| --- | --- | --- |
| `btstack-synchronous-gap-buffer.patch` | `lib/btstack` | Release the shared HCI command buffer after synchronous local-name and EIR writes. |
| `pico-sdk-static-bt-firmware-buffers.patch` | `lib/pico-sdk` | Use static firmware-download buffers when `MICROPY_GAMEPAD_BLUEPAD32` is defined. |
| `bluepad32-classic-reconnect.patch` | `usermods/gamepad/lib/bluepad32` | Use legacy PIN 0000 for the tested 8BitDo E4:17:D8 address family; preserve pairing keys after transient connection failures. |
| `bluepad32-pico-micropython-compat.patch` | `usermods/gamepad/lib/bluepad32` | Avoid newlib stdout setup/output in MicroPython and guard a HID event absent from the old BTstack. |

The Bluepad32 patch also retains startup scratch-register checkpoints; they
do not enable the watchdog. SDK architecture/transport watchdog diagnostics
were removed. `MICROPY_GAMEPAD_TRACE=ON` enables the adapter's separate HCI
trace and Bluepad32 connection logs through MicroPython's printer; the final
firmware has it off. The HCI trace excludes Link Key Request Reply payloads.

The Pico SDK 2.3.1 comparison needed additional HID API name aliases, which
are confined to `baseline/bluepad32/examples/pico_w/CMakeLists.txt` and are
not required by the existing MicroPython BTstack checkout.

Source revisions and hardware verification are recorded in `../BASELINE.md`.
