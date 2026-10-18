#include "input.h"
#include "rtos_objects.h"
#include "display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"

void InputTask(void *pvParameters)
{
    DisplayMode currentMode = DisplayMode::TEMPERATURE;

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
                currentMode = nextDisplayMode(currentMode);
            }
            else
            {
                currentMode = previousDisplayMode(currentMode);
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