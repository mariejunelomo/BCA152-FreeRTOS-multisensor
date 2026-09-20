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

/* =========================================================
   PIN DEFINITIONS
   ========================================================= */

#define DHT_PIN             GPIO_NUM_4

#define LDR_ADC_UNIT        ADC_UNIT_1
#define LDR_ADC_CHANNEL     ADC_CHANNEL_6

#define OLED_SDA_PIN        GPIO_NUM_21
#define OLED_SCL_PIN        GPIO_NUM_22
#define OLED_I2C_PORT       I2C_NUM_0
#define OLED_ADDR           0x3C

#define OLED_WIDTH          128
#define OLED_HEIGHT         64

/* =========================================================
   SENSOR DATA
   ========================================================= */

typedef struct
{
    float temperature;
    float humidity;
    int lightLevel;
    bool motionDetected;
} SensorData;

/* =========================================================
   GLOBALS
   ========================================================= */

static QueueHandle_t sensorQueue = NULL;

static adc_oneshot_unit_handle_t adc_handle = NULL;

/* =========================================================
   OLED
   ========================================================= */

static void oled_command(uint8_t command)
{
    uint8_t data[2] = {
        0x00,
        command
    };

    i2c_master_write_to_device(
        OLED_I2C_PORT,
        OLED_ADDR,
        data,
        sizeof(data),
        pdMS_TO_TICKS(100)
    );
}

static void oled_data(uint8_t data_byte)
{
    uint8_t data[2] = {
        0x40,
        data_byte
    };

    i2c_master_write_to_device(
        OLED_I2C_PORT,
        OLED_ADDR,
        data,
        sizeof(data),
        pdMS_TO_TICKS(100)
    );
}

static void oled_init(void)
{
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = OLED_SDA_PIN,
        .scl_io_num = OLED_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000
    };

    ESP_ERROR_CHECK(
        i2c_param_config(
            OLED_I2C_PORT,
            &config
        )
    );

    ESP_ERROR_CHECK(
        i2c_driver_install(
            OLED_I2C_PORT,
            config.mode,
            0,
            0,
            0
        )
    );

    vTaskDelay(pdMS_TO_TICKS(100));

    oled_command(0xAE);
    oled_command(0x20);
    oled_command(0x00);
    oled_command(0xB0);
    oled_command(0xC8);
    oled_command(0x00);
    oled_command(0x10);
    oled_command(0x40);
    oled_command(0x81);
    oled_command(0x7F);
    oled_command(0xA1);
    oled_command(0xA6);
    oled_command(0xA8);
    oled_command(0x3F);
    oled_command(0xA4);
    oled_command(0xD3);
    oled_command(0x00);
    oled_command(0xD5);
    oled_command(0x80);
    oled_command(0xD9);
    oled_command(0xF1);
    oled_command(0xDA);
    oled_command(0x12);
    oled_command(0xDB);
    oled_command(0x40);
    oled_command(0x8D);
    oled_command(0x14);
    oled_command(0xAF);
}

static void oled_set_cursor(
    uint8_t page,
    uint8_t column
)
{
    oled_command(0xB0 + page);
    oled_command(0x00 + (column & 0x0F));
    oled_command(0x10 + ((column >> 4) & 0x0F));
}

static void oled_clear(void)
{
    for (uint8_t page = 0; page < 8; page++)
    {
        oled_set_cursor(page, 0);

        for (uint8_t column = 0;
             column < OLED_WIDTH;
             column++)
        {
            oled_data(0x00);
        }
    }
}

/* =========================================================
   OLED FONT
   ========================================================= */

static void oled_write_char(
    uint8_t page,
    uint8_t column,
    char c
)
{
    uint8_t pixels[5] = {0};

    switch (c)
    {
        /* ----------------------------
           Letters
           ---------------------------- */

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

        case 'N':
            pixels[0] = 0x7F;
            pixels[1] = 0x06;
            pixels[2] = 0x18;
            pixels[3] = 0x60;
            pixels[4] = 0x7F;
            break;

        case 'O':
            pixels[0] = 0x3E;
            pixels[1] = 0x41;
            pixels[2] = 0x41;
            pixels[3] = 0x41;
            pixels[4] = 0x3E;
            break;

        case 'P':
            pixels[0] = 0x7F;
            pixels[1] = 0x09;
            pixels[2] = 0x09;
            pixels[3] = 0x09;
            pixels[4] = 0x06;
            break;

        case 'R':
            pixels[0] = 0x7F;
            pixels[1] = 0x09;
            pixels[2] = 0x19;
            pixels[3] = 0x29;
            pixels[4] = 0x46;
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

        /* ----------------------------
           Numbers
           ---------------------------- */

        case '0':
            pixels[0] = 0x3E;
            pixels[1] = 0x51;
            pixels[2] = 0x49;
            pixels[3] = 0x45;
            pixels[4] = 0x3E;
            break;

        case '1':
            pixels[0] = 0x00;
            pixels[1] = 0x42;
            pixels[2] = 0x7F;
            pixels[3] = 0x40;
            pixels[4] = 0x00;
            break;

        case '2':
            pixels[0] = 0x42;
            pixels[1] = 0x61;
            pixels[2] = 0x51;
            pixels[3] = 0x49;
            pixels[4] = 0x46;
            break;

        case '3':
            pixels[0] = 0x21;
            pixels[1] = 0x41;
            pixels[2] = 0x45;
            pixels[3] = 0x4B;
            pixels[4] = 0x31;
            break;

        case '4':
            pixels[0] = 0x18;
            pixels[1] = 0x14;
            pixels[2] = 0x12;
            pixels[3] = 0x7F;
            pixels[4] = 0x10;
            break;

        case '5':
            pixels[0] = 0x27;
            pixels[1] = 0x45;
            pixels[2] = 0x45;
            pixels[3] = 0x45;
            pixels[4] = 0x39;
            break;

        case '6':
            pixels[0] = 0x3C;
            pixels[1] = 0x4A;
            pixels[2] = 0x49;
            pixels[3] = 0x49;
            pixels[4] = 0x30;
            break;

        case '7':
            pixels[0] = 0x01;
            pixels[1] = 0x71;
            pixels[2] = 0x09;
            pixels[3] = 0x05;
            pixels[4] = 0x03;
            break;

        case '8':
            pixels[0] = 0x36;
            pixels[1] = 0x49;
            pixels[2] = 0x49;
            pixels[3] = 0x49;
            pixels[4] = 0x36;
            break;

        case '9':
            pixels[0] = 0x06;
            pixels[1] = 0x49;
            pixels[2] = 0x49;
            pixels[3] = 0x29;
            pixels[4] = 0x1E;
            break;

        /* ----------------------------
           Symbols
           ---------------------------- */

        case '.':
            pixels[2] = 0x60;
            pixels[3] = 0x60;
            break;

        case '-':
            pixels[0] = 0x08;
            pixels[1] = 0x08;
            pixels[2] = 0x08;
            pixels[3] = 0x08;
            pixels[4] = 0x08;
            break;

        case ' ':
        default:
            break;
    }

    oled_set_cursor(page, column);

    for (int i = 0; i < 5; i++)
    {
        oled_data(pixels[i]);
    }

    oled_data(0x00);
}

static void oled_write_string(
    uint8_t page,
    uint8_t column,
    const char *text
)
{
    while (*text != '\0')
    {
        oled_write_char(
            page,
            column,
            *text
        );

        column += 6;
        text++;
    }
}

/* =========================================================
   DHT22
   ========================================================= */

static bool dht22_read(
    float *temperature,
    float *humidity
)
{
    uint8_t data[5] = {
        0, 0, 0, 0, 0
    };

    printf("DHT22: Starting read...\n");

    /*
     * Send start signal.
     * Host pulls data LOW for at least 1 ms.
     */
    gpio_set_direction(
        DHT_PIN,
        GPIO_MODE_OUTPUT
    );

    gpio_set_level(
        DHT_PIN,
        0
    );

    /*
     * Use the same timing method
     * as the functioning reference.
     */
    esp_rom_delay_us(1200);

    /*
     * Release the line.
     */
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

    /*
     * Wait for DHT22 response LOW.
     */
    int64_t start =
        esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 1)
    {
        if (
            esp_timer_get_time() - start > 100
        )
        {
            printf(
                "DHT22: Response LOW timeout\n"
            );

            return false;
        }
    }

    /*
     * Wait for DHT22 response HIGH.
     */
    start =
        esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 0)
    {
        if (
            esp_timer_get_time() - start > 100
        )
        {
            printf(
                "DHT22: Response HIGH timeout\n"
            );

            return false;
        }
    }

    /*
     * Wait for response HIGH to finish.
     */
    start =
        esp_timer_get_time();

    while (gpio_get_level(DHT_PIN) == 1)
    {
        if (
            esp_timer_get_time() - start > 100
        )
        {
            printf(
                "DHT22: Response DATA timeout\n"
            );

            return false;
        }
    }

    /*
     * Read 40 bits.
     */
    for (int i = 0; i < 40; i++)
    {
        /*
         * Wait for beginning of
         * the HIGH pulse.
         */
        start =
            esp_timer_get_time();

        while (gpio_get_level(DHT_PIN) == 0)
        {
            if (
                esp_timer_get_time() - start > 100
            )
            {
                printf(
                    "DHT22: Bit %d LOW timeout\n",
                    i
                );

                return false;
            }
        }

        /*
         * Measure HIGH pulse duration.
         */
        int64_t high_start =
            esp_timer_get_time();

        while (gpio_get_level(DHT_PIN) == 1)
        {
            if (
                esp_timer_get_time() -
                high_start > 100
            )
            {
                printf(
                    "DHT22: Bit %d HIGH timeout\n",
                    i
                );

                return false;
            }
        }

        int64_t pulse_length =
            esp_timer_get_time() -
            high_start;

        int byte_index =
            i / 8;

        int bit_index =
            7 - (i % 8);

        /*
         * Approximately:
         *
         * ~26-28 us = 0
         * ~70 us    = 1
         */
        if (pulse_length > 40)
        {
            data[byte_index] |=
                (1 << bit_index);
        }
    }

    printf(
        "DHT22 raw data: %02X %02X %02X %02X %02X\n",
        data[0],
        data[1],
        data[2],
        data[3],
        data[4]
    );

    /*
     * Check checksum.
     */
    uint8_t checksum =
        data[0] +
        data[1] +
        data[2] +
        data[3];

    if (checksum != data[4])
    {
        printf(
            "DHT22: Checksum ERROR "
            "(calculated %02X, received %02X)\n",
            checksum,
            data[4]
        );

        return false;
    }

    /*
     * Humidity.
     */
    int rawHumidity =
        ((int)data[0] << 8) |
        data[1];

    *humidity =
        rawHumidity / 10.0f;

    /*
     * Temperature.
     */
    int rawTemperature =
        ((int)data[2] << 8) |
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

    /*
     * Reject impossible all-zero readings.
     */
    if (
        *temperature == 0.0f &&
        *humidity == 0.0f
    )
    {
        printf(
            "DHT22: Invalid all-zero reading\n"
        );

        return false;
    }

    printf(
        "DHT22: Temperature = %.2f C, "
        "Humidity = %.2f %%\n",
        *temperature,
        *humidity
    );

    printf(
        "DHT22: Read SUCCESS\n"
    );

    return true;
}

/* =========================================================
   LDR
   ========================================================= */

static void ldr_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = LDR_ADC_UNIT
    };

    ESP_ERROR_CHECK(
        adc_oneshot_new_unit(
            &init_config,
            &adc_handle
        )
    );

    adc_oneshot_chan_cfg_t channel_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12
    };

    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            adc_handle,
            LDR_ADC_CHANNEL,
            &channel_config
        )
    );
}

static int read_light_level(void)
{
    int raw = 0;

    esp_err_t result =
        adc_oneshot_read(
            adc_handle,
            LDR_ADC_CHANNEL,
            &raw
        );

    if (result != ESP_OK)
    {
        printf(
            "LDR ADC read failed\n"
        );

        return 0;
    }

    int lightLevel =
        (raw * 100) / 4095;

    if (lightLevel < 0)
    {
        lightLevel = 0;
    }

    if (lightLevel > 100)
    {
        lightLevel = 100;
    }

    printf(
        "LDR Raw: %d\n",
        raw
    );

    printf(
        "Light Level: %d %%\n",
        lightLevel
    );

    return lightLevel;
}

/* =========================================================
   SENSOR TASK
   ========================================================= */

static void SensorTask(void *pvParameters)
{
    SensorData sensorData;

    TickType_t lastWakeTime =
        xTaskGetTickCount();

    while (1)
    {
        float temperature = 0.0f;
        float humidity = 0.0f;

        /*
         * DHT22
         */
        if (
            dht22_read(
                &temperature,
                &humidity
            )
        )
        {
            printf(
                "DHT22 read SUCCESS\n"
            );

            sensorData.temperature =
                temperature;

            sensorData.humidity =
                humidity;
        }
        else
        {
            printf(
                "DHT22 read FAILED\n"
            );

            sensorData.temperature =
                0.0f;

            sensorData.humidity =
                0.0f;
        }

        /*
         * LDR
         */
        sensorData.lightLevel =
            read_light_level();

        /*
         * Motion sensor will be
         * added in a later step.
         */
        sensorData.motionDetected =
            false;

        /*
         * Serial output.
         */
        printf(
            "Temperature: %.2f C\n",
            sensorData.temperature
        );

        printf(
            "Humidity: %.2f %%\n",
            sensorData.humidity
        );

        /*
         * Send sensor data to queue.
         */
        if (
            xQueueSend(
                sensorQueue,
                &sensorData,
                pdMS_TO_TICKS(100)
            ) == pdPASS
        )
        {
            printf(
                "SensorData sent to DisplayTask queue\n"
            );
        }
        else
        {
            printf(
                "Failed to send SensorData to queue\n"
            );
        }

        printf(
            "SensorTask waiting 2 sec\n"
        );

        /*
         * Periodic execution.
         * Required by the laboratory.
         */
        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(2000)
        );
    }
}

/* =========================================================
   DISPLAY TASK
   ========================================================= */

static void DisplayTask(void *pvParameters)
{
    SensorData sensorData;

    printf(
        "DisplayTask started\n"
    );

    /*
     * OLED is owned ONLY by DisplayTask.
     */
    oled_init();
    oled_clear();

    while (1)
    {
        if (
            xQueueReceive(
                sensorQueue,
                &sensorData,
                portMAX_DELAY
            ) == pdPASS
        )
        {
            oled_clear();

            /*
             * ROOM MONITOR
             */
            oled_write_string(
                0,
                28,
                "ROOM MONITOR"
            );

            /*
             * TEMPERATURE
             */
            oled_write_string(
                2,
                20,
                "TEMPERATURE"
            );

            /*
             * Temperature value.
             */
            char temperatureText[16];

            snprintf(
                temperatureText,
                sizeof(temperatureText),
                "%.1f C",
                sensorData.temperature
            );

            oled_write_string(
                4,
                34,
                temperatureText
            );

            printf(
                "DisplayTask updated OLED: "
                "Temperature = %.2f C\n",
                sensorData.temperature
            );
        }
    }
}

/* =========================================================
   APP MAIN
   ========================================================= */

void app_main(void)
{
    printf("\n");

    printf(
        "BCA152 FreeRTOS Multisensor\n"
    );

    printf(
        "System starting...\n"
    );

    /*
     * Initialize LDR.
     */
    ldr_init();

    /*
     * Configure DHT22.
     */
    gpio_set_direction(
        DHT_PIN,
        GPIO_MODE_INPUT
    );

    gpio_pullup_en(DHT_PIN);

    /*
     * Create Sensor Queue.
     */
    sensorQueue =
        xQueueCreate(
            5,
            sizeof(SensorData)
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

    /*
     * Create SensorTask.
     *
     * Priority = 2
     */
    xTaskCreate(
        SensorTask,
        "SensorTask",
        4096,
        NULL,
        2,
        NULL
    );

    /*
     * Create DisplayTask.
     *
     * Priority = 1
     */
    xTaskCreate(
        DisplayTask,
        "DisplayTask",
        4096,
        NULL,
        1,
        NULL
    );
}