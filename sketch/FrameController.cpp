#include "FrameController.hpp"
#include <string.h>
#include <stdlib.h>

FrameController Device;

FrameController::FrameController() : opState(OP_IDLE), homeStartMs(0) {
    for (int i = 0; i < AXIS_COUNT; i++) {
        homePhase[i] = HOME_PHASE_IDLE;
    }
}

bool FrameController::IsBusy() const {
    return opState != OP_IDLE;
}

static bool ParseInt(const char *str, int32_t *out) {
    if (str == nullptr || *str == '\0') {
        return false;
    }
    char *end;
    long value = strtol(str, &end, 10);
    if (*end != '\0') {
        return false;
    }
    *out = (int32_t)value;
    return true;
}

void FrameController::Update() {
    switch (opState) {
        case OP_IDLE:
            break;
        case OP_MOVING:
            PollMove();
            break;
        case OP_HOMING:
            PollHome();
            break;
    }
}

void FrameController::HandleCommand(const CommandLine &line) {
    if (opState != OP_IDLE) {
        SerialSendLine("ERR busy");
        return;
    }

    if (strcmp(line.command, "MOVE") == 0) {
        HandleMove(line);
    } else if (strcmp(line.command, "MOVE_REL") == 0) {
        HandleMoveRelative(line);
    } else if (strcmp(line.command, "HOME") == 0) {
        HandleHome(line);
    } else if (strcmp(line.command, "CLEAR_ALERTS") == 0) {
        HandleClearAlerts(line);
    } else if (strcmp(line.command, "ZERO") == 0) {
        HandleZero(line);
    } else if (strcmp(line.command, "STATUS") == 0) {
        // TODO: status reply
    } else {
        SerialSendLine("ERR unknown command");
    }
}

void FrameController::HandleMove(const CommandLine &line) {
    if (line.num_args != 2) {
        SerialSendLine("ERR MOVE requires 2 args: X Y");
        return;
    }

    int32_t x_mm, y_mm;
    
    if (!ParseInt(line.args[0], &x_mm) || !ParseInt(line.args[1], &y_mm)) {
        SerialSendLine("ERR MOVE args must be integers");
        return;
    }

    if (x_mm < 0 || x_mm > X_LENGTH_MM || y_mm < 0 || y_mm > Y_LENGTH_MM) {
        SerialSendLine("ERR MOVE args out of bounds");
        return;
    }

    if (!MotionCanMove(AXIS_X) || !MotionCanMove(AXIS_Y)) {
        SerialSendLine("ERR alert present, clear before moving");
        return;
    }


    
    MotionMoveAbsoluteMM(AXIS_X, x_mm);
    MotionMoveAbsoluteMM(AXIS_Y, y_mm);
    opState = OP_MOVING;
}

void FrameController::HandleMoveRelative(const CommandLine &line) {
    if (line.num_args != 2) {
        SerialSendLine("ERR MOVE_REL requires 2 args: dX dY");
        return;
    }

    int32_t dx_mm, dy_mm;
    if (!ParseInt(line.args[0], &dx_mm) || !ParseInt(line.args[1], &dy_mm)) {
        SerialSendLine("ERR MOVE_REL args must be integers");
        return;
    }

    // No bounds check against X_LENGTH_MM/Y_LENGTH_MM here: unlike MOVE, a
    // delta isn't a resulting absolute position, and computing one would
    // mean reading current position ourselves - exactly what using
    // MOVE_TARGET_REL_END_POSN was chosen to avoid. Over-travel falls back
    // to the limit switches, same as any other move.
    if (!MotionCanMove(AXIS_X) || !MotionCanMove(AXIS_Y)) {
        SerialSendLine("ERR alert present, clear before moving");
        return;
    }

    MotionMoveRelativeMM(AXIS_X, dx_mm);
    MotionMoveRelativeMM(AXIS_Y, dy_mm);
    opState = OP_MOVING;
}

void FrameController::PollMove() {
    MotionStatus x = MotionGetStatus(AXIS_X);
    MotionStatus y = MotionGetStatus(AXIS_Y);

    if (x.alertsPresent || y.alertsPresent) {
        SerialSendLine("ERR fault during move");
        opState = OP_IDLE;
        return;
    }

    if (x.atTarget && y.atTarget) {
        SerialSendLine("OK");
        opState = OP_IDLE;
    }
}

void FrameController::HandleHome(const CommandLine &line) {
    if (!StartHoming()) {
        SerialSendLine("ERR alert present, clear before homing");
    }
}

bool FrameController::StartHoming() {
    if (!MotionCanMove(AXIS_X) || !MotionCanMove(AXIS_Y)) {
        return false;
    }

    homeStartMs = Milliseconds();
    for (int i = 0; i < AXIS_COUNT; i++) {
        homePhase[i] = HOME_PHASE_SEEKING;
    }
    // TEMP: seeking + instead of - for bench testing. Swap back to
    // MotionSeekNegativeLimit (and the matching calls in UpdateHomePhase
    // below) once testing toward - is possible again - see Motion.hpp.
    MotionSeekPositiveLimit(AXIS_X);
    MotionSeekPositiveLimit(AXIS_Y);
    opState = OP_HOMING;
    return true;
}

void FrameController::PollHome() {
    HomeResult x = UpdateHomePhase(AXIS_X);
    HomeResult y = UpdateHomePhase(AXIS_Y);

    if (x == HOME_FAILED || y == HOME_FAILED) {
        SerialSendLine("ERR homing failed");
        opState = OP_IDLE;
        return;
    }

    if (x == HOME_DONE && y == HOME_DONE) {
        SerialSendLine("OK");
        opState = OP_IDLE;
    }
}

FrameController::HomeResult FrameController::UpdateHomePhase(MotionAxis axis) {
    if (homePhase[axis] != HOME_PHASE_IDLE && Milliseconds() - homeStartMs > HOMING_TIMEOUT_MS) {
        homePhase[axis] = HOME_PHASE_IDLE;
        return HOME_FAILED;
    }

    switch (homePhase[axis]) {
        case HOME_PHASE_SEEKING:
            // TEMP: matches MotionSeekPositiveLimit in StartHoming() above.
            if (MotionPositiveLimitTripped(axis)) {
                MotionBackOffFromPositiveLimit(axis);
                homePhase[axis] = HOME_PHASE_BACKOFF;
                return HOME_IN_PROGRESS;
            }
            if (!MotionCanMove(axis)) {
                homePhase[axis] = HOME_PHASE_IDLE;
                return HOME_FAILED;
            }
            return HOME_IN_PROGRESS;

        case HOME_PHASE_BACKOFF: {
            MotionStatus status = MotionGetStatus(axis);
            if (status.alertsPresent) {
                homePhase[axis] = HOME_PHASE_IDLE;
                return HOME_FAILED;
            }
            if (status.atTarget) {
                MotionZero(axis);
                homePhase[axis] = HOME_PHASE_IDLE;
                return HOME_DONE;
            }
            return HOME_IN_PROGRESS;
        }

        default:
            return HOME_DONE;
    }
}

void FrameController::HandleClearAlerts(const CommandLine &line) {
    MotionClearAlerts(AXIS_X);
    MotionClearAlerts(AXIS_Y);
    SerialSendLine("OK");
}

void FrameController::HandleZero(const CommandLine &line) {
    MotionZero(AXIS_X);
    MotionZero(AXIS_Y);
    SerialSendLine("OK");
}
