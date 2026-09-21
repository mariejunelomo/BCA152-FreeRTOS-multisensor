#include "rtos_objects.h"

QueueHandle_t sensorQueue = NULL;
QueueHandle_t displayModeQueue = NULL;
QueueHandle_t motionQueue = NULL;
QueueHandle_t stateQueue = NULL;

EventGroupHandle_t systemEventGroup = NULL;

SemaphoreHandle_t serialMutex = NULL;

void safe_printf(const char *format, ...)
{
    va_list args;

    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE)
    {
        va_start(args, format);
        vprintf(format, args);
        va_end(args);

        xSemaphoreGive(serialMutex);
    }
}