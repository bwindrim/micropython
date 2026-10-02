# Pico W startup baseline and integration review

The isolated build lives in `baseline/build`. The existing MicroPython UF2 and
all existing integration changes are preserved.

## Sources

- Pico SDK: `079c6f39023649b154152db30f1d781e884879bc` (2.3.1), exported
  from the SDK used by MicroPython, without local diagnostic edits.
- CYW43 driver: `055d64274b014dd7b1c2fc94d26e8a18face7124`, exported without edits.
- Bluepad32: `e9b755faabc240585da42e6d26164bb2cdd064d3`, exported without edits,
  except the compatibility guard below.
- BTstack and TinyUSB: the same `lib/btstack` and `lib/tinyusb` checkouts
  used by MicroPython, linked into the isolated SDK. These are not necessarily
  the revisions pinned by the SDK's submodule metadata.

The unmodified Bluepad32 source fails to compile because this BTstack lacks
`GATTSERVICE_SUBEVENT_HID_REPORT_WRITTEN`. The isolated copy wraps only that
case in `#ifdef GATTSERVICE_SUBEVENT_HID_REPORT_WRITTEN`, matching the existing
integration's compatibility fix. The example's CMake, main, platform callbacks,
logging, firmware allocation and startup sequence remain unchanged.

The example uses `pico_cyw43_arch_none`, which selects the SDK's threadsafe
background async context, rather than MicroPython's polling context. It links
Classic and BLE, with CYW43_LWIP=0. The presence of lwIP sources for SDK target
discovery does not mean lwIP is linked into the firmware.

## Build

From the repository root, after exporting the sources above:

```sh
cmake -S usermods/gamepad/baseline/bluepad32/examples/pico_w \
  -B usermods/gamepad/baseline/build \
  -DPICO_SDK_PATH="$PWD/usermods/gamepad/baseline/pico-sdk" \
  -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -Dpicotool_DIR="$PWD/ports/rp2/build-RPI_PICO_W/_deps/picotool"
cmake --build usermods/gamepad/baseline/build -j4
```

Artifact: `baseline/build/bluepad32_picow_example_app.uf2`.
USB CDC is enabled; UART stdio is disabled. Enter BOOTSEL, copy the UF2, then
capture USB serial at 115200. Look for `my_platform: on_init_complete()` and
scanning output. The example deletes stored Bluetooth keys when init completes.

## Review findings requiring follow-up

1. `0x47500061` is a last-read checkpoint, not a count of received packets.
   The transport drains packets in a loop, and its final empty read overwrites
   `0x47500062`. It also conflates a read error with an empty successful read.
   Add persistent RX/command counters and an error value before concluding
   that no HCI events arrive.
2. Both modified SDK architecture initializers enable an eight-second watchdog
   unconditionally. A standalone example using those edited sources would reset
   unless something disables or feeds it. The isolated baseline avoids this.
3. Reducing `MICROPY_HW_FLASH_STORAGE_BYTES` alone does not reserve the final
   sectors: `rp2_flash.c` defaults the base to flash size minus filesystem size.
   The filesystem still reaches the end of flash, overlapping BTstack TLV.
   Fix both the filesystem address and linker reservation consistently before
   using pairing storage. Existing filesystem data needs migration consideration.
4. Disconnect originally cleared the Python state but left status at 4; the
   adapter now returns to status 2 on disconnect. Status 4 and connected=true
   are now set by `on_device_ready` after parser setup. Only gamepad reports
   update the snapshot. The firmware currently allows one device.
5. `btstack_run_loop_execute()` polls the async context and waits for work.
   `cyw43_arch_poll()` polls that same context in a polling architecture; its
   workers already dispatch data sources, callbacks and timers. Missing the
   blocking execute call alone does not establish a need for a separate core.
   The direct `cyw43_poll()` call bypasses the normal worker scheduling path.

## Hardware test status

The build passed after the single HID compatibility guard. UF2 SHA-256:
`a1acd43450a8a334d6ed7d8ba219dff4c4797f386c2c9a70505ec2d33334b9f6`.
The firmware was copied to the Pico W's BOOTSEL drive and USB serial captured
for 30 seconds in `baseline/serial-baseline.log`. It prints Bluepad32 4.2.0,
BTstack 1.6.1, platform initialization, Classic/BLE enabled, and Bluetooth
allowlist disabled. It never prints `my_platform: on_init_complete()` or
scanning output during that capture. The board is left running this baseline.

This reproduces failure to complete startup without MicroPython, using the
normal SDK background context and `btstack_run_loop_execute()`. It does not
prove zero HCI packets were received: this build has no HCI packet trace.
The subsequent HCI trace investigation below identifies the cause of this
baseline failure without changing the polling architecture or pairing modes.

## HCI trace and confirmed BTstack fix

The current BTstack checkout is from June 2023. SDK 2.3.1 pins
`eb0bb8b5ea6d234ccb940313b47f7a5c3b4e20ec` from August 2026. An isolated
SDK-pinned build required aliases for Bluepad32's `hids_client_*` API names,
which BTstack renamed to `hids_host_*`. It reached HCI working and scanned.

`baseline/serial-trace.log` records the original BTstack sending and receiving
many HCI commands successfully, then stopping after Write Local Name (0x0c13)
completes. `gap_run_set_local_name()` and `gap_run_set_eir_data()` reserve the
shared command buffer and bypass `hci_send_cmd_va_arg()`'s synchronous-buffer
release. The small backport in `patches/btstack-synchronous-gap-buffer.patch`
adds that release using the existing stack's send semantics. It is applied to
`lib/btstack/src/hci.c`; no dependency revision was changed.

The backported baseline reaches `HCI_STATE_WORKING` and
`my_platform: on_init_complete()`, then sends Classic periodic inquiry and
BLE scan-enable commands. Evidence is in `baseline/serial-backport.log`.
The SDK-pinned comparison trace is `baseline/serial-sdk-pinned.log`.

The MicroPython build now reserves the same final 8192 bytes in both its linker
layout and runtime filesystem address calculation. FLASH_FS is
0x1012c000..0x101fe000, and BTstack TLV starts at 0x101fe000. The filesystem
start matches the stock Pico W layout. Data written by the earlier shifted
filesystem configuration is not automatically migrated.

The first MicroPython trace confirms HCI working but halts during Bluepad32
setup. Review identified another required custom-platform callback:
`on_oob_event` is called without a null check when scanning starts (and when a
system-button event occurs). The adapter now supplies a no-op implementation.
The trace before this fix is
`baseline/serial-micropython-before-oob-fix.log`. With the callback supplied,
MicroPython reaches status 2 and sends Classic inquiry and BLE scan-enable
commands. Foreground polling completes normally.

The callback fix and disconnect-status correction both build successfully.
The current `ports/rp2/build-RPI_PICO_W/firmware.uf2` includes them, the BTstack
backport, the corrected flash layout and HCI tracing disabled. This quiet
firmware was flashed and tested for 30 seconds: start returned, status changed
from 1 to 2, and polling completed at the raw REPL without a reset or hang.
`baseline/serial-micropython.log` records this final test. After excluding the
network module and cleanly rebuilding, the flashed firmware progresses through
statuses 1, 2, 3 and 4, with `read()[0]` true. A subsequent one-minute report
capture records 79 state changes, including button masks 1, 2, 4 and 8 and
directional axes -512, 0 and 511. See `baseline/serial-reports.log`.
In the tested Zero 2 mode the directional controls are reported as axes and
the D-pad mask stays zero. The battery report is unavailable (returned as 0).
An additional on-board API check confirms repeated `start()` preserves state,
`started()` is true, `read()` has the six documented fields, and D-pad constants
are (1, 2, 4, 8). It also found the board header still enabled the `network`
module shell despite the disabled Wi-Fi backend. The board header now sets
`MICROPY_PY_NETWORK=0` for the gamepad configuration. A clean rebuild removes
stale generated module-table entries. The final on-board check confirms
`network`, `bluetooth` and `socket` imports all raise ImportError, and the other
API checks still pass at status 4. Evidence is in `baseline/serial-api-check.log`.
The ELF also contains no network/socket/Bluetooth module or lwIP symbols.
Final UF2 SHA-256:
`cb72b7a2998ec13e3d5a2a1792a7ed114a9cf4f17349029ce847cebb60ddf0f7`.

A full Pico `machine.reset()` clears the C module state and startup reaches
status 2 again. The still-powered controller did not automatically reconnect
within that first 30-second capture (`baseline/serial-restart.log`). After the
user power-cycled the Zero 2 in the same mode without deleting pairing, the
module progresses through 2, 3 and 4 and `read()[0]` becomes true again. This
reconnection capture completes normally in `baseline/serial-reconnect.log`.
Both Classic and BLE scanning have been verified at HCI level, but the input
and reconnection tests cover the user's existing controller mode, not a second
controller mode or transport. The board is left running the quiet firmware at
the responsive REPL with the controller connected. Python must continue to
call `gamepad.poll()` to service new Bluetooth activity.

Set `MICROPY_GAMEPAD_TRACE=ON` in CMake to route command/command-response HCI
tracing through MicroPython's printer. This opt-in diagnostic is disabled
when that CMake option is off. The SDK architecture, transport and CYW43 driver
watchdog diagnostics were removed, leaving only static firmware scratch
buffers as an SDK patch. `gamepad.start()` no longer enables or disables the
hardware watchdog. Its `diagnose()` checkpoint ends at 0x47500020 after start.
The adapter now polls only `cyw43_arch_poll()`; direct driver polling and
forced BTstack data-source polling were removed and the quiet build retested.
The earlier SDK diagnostic diffs are preserved in `baseline/` for reference.

Three small dependency patch files are saved under `patches/`: the BTstack
buffer release, the SDK static firmware buffers, and Bluepad32's MicroPython
stdout/init and missing-HID-event compatibility edits. Apply each in its
respective dependency directory when recreating this workspace.

## Follow-up: input and reconnection remain unverified

The user subsequently reported no button input in any supplied test and an
unconnected flashing Zero 2 after restarting. Previous input captures lacked
device identity, so they cannot conclusively identify the Zero 2. A ready
callback and cached connected flag are insufficient evidence of a live link.

Tests 1 and 2 omitted polling, and tests 5 and 7 exited upon reaching scanning.
The adapter now services the polling async context through MicroPython's normal
event hook and limits event waits to 5 ms after start. Polling is guarded against
reentry, interrupts and core 1. It remains available explicitly for application
loops. Tests 1 and 2 now also explicitly poll. `monitor.py` prints identity,
status, state changes and parsed report counts. `gamepad.info()` retains device
identity after disconnect and marks `ready` false.

The event-hook firmware builds successfully; USB flashing and live identity,
input and reconnection verification are pending. CPU loops and long native
operations that do not handle MicroPython events still require explicit polling.

The event-hook UF2 (`fd27ca0bd3b9c0f5f51df0d58804e8872253b69fa1cd8fefdfbca4d0829e3f27`)
was flashed. `baseline/serial-event-reports.log` identifies
`8BitDo Zero 2 gamepad`, address E4:17:D8:96:00:FF, VID/PID 045e:02e0.
Without explicit polling, A produced mask 2 and release 0; left produced
axis_x=-512. The device power cycle produced status 2/ready false, then status
4/ready true with the same identity and new A reports. Bluepad32's Classic path
leaves conn.protocol unspecified (0); the diagnostic preserves that value.

`baseline/serial-event-restart.log` records a Pico reset and successful startup.
The still-powered controller remained unconnected until power-cycled; then
it reconnected and A produced mask 2. Automatic recovery with the controller
continuously powered is therefore still a limitation, rather than a verified
feature. These captures use sleeps and event handling without explicit poll.

`baseline/serial-event-api-check.log` verifies increasing report count during
a 1500 ms sleep without explicit polling, idempotent start, tuple/constants,
and ImportError for network, bluetooth and socket. The board was left at the
normal REPL with status 4 and the identified Zero 2 connected.

## Fixing restart recovery

The initial link-key-only retry was insufficient. A trace exposed incoming
legacy PIN pairing answered with Bluepad32's Wii host-address PIN, followed
by authentication failure (0x05). The module-specific Classic patch uses
PIN 0000 for the tested E4:17:D8 8BitDo address family. A later trace confirmed
successful authentication and parsed gamepad input.

The adapter now saves the last Classic controller address and name after
a valid gamepad report in TLV tag GP02 (0x47503032), inside the reserved
Bluetooth sectors. It avoids rewriting unchanged identity. It can load the
experimental GP01 address record or a stored link key as fallback. Every
five seconds while no controller is ready, it attempts Bluepad32's normal
HID connection flow to that peer, using the saved name to avoid remote-name
paging timeout collisions. Existing device slots and connection timers
prevent overlapping retries. BLE scanning is unchanged.

Bluepad32 previously dropped keys on every failed L2CAP connection, including
transient 0x0b simultaneous-connect failures. The module-specific patch now
drops keys only for authentication/key/security failures. The latest
compatibility patch supports optional MicroPython connection logs. Normal
firmware has tracing off; HCI tracing excludes link-key reply payloads.

The final quiet UF2 SHA-256 is
`47d785470080f81a3988822b57dad63be7db98bf97b9eeb59d7a50e5c93ff90a`.
`baseline/serial-saved-peer-flash.log` records its fresh boot and automatic
reconnection to the identified Zero 2, followed by A press (mask 2), release
(0), and continuing parsed reports. No controller power-cycle or re-pairing
was requested for this final flash test.

`baseline/serial-final-restart.log` additionally confirms a Pico-only
`machine.reset()` followed by automatic status 4, the same Zero 2 identity,
and increasing parsed reports, without asking for a controller restart or
pairing. Runtime imports of network, bluetooth and socket still fail as
intended. The board is left running the final quiet firmware at the REPL.

## Controlled reset audit: previous automatic-recovery claim withdrawn

The user confirmed that Pico resets still require a Zero 2 power cycle. Earlier
captures did not independently establish continuous controller power. The new
`baseline/controlled_recovery.py` waits for connected plus a parsed report and
immediately invokes machine.reset(), then measures recovery without user
intervention. `baseline/serial-controlled-page-timeout.log` shows GP02 loaded,
five retry attempts, repeated HCI page timeout (0x04), and no connection in
90 seconds. This reproduces the limitation without an initial controller
auto-off window. Prior successful captures therefore do not prove recovery
after abrupt reset. Shorter supervision timeout and clean software-reset
disconnection are being evaluated separately.

The shorter Classic supervision timeout also produced five failed paging
attempts over 90 seconds (`serial-controlled-supervision.log`). A temporary
pre-reset disconnect hook did not restore recovery (`serial-controlled-clean-reset.log`).
A final 30-second check (`serial-controlled-disconnect-check.log`) recorded
watchdog scratch marker 0x47520003, proving the disconnect callback completed
before machine.reset(). The user observed flashing, approximately one second
of steady LED, then the Zero 2 turning off. This clean-disconnect path cannot
restore a controller that powers itself off. The supervision and reset-hook
experiments were removed; no reset interception remains in the RP2 port.

The final firmware retains saved-peer retries and the new `info()` diagnostics
(saving status, attempt count, FSM state, last connection/authentication error).
The limitation is unresolved for abrupt Pico reset in this tested mode. The
controlled test scripts eliminate the initial auto-off window; initial
connection plus a parsed report automatically triggers the reset. No controller
mode or firmware version was changed. Subsequent investigation should compare
controller modes or firmware, rather than claiming these Pico retry changes
solved recovery.

Final diagnostic UF2 SHA-256:
`5f3fb3f9d3225720c1a272317ee5d3a2a80aef48ff955f5fdf83c0afc3c94d84`.
It was flashed, reached scanning, and loaded the saved peer. The board is left
at the REPL with Bluetooth running. Full initial-connection/reset captures
are archived as serial-controlled-abrupt-full.log and
serial-controlled-disconnect-full.log. The reusable controlled_recovery.py
now exits with failure unless RECOVERY_CONFIRMED is captured.
