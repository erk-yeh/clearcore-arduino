#include "FrameController.hpp"
#include "Motion.hpp"
#include "Serial.hpp"

void setup() {
    motor_init();
    MotionEnable(AXIS_X, true);
    MotionEnable(AXIS_Y, true);
    // TODO: homing goes here once implemented (before SerialInit, per Plan.md)
    SerialInit();
    SerialSendLine("Connection made");
}

void loop() {
    Device.Update();

    CommandLine line;
    if (SerialReadLine(&line)) {
        if (line.command == nullptr) {
            SerialSendLine("ERR empty command");
            return;
        }
        Device.HandleCommand(line);
    }
}
