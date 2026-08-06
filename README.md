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
arduino-cli compile --fqbn ClearCore:sam:ClearCore --build-path ./build ./sketch
arduino-cli upload  --fqbn ClearCore:sam:ClearCore -p <port> ./sketch
arduino-cli monitor -p <port> -c baudrate=115200
```

`--build-path ./build` matters: it's what makes the `.elf` land somewhere
predictable for the debugger, instead of arduino-cli's usual hidden cache
directory.

## Debugging (once you have an SWD probe)

1. Install the **Cortex-Debug** VSCode extension.
2. Install OpenOCD separately (not bundled by Teknic's package):
   `apt install openocd` / `brew install open-ocd` / Windows binary.
3. Edit `.vscode/settings.json` — replace `REPLACE_ME` with your actual home
   directory. Find the exact installed toolchain path with:
   ```bash
   arduino-cli config dump | grep directories -A2
   ```
   (Look under `data`, then `packages/ClearCore/tools/arm-none-eabi-gcc/`.)
4. Edit `openocd.cfg` — the J-Link interface line should work as-is; if
   you're using an Atmel-ICE instead, verify the correct OpenOCD interface
   script yourself before relying on it (see the comment in that file — I
   couldn't confirm this one).
5. Flash normally via `bossac` (the upload task above) first.
6. Hit F5. This **attaches** rather than reflashing through OpenOCD, so it
   won't touch the bootloader-protected flash region — you're just setting
   breakpoints and stepping through whatever's already running.

## What's verified vs. not

- **Verified**: Teknic's board package URL, its bundled toolchain
  dependencies, and OpenOCD's `atsame5x.cfg` explicitly supporting
  `SAME53N19A` (checked against OpenOCD's own source).
- **Not verified**: the exact `arm-none-eabi-gcc` install path on your
  machine (varies by OS/version — that's why `settings.json` has a
  placeholder instead of a guessed path), whether Atmel-ICE needs a
  different interface script than J-Link, and whether there's an
  accessible SWD header/pads on the ClearCore board itself (check the
  hardware manual before assuming).
- **Untested end-to-end**: I don't have the hardware or a debug probe to
  actually run this attach flow — treat `launch.json` and `openocd.cfg` as
  a well-informed starting point, not a confirmed-working config.

## Sketch placeholder

`sketch/sketch.ino` is intentionally empty of ClearCore-specific pin names —
check `variants/clearcore/pins_arduino.h` inside the installed package, or
the Doxygen Arduino API reference, before wiring up real I/O.
