#include "alarm.h"
#include "sensors.h"
#include "rtos_objects.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"

#define BUZZER_PIN GPIO_NUM_26
#define EVENT_ALARM BIT2

AlarmState evaluateTemperature(float temperature)
{
    if (temperature < 18.0f)
    {
        return LOW_TEMPERATURE;
    }

    if (temperature > 30.0f)
    {
        return HIGH_TEMPERATURE;
    }

    return NORMAL;
}

void AlarmTask(void *pvParameters)
{
    (void)pvParameters;

    gpio_reset_pin(BUZZER_PIN);
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BUZZER_PIN, 0);

    safe_printf("AlarmTask started. Buzzer GPIO: 26\n");

    SensorData data;

    while (1)
    {
        if (xQueueReceive(alarmQueue, &data, portMAX_DELAY) == pdPASS)
        {
            // Don't alarm based on a failed DHT22 reading.
            if (!data.temperatureValid)
            {
                gpio_set_level(BUZZER_PIN, 0);
                xEventGroupClearBits(systemEventGroup, EVENT_ALARM);

                safe_printf("AlarmTask: Invalid temperature reading; buzzer OFF\n");
                continue;
            }

            AlarmState alarmState = evaluateTemperature(data.temperature);

            if (alarmState == LOW_TEMPERATURE)
            {
                gpio_set_level(BUZZER_PIN, 1);
                xEventGroupSetBits(systemEventGroup, EVENT_ALARM);

                safe_printf(
                    "ALARM: Low temperature %.1f C! Buzzer ON\n",
                    data.temperature
                );
            }
            else if (alarmState == HIGH_TEMPERATURE)
            {
                gpio_set_level(BUZZER_PIN, 1);
                xEventGroupSetBits(systemEventGroup, EVENT_ALARM);

                safe_printf(
                    "ALARM: High temperature %.1f C! Buzzer ON\n",
                    data.temperature
                );
            }
            else
            {
                gpio_set_level(BUZZER_PIN, 0);
                xEventGroupClearBits(systemEventGroup, EVENT_ALARM);

                safe_printf(
                    "Temperature %.1f C is normal. Buzzer OFF\n",
                    data.temperature
                );
            }
        }
    }
}