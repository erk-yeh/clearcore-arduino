# ClearCore Arduino wrapper + VSCode scaffold

Drives Teknic's official `ClearCore:sam` Arduino board package from
`arduino-cli` instead of the Arduino IDE, with SWD debugging wired up via
Cortex-Debug + OpenOCD for whenever you get a debug probe.

## Setup

```bash
./tools/setup_board.sh          # registers + installs Teknic's board package
```

This is genuinely low-friction: the package's own `toolsDependencies`
(`arm-none-eabi-gcc`, `bossac`, CMSIS) get pulled automatically by
`arduino-cli`, same as they would in the Arduino IDE. No manual dependency
tree to assemble, unlike the native scaffold.

## Build / upload / monitor

Use the VSCode tasks (Terminal > Run Task), or directly:

```bash
arduino-cli compile --fqbn ClearCore:sam:clearcore --build-path ./build ./sketch
arduino-cli upload  --fqbn ClearCore:sam:clearcore -p <port> ./sketch
arduino-cli monitor -p <port> -c baudrate=115200
```

Note the lowercase `clearcore` board-ID segment — the package index's display
name ("ClearCore") doesn't match the FQBN's actual casing; always check
`arduino-cli board listall` if unsure.

`--build-path ./build` matters: it's what makes the `.elf` land somewhere
predictable for the debugger, instead of arduino-cli's usual hidden cache
directory.

## Debugging (once you have an SWD probe)

Two probe setups are wired up in `launch.json`. Which one applies depends on
which probe you actually own.

### Option A — J-Link / generic OpenOCD probe ("Attach to ClearCore (SWD)")

1. Install the **Cortex-Debug** VSCode extension.
2. Install OpenOCD separately (not bundled by Teknic's package):
   `apt install openocd` / `brew install open-ocd` / Windows binary.
3. `.vscode/settings.json` already sets `cortex-debug.armToolchainPath` to
   the Arduino-bundled toolchain (`packages/arduino/tools/arm-none-eabi-gcc/7-2017q4/bin`
   — note it resolves under the `arduino` package, not `ClearCore`, since
   ClearCore declares gcc as a dependency rather than vendoring its own copy).
4. Edit `openocd.cfg` — the J-Link interface line should work as-is; if
   you're using an Atmel-ICE instead, verify the correct OpenOCD interface
   script yourself before relying on it (see the comment in that file — I
   couldn't confirm this one).
5. Flash normally via `bossac` (the upload task above) first.
6. Run the **"Attach to ClearCore (SWD)"** launch config. This **attaches**
   rather than reflashing through OpenOCD, so it won't touch the
   bootloader-protected flash region — you're just setting breakpoints and
   stepping through whatever's already running.

### Option B — PEmicro Multilink LC ("ClearCore Debug (PEmicro)")

Cable: Tag-Connect **TC2030-CTX** (legged, 6-pin SWD) → Multilink **Port G**
(the 10-pin 0.05" CORTEX-10 header — not Port A, which is BDM-only and
doesn't apply to this Cortex-M4F part). Align the ribbon's red stripe with
pin 1 on Port G.

This path is unusually fragmented because PEmicro's ARM tooling isn't built
with VS Code in mind, and the Arduino-bundled toolchain isn't built with
modern debugging in mind. Every step below was necessary.

1. **Get the PEmicro GDB server.** PEmicro distributes `pegdbserver_console.exe`
   as part of an "Eclipse plug-in," but the binary itself has no Eclipse
   dependency:
   - Download PEmicro's "GDB Server for ARM devices" package.
   - Copy out `com.pemicro.debug.gdbjtag.pne_<version>.jar` (the large,
     ~130MB one), rename the copy's extension to `.zip`, and extract it.
   - Find `pegdbserver_console.exe` nested under something like `.../gdi/`
     or `.../win32/`.
   - Copy the **whole containing folder** (it needs its sibling DLLs) to a
     stable location, e.g. `C:\PEmicro\gdbserver\`.
   - If your target device doesn't show up in `-devicelist`, the bundled
     `supportFiles_ARM` folder may be stale — download PEmicro's latest
     standalone ARM device support files and replace it. It must sit as a
     sibling to the `gdbserver` folder:
     ```
     C:\PEmicro\gdbserver\pegdbserver_console.exe
     C:\PEmicro\supportFiles_ARM\...
     ```
   - Verify standalone, before touching VS Code:
     ```powershell
     cd C:\PEmicro\gdbserver
     .\pegdbserver_console.exe -devicelist > devices.txt   # confirm your part is listed
     .\pegdbserver_console.exe -startserver -device=Microchip_SAME_ATSAME53N19A
     ```
     PEmicro's device strings don't match part numbers literally — for the
     ClearCore it's `Microchip_SAME_ATSAME53N19A`, confirm via `-devicelist`
     rather than guessing. A successful run reports an in-circuit debug
     session with servers running. **Stop this process** before launching
     from VS Code — Cortex-Debug starts and manages its own server instance,
     and a leftover manual one will hold the USB connection open and block it.

2. **Install a separate, modern GDB.** Cortex-Debug needs GDB ≥ 9. The
   Arduino-bundled toolchain (`arm-none-eabi-gcc/7-2017q4`) ships GDB 8.0.50
   — too old, and fails with "GDB could not start as expected. Bad
   installation or version mismatch." Install a current **Arm GNU Toolchain**
   release just for debugging (don't replace the build toolchain — compiling
   should keep using whatever arduino-cli resolves):
   - Download from Arm's GitLab releases (`gnu-toolchains-for-arm`), Windows
     host / arm-none-eabi target build.
   - Verify after installing: `arm-none-eabi-gdb.exe --version` should report
     ≥ 9. (This project's own install resolves to `15.3.Rel1`, GDB
     `16.3.90`, under
     `C:/Program Files (x86)/Arm/GNU Toolchain mingw-w64-i686-arm-none-eabi/bin`
     — confirm your own install path rather than assuming it matches.)

3. Install the **Cortex-Debug** VSCode extension (same one as Option A).

4. `launch.json`'s `"ClearCore Debug (PEmicro)"` config should already have:
   - `armToolchainPath` → the new standalone toolchain's `bin`, **not** the
     Arduino-bundled 2017 one (this config needs `arm-none-eabi-gdb` ≥ 9).
   - `serverpath` → `pegdbserver_console.exe`'s location.
   - `executable` → confirm this actually matches your current
     `arduino-cli compile --build-path ./build` output filename; it's
     `build/sketch.ino.elf` for this project, but re-check after any
     arduino-cli version change.
   - `showDevDebugOutput: "raw"` — leave this on until the connection is
     proven reliable; it surfaces raw GDB/server chatter in the Debug
     Console, the fastest way to diagnose failures.

5. Flash the board first (`bossac`/the upload task), then run the
   **"ClearCore Debug (PEmicro)"** launch config (F5). Confirm the firmware
   running on the board matches the `.elf` you're pointing the debugger at.

## What's verified vs. not

- **Verified**: Teknic's board package URL and its bundled toolchain
  dependencies; OpenOCD's `atsame5x.cfg` explicitly supporting `SAME53N19A`
  (checked against OpenOCD's own source); the PEmicro Multilink LC connects
  standalone (`pegdbserver_console.exe -startserver` reports an in-circuit
  debug session); the standalone Arm GNU Toolchain install has
  `arm-none-eabi-gdb.exe` at GDB 16.3.90 (well above the ≥9 requirement); the
  compiled `.elf` retains debug sections (`.debug_info`, `.debug_line`, etc.
  — not stripped).
- **Not verified**: whether Atmel-ICE needs a different OpenOCD interface
  script than J-Link, and whether there's an accessible SWD header/pads on
  the ClearCore board itself beyond the TC2030-CTX footprint used for the
  PEmicro path (check the hardware manual before assuming for other probes).
- **Untested end-to-end**: the OpenOCD/attach flow (Option A) — no J-Link or
  Atmel-ICE hardware has been used to actually run it; treat `launch.json`'s
  OpenOCD config and `openocd.cfg` as a well-informed starting point, not a
  confirmed-working config. The PEmicro path (Option B) has a confirmed
  standalone probe connection, but a full F5 breakpoint-hit session in VS
  Code should still be confirmed on first use.

## Sketch placeholder

`sketch/sketch.ino` is intentionally empty of ClearCore-specific pin names —
check `variants/clearcore/pins_arduino.h` inside the installed package, or
the Doxygen Arduino API reference, before wiring up real I/O.
