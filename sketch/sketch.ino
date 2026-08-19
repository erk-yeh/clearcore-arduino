#include "Motion.hpp"
#include "Serial.hpp"
#include <string.h>
#include <stdlib.h>

enum OpState { OP_IDLE, OP_MOVING, OP_HOMING };
static OpState opState = OP_IDLE;

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

static void HandleMove(const CommandLine &line) {
    if (line.num_args != 2) {
        SerialSendLine("ERR MOVE requires 2 args: X Y");
        return;
    }

    int32_t x_mm, y_mm;
    if (!ParseInt(line.args[0], &x_mm) || !ParseInt(line.args[1], &y_mm)) {
        SerialSendLine("ERR MOVE args must be integers");
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

static void PollMove() {
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

static void HandleHome() {
    if (!MotionCanMove(AXIS_X) || !MotionCanMove(AXIS_Y)) {
        SerialSendLine("ERR alert present, clear before homing");
        return;
    }

    MotionHomeStart(AXIS_X);
    MotionHomeStart(AXIS_Y);
    opState = OP_HOMING;
}

static void PollHome() {
    MotionHomeResult x = MotionHomeUpdate(AXIS_X);
    MotionHomeResult y = MotionHomeUpdate(AXIS_Y);

    if (x == HOMING_FAILED || y == HOMING_FAILED) {
        SerialSendLine("ERR homing failed");
        opState = OP_IDLE;
        return;
    }

    if (x == HOMING_DONE && y == HOMING_DONE) {
        SerialSendLine("OK");
        opState = OP_IDLE;
    }
}

void setup() {
    motor_init();
    MotionEnable(AXIS_X, true);
    MotionEnable(AXIS_Y, true);
    // TODO: homing goes here once implemented (before SerialInit, per Plan.md)
    SerialInit();
    SerialSendLine("Connection made");
}

void loop() {
    if (opState == OP_MOVING) {
        PollMove();
    } else if (opState == OP_HOMING) {
        PollHome();
    }

    CommandLine line;
    if (SerialReadLine(&line)) {
        if (line.command == nullptr) {
            SerialSendLine("ERR empty command");
            return;
        }

        if (opState != OP_IDLE) {
            SerialSendLine("ERR busy");
            return;
        }

        if (strcmp(line.command, "MOVE") == 0) {
            HandleMove(line);
        } else if (strcmp(line.command, "HOME") == 0) {
            HandleHome();
        } else if (strcmp(line.command, "STATUS") == 0) {
            // TODO: status reply
        } else {
            SerialSendLine("ERR unknown command");
        }
    }
}
