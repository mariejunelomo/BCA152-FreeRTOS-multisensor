#include "alarm.h"

AlarmState evaluateTemperature(float temperature)
{
    if (temperature < 18.0f) {
        return LOW_TEMPERATURE;
    }

    if (temperature > 30.0f) {
        return HIGH_TEMPERATURE;
    }

    return NORMAL;
}