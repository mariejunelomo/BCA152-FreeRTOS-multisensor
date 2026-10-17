#ifndef INPUT_H
#define INPUT_H

#include "display.h"

void InputTask(void *pvParameters);

DisplayMode nextDisplayMode(DisplayMode mode);
DisplayMode previousDisplayMode(DisplayMode mode);

#endif