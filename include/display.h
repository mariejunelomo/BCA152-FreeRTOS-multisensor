
#ifndef DISPLAY_H
#define DISPLAY_H

#include "sensors.h"

typedef enum
{
    TEMPERATURE,
    HUMIDITY,
    LIGHT,
    MOTION
} DisplayMode;

void DisplayTask(void *pvParameters);

#endif