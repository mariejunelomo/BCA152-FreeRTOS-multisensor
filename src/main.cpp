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
#include "driver/i2c.h"

// =========================================================
// PIN DEFINITIONS
// =========================================================

#define OLED_SDA_PIN        GPIO_NUM_21
#define OLED_SCL_PIN        GPIO_NUM_22
#define OLED_I2C_PORT       I2C_NUM_0
#define OLED_ADDR           0x3C
#define OLED_WIDTH          128
#define OLED_HEIGHT         64

#define ENCODER_CLK_PIN     GPIO_NUM_32
#define ENCODER_DT_PIN      GPIO_NUM_33
#define ENCODER_SW_PIN      GPIO_NUM_25

// =========================================================
// STEP 35 - EVENT GROUP
// =========================================================

// EVENT_ACTIVE: Set/cleared by MotionTask
// EVENT_MOTION: Set/cleared by MotionTask
// EVENT_ALARM: Set/cleared by AlarmTask

// =========================================================
// APP MAIN
// =========================================================

extern "C" void app_main(void)
{
    // STEP 36 - CREATE SERIAL MUTEX FIRST

    serialMutex = xSemaphoreCreateMutex();

    if (serialMutex == NULL)
    {
        printf("Failed to create Serial Mutex\n");
        return;
    }

    safe_printf("\n");
    safe_printf("BCA152 FreeRTOS Multisensor\n");
    safe_printf("System starting...\n");
    safe_printf("\n===== STEP 36: SERIAL MUTEX =====\n");
    safe_printf("Serial Mutex created successfully\n");
    safe_printf("Shared Serial output is protected by serialMutex\n");
    safe_printf("=================================\n\n");

    // ALARM LOGIC TESTS

    safe_printf("===== ALARM LOGIC TESTS =====\n");
    safe_printf("Alarm Test 17.9 C = %d\n", evaluateTemperature(17.9f));
    safe_printf("Alarm Test 18.0 C = %d\n", evaluateTemperature(18.0f));
    safe_printf("Alarm Test 24.0 C = %d\n", evaluateTemperature(24.0f));
    safe_printf("Alarm Test 30.0 C = %d\n", evaluateTemperature(30.0f));
    safe_printf("Alarm Test 30.1 C = %d\n", evaluateTemperature(30.1f));
    safe_printf("============================\n\n");

    // CONFIGURE ROTARY ENCODER

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

    // CONFIGURE PIR

    gpio_config_t pir_config = {
        .pin_bit_mask = (1ULL << PIR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&pir_config));

    // SENSOR QUEUE

    sensorQueue = xQueueCreate(5, sizeof(SensorData));

    if (sensorQueue == NULL)
    {
        safe_printf("Failed to create Sensor Queue\n");
        return;
    }

    safe_printf("Sensor Queue created successfully\n");

    // ALARM QUEUE - NEW

    alarmQueue = xQueueCreate(5, sizeof(SensorData));

    if (alarmQueue == NULL)
    {
        safe_printf("Failed to create Alarm Queue\n");
        return;
    }

    safe_printf("Alarm Queue created successfully\n");

    // DISPLAY MODE QUEUE

    displayModeQueue = xQueueCreate(1, sizeof(DisplayMode));

    if (displayModeQueue == NULL)
    {
        safe_printf("Failed to create Display Mode Queue\n");
        return;
    }

    safe_printf("Display Mode Queue created successfully\n");

    // MOTION QUEUE

    motionQueue = xQueueCreate(1, sizeof(bool));

    if (motionQueue == NULL)
    {
        safe_printf("Failed to create Motion Queue\n");
        return;
    }

    safe_printf("Motion Queue created successfully\n");

    // SYSTEM STATE QUEUE

    stateQueue = xQueueCreate(1, sizeof(SystemState));

    if (stateQueue == NULL)
    {
        safe_printf("Failed to create System State Queue\n");
        return;
    }

    safe_printf("System State Queue created successfully\n");

    // EVENT GROUP - STEP 35

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

    // CREATE TASKS

    xTaskCreate(
        SensorTask,
        "SensorTask",
        4096,
        NULL,
        2,
        NULL
    );

    xTaskCreate(
        AlarmTask,
        "AlarmTask",
        3072,
        NULL,
        2,
        NULL
    );

    xTaskCreate(
        MotionTask,
        "MotionTask",
        4096,
        NULL,
        3,
        NULL
    );

    xTaskCreate(
        DisplayTask,
        "DisplayTask",
        4096,
        NULL,
        1,
        NULL
    );

    xTaskCreate(
        InputTask,
        "InputTask",
        4096,
        NULL,
        3,
        NULL
    );

    safe_printf("All FreeRTOS tasks created\n");
}