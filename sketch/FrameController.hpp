#pragma once
#include "Motion.hpp"
#include "Serial.hpp"

#define X_LENGTH_MM 1500 
#define Y_LENGTH_MM 1500 



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

    private:
        enum OpState { OP_IDLE, OP_MOVING, OP_HOMING };
        enum HomePhase { HOME_PHASE_IDLE, HOME_PHASE_SEEKING, HOME_PHASE_BACKOFF };
        enum HomeResult { HOME_IN_PROGRESS, HOME_DONE, HOME_FAILED };

        OpState opState;
        HomePhase homePhase[AXIS_COUNT];

        void HandleMove(const CommandLine &line);
        void HandleHome(const CommandLine &line);
        void HandleClearAlerts(const CommandLine &line);
        void HandleZero(const CommandLine &line);

        void PollMove();
        void PollHome();
        HomeResult UpdateHomePhase(MotionAxis axis);
};

extern FrameController Device;
