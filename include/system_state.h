#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <stdbool.h>

typedef enum {
    ACTIVE,
    INACTIVE
} SystemState;

SystemState evaluateSystemState(
    SystemState currentState,
    bool motionDetected,
    bool timeoutExpired
);

#endif