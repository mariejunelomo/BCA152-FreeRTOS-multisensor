#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>

const char* TAG = "MAIN";

void taskA(void *pvParameters)
{
    while (1)
    {
        printf("Task A running\n");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void taskB(void *pvParameters)
{
    while (1)
    {
        printf("Task B running\n");

        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "BCA152 FreeRTOS Multisensor");
    ESP_LOGI(TAG, "System starting...");

    xTaskCreate(
        taskA,
        "Task A",
        2048,
        NULL,
        1,
        NULL
    );

    xTaskCreate(
        taskB,
        "Task B",
        2048,
        NULL,
        1,
        NULL
    );
}