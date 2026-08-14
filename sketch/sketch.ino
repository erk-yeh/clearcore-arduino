#include "Motion.hpp"
#include "Serial.hpp"

void setup() {
    motor_init();
    // TODO: homing goes here once implemented (before SerialInit, per Plan.md)
    SerialInit();
}

void loop() {
    SerialLine line;
    if (SerialReadLine(&line)) {
        // TODO: dispatch on line.command / line.args (command grammar TBD)
    }
}
