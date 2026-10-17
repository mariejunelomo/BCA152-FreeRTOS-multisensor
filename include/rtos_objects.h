#ifndef RTOS_OBJECTS_H
#define RTOS_OBJECTS_H

#include <stdio.h>
#include <stdarg.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_bit_defs.h"

// Event group bits (systemEventGroup)
#define EVENT_ACTIVE BIT0
#define EVENT_MOTION BIT1
#define EVENT_ALARM  BIT2

extern QueueHandle_t sensorQueue;
extern QueueHandle_t alarmQueue;
extern QueueHandle_t displayModeQueue;
extern QueueHandle_t motionQueue;
extern QueueHandle_t stateQueue;

extern EventGroupHandle_t systemEventGroup;
extern SemaphoreHandle_t serialMutex;
extern adc_oneshot_unit_handle_t adc_handle;

void safe_printf(const char *format, ...);

#endif