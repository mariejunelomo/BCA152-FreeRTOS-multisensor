#include "sensors.h"
#include "alarm.h"
#include "rtos_objects.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_err.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define DHT_PIN             GPIO_NUM_4

#define LDR_ADC_UNIT        ADC_UNIT_1
#define LDR_ADC_CHANNEL     ADC_CHANNEL_6

#define LDR_RAW_BRIGHT      32
#define LDR_RAW_DARK        4095

#define EVENT_ALARM         BIT2


/* =========================================
   DHT22 SENSOR READING
   ========================================= */

static bool dht_read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0};

    safe_printf("DHT22: Starting read...\n");

    gpio_set_direction(DHT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT_PIN, 0);
    esp_rom_delay_us(1200);

    gpio_set_level(DHT_PIN, 1);
    esp_rom_delay_us(30);

    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT);
    gpio_pullup_en(DHT_PIN);

    int64_t start = esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 1)
    {
        if (esp_timer_get_time() - start > 100)
        {
            safe_printf("DHT22: Response LOW timeout\n");
            return false;
        }
    }

    start = esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 0)
    {
        if (esp_timer_get_time() - start > 100)
        {
            safe_printf("DHT22: Response HIGH timeout\n");
            return false;
        }
    }

    start = esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 1)
    {
        if (esp_timer_get_time() - start > 100)
        {
            safe_printf("DHT22: Response DATA timeout\n");
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
                safe_printf("DHT22: Bit %d LOW timeout\n", i);
                return false;
            }
        }

        int64_t high_start = esp_timer_get_time();

        while (gpio_get_level(DHT_PIN) == 1)
        {
            if (esp_timer_get_time() - high_start > 100)
            {
                safe_printf("DHT22: Bit %d HIGH timeout\n", i);
                return false;
            }
        }

        int64_t pulse_length =
            esp_timer_get_time() - high_start;

        int byte_index = i / 8;
        int bit_index = 7 - (i % 8);

        if (pulse_length > 40)
        {
            data[byte_index] |= (1 << bit_index);
        }
    }

    safe_printf(
        "DHT22 raw data: %02X %02X %02X %02X %02X\n",
        data[0], data[1], data[2], data[3], data[4]
    );

    uint8_t checksum =
        data[0] + data[1] + data[2] + data[3];

    if (checksum != data[4])
    {
        safe_printf(
            "DHT22: Checksum ERROR (calculated %02X, received %02X)\n",
            checksum, data[4]
        );
        return false;
    }

    int rawHumidity = (data[0] << 8) | data[1];
    *humidity = rawHumidity / 10.0f;

    int rawTemperature = (data[2] << 8) | data[3];

    if (rawTemperature & 0x8000)
    {
        rawTemperature &= 0x7FFF;
        *temperature = -(rawTemperature / 10.0f);
    }
    else
    {
        *temperature = rawTemperature / 10.0f;
    }

    if (*temperature == 0.0f && *humidity == 0.0f)
    {
        safe_printf("DHT22: Invalid all-zero reading\n");
        return false;
    }

    safe_printf(
        "DHT22: Temperature = %.2f C, Humidity = %.2f %%\n",
        *temperature, *humidity
    );

    safe_printf("DHT22: Read SUCCESS\n");

    return true;
}


/* =========================================
   LDR INITIALIZATION
   ========================================= */

static void ldr_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {};
    init_config.unit_id = LDR_ADC_UNIT;

    ESP_ERROR_CHECK(
        adc_oneshot_new_unit(&init_config, &adc_handle)
    );

    adc_oneshot_chan_cfg_t config = {};
    config.bitwidth = ADC_BITWIDTH_DEFAULT;
    config.atten = ADC_ATTEN_DB_12;

    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            adc_handle,
            LDR_ADC_CHANNEL,
            &config
        )
    );

    safe_printf("LDR: ADC initialized successfully\n");
}


/* =========================================
   READ LDR BRIGHTNESS
   ========================================= */

static int read_light_level(void)
{
    int rawValue = 0;

    printf("LDR: read_light_level() called\n");
    fflush(stdout);

    esp_err_t result = adc_oneshot_read(
        adc_handle,
        LDR_ADC_CHANNEL,
        &rawValue
    );

    if (result != ESP_OK)
    {
        printf(
            "LDR: ADC read failed, error = %s\n",
            esp_err_to_name(result)
        );
        fflush(stdout);
        return 0;
    }

    printf("LDR: Raw ADC = %d\n", rawValue);
    fflush(stdout);

    int lightLevel =
        ((LDR_RAW_DARK - rawValue) * 100) /
        (LDR_RAW_DARK - LDR_RAW_BRIGHT);

    if (rawValue <= LDR_RAW_BRIGHT + 8)
    {
        lightLevel = 100;
    }

    if (lightLevel < 0)
        lightLevel = 0;

    if (lightLevel > 100)
        lightLevel = 100;

    printf("LDR: Brightness = %d%%\n", lightLevel);
    fflush(stdout);

    return lightLevel;
}


/* =========================================
   FREERTOS SENSOR TASK
   ========================================= */


void SensorTask(void *pvParameters)
{
    (void)pvParameters;

    safe_printf("SensorTask started\n");

    SensorData sensorData = {};
    TickType_t lastWakeTime = xTaskGetTickCount();

    ldr_init();

    while (1)
    {
        float temperature = 0.0f;
        float humidity = 0.0f;

        bool dhtSuccess = dht_read(&temperature, &humidity);

        sensorData.temperatureValid = dhtSuccess;

        if (dhtSuccess)
        {
            sensorData.temperature = temperature;
            sensorData.humidity = humidity;
        }
        else
        {
            safe_printf("SensorTask: DHT22 read failed\n");

            sensorData.temperature = 0.0f;
            sensorData.humidity = 0.0f;
        }

        sensorData.lightLevel = read_light_level();
        sensorData.motionDetected = false;

        // Send a copy to the display's queue.
        if (xQueueSend(sensorQueue, &sensorData, 0) != pdPASS)
        {
            safe_printf("SensorTask: sensorQueue full\n");
        }

        // Send another copy to the alarm's separate queue.
        if (xQueueSend(alarmQueue, &sensorData, 0) != pdPASS)
        {
            safe_printf("SensorTask: alarmQueue full\n");
        }

        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(2000)
        );
    }
}