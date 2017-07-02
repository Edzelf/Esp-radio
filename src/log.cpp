#include <log.h>

char *log(const char *format, ...) {

    static char sbuf[DEBUG_BUFFER_SIZE];                    // For debug lines
    va_list varArgs;                                        // For variable number of params

    va_start(varArgs, format);                              // Prepare parameters
    vsnprintf(sbuf, sizeof(sbuf), format, varArgs);         // Format the message
    va_end(varArgs);                                        // End of using parameters

    if (DEBUG) {                                            // DEBUG on?
        Serial.print("D: ");                                // Yes, print prefix
        Serial.println(sbuf);                               // and the info
    }

    return sbuf;
}