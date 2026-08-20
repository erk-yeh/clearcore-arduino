#include "FrameController.hpp"
#include "Motion.hpp"
#include "Serial.hpp"

void setup() {
    motor_init();

    // Homing enables both axes itself (see MotionSeekNegativeLimit). This
    // blocks setup(), which is fine — nothing else can happen yet anyway,
    // since serial isn't open. HOMING_TIMEOUT_MS bounds the wait so a stuck
    // switch can't prevent the serial port from ever opening; StartHoming()
    // returning false (pre-existing alert) also falls straight through.
    Device.StartHoming();
    while (Device.IsBusy()) {
        Device.Update();
    }

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
