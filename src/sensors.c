#include "sensors.h"
#include "alarm.h"
#include "rtos_objects.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

#include <stdio.h>
#include <stdbool.h>

#define DHT_PIN             GPIO_NUM_4

#define LDR_ADC_CHANNEL     ADC_CHANNEL_6

#define EVENT_ALARM         BIT2

static bool dht_read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0};

    gpio_set_direction(DHT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT_PIN, 0);
    esp_rom_delay_us(20000);

    gpio_set_level(DHT_PIN, 1);
    esp_rom_delay_us(30);

    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT);

    int64_t start = esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 1)
    {
        if ((esp_timer_get_time() - start) > 100)
            return false;
    }

    start = esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 0)
    {
        if ((esp_timer_get_time() - start) > 100)
            return false;
    }

    start = esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 1)
    {
        if ((esp_timer_get_time() - start) > 100)
            return false;
    }

    for (int i = 0; i < 40; i++)
    {
        while (gpio_get_level(DHT_PIN) == 0)
        {
            if ((esp_timer_get_time() - start) > 1000)
                return false;
        }

        int64_t highStart = esp_timer_get_time();

        while (gpio_get_level(DHT_PIN) == 1)
        {
            if ((esp_timer_get_time() - highStart) > 100)
                return false;
        }

        int64_t highTime =
            esp_timer_get_time() - highStart;

        data[i / 8] <<= 1;

        if (highTime > 40)
            data[i / 8] |= 1;
    }

    uint8_t checksum =
        data[0] + data[1] + data[2] + data[3];

    if (checksum != data[4])
        return false;

    *humidity =
        ((data[0] << 8) | data[1]) / 10.0f;

    int16_t rawTemperature =
        (data[2] << 8) | data[3];

    if (rawTemperature & 0x8000)
    {
        rawTemperature &= 0x7FFF;
        *temperature =
            -(rawTemperature / 10.0f);
    }
    else
    {
        *temperature =
            rawTemperature / 10.0f;
    }

    return true;
}

static void ldr_init(void)
{
    adc_oneshot_chan_cfg_t config =
    {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12
    };

    adc_oneshot_config_channel(
        adc_handle,
        LDR_ADC_CHANNEL,
        &config
    );
}

static int read_light_level(void)
{
    int rawValue = 0;

    if (adc_oneshot_read(
            adc_handle,
            LDR_ADC_CHANNEL,
            &rawValue) != ESP_OK)
    {
        return 0;
    }

    int lightLevel =
        (rawValue * 100) / 4095;

    if (lightLevel < 0)
        lightLevel = 0;

    if (lightLevel > 100)
        lightLevel = 100;

    return lightLevel;
}

void SensorTask(void *pvParameters)
{
    SensorData sensorData;

    TickType_t lastWakeTime =
        xTaskGetTickCount();

    ldr_init();

    while (1)
    {
        float temperature = 0.0f;
        float humidity = 0.0f;

        bool dhtSuccess =
            dht_read(&temperature, &humidity);

        if (dhtSuccess)
        {
            sensorData.temperature =
                temperature;

            sensorData.humidity =
                humidity;

            AlarmState alarmState =
                evaluateTemperature(temperature);

            if (alarmState == HIGH_TEMPERATURE)
            {
                xEventGroupSetBits(
                    systemEventGroup,
                    EVENT_ALARM
                );
            }
            else
            {
                xEventGroupClearBits(
                    systemEventGroup,
                    EVENT_ALARM
                );
            }
        }
        else
        {
            safe_printf(
                "DHT22 read failed\r\n"
            );

            sensorData.temperature = 0.0f;
            sensorData.humidity = 0.0f;
        }

        sensorData.lightLevel =
            read_light_level();

        sensorData.motionDetected = false;

        xQueueSend(
            sensorQueue,
            &sensorData,
            0
        );

        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(2000)
        );
    }
}