
#include "rtos_objects.h"

QueueHandle_t sensorQueue = NULL;
QueueHandle_t alarmQueue = NULL;
QueueHandle_t displayModeQueue = NULL;
QueueHandle_t motionQueue = NULL;
QueueHandle_t stateQueue = NULL;

EventGroupHandle_t systemEventGroup = NULL;
SemaphoreHandle_t serialMutex = NULL;

adc_oneshot_unit_handle_t adc_handle = NULL;

void safe_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    if (serialMutex != NULL &&
        xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE)
    {
        vprintf(format, args);
        xSemaphoreGive(serialMutex);
    }
    else if (serialMutex == NULL)
    {
        vprintf(format, args);
    }

    va_end(args);
}