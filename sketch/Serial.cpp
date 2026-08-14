#include "Serial.hpp"
#include <string.h>

#define SERIAL_BAUD_RATE 115200

void SerialInit() {
    ConnectorUsb.Mode(Connector::USB_CDC);
    ConnectorUsb.Speed(SERIAL_BAUD_RATE);
    ConnectorUsb.PortOpen();
    while (!ConnectorUsb) {
        continue;
    }
}

void SerialLine::ReadLine(const char *rawLine) {
    strncpy(raw, rawLine, sizeof(raw) - 1);
    raw[sizeof(raw) - 1] = '\0';

    num_args = 0;
    command = strtok(raw, " ");
    char *token;
    while (num_args < MAX_ARGS && (token = strtok(nullptr, " ")) != nullptr) {
        args[num_args++] = token;
    }
}

bool SerialReadLine(SerialLine *line) {
    static char lineBuffer[LINE_BUFFER_LEN];
    static size_t lineLength = 0;

    while (ConnectorUsb.CharPeek() != -1) {

        char c = (char)ConnectorUsb.CharGet();

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            lineBuffer[lineLength] = '\0';
            line->ReadLine(lineBuffer);
            lineLength = 0;
            return true;
        }

        if (lineLength < LINE_BUFFER_LEN - 1) {
            lineBuffer[lineLength++] = c;
        }
        // else: silently drop overflow chars until the next newline.
    }
    return false;
}
