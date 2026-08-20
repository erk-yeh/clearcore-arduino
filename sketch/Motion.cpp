#include "Motion.hpp"

static MotorDriver *const motors[AXIS_COUNT] = { &ConnectorM0, &ConnectorM1 };

// Assumed direction pending confirmation against actual physical wiring.
static const ClearCorePins limitPinNeg[AXIS_COUNT] = { CLEARCORE_PIN_DI6, CLEARCORE_PIN_IO0 };
static const ClearCorePins limitPinPos[AXIS_COUNT] = { CLEARCORE_PIN_DI7, CLEARCORE_PIN_IO1 };
static const ClearCorePins eStopPin = CLEARCORE_PIN_DI8;

int motor_init() {
    MotorMgr.MotorInputClocking(MotorManager::CLOCK_RATE_NORMAL);
    MotorMgr.MotorModeSet(MotorManager::MOTOR_M0M1, Connector::CPM_MODE_STEP_AND_DIR);

    ConnectorDI6.Mode(Connector::INPUT_DIGITAL);
    ConnectorDI7.Mode(Connector::INPUT_DIGITAL);
    ConnectorDI8.Mode(Connector::INPUT_DIGITAL);
    ConnectorIO0.Mode(Connector::INPUT_DIGITAL);
    ConnectorIO1.Mode(Connector::INPUT_DIGITAL);

    for (int i = 0; i < AXIS_COUNT; i++) {
        motors[i]->VelMax(velocity_max);
        motors[i]->AccelMax(acceleration_max);
        motors[i]->LimitSwitchNeg(limitPinNeg[i]);
        motors[i]->LimitSwitchPos(limitPinPos[i]);
        motors[i]->EStopConnector(eStopPin);
    }

    return 0;
}

void MotionEnable(MotionAxis axis, bool enable) {
    motors[axis]->EnableRequest(enable);
}

bool MotionCanMove(MotionAxis axis) {
    return !motors[axis]->StatusReg().bit.AlertsPresent;
}

bool MotionMoveAbsolute(MotionAxis axis, int32_t position) {
    MotorDriver *motor = motors[axis];
    if (motor->StatusReg().bit.AlertsPresent) {
        return false;
    }
    return motor->Move(position, MotorDriver::MOVE_TARGET_ABSOLUTE);
}

bool MotionMoveAbsoluteMM(MotionAxis axis, int32_t mm) {
    return MotionMoveAbsolute(axis, mm * STEPS_PER_MM);
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

void MotionSeekNegativeLimit(MotionAxis axis) {
    MotorDriver *motor = motors[axis];
    motor->EnableRequest(true);
    motor->MoveVelocity(-HOMING_VELOCITY);
}

bool MotionNegativeLimitTripped(MotionAxis axis) {
    return motors[axis]->AlertReg().bit.MotionCanceledNegativeLimit;
}

void MotionBackOffFromLimit(MotionAxis axis) {
    MotorDriver *motor = motors[axis];
    motor->ClearAlerts();
    // Back off to an absolute target rather than a relative move:
    // MOVE_TARGET_REL_END_POSN is relative to the end position of the last
    // *commanded* move, which is ill-defined here since the seek was a
    // velocity move cut short by the limit trip.
    int32_t backoffTarget = motor->PositionRefCommanded() + HOMING_BACKOFF_STEPS;
    motor->Move(backoffTarget, MotorDriver::MOVE_TARGET_ABSOLUTE);
}
