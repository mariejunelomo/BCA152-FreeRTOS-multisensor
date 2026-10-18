#ifndef INPUT_H
#define INPUT_H

#include "driver/gpio.h"
#include "display_mode.h"

#define ENCODER_CLK_PIN GPIO_NUM_32
#define ENCODER_DT_PIN  GPIO_NUM_33
#define ENCODER_SW_PIN  GPIO_NUM_25

void InputTask(void *pvParameters);

#endif