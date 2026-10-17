#ifndef MOTION_H
#define MOTION_H

#include "driver/gpio.h"

#define PIR_PIN GPIO_NUM_27

void MotionTask(void *pvParameters);

#endif