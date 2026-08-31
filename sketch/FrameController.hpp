#pragma once
#include "Motion.hpp"
#include "Serial.hpp"

#define X_LENGTH_MM 1500
#define Y_LENGTH_MM 1500

// Placeholder pending mechanical calibration — see Plan.md. Sized generously
// above the worst-case seek time (max axis travel / HOMING_VELOCITY, with a
// 2x margin) so it won't spuriously trip during a legitimate long home:
// ~1500mm / 100 steps/mm / 400 steps/sec * 2 ~= 750s. Needs updating if
// HOMING_VELOCITY or the axis lengths are recalibrated.
#define HOMING_TIMEOUT_MS 750000UL


// Owns device-level state and sequencing (what operation is running, what
// phase a multi-step operation like homing is in) and reports back to the
// caller over Serial. Motion.hpp/.cpp stays a stateless ClearCore adapter;
// this class is the only place that remembers "what are we doing right now."
class FrameController {
    public:
        FrameController();

        // Call once per loop() to advance any in-progress operation and
        // send its completion reply when it finishes.
        void Update();

        // Dispatches a parsed command line. Replies immediately for
        // rejected/invalid commands; for MOVE/HOME, the completion reply
        // comes later from Update().
        void HandleCommand(const CommandLine &line);

        // True while a MOVE or HOME is in progress. Exposed so setup() can
        // block on boot-time homing without duplicating HandleHome's logic.
        bool IsBusy() const;

        // Pre-flight checks both axes and, if clear, starts homing (does not
        // block). Returns false without starting if either axis already has
        // an alert present. Public so setup() can trigger boot-time homing
        // directly, without going through the serial command dispatch.
        bool StartHoming();

    private:
        enum OpState { OP_IDLE, OP_MOVING, OP_HOMING };
        enum HomePhase { HOME_PHASE_IDLE, HOME_PHASE_SEEKING, HOME_PHASE_BACKOFF };
        enum HomeResult { HOME_IN_PROGRESS, HOME_DONE, HOME_FAILED };

        OpState opState;
        HomePhase homePhase[AXIS_COUNT];
        uint32_t homeStartMs;

        void HandleMove(const CommandLine &line);
        void HandleMoveRelative(const CommandLine &line);
        void HandleHome(const CommandLine &line);
        void HandleClearAlerts(const CommandLine &line);
        void HandleZero(const CommandLine &line);

        void PollMove();
        void PollHome();
        HomeResult UpdateHomePhase(MotionAxis axis);
};

extern FrameController Device;
