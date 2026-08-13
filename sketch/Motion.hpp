#pragma once
#include "ClearCore.h"

enum MotionAxis {
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_COUNT
};

#define velocity_max 1000
#define acceleration_max 1000

struct MotionStatus {
    int32_t position;
    bool moving;
    bool atTarget;
    bool faulted;
    bool alertsPresent;
};

int motor_init();

void MotionEnable(MotionAxis axis, bool enable);
bool MotionMoveAbsolute(MotionAxis axis, int32_t position);
void MotionStop(MotionAxis axis);
void MotionClearAlerts(MotionAxis axis);
void MotionZero(MotionAxis axis);
MotionStatus MotionGetStatus(MotionAxis axis);
