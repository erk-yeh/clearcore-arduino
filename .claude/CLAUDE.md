# ClearCore Experiment Automation — Project Notes

## What this is

A control PC orchestrates an experiment: sends motion commands to a Teknic
ClearCore (driving ClearPath-SD servos) over USB serial, then separately
triggers an Anritsu VNA measurement via SCPI/PyVISA, logs data, repeats.
Accuracy requirement ~0.5 cm; encoders used for stall detection. ClearCore is
NOT used for SCPI/VISA — that logic belongs on the PC side.

## Build path decision

Using the **Arduino wrapper** (Teknic's `ClearCore:sam` board package via
`arduino-cli`), not the native `libClearCore` + CMake path, because it's
dramatically lower-friction: Teknic's board package bundles its own compiler,
`bossac`, and CMSIS via `arduino-cli core install` — no manual toolchain
assembly. A native scaffold was also built (separate `native/` project) if
we ever need the fuller API or real hardware debugging without the Arduino
wrapper's simplified API in the way — see that project's own README for why
it's more work (Teknic's native repo doesn't ship a startup file, linker
script, or CMSIS device headers; those had to be sourced from the Arduino
wrapper repo and from ARM's CMSIS_5 repo instead).

## Environment: native Windows, not WSL

Deliberately NOT using WSL for this, even though the bash tooling would be
cleaner there. Reason: WSL2 has no native USB access; bridging it via
`usbipd-win` is fragile specifically for boards with a resetting bootloader.
ClearCore's UF2 bootloader disconnects/re-enumerates during upload and when
manually triggered — exactly the pattern `usbipd-win` has open, unresolved
GitHub issues about (auto-attach doesn't reliably catch brief bootloader-mode
enumeration windows). Not worth fighting for a board that resets on every
upload. Flash/monitor/debug all happen from native Windows PowerShell.

## Critical gotcha: the FQBN is lowercase

`arduino-cli board listall` shows the actual board ID as:

```
ClearCore:sam:clearcore
```

NOT `ClearCore:sam:ClearCore`. The package index's display name
(`"boards": [{"name": "ClearCore"}]`) is not the same as the FQBN's board-ID
segment. Always verify FQBNs against `board listall` output rather than
assuming the display name matches the ID casing.

## Windows-specific setup notes

- `arduino-cli` PATH changes require a full VSCode **restart** (not just a
  new terminal tab) — VSCode inherits environment variables once at launch.
- Winget-installed CLI tools often alias through
  `%LOCALAPPDATA%\Microsoft\WindowsApps\`, which is usually already on PATH.
- `arduino-cli`'s data directory on Windows is `%LOCALAPPDATA%\Arduino15`
  (NOT `~/.arduino15` — that's the Linux/Mac path, don't confuse the two when
  setting `cortex-debug.armToolchainPath` in `.vscode/settings.json`). That
  setting points into the **`arduino`** package subtree, not the `ClearCore`
  one — see "Where the tools actually land" below.
- VSCode resolves `${env:LOCALAPPDATA}` inside `.vscode/*.json`, so prefer it
  over a hardcoded `C:\Users\<name>\...` — keeps these files portable.
- `.vscode/*.json` are JSONC: comments are legal there. PowerShell's
  `ConvertFrom-Json` (5.1) is not JSONC-aware and will report a bogus syntax
  error on them — don't "fix" a file based on that; strip `//` lines first if
  you need to validate one from the shell.
- The `.sh` scripts in `tools/` need Git Bash (or WSL) to run — they will
  not run in plain PowerShell/CMD. Force LF line endings on them via
  `.gitattributes` (`*.sh text eol=lf`) once this is a real git repo, or
  Windows CRLF checkout will break them with a "bad interpreter" error.
  `tools/*.ps1` are the native-Windows counterparts and have no such problem.
- Tasks that shell out pin `powershell.exe -NoProfile -ExecutionPolicy Bypass`
  explicitly in `tasks.json`. Don't rely on the default terminal profile — the
  port-resolution substitution `-p (./tools/clearcore-port.ps1)` is PowerShell
  syntax and silently passes a literal string under cmd.exe.

## Board package

Registered via:
```
https://www.teknic.com/files/downloads/package_clearcore_index.json
```
Bundles `arm-none-eabi-gcc` (7-2017q4), `bossac` (1.9.1-arduino1), and CMSIS
(4.5.0) automatically on `core install` — verified against the package
index contents directly, not assumed.

### Where the tools actually land (verified on disk, 2026-08-06)

The bundled tools do NOT live under the ClearCore package. There is no
`packages/ClearCore/tools/` directory at all — ClearCore declares gcc,
`bossac`, and CMSIS as *dependencies on the `arduino` package* rather than
vendoring its own copies, so they resolve to the shared location:

```
%LOCALAPPDATA%\Arduino15\packages\arduino\tools\arm-none-eabi-gcc\7-2017q4\
%LOCALAPPDATA%\Arduino15\packages\arduino\tools\bossac\1.9.1-arduino1\
%LOCALAPPDATA%\Arduino15\packages\arduino\tools\CMSIS\4.5.0\
```

Core sources, by contrast, are under the ClearCore package as expected:

```
%LOCALAPPDATA%\Arduino15\packages\ClearCore\hardware\sam\1.7.1\
  cores\arduino\                                      # Arduino.h, wrapper API
  variants\clearcore\                                 # pins_arduino.h
  variants\clearcore\Third Party\SAME53\CMSIS\Device\Include\   # note the space
  Teknic\libClearCore\inc\                            # ClearCore.h lives HERE
  Teknic\LwIP\LwIP\{src,port}\include\
```

`ClearCore.h` is in `Teknic\libClearCore\inc\`, not in `cores\` or
`variants\` — that's the one most likely to be guessed wrong.

## IntelliSense (C/C++ extension)

`arduino-cli` compiling successfully tells you nothing about whether
IntelliSense is configured — they're completely separate. "#include errors
detected... squiggles are disabled for this translation unit" means
`.vscode/c_cpp_properties.json` is missing or wrong, not that the build is
broken.

Do NOT hand-write include paths. Have `arduino-cli` emit ground truth:

```
arduino-cli compile --fqbn ClearCore:sam:clearcore --build-path ./build \
  --only-compilation-database ./sketch
```

This writes `build/compile_commands.json` with the exact flags. Wired up as
the `arduino-cli: compilation database` task. Two caveats:

- The database only records the generated `build/sketch/sketch.ino.cpp`,
  never the `.ino` itself — so `c_cpp_properties.json` needs an explicit
  `includePath`/`defines` fallback for the sketch to resolve.
- `arduino-cli` implicitly prepends `Arduino.h` to every sketch. IntelliSense
  can't know that; it needs a `forcedInclude` entry.

`build/` is gitignored and the database holds absolute paths, so it does not
survive a fresh clone — the fallback `includePath` is what covers that gap.
Those fallback paths hardcode core version **1.7.1**; bump them on core
update.

## Bootloader / flashing facts (verified against Teknic's own docs)

- Bootloader occupies `0x0000`–`0x4000`, write-protected; user code starts
  at `0x4000`. Any manual `bossac` invocation must use `--offset=0x4000`.
- **CONFIRMED empirically (2026-08-06): `arduino-cli upload` DOES auto-trigger
  bootloader mode.** A plain upload against the application-mode port erased,
  wrote, and verified 266 pages with no manual reset. The double-tap-reset
  fallback is not part of the normal workflow — only reach for it if an upload
  genuinely fails.
- **The board changes COM port across an upload.** Observed COM3 -> COM4, and
  the pre-upload `board list` showed it on *both* `COM3` (serial, identified as
  Teknic ClearCore) and `UF2_Board` (uf2conv, "Unknown") at once. `arduino-cli`
  reports the new one as `New upload port: COM4 (serial)` and follows it fine
  mid-upload, but anything you run *afterwards* — a serial monitor, a PC-side
  control script — must re-resolve the port. Do not hardcode it.
  `tools/clearcore-port.ps1` resolves it from `board list --format json` by
  FQBN match; the tasks call it inline so the buttons never go stale. The port
  entry also carries a stable `hardware_id`/`serialNumber`
  (`C06FCE93534C374A4A202020FF0D1C40`) if you ever need to disambiguate two
  boards rather than just match on FQBN.
- **`upload` does not inherit `compile`'s `--build-path`.** If you compile with
  `--build-path ./build`, you must upload with `--input-dir ./build`. Otherwise
  upload looks in `%LOCALAPPDATA%\arduino\sketches\<hash>\` and fails with
  "Compiled sketch not found in ..." even though the binary exists.

## Debugging (once a probe is available)

- OpenOCD's `target/atsame5x.cfg` explicitly lists `SAME53N19A` in its parts
  table — verified against OpenOCD's own source, genuinely supported, not a
  workaround.
- ClearCore has a physical Tag-Connect-style SWD header — confirmed via
  Teknic selling a specific cable (`TC2030-CTX-LEMTA`) for exactly this,
  used with an Atmel-ICE.
- J-Link's OpenOCD interface script (`interface/jlink.cfg`) is well-
  established. Atmel-ICE's correct interface script is NOT verified — check
  this directly once the probe is in hand rather than assuming CMSIS-DAP.
- Use **attach** mode in `launch.json`, not flash-through-OpenOCD — flash
  normally via `bossac`/`arduino-cli upload` first, then attach purely for
  breakpoints. This avoids teaching OpenOCD about the bootloader-protected
  memory region at all.
- Arduino IDE's built-in "Debug" button does NOT work for ClearCore either
  — confirmed by reading Teknic's actual `platform.txt`, which has no
  `debug.*` keys. This isn't something lost by using VSCode instead of the
  Arduino IDE; neither supports it out of the box.

## Things intentionally left unverified — check before relying on them

- Exact ClearCore pin macros — still not guessed anywhere in this codebase.
  The file is now confirmed present at
  `%LOCALAPPDATA%\Arduino15\packages\ClearCore\hardware\sam\1.7.1\variants\clearcore\pins_arduino.h`,
  but its *contents* have not been read. Open it (or the Doxygen Arduino API
  reference) before wiring up real I/O.
- Whether there's a labeled SWD header vs. bare test points on the board.
- Atmel-ICE's correct OpenOCD interface config file.

## Key references

- libClearCore Doxygen: https://teknic-inc.github.io/ClearCore-library/
- ClearCore Arduino API reference: https://teknic-inc.github.io/ClearCore-library/ArduinoRef.html
- Hardware/wiring manual: https://teknic.com/files/downloads/clearcore_user_manual.pdf
- Native library repo: https://github.com/Teknic-Inc/ClearCore-library
- Arduino wrapper repo: https://github.com/Teknic-Inc/ClearCore-Arduino-wrapper