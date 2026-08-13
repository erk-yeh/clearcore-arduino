#include "Motion.hpp"

static MotorDriver *const motors[AXIS_COUNT] = { &ConnectorM0, &ConnectorM1 };

int motor_init() {
    MotorMgr.MotorInputClocking(MotorManager::CLOCK_RATE_NORMAL);
    MotorMgr.MotorModeSet(MotorManager::MOTOR_M0M1, Connector::CPM_MODE_STEP_AND_DIR);

    for (int i = 0; i < AXIS_COUNT; i++) {
        motors[i]->VelMax(velocity_max);
        motors[i]->AccelMax(acceleration_max);
    }

    return 0;
}

void MotionEnable(MotionAxis axis, bool enable) {
    motors[axis]->EnableRequest(enable);
}

bool MotionMoveAbsolute(MotionAxis axis, int32_t position) {
    MotorDriver *motor = motors[axis];
    if (motor->StatusReg().bit.AlertsPresent) {
        return false;
    }
    return motor->Move(position, MotorDriver::MOVE_TARGET_ABSOLUTE);
}

void MotionStop(MotionAxis axis) {
    motors[axis]->MoveStopDecel();
}

void MotionClearAlerts(MotionAxis axis) {
    motors[axis]->ClearAlerts();
}

void MotionZero(MotionAxis axis) {
    motors[axis]->PositionRefSet(0);
}

MotionStatus MotionGetStatus(MotionAxis axis) {
    MotorDriver *motor = motors[axis];
    volatile const MotorDriver::StatusRegMotor &reg = motor->StatusReg();

    MotionStatus status;
    status.position = motor->PositionRefCommanded();
    status.moving = reg.bit.StepsActive;
    status.atTarget = reg.bit.AtTargetPosition;
    status.faulted = reg.bit.MotorInFault;
    status.alertsPresent = reg.bit.AlertsPresent;
    return status;
}
