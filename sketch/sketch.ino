#include "ClearCore.h"

bool outputState;

void setup() {
    // IO-0 through IO-5 are the only connectors that support digital output.
    ConnectorIO0.Mode(Connector::OUTPUT_DIGITAL);
    outputState = true;
}

void loop() {
    ConnectorIO0.State(outputState);
    outputState = !outputState;
    delay(1000);
}