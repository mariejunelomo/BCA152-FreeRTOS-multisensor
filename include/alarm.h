#ifndef ALARM_H
#define ALARM_H

typedef enum {
    NORMAL,
    LOW_TEMPERATURE,
    HIGH_TEMPERATURE
} AlarmState;

AlarmState evaluateTemperature(float temperature);

#endif