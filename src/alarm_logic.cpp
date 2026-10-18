#include "alarm.h"

#define LOW_TEMPERATURE_LIMIT_C   18.0f
#define HIGH_TEMPERATURE_LIMIT_C  30.0f

AlarmState evaluateTemperature(float temperature)
{
    if (temperature < LOW_TEMPERATURE_LIMIT_C)
    {
        return LOW_TEMPERATURE;
    }

    if (temperature > HIGH_TEMPERATURE_LIMIT_C)
    {
        return HIGH_TEMPERATURE;
    }

    return NORMAL;
}