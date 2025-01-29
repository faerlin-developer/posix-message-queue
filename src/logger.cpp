
#include <cstdarg>
#include <iostream>
#include "timer.h"

void logInfo(const char *format, ...) {

    auto timestamp = now().c_str();
    printf("[INFO] %s ", timestamp);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    fflush(stdout);
}

void logError(const char *format, ...) {

    auto timestamp = now().c_str();
    fprintf(stderr, "[ERROR] %s ", timestamp);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fflush(stderr);
}