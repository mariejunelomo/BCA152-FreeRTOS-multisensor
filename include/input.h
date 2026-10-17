#ifndef INPUT_H
#define INPUT_H

#include "driver/gpio.h"
#include "display.h"

#define ENCODER_CLK_PIN GPIO_NUM_32
#define ENCODER_DT_PIN  GPIO_NUM_33
#define ENCODER_SW_PIN  GPIO_NUM_25

void InputTask(void *pvParameters);

DisplayMode nextDisplayMode(DisplayMode mode);
DisplayMode previousDisplayMode(DisplayMode mode);

#endif