#include "Motion.hpp"
#include "Serial.hpp"
#include <string.h>

void setup() {
    //motor_init();
    // TODO: homing goes here once implemented (before SerialInit, per Plan.md)
    SerialInit();
    SerialSendLine("Connection made\n");
}

void loop() {
    SerialLine line;
    if (SerialReadLine(&line)) {
        char reply[LINE_BUFFER_LEN] = "";
        if (line.command != nullptr) {
            strcat(reply, line.command);
        }
        for (int i = 0; i < line.num_args; i++) {
            strcat(reply, " ");
            strcat(reply, line.args[i]);
        }
        SerialSendLine(reply);
        // TODO: dispatch on line.command / line.args (command grammar TBD)
    }
}
