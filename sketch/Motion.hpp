#pragma once
#include "ClearCore.h"

enum MotionAxis {
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_COUNT
};

#define velocity_max 1000
#define acceleration_max 1000

// Placeholder pending steps-per-mm mechanical calibration (lead screw pitch,
// MSP Positioning Resolution) — see Plan.md. Shared across both axes.
#define STEPS_PER_MM 800 / 8 

struct MotionStatus {
    int32_t position;
    bool moving;
    bool atTarget;
    bool faulted;
    bool alertsPresent;
};

// Placeholder pending mechanical calibration — see Plan.md.
#define HOMING_VELOCITY 400
#define HOMING_BACKOFF_STEPS 200

enum MotionHomeResult {
    HOMING_IN_PROGRESS,
    HOMING_DONE,
    HOMING_FAILED
};

int motor_init();

void MotionEnable(MotionAxis axis, bool enable);
bool MotionCanMove(MotionAxis axis);
bool MotionMoveAbsolute(MotionAxis axis, int32_t position);
bool MotionMoveAbsoluteMM(MotionAxis axis, int32_t mm);
void MotionStop(MotionAxis axis);
void MotionClearAlerts(MotionAxis axis);
void MotionZero(MotionAxis axis);
MotionStatus MotionGetStatus(MotionAxis axis);

// Homes toward the negative limit switch. Non-blocking: call once to start,
// then poll MotionHomeUpdate() every loop() until it returns other than
// HOMING_IN_PROGRESS. Returns false without starting if alerts are already
// present on this axis.
bool MotionHomeStart(MotionAxis axis);
MotionHomeResult MotionHomeUpdate(MotionAxis axis);
