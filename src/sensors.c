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

#define LDR_ADC_UNIT        ADC_UNIT_1

#define LDR_ADC_CHANNEL     ADC_CHANNEL_6

#define EVENT_ALARM         BIT2

static bool dht_read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0, 0, 0, 0, 0};

    safe_printf("DHT22: Starting read...\n");

    gpio_set_direction(
        DHT_PIN,
        GPIO_MODE_OUTPUT
    );

    gpio_set_level(DHT_PIN, 0);
    esp_rom_delay_us(1200);

    gpio_set_level(DHT_PIN, 1);
    esp_rom_delay_us(30);

    gpio_set_direction(
        DHT_PIN,
        GPIO_MODE_INPUT
    );

    gpio_pullup_en(DHT_PIN);

    int64_t start = esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 1)
    {
        if (esp_timer_get_time() - start > 100)
        {
            safe_printf(
                "DHT22: Response LOW timeout\n"
            );
            return false;
        }
    }

    start = esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 0)
    {
        if (esp_timer_get_time() - start > 100)
        {
            safe_printf(
                "DHT22: Response HIGH timeout\n"
            );
            return false;
        }
    }

    start = esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 1)
    {
        if (esp_timer_get_time() - start > 100)
        {
            safe_printf(
                "DHT22: Response DATA timeout\n"
            );
            return false;
        }
    }

    for (int i = 0; i < 40; i++)
    {
        start = esp_timer_get_time();

        while (gpio_get_level(DHT_PIN) == 0)
        {
            if (esp_timer_get_time() - start > 100)
            {
                safe_printf(
                    "DHT22: Bit %d LOW timeout\n",
                    i
                );
                return false;
            }
        }

        int64_t high_start =
            esp_timer_get_time();

        while (gpio_get_level(DHT_PIN) == 1)
        {
            if (esp_timer_get_time() - high_start > 100)
            {
                safe_printf(
                    "DHT22: Bit %d HIGH timeout\n",
                    i
                );
                return false;
            }
        }

        int64_t pulse_length =
            esp_timer_get_time() - high_start;

        int byte_index = i / 8;
        int bit_index = 7 - (i % 8);

        if (pulse_length > 40)
        {
            data[byte_index] |=
                (1 << bit_index);
        }
    }

    safe_printf(
        "DHT22 raw data: "
        "%02X %02X %02X %02X %02X\n",
        data[0],
        data[1],
        data[2],
        data[3],
        data[4]
    );

    uint8_t checksum =
        data[0] +
        data[1] +
        data[2] +
        data[3];

    if (checksum != data[4])
    {
        safe_printf(
            "DHT22: Checksum ERROR "
            "(calculated %02X, received %02X)\n",
            checksum,
            data[4]
        );

        return false;
    }

    int rawHumidity =
        (data[0] << 8) |
        data[1];

    *humidity =
        rawHumidity / 10.0f;

    int rawTemperature =
        (data[2] << 8) |
        data[3];

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

    if (
        *temperature == 0.0f &&
        *humidity == 0.0f
    )
    {
        safe_printf(
            "DHT22: Invalid all-zero reading\n"
        );

        return false;
    }

    safe_printf(
        "DHT22: Temperature = %.2f C, "
        "Humidity = %.2f %%\n",
        *temperature,
        *humidity
    );

    safe_printf(
        "DHT22: Read SUCCESS\n"
    );

    return true;
}
static void ldr_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config =
    {
        .unit_id = LDR_ADC_UNIT
    };

    ESP_ERROR_CHECK(
        adc_oneshot_new_unit(
            &init_config,
            &adc_handle
        )
    );

    adc_oneshot_chan_cfg_t config =
    {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12
    };

    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            adc_handle,
            LDR_ADC_CHANNEL,
            &config
        )
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