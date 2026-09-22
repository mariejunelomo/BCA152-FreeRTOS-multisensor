#include "system_state.h"

SystemState evaluateSystemState(
    SystemState currentState,
    bool motionDetected,
    bool timeoutExpired
)
{
    if (currentState == INACTIVE && motionDetected) {
        return ACTIVE;
    }

    if (currentState == ACTIVE && timeoutExpired) {
        return INACTIVE;
    }

    return currentState;
}