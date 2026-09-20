#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

// ====================================================
// Pin definitions
// ====================================================

#define DHT_PIN GPIO_NUM_4

#define LDR_PIN GPIO_NUM_34
#define LDR_ADC_CHANNEL ADC_CHANNEL_6

// OLED I2C
#define OLED_SDA GPIO_NUM_21
#define OLED_SCL GPIO_NUM_22
#define OLED_ADDR 0x3C
#define I2C_PORT I2C_NUM_0

// ====================================================
// Sensor Data Structure
// ====================================================

struct SensorData
{
    float temperature;
    float humidity;
    int lightLevel;
    bool motionDetected;
};

// ====================================================
// Sensor Queue
// ====================================================

QueueHandle_t sensorQueue;

// ====================================================
// OLED FUNCTIONS
// ====================================================

static void oled_command(uint8_t command)
{
    uint8_t data[2];

    data[0] = 0x00;
    data[1] = command;

    i2c_master_write_to_device(
        I2C_PORT,
        OLED_ADDR,
        data,
        sizeof(data),
        pdMS_TO_TICKS(100)
    );
}

static void oled_data(const uint8_t *data, size_t length)
{
    uint8_t buffer[129];

    if (length > 128)
    {
        return;
    }

    buffer[0] = 0x40;

    for (size_t i = 0; i < length; i++)
    {
        buffer[i + 1] = data[i];
    }

    i2c_master_write_to_device(
        I2C_PORT,
        OLED_ADDR,
        buffer,
        length + 1,
        pdMS_TO_TICKS(100)
    );
}

static void oled_init(void)
{
    i2c_config_t config = {0};

    config.mode = I2C_MODE_MASTER;
    config.sda_io_num = OLED_SDA;
    config.scl_io_num = OLED_SCL;
    config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    config.scl_pullup_en = GPIO_PULLUP_ENABLE;
    config.master.clk_speed = 400000;

    i2c_param_config(I2C_PORT, &config);

    i2c_driver_install(
        I2C_PORT,
        config.mode,
        0,
        0,
        0
    );

    vTaskDelay(pdMS_TO_TICKS(100));

    // SSD1306 initialization
    oled_command(0xAE);
    oled_command(0xD5);
    oled_command(0x80);
    oled_command(0xA8);
    oled_command(0x3F);
    oled_command(0xD3);
    oled_command(0x00);
    oled_command(0x40);
    oled_command(0x8D);
    oled_command(0x14);
    oled_command(0x20);
    oled_command(0x00);
    oled_command(0xA1);
    oled_command(0xC8);
    oled_command(0xDA);
    oled_command(0x12);
    oled_command(0x81);
    oled_command(0xCF);
    oled_command(0xD9);
    oled_command(0xF1);
    oled_command(0xDB);
    oled_command(0x40);
    oled_command(0xA4);
    oled_command(0xA6);
    oled_command(0xAF);
}

static void oled_clear(void)
{
    uint8_t blank[128] = {0};

    for (int page = 0; page < 8; page++)
    {
        oled_command(0xB0 + page);
        oled_command(0x00);
        oled_command(0x10);
        oled_data(blank, 128);
    }
}

static void oled_set_cursor(int x, int page)
{
    oled_command(0xB0 + page);
    oled_command(0x00 + (x & 0x0F));
    oled_command(0x10 + ((x >> 4) & 0x0F));
}

// ====================================================
// 5x7 FONT
// ====================================================

static const uint8_t fontDigits[][5] =
{
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}  // 9
};

static void oled_char(int x, int page, char c)
{
    uint8_t pixels[6] = {0, 0, 0, 0, 0, 0};

    if (c >= '0' && c <= '9')
    {
        const uint8_t *font = fontDigits[c - '0'];

        for (int i = 0; i < 5; i++)
        {
            pixels[i] = font[i];
        }
    }
    else
    {
        switch (c)
        {
            case 'A':
                pixels[0] = 0x7E;
                pixels[1] = 0x09;
                pixels[2] = 0x09;
                pixels[3] = 0x09;
                pixels[4] = 0x7E;
                break;

            case 'C':
                pixels[0] = 0x3E;
                pixels[1] = 0x41;
                pixels[2] = 0x41;
                pixels[3] = 0x41;
                pixels[4] = 0x22;
                break;

            case 'E':
                pixels[0] = 0x7F;
                pixels[1] = 0x49;
                pixels[2] = 0x49;
                pixels[3] = 0x49;
                pixels[4] = 0x41;
                break;

            case 'G':
                pixels[0] = 0x3E;
                pixels[1] = 0x41;
                pixels[2] = 0x49;
                pixels[3] = 0x49;
                pixels[4] = 0x7A;
                break;

            case 'H':
                pixels[0] = 0x7F;
                pixels[1] = 0x08;
                pixels[2] = 0x08;
                pixels[3] = 0x08;
                pixels[4] = 0x7F;
                break;

            case 'I':
                pixels[0] = 0x00;
                pixels[1] = 0x41;
                pixels[2] = 0x7F;
                pixels[3] = 0x41;
                pixels[4] = 0x00;
                break;

            case 'L':
                pixels[0] = 0x7F;
                pixels[1] = 0x40;
                pixels[2] = 0x40;
                pixels[3] = 0x40;
                pixels[4] = 0x40;
                break;

            case 'M':
                pixels[0] = 0x7F;
                pixels[1] = 0x06;
                pixels[2] = 0x18;
                pixels[3] = 0x06;
                pixels[4] = 0x7F;
                break;

            case 'P':
                pixels[0] = 0x7F;
                pixels[1] = 0x09;
                pixels[2] = 0x09;
                pixels[3] = 0x09;
                pixels[4] = 0x06;
                break;

            case 'T':
                pixels[0] = 0x01;
                pixels[1] = 0x01;
                pixels[2] = 0x7F;
                pixels[3] = 0x01;
                pixels[4] = 0x01;
                break;

            case 'U':
                pixels[0] = 0x3F;
                pixels[1] = 0x40;
                pixels[2] = 0x40;
                pixels[3] = 0x40;
                pixels[4] = 0x3F;
                break;

            case '-':
                pixels[0] = 0x08;
                pixels[1] = 0x08;
                pixels[2] = 0x08;
                pixels[3] = 0x08;
                pixels[4] = 0x08;
                break;

            case ' ':
                break;

            default:
                break;
        }
    }

    oled_set_cursor(x, page);
    oled_data(pixels, 6);
}

// ====================================================
// Draw integer number
// ====================================================

static void oled_number(int x, int page, int number)
{
    if (number < 0)
    {
        oled_char(x, page, '-');
        number = -number;
        x += 6;
    }

    if (number >= 100)
    {
        oled_char(
            x,
            page,
            '0' + (number / 100)
        );

        number %= 100;
        x += 6;
    }

    if (number >= 10)
    {
        oled_char(
            x,
            page,
            '0' + (number / 10)
        );

        number %= 10;
        x += 6;
    }

    oled_char(
        x,
        page,
        '0' + number
    );
}

// ====================================================
// DHT22
// ====================================================

static bool dht22_read(
    float *temperature,
    float *humidity
)
{
    uint8_t data[5] = {
        0,
        0,
        0,
        0,
        0
    };

    gpio_set_direction(
        DHT_PIN,
        GPIO_MODE_OUTPUT
    );

    gpio_set_level(
        DHT_PIN,
        0
    );

    esp_rom_delay_us(1200);

    gpio_set_level(
        DHT_PIN,
        1
    );

    esp_rom_delay_us(30);

    gpio_set_direction(
        DHT_PIN,
        GPIO_MODE_INPUT
    );

    gpio_pullup_en(DHT_PIN);

    int64_t start =
        esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 1)
    {
        if (esp_timer_get_time() - start > 100)
        {
            return false;
        }
    }

    start = esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 0)
    {
        if (esp_timer_get_time() - start > 100)
        {
            return false;
        }
    }

    start = esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 1)
    {
        if (esp_timer_get_time() - start > 100)
        {
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
                return false;
            }
        }

        int64_t high_start =
            esp_timer_get_time();

        while (gpio_get_level(DHT_PIN) == 1)
        {
            if (esp_timer_get_time() - high_start > 100)
            {
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

    uint8_t checksum =
        data[0] +
        data[1] +
        data[2] +
        data[3];

    if (checksum != data[4])
    {
        return false;
    }

    *humidity =
        ((data[0] << 8) | data[1]) / 10.0f;

    int16_t raw_temperature =
        (data[2] << 8) | data[3];

    if (raw_temperature & 0x8000)
    {
        raw_temperature &= 0x7FFF;

        *temperature =
            -(raw_temperature / 10.0f);
    }
    else
    {
        *temperature =
            raw_temperature / 10.0f;
    }

    return true;
}

// ====================================================
// SensorTask
// ====================================================

void sensorTask(void *parameter)
{
    struct SensorData sensorData;

    TickType_t lastWakeTime =
        xTaskGetTickCount();

    // ADC setup
    adc_oneshot_unit_handle_t adc_handle;

    adc_oneshot_unit_init_cfg_t adc_init_config = {
        .unit_id = ADC_UNIT_1,
    };

    esp_err_t adc_result =
        adc_oneshot_new_unit(
            &adc_init_config,
            &adc_handle
        );

    if (adc_result != ESP_OK)
    {
        printf(
            "LDR ADC initialization failed\n"
        );

        vTaskDelete(NULL);
        return;
    }

    adc_oneshot_chan_cfg_t adc_channel_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };

    adc_result =
        adc_oneshot_config_channel(
            adc_handle,
            LDR_ADC_CHANNEL,
            &adc_channel_config
        );

    if (adc_result != ESP_OK)
    {
        printf(
            "LDR ADC channel configuration failed\n"
        );

        vTaskDelete(NULL);
        return;
    }

    while (1)
    {
        // --------------------------------------------
        // DHT22
        // --------------------------------------------

        if (dht22_read(
                &sensorData.temperature,
                &sensorData.humidity))
        {
            printf(
                "Temperature: %.2f C\n",
                sensorData.temperature
            );

            printf(
                "Humidity: %.2f %%\n",
                sensorData.humidity
            );
        }
        else
        {
            printf(
                "DHT22 reading failed\n"
            );

            sensorData.temperature = 0.0f;
            sensorData.humidity = 0.0f;
        }

        // --------------------------------------------
        // LDR
        // --------------------------------------------

        int ldr_raw = 0;

        adc_result =
            adc_oneshot_read(
                adc_handle,
                LDR_ADC_CHANNEL,
                &ldr_raw
            );

        if (adc_result == ESP_OK)
        {
            sensorData.lightLevel =
                (ldr_raw * 100) / 4095;

            printf(
                "LDR Raw: %d\n",
                ldr_raw
            );

            printf(
                "Light Level: %d %%\n",
                sensorData.lightLevel
            );
        }
        else
        {
            printf(
                "LDR reading failed\n"
            );

            sensorData.lightLevel = 0;
        }

        // PIR will be added later
        sensorData.motionDetected = false;

        // --------------------------------------------
        // Send data to DisplayTask
        // --------------------------------------------

        if (xQueueSend(
                sensorQueue,
                &sensorData,
                portMAX_DELAY
            ) != pdPASS)
        {
            printf(
                "Failed to send SensorData to queue\n"
            );
        }
        else
        {
            printf(
                "SensorData sent to DisplayTask queue\n"
            );
        }

        printf(
            "SensorTask waiting 2 sec\n"
        );

        // Periodic execution
        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(2000)
        );
    }
}

// ====================================================
// DisplayTask
// OLED is owned ONLY by DisplayTask
// ====================================================

void displayTask(void *parameter)
{
    struct SensorData sensorData;

    oled_init();

    oled_clear();

    printf(
        "DisplayTask started\n"
    );

    while (1)
    {
        if (xQueueReceive(
                sensorQueue,
                &sensorData,
                portMAX_DELAY
            ) == pdPASS)
        {
            oled_clear();

            // ----------------------------------------
            // Temperature
            // ----------------------------------------

            oled_char(0, 0, 'T');
            oled_char(6, 0, 'E');
            oled_char(12, 0, 'M');
            oled_char(18, 0, 'P');

            int temperature =
                (int)sensorData.temperature;

            oled_number(
                36,
                0,
                temperature
            );

            // ----------------------------------------
            // Humidity
            // ----------------------------------------

            oled_char(0, 2, 'H');
            oled_char(6, 2, 'U');
            oled_char(12, 2, 'M');

            int humidity =
                (int)sensorData.humidity;

            oled_number(
                30,
                2,
                humidity
            );

            // ----------------------------------------
            // Light
            // ----------------------------------------

            oled_char(0, 4, 'L');
            oled_char(6, 4, 'I');
            oled_char(12, 4, 'G');
            oled_char(18, 4, 'H');
            oled_char(24, 4, 'T');

            oled_number(
                42,
                4,
                sensorData.lightLevel
            );

            printf(
                "DisplayTask updated OLED: "
                "T=%.2f C, H=%.2f %%, Light=%d %%\n",
                sensorData.temperature,
                sensorData.humidity,
                sensorData.lightLevel
            );
        }
    }
}

// ====================================================
// Main
// ====================================================

void app_main(void)
{
    printf("\n");

    printf(
        "BCA152 FreeRTOS Multisensor\n"
    );

    printf(
        "System starting...\n"
    );

    // Configure DHT22
    gpio_set_direction(
        DHT_PIN,
        GPIO_MODE_INPUT
    );

    gpio_pullup_en(DHT_PIN);

    // Create Sensor Queue
    sensorQueue =
        xQueueCreate(
            10,
            sizeof(struct SensorData)
        );

    if (sensorQueue == NULL)
    {
        printf(
            "Failed to create Sensor Queue\n"
        );

        return;
    }

    printf(
        "Sensor Queue created successfully\n"
    );

    // Create SensorTask
    xTaskCreate(
        sensorTask,
        "SensorTask",
        4096,
        NULL,
        2,
        NULL
    );

    // Create DisplayTask
    xTaskCreate(
        displayTask,
        "DisplayTask",
        4096,
        NULL,
        1,
        NULL
    );
}