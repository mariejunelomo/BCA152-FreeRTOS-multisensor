#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>

typedef struct
{
    float temperature;
    float humidity;
    int lightLevel;
    bool motionDetected;
} SensorData;

void SensorTask(void *pvParameters);

#endif