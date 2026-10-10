#include "input.h"
#include "rtos_objects.h"
#include "display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"

#define ENCODER_CLK_PIN GPIO_NUM_32
#define ENCODER_DT_PIN  GPIO_NUM_33

void InputTask(void *pvParameters)
{
    DisplayMode currentMode = TEMPERATURE;

    int lastCLK = gpio_get_level(ENCODER_CLK_PIN);

    safe_printf("InputTask started\n");
    safe_printf("Current DisplayMode: TEMPERATURE\n");

    while (1)
    {
        int currentCLK = gpio_get_level(ENCODER_CLK_PIN);

        if (currentCLK != lastCLK && currentCLK == 0)
        {
            int currentDT = gpio_get_level(ENCODER_DT_PIN);

            if (currentDT != currentCLK)
            {
                if (currentMode == MOTION)
                {
                    currentMode = TEMPERATURE;
                }
                else
                {
                    currentMode = (DisplayMode)(currentMode + 1);
                }
            }
            else
            {
                if (currentMode == TEMPERATURE)
                {
                    currentMode = MOTION;
                }
                else
                {
                    currentMode = (DisplayMode)(currentMode - 1);
                }
            }

            xQueueOverwrite(displayModeQueue, &currentMode);

            safe_printf(
                "InputTask: DisplayMode changed to %d\n",
                (int)currentMode
            );

            vTaskDelay(pdMS_TO_TICKS(20));
        }

        lastCLK = currentCLK;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}