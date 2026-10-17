
#ifndef DISPLAY_H
#define DISPLAY_H

#include "sensors.h"

enum class DisplayMode
{
    TEMPERATURE,
    HUMIDITY,
    LIGHT,
    MOTION
};

void DisplayTask(void *pvParameters);

#endif