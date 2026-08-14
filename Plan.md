# Firmware Plan — ClearCore Motion Controller

Design notes for the sketch itself (`sketch/`), as opposed to build/toolchain
notes which live in `.claude/CLAUDE.md`. This is a living document — update it
as decisions get made or revisited, don't just append.

## Roles

- **PC (control laptop):** owns experiment sequencing, SCPI/VISA to the VNA,
  logging, and (per the serial protocol, TBD) unit conversion / calibration.
  ClearCore is NOT used for SCPI/VISA.
- **ClearCore:** drives two ClearPath-SD servos (X on `ConnectorM0`, Y on
  `ConnectorM1`, Y is the **vertical** axis) in Step & Direction mode, over
  USB serial commands from the PC. Accuracy requirement ~0.5 cm.

## Module split (`sketch/`)

- `Motion.hpp`/`.cpp` — owns everything ClearPath-facing: motor init (input
  clocking, Step & Direction mode, HLFB mode/carrier, vel/accel limits),
  homing, and a small API the dispatch layer calls into (enable, move,
  stop, clear alerts, zero, status). No serial code here.
- `Serial.hpp`/`.cpp` — owns the USB link and line framing only (open the
  port, buffer/read a line non-blocking, send replies). No motor-specific
  logic here. Wire protocol (exact command grammar, units) is intentionally
  **not designed yet** — deferred until the rest of the structure is settled.
- `sketch.ino` — thin dispatch shim: `setup()` calls `MotionInit()` /
  `SerialInit()`; `loop()` polls `Serial` for a line and calls into `Motion`.

## Key hardware facts (verified against installed ClearCore 1.7.1 headers/examples)

- `MotorDriver` (`Teknic/libClearCore/inc/MotorDriver.h`) has no `begin()`.
  Real lifecycle: `EnableRequest(true)` to enable, then `Move(dist, moveTarget)`
  for position moves or `MoveVelocity(v)` for velocity moves. Both return
  immediately — the step generator runs in the background off SysTick, so
  motion is inherently non-blocking at the hardware level.
- Per-axis fault/stall info comes from HLFB, exposed via `StatusReg()`
  (`AtTargetPosition`, `MotorInFault`, `AlertsPresent`, `ReadyState`) and
  `HlfbState()`. This reflects the ClearPath-SD's own internal encoder —
  **no extra ClearCore-side wiring needed** for stall detection.
- ClearCore has exactly **one** hardware quadrature decoder (`EncoderIn`),
  fed through an external adapter board (CL-ENCDR-DFIN), shared board-wide
  and stealing DI-6/7/8 when used. Not usable independently on both X and Y
  at once — this is why stall detection is HLFB-based instead.
- **`PositionRefCommanded` is a boot-volatile step counter.** It starts at 0
  on every power-up and does not persist across power cycles. Confirmed via
  Teknic's `UserSeeksHome.cpp` example, which only calls `PositionRefSet(0)`
  *after* a physical homing sequence completes. This means "absolute
  position" is only meaningful relative to wherever the axis was last homed
  **this session** — rehoming must happen every power-up before any
  absolute move is trustworthy. Confirmed with the user this is expected/fine.
- Teknic's homing pattern (`UserSeeksHome.cpp`): enable motor, velocity-move
  toward a hardstop (fast, then slow), wait for HLFB to re-assert (= clamped
  against the hardstop, using the same stall signal as runtime stall
  detection), stop, move a small offset away from the hardstop, wait for
  HLFB to assert again, then `PositionRefSet(0)`. Requires the ClearPath
  itself configured in MSP for "User seeks home" homing style.
  **We are diverging from this for our own homing**, since we have real
  limit switches: home to a `LimitSwitchPos`/`LimitSwitchNeg` trip (which
  auto-decelerates and raises a `MotionCanceled*Limit` alert) rather than
  intentionally stalling against a hardstop. See the homing section below.
- Reference command dispatcher exists at
  `libraries/SerialCommunication/examples/ClearCoreCommandProtocol/ClearCoreCommandProtocol.ino`
  (Teknic-provided) — full text-command-over-`ConnectorUsb` example covering
  enable/disable, position/velocity moves, status queries, alert clearing,
  and limits. Worth reviewing again once the wire protocol is actually
  designed, but its command grammar is single-letter/numeric-motor-index and
  fully synchronous (blocks on `while` loops) — our dispatch should differ by
  staying non-blocking per loop() iteration.

## Decisions made so far

- **Move semantics: absolute positioning**, not relative/incremental.
- **Stall detection: HLFB per axis** (via `StatusReg()`/`HlfbState()`), not
  ClearCore's shared `EncoderIn` channel.
- **Serial protocol style: text line-based** (exact grammar TBD).
- **Command handling: async at the firmware level** — `MOVE` returns as soon
  as the step generator accepts it; `loop()` keeps servicing other input
  (status queries, stop) while a move is in progress, and the "it arrived"
  message to the PC is sent unsolicited once `AtTargetPosition` + HLFB
  reasserted are detected, not blocked on synchronously.
- **Rehoming happens every power-up** — confirmed necessary and expected,
  not something to try to avoid via persisted position.
- **Units on the wire: mm.** Integer mm gives 10x finer addressable
  resolution than the 0.5 cm accuracy spec needs, no decimals required in
  the protocol. The steps-per-mm conversion constant lives in `Motion`
  (it's a mechanical calibration fact), not `Serial` — `Serial` stays
  unaware of steps entirely; something like `MotionMoveAbsoluteMM(axis, mm)`
  converts and calls the existing `MotionMoveAbsolute`.
- **Reply semantics: silence until done.** No separate fast "accepted" ack
  for a `MOVE` that's going to succeed — a command gets exactly one
  terminal reply: an immediate rejection (bad syntax, alert present, etc.)
  if applicable, otherwise silence during the move and a single unsolicited
  reply once it actually completes. Chosen to keep the PC's waiting logic
  simple — it only ever waits for one of exactly two outcomes (ACK or NAK)
  per command, never has to distinguish "accepted" from "done".
- **No firmware-side "not homed" rejection.** `Motion` does not track a
  per-axis homed flag and does not refuse moves before homing. Justified
  by the init ordering below (homing always completes before serial is
  even brought up, so there's no window for a premature `MOVE` around
  boot) plus trusting the PC to decide whether to re-home after that
  (e.g. after a fault) rather than firmware enforcing it. The limit
  switches remain the fallback safety net regardless.
- **Coordinated (point-based) motion, not independent per-axis commands.**
  One `MOVE`-style command carries both X and Y; `Motion` fires both
  (non-blocking) `Move()` calls back-to-back — confirmed via Teknic's
  `DualAxisSynchronized` example that this needs no waiting/status-check in
  between, the step generators run independently per axis in the
  background — and waits for both to reach target before sending back a
  single reply. Chosen over independent per-axis messages to minimize
  traffic and avoid the PC having to correlate two separate completion
  events per logical move.

## Motor configuration

- **Clock rate: `CLOCK_RATE_NORMAL`.** Global across all four motor
  connectors (can't be set per-motor); Teknic's docs warn HIGH "may cause
  errors" with ClearPath motors, so this isn't really a choice.
- **Mode: `CPM_MODE_STEP_AND_DIR` via `MotorModeSet(MOTOR_M0M1, ...)`.**
  Mode is set per-pair (M0+M1 together, or M2+M3 together), not per
  individual motor — not a constraint here since X and Y are exactly M0/M1
  and both want Step & Direction anyway.
- **HLFB mode/carrier — still open, pending MSP-side drive config.**
  Firmware's `HlfbMode()`/`HlfbCarrier()` must match how each ClearPath-SD
  drive is actually configured in MSP (Teknic's drive-config software,
  separate from ClearCore). Reference examples use
  `HLFB_MODE_HAS_BIPOLAR_PWM` + `HLFB_CARRIER_482_HZ`, matching MSP's
  "ASG-Position w/Measured Torque" @ 482 Hz — but that's only correct if the
  drives are actually set that way. Not yet checked against the real
  drives.
- **VelMax/AccelMax — placeholders, pending mechanical calibration.**
  `Motion.hpp` currently has `velocity_max`/`acceleration_max` = 1000 as
  stand-ins. Real values (step pulses/sec, pulses/sec²) depend on a
  linear-distance-per-step mapping (lead screw pitch, MSP "Positioning
  Resolution" setting) that hasn't been worked out yet. Plan is to map that
  out, then calculate real max values from it.

## Implementation progress

- **`Motion.hpp`/`.cpp` — basic per-axis wrappers coded and compiling.**
  `MotionAxis` enum + `motors[]` array (replaced the earlier `motor_x`/
  `motor_y` macros so axis can be selected at runtime, needed once the
  serial dispatcher has to pick an axis from parsed input).
  `motor_init()`, `MotionEnable`, `MotionMoveAbsolute`, `MotionStop`,
  `MotionClearAlerts`, `MotionZero`, `MotionGetStatus` are real, not
  stubs — everything not gated on an open decision below. `motor_init()`
  now also configures the limit switch / E-Stop pins (see I/O wiring
  section) and sets their connector modes to `INPUT_DIGITAL`.
- `Serial.hpp`/`.cpp` — not started.
- `sketch.ino` — still the original blink example; not yet wired to
  `Motion`, since dispatch needs the (deferred) wire protocol.

## I/O wiring (limit switches, E-Stop, brake)

- **4 limit switches (2 per axis)** — X-pos, X-neg, Y-pos, Y-neg — plus
  **1 shared E-Stop** sensor = 5 digital inputs total, wired to
  `MotorDriver::LimitSwitchPos()`/`LimitSwitchNeg()`/`EStopConnector()` per
  axis (E-Stop pin shared across both `ConnectorM0` and `ConnectorM1`).
- **All 5 must be Normally Closed (NC)** — required by ClearCore's
  `LimitSwitchPos`/`LimitSwitchNeg`/`EStopConnector` implementation, and
  deliberately chosen so a disconnect/broken wire reads as "triggered"
  rather than silently reading as "fine" (fail-safe: distinguishes a wiring
  fault from genuinely clear travel).
- **Pin assignment (final):** `DI8` = E-Stop (shared, both axes),
  `DI6`/`DI7` = X-neg/X-pos, `IO0`/`IO1` = Y-neg/Y-pos. Leaves `IO2`-`IO5`
  free for future digital output use. Coded in `Motion.cpp`'s `motor_init()`
  via `LimitSwitchNeg`/`LimitSwitchPos`/`EStopConnector`, each pin first set
  to `Connector::INPUT_DIGITAL` (matching Teknic's examples, which do this
  explicitly even for `DI6`-`DI8`). **The +/- direction-to-pin mapping
  (which physical switch is "pos" vs. "neg") is an assumption, not yet
  confirmed against actual wiring** — swapping it later is a one-line fix.
- **Brake (Y/vertical axis only) is NOT a ClearCore concern.** Wired
  directly into the power line rather than through `MotorDriver::
  BrakeOutput()` — a fail-safe brake that disengages only while powered, so
  it auto-engages on any power loss. No firmware configuration needed for
  it at all.

## Initialization sequence (order updated — homing before serial bring-up)

1. **Boot / hardware config** — `MotorMgr.MotorInputClocking(...)`, put M0/M1
   into `CPM_MODE_STEP_AND_DIR`, set HLFB mode + 482 Hz carrier, set default
   `VelMax`/`AccelMax`.
2. **Homing** — each axis seeks its limit switch, backs off, zeros its
   position reference. Happens **before serial is even brought up** — this
   is deliberate: it means there is no window where the PC could send a
   `MOVE` before homing finishes, since it can't send anything at all yet.
3. **Serial bring-up** — open `ConnectorUsb`, block until the host has
   actually opened the port (`while (!ConnectorUsb)`), not just a fixed
   delay.
4. **Ready** — once serial is open (homing is already done by this point),
   ClearCore sends one unsolicited line so the PC knows it's safe to start
   issuing absolute moves.
5. **Command loop** — wait for a command, execute (non-blocking), report
   back once (see reply semantics above), wait again. A later explicit
   `HOME` command re-runs step 2 on demand, e.g. after a fault — whether to
   use it is left to the PC/operator, not enforced by firmware.

## Homing sequence (limit-switch based; see divergence note above)

Per axis: enable, confirm no pre-existing alerts, `MoveVelocity()` at a
slow homing-specific speed toward the "home" limit switch, poll for the
resulting `MotionCanceled*Limit` alert bit (the trip), `ClearAlerts()`,
move a small fixed offset away from the switch (must be the opposite
direction — the docs warn the alert reappears if you move toward the limit
again), wait for that move to complete, then `PositionRefSet(0)`.

Still open:
- **Which limit switch is "home" per axis** — positive or negative for X;
  top or bottom for Y. For Y specifically, gravity matters: homing toward
  the bottom means the seek move is gravity-assisted, homing toward the
  top means lifting against gravity the whole approach.
- **Sequential vs. simultaneous homing of X and Y.** Sequential is simpler
  to reason about and keeps only one axis moving unattended at a time;
  simultaneous is faster.
- **Homing velocity as its own constant**, separate from general
  `VelMax` (which is itself still a placeholder) — homing should probably
  be slower than normal operation regardless of what that ends up being.

## Open questions (not yet decided)

- **Exact command grammar** — deferred; to be designed after the
  above structural questions are settled. (Units are settled — mm — this
  is just the literal syntax, e.g. `MOVE 150 80` vs. something else.)
- **HLFB drive-side config** — need to check each ClearPath-SD's actual MSP
  HLFB mode/carrier setting before finalizing firmware `HlfbMode()`/
  `HlfbCarrier()` calls.
- **Linear-distance-per-step mapping** — needed to calculate real
  `VelMax`/`AccelMax`, currently placeholders.
