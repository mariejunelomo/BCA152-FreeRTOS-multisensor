#include "motion.h"
#include "display.h"
#include "input.h"
#include "alarm.h"
#include "system_state.h"
#include "rtos_objects.h"
#include "sensors.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"

// =========================================================
// EVENT GROUP BITS (defined in rtos_objects.h)
// =========================================================
// EVENT_ACTIVE: set/cleared by MotionTask
// EVENT_MOTION: set/cleared by MotionTask
// EVENT_ALARM:  set/cleared by AlarmTask

// =========================================================
// APP MAIN
// =========================================================

extern "C" void app_main(void)
{
    // Serial mutex must exist before any safe_printf() call

    serialMutex = xSemaphoreCreateMutex();

    if (serialMutex == NULL)
    {
        printf("Failed to create Serial Mutex\n");
        return;
    }

    safe_printf("\n");
    safe_printf("BCA152 FreeRTOS Multisensor\n");
    safe_printf("System starting...\n");
    safe_printf("\nSerial Mutex created successfully\n");
    safe_printf("Shared Serial output is protected by serialMutex\n\n");

    // Hardware initialization: rotary encoder

    gpio_config_t encoder_config = {
        .pin_bit_mask =
            (1ULL << ENCODER_CLK_PIN) |
            (1ULL << ENCODER_DT_PIN) |
            (1ULL << ENCODER_SW_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&encoder_config));

    // Hardware initialization: PIR sensor

    gpio_config_t pir_config = {
        .pin_bit_mask = (1ULL << PIR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&pir_config));

    // FreeRTOS objects: queues

    sensorQueue = xQueueCreate(5, sizeof(SensorData));

    if (sensorQueue == NULL)
    {
        safe_printf("Failed to create Sensor Queue\n");
        return;
    }

    safe_printf("Sensor Queue created successfully\n");

    alarmQueue = xQueueCreate(5, sizeof(SensorData));

    if (alarmQueue == NULL)
    {
        safe_printf("Failed to create Alarm Queue\n");
        return;
    }

    safe_printf("Alarm Queue created successfully\n");

    displayModeQueue = xQueueCreate(1, sizeof(DisplayMode));

    if (displayModeQueue == NULL)
    {
        safe_printf("Failed to create Display Mode Queue\n");
        return;
    }

    safe_printf("Display Mode Queue created successfully\n");

    motionQueue = xQueueCreate(1, sizeof(bool));

    if (motionQueue == NULL)
    {
        safe_printf("Failed to create Motion Queue\n");
        return;
    }

    safe_printf("Motion Queue created successfully\n");

    stateQueue = xQueueCreate(1, sizeof(SystemState));

    if (stateQueue == NULL)
    {
        safe_printf("Failed to create System State Queue\n");
        return;
    }

    safe_printf("System State Queue created successfully\n");

    // FreeRTOS objects: event group

    systemEventGroup = xEventGroupCreate();

    if (systemEventGroup == NULL)
    {
        safe_printf("Failed to create System Event Group\n");
        return;
    }

    safe_printf("System Event Group created successfully\n");
    safe_printf("EVENT_ACTIVE = BIT0\n");
    safe_printf("EVENT_MOTION = BIT1\n");
    safe_printf("EVENT_ALARM  = BIT2\n");

    // Task creation

    xTaskCreate(SensorTask,  "SensorTask",  4096, NULL, 2, NULL);
    xTaskCreate(AlarmTask,   "AlarmTask",   3072, NULL, 2, NULL);
    xTaskCreate(MotionTask,  "MotionTask",  4096, NULL, 3, NULL);
    xTaskCreate(DisplayTask, "DisplayTask", 4096, NULL, 1, NULL);
    xTaskCreate(InputTask,   "InputTask",   4096, NULL, 3, NULL);

    safe_printf("All FreeRTOS tasks created\n");
}