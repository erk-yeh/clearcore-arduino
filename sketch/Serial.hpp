#pragma once
#include "ClearCore.h"

#define MAX_ARGS 3
#define LINE_BUFFER_LEN 64

void SerialInit();
void SerialSend(const char *text);
void SerialSendLine(const char *text);

class SerialLine {
    public:
        // Copies rawLine into this object's own storage and tokenizes it
        // (whitespace-separated) into command + args.
        void ReadLine(const char *rawLine);

        char *command;
        char *args[MAX_ARGS];
        int num_args;

    private:
        char raw[LINE_BUFFER_LEN];
};

// Non-blocking: returns true (and fills `line`) at most once per completed
// line. Returns false immediately if a full line hasn't arrived yet.
bool SerialReadLine(SerialLine *line);