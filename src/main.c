#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

/* =========================================================
   PIN DEFINITIONS
   ========================================================= */

#define DHT_PIN             GPIO_NUM_4
#define PIR_PIN             GPIO_NUM_27

#define LDR_ADC_UNIT        ADC_UNIT_1
#define LDR_ADC_CHANNEL     ADC_CHANNEL_6

#define OLED_SDA_PIN        GPIO_NUM_21
#define OLED_SCL_PIN        GPIO_NUM_22
#define OLED_I2C_PORT       I2C_NUM_0
#define OLED_ADDR           0x3C
#define OLED_WIDTH          128
#define OLED_HEIGHT         64

#define ENCODER_CLK_PIN     GPIO_NUM_32
#define ENCODER_DT_PIN      GPIO_NUM_33
#define ENCODER_SW_PIN      GPIO_NUM_25

/* =========================================================
   STEP 35 - EVENT GROUP
   ========================================================= */

#define EVENT_ACTIVE  BIT0
#define EVENT_MOTION  BIT1
#define EVENT_ALARM   BIT2

/*
 * EVENT_ACTIVE
 * Producer : MotionTask
 * Consumer : DisplayTask
 * Set      : When system becomes ACTIVE
 * Cleared  : When system becomes INACTIVE
 *
 * EVENT_MOTION
 * Producer : MotionTask
 * Consumer : DisplayTask
 * Set      : When PIR detects motion
 * Cleared  : When no motion is detected
 *
 * EVENT_ALARM
 * Producer : SensorTask
 * Consumer : DisplayTask
 * Set      : When temperature is below 18 C or above 30 C
 * Cleared  : When temperature returns to normal range
 */

/* =========================================================
   DISPLAY MODE
   ========================================================= */

typedef enum
{
    TEMPERATURE,
    HUMIDITY,
    LIGHT,
    MOTION
} DisplayMode;

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
   ALARM STATE / TEMPERATURE DECISION LOGIC
   ========================================================= */

typedef enum
{
    NORMAL,
    LOW_TEMPERATURE,
    HIGH_TEMPERATURE
} AlarmState;

/*
 * Below 18 C  -> LOW_TEMPERATURE
 * 18 to 30 C  -> NORMAL
 * Above 30 C  -> HIGH_TEMPERATURE
 */

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

/* =========================================================
   SYSTEM STATE
   ========================================================= */

typedef enum
{
    ACTIVE,
    INACTIVE
} SystemState;

/*
 * ACTIVE -> INACTIVE after inactivity timeout.
 * INACTIVE -> ACTIVE when motion is detected.
 */

SystemState evaluateSystemState(
    SystemState currentState,
    bool motionDetected,
    bool inactivityTimeout
)
{
    if (currentState == ACTIVE && inactivityTimeout)
    {
        return INACTIVE;
    }

    if (currentState == INACTIVE && motionDetected)
    {
        return ACTIVE;
    }

    return currentState;
}

/* =========================================================
   GLOBALS
   ========================================================= */

static QueueHandle_t sensorQueue = NULL;
static QueueHandle_t displayModeQueue = NULL;
static QueueHandle_t motionQueue = NULL;
static QueueHandle_t stateQueue = NULL;

static EventGroupHandle_t systemEventGroup = NULL;

static adc_oneshot_unit_handle_t adc_handle = NULL;

/* =========================================================
   OLED
   ========================================================= */

static void oled_command(uint8_t command)
{
    uint8_t data[2] =
    {
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
    uint8_t data[2] =
    {
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
    i2c_config_t config =
    {
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

static void oled_set_power(bool enabled)
{
    if (enabled)
    {
        oled_command(0xAF);
    }
    else
    {
        oled_command(0xAE);
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

        case 'D':
            pixels[0] = 0x7F;
            pixels[1] = 0x41;
            pixels[2] = 0x41;
            pixels[3] = 0x22;
            pixels[4] = 0x1C;
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

        case 'Y':
            pixels[0] = 0x03;
            pixels[1] = 0x0C;
            pixels[2] = 0x70;
            pixels[3] = 0x0C;
            pixels[4] = 0x03;
            break;

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

        case '%':
            pixels[0] = 0x63;
            pixels[1] = 0x13;
            pixels[2] = 0x08;
            pixels[3] = 0x64;
            pixels[4] = 0x63;
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
    uint8_t data[5] = {0, 0, 0, 0, 0};

    printf("DHT22: Starting read...\n");

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

    /* Wait for DHT22 response LOW */

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

    /* Wait for DHT22 response HIGH */

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

    /* Wait for response HIGH to finish */

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

    /* Read 40 bits */

    for (int i = 0; i < 40; i++)
    {
        /* Wait for beginning of HIGH pulse */

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

        /* Measure HIGH pulse */

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

    /* Check checksum */

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

    /* Humidity */

    int rawHumidity =
        ((int)data[0] << 8) |
        data[1];

    *humidity =
        rawHumidity / 10.0f;

    /* Temperature */

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

    /* Reject impossible all-zero readings */

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

    adc_oneshot_chan_cfg_t channel_config =
    {
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

        bool dhtSuccess =
            dht22_read(
                &temperature,
                &humidity
            );

        /* DHT22 */

        if (dhtSuccess)
        {
            printf(
                "DHT22 read SUCCESS\n"
            );

            sensorData.temperature =
                temperature;

            sensorData.humidity =
                humidity;

            /*
             * STEP 35:
             * Generate EVENT_ALARM based on
             * the latest valid temperature.
             */

            AlarmState alarmState =
                evaluateTemperature(
                    temperature
                );

            if (alarmState != NORMAL)
            {
                xEventGroupSetBits(
                    systemEventGroup,
                    EVENT_ALARM
                );

                printf(
                    "EVENT_ALARM: SET\n"
                );
            }
            else
            {
                xEventGroupClearBits(
                    systemEventGroup,
                    EVENT_ALARM
                );

                printf(
                    "EVENT_ALARM: CLEARED\n"
                );
            }
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

            /*
             * Do not keep an old alarm active
             * when a new temperature reading
             * was not obtained.
             */

            xEventGroupClearBits(
                systemEventGroup,
                EVENT_ALARM
            );
        }

        /* LDR */

        sensorData.lightLevel =
            read_light_level();

        /*
         * Motion is owned by MotionTask.
         * DisplayTask gets the latest motion
         * state from motionQueue.
         */

        sensorData.motionDetected =
            false;

        printf(
            "Temperature: %.2f C\n",
            sensorData.temperature
        );

        printf(
            "Humidity: %.2f %%\n",
            sensorData.humidity
        );

        /* Send sensor data */

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
         *
         * This satisfies the laboratory requirement
         * to use vTaskDelayUntil().
         */

        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(2000)
        );
    }
}

/* =========================================================
   MOTION TASK
   STEP 31 + STEP 32/34 + STEP 35
   ========================================================= */

static void MotionTask(void *pvParameters)
{
    bool motionDetected = false;
    bool timeoutReported = false;

    SystemState currentState =
        ACTIVE;

    TickType_t lastMotionTime =
        xTaskGetTickCount();

    const TickType_t inactivityTimeout =
        pdMS_TO_TICKS(15000);

    printf(
        "MotionTask started\n"
    );

    /*
     * Publish initial ACTIVE state.
     */

    xQueueOverwrite(
        stateQueue,
        &currentState
    );

    /*
     * STEP 35:
     * Initial system state is ACTIVE.
     */

    xEventGroupSetBits(
        systemEventGroup,
        EVENT_ACTIVE
    );

    printf(
        "EVENT_ACTIVE: SET\n"
    );

    while (1)
    {
        int pirLevel =
            gpio_get_level(PIR_PIN);

        /*
         * PIR HIGH means motion detected.
         */

        if (pirLevel == 1)
        {
            if (!motionDetected)
            {
                printf(
                    "MotionTask: MOTION DETECTED\n"
                );

                /*
                 * STEP 35:
                 * Motion event is produced
                 * by MotionTask.
                 */

                xEventGroupSetBits(
                    systemEventGroup,
                    EVENT_MOTION
                );

                printf(
                    "EVENT_MOTION: SET\n"
                );
            }

            motionDetected = true;

            /*
             * Allow another inactivity timeout
             * after motion stops.
             */

            timeoutReported = false;

            /*
             * Reset inactivity timer.
             */

            lastMotionTime =
                xTaskGetTickCount();

            /*
             * If system is INACTIVE,
             * motion changes it back to ACTIVE.
             */

            if (currentState == INACTIVE)
            {
                SystemState previousState =
                    currentState;

                currentState =
                    evaluateSystemState(
                        currentState,
                        true,
                        false
                    );

                if (currentState != previousState)
                {
                    printf(
                        "SystemState: "
                        "INACTIVE -> ACTIVE\n"
                    );

                    xQueueOverwrite(
                        stateQueue,
                        &currentState
                    );

                    /*
                     * STEP 35:
                     * System became ACTIVE.
                     */

                    xEventGroupSetBits(
                        systemEventGroup,
                        EVENT_ACTIVE
                    );

                    printf(
                        "EVENT_ACTIVE: SET\n"
                    );
                }
            }
        }
        else
        {
            if (motionDetected)
            {
                printf(
                    "MotionTask: NO MOTION\n"
                );

                /*
                 * STEP 35:
                 * Clear motion event when
                 * motion is no longer detected.
                 */

                xEventGroupClearBits(
                    systemEventGroup,
                    EVENT_MOTION
                );

                printf(
                    "EVENT_MOTION: CLEARED\n"
                );
            }

            motionDetected = false;

            /*
             * Check whether 15 seconds have passed
             * without motion.
             */

            bool inactivityTimeoutReached =
                (
                    !timeoutReported &&
                    (
                        xTaskGetTickCount() -
                        lastMotionTime
                    ) >= inactivityTimeout
                );

            if (inactivityTimeoutReached)
            {
                timeoutReported = true;

                printf(
                    "MotionTask: "
                    "NO MOTION - "
                    "15 SECOND TIMEOUT\n"
                );

                /*
                 * ACTIVE -> INACTIVE
                 */

                if (currentState == ACTIVE)
                {
                    SystemState previousState =
                        currentState;

                    currentState =
                        evaluateSystemState(
                            currentState,
                            false,
                            true
                        );

                    if (currentState != previousState)
                    {
                        printf(
                            "SystemState: "
                            "ACTIVE -> INACTIVE\n"
                        );

                        xQueueOverwrite(
                            stateQueue,
                            &currentState
                        );

                        /*
                         * STEP 35:
                         * System is no longer ACTIVE.
                         */

                        xEventGroupClearBits(
                            systemEventGroup,
                            EVENT_ACTIVE
                        );

                        printf(
                            "EVENT_ACTIVE: CLEARED\n"
                        );
                    }
                }
            }
        }

        /*
         * Keep latest motion state available
         * to DisplayTask.
         */

        xQueueOverwrite(
            motionQueue,
            &motionDetected
        );

        /*
         * Poll PIR every 100 ms.
         * This prevents a busy loop.
         */

        vTaskDelay(
            pdMS_TO_TICKS(100)
        );
    }
}

/* =========================================================
   INPUT TASK
   ========================================================= */

static void InputTask(void *pvParameters)
{
    DisplayMode currentMode =
        TEMPERATURE;

    int lastCLK =
        gpio_get_level(
            ENCODER_CLK_PIN
        );

    printf(
        "InputTask started\n"
    );

    printf(
        "Current DisplayMode: TEMPERATURE\n"
    );

    while (1)
    {
        int currentCLK =
            gpio_get_level(
                ENCODER_CLK_PIN
            );

        /*
         * Detect falling edge on CLK.
         */

        if (
            currentCLK != lastCLK &&
            currentCLK == 0
        )
        {
            int currentDT =
                gpio_get_level(
                    ENCODER_DT_PIN
                );

            /*
             * Determine direction.
             */

            if (currentDT != currentCLK)
            {
                /*
                 * Clockwise
                 */

                if (currentMode == MOTION)
                {
                    currentMode =
                        TEMPERATURE;
                }
                else
                {
                    currentMode++;
                }
            }
            else
            {
                /*
                 * Counter-clockwise
                 */

                if (currentMode == TEMPERATURE)
                {
                    currentMode =
                        MOTION;
                }
                else
                {
                    currentMode--;
                }
            }

            /*
             * Send selected mode
             * to DisplayTask.
             */

            xQueueSend(
                displayModeQueue,
                &currentMode,
                0
            );

            printf(
                "InputTask: "
                "DisplayMode changed to %d\n",
                currentMode
            );

            /*
             * Encoder debounce.
             */

            vTaskDelay(
                pdMS_TO_TICKS(20)
            );
        }

        lastCLK =
            currentCLK;

        /*
         * Poll encoder every 10 ms.
         */

        vTaskDelay(
            pdMS_TO_TICKS(10)
        );
    }
}

/* =========================================================
   DISPLAY TASK
   STEP 35 CONSUMER
   ========================================================= */

static void DisplayTask(void *pvParameters)
{
    SensorData sensorData =
    {
        0
    };

    DisplayMode currentMode =
        TEMPERATURE;

    SystemState currentState =
        ACTIVE;

    bool oledEnabled = true;

    EventBits_t lastEventBits =
        0;

    printf(
        "DisplayTask started\n"
    );

    /*
     * OLED is owned ONLY by DisplayTask.
     */

    oled_init();
    oled_clear();
    oled_set_power(true);

    while (1)
    {
        /*
         * =================================================
         * STEP 35 - CONSUME EVENT GROUP
         * =================================================
         *
         * DisplayTask monitors all three system events.
         */

        EventBits_t eventBits =
            xEventGroupGetBits(
                systemEventGroup
            );

        /*
         * Print event changes only when
         * the event state changes.
         */

        if (eventBits != lastEventBits)
        {
            if (
                eventBits & EVENT_ACTIVE
            )
            {
                if (
                    !(lastEventBits & EVENT_ACTIVE)
                )
                {
                    printf(
                        "DisplayTask: "
                        "EVENT_ACTIVE received\n"
                    );
                }
            }
            else
            {
                if (
                    lastEventBits & EVENT_ACTIVE
                )
                {
                    printf(
                        "DisplayTask: "
                        "EVENT_ACTIVE cleared\n"
                    );
                }
            }

            if (
                eventBits & EVENT_MOTION
            )
            {
                if (
                    !(lastEventBits & EVENT_MOTION)
                )
                {
                    printf(
                        "DisplayTask: "
                        "EVENT_MOTION received\n"
                    );
                }
            }
            else
            {
                if (
                    lastEventBits & EVENT_MOTION
                )
                {
                    printf(
                        "DisplayTask: "
                        "EVENT_MOTION cleared\n"
                    );
                }
            }

            if (
                eventBits & EVENT_ALARM
            )
            {
                if (
                    !(lastEventBits & EVENT_ALARM)
                )
                {
                    printf(
                        "DisplayTask: "
                        "EVENT_ALARM received\n"
                    );
                }
            }
            else
            {
                if (
                    lastEventBits & EVENT_ALARM
                )
                {
                    printf(
                        "DisplayTask: "
                        "EVENT_ALARM cleared\n"
                    );
                }
            }

            lastEventBits =
                eventBits;
        }

        /*
         * Receive latest system state.
         */

        SystemState newState;

        while (
            xQueueReceive(
                stateQueue,
                &newState,
                0
            ) == pdPASS
        )
        {
            if (newState != currentState)
            {
                currentState =
                    newState;

                if (currentState == INACTIVE)
                {
                    /*
                     * Turn OLED off.
                     */

                    oled_clear();

                    oled_set_power(false);

                    oledEnabled = false;

                    printf(
                        "DisplayTask: "
                        "OLED OFF - "
                        "system INACTIVE\n"
                    );
                }
                else
                {
                    /*
                     * Restore OLED.
                     */

                    oled_set_power(true);

                    oledEnabled = true;

                    printf(
                        "DisplayTask: "
                        "OLED ON - "
                        "system ACTIVE\n"
                    );
                }
            }
        }

        /*
         * INACTIVE:
         *
         * Keep DisplayTask alive so that it can
         * receive the ACTIVE state later.
         *
         * Drain sensor data so SensorTask's queue
         * does not remain permanently full.
         */

        if (currentState == INACTIVE)
        {
            SensorData discardedData;

            while (
                xQueueReceive(
                    sensorQueue,
                    &discardedData,
                    0
                ) == pdPASS
            )
            {
                /*
                 * Intentionally discard sensor data
                 * while the display is inactive.
                 */
            }

            /*
             * MotionTask remains active independently.
             */

            vTaskDelay(
                pdMS_TO_TICKS(100)
            );

            continue;
        }

        /*
         * ACTIVE:
         *
         * Check whether InputTask selected
         * another display page.
         */

        DisplayMode newMode;

        if (
            xQueueReceive(
                displayModeQueue,
                &newMode,
                0
            ) == pdPASS
        )
        {
            currentMode =
                newMode;

            printf(
                "DisplayTask: "
                "Switched to DisplayMode %d\n",
                currentMode
            );
        }

        /*
         * Receive latest motion state.
         */

        bool motionState;

        while (
            xQueueReceive(
                motionQueue,
                &motionState,
                0
            ) == pdPASS
        )
        {
            sensorData.motionDetected =
                motionState;
        }

        /*
         * Receive sensor information.
         */

        if (
            xQueueReceive(
                sensorQueue,
                &sensorData,
                pdMS_TO_TICKS(100)
            ) == pdPASS
        )
        {
            /*
             * Check for newer display mode.
             */

            while (
                xQueueReceive(
                    displayModeQueue,
                    &newMode,
                    0
                ) == pdPASS
            )
            {
                currentMode =
                    newMode;
            }

            /*
             * Check for newest motion state.
             */

            while (
                xQueueReceive(
                    motionQueue,
                    &motionState,
                    0
                ) == pdPASS
            )
            {
                sensorData.motionDetected =
                    motionState;
            }

            /*
             * Update OLED only while ACTIVE.
             */

            if (
                oledEnabled &&
                currentState == ACTIVE
            )
            {
                oled_clear();

                /*
                 * Title
                 */

                oled_write_string(
                    0,
                    28,
                    "ROOM MONITOR"
                );

                /*
                 * Selected measurement
                 */

                switch (currentMode)
                {
                    case TEMPERATURE:
                    {
                        char temperatureText[16];

                        oled_write_string(
                            2,
                            20,
                            "TEMPERATURE"
                        );

                        snprintf(
                            temperatureText,
                            sizeof(temperatureText),
                            "%.1f C",
                            sensorData.temperature
                        );

                        oled_write_string(
                            4,
                            40,
                            temperatureText
                        );

                        printf(
                            "DisplayTask: "
                            "Temperature page = "
                            "%.2f C\n",
                            sensorData.temperature
                        );

                        break;
                    }

                    case HUMIDITY:
                    {
                        char humidityText[16];

                        oled_write_string(
                            2,
                            34,
                            "HUMIDITY"
                        );

                        snprintf(
                            humidityText,
                            sizeof(humidityText),
                            "%.1f %%",
                            sensorData.humidity
                        );

                        oled_write_string(
                            4,
                            40,
                            humidityText
                        );

                        printf(
                            "DisplayTask: "
                            "Humidity page = "
                            "%.2f %%\n",
                            sensorData.humidity
                        );

                        break;
                    }

                    case LIGHT:
                    {
                        char lightText[16];

                        oled_write_string(
                            2,
                            46,
                            "LIGHT"
                        );

                        snprintf(
                            lightText,
                            sizeof(lightText),
                            "%d %%",
                            sensorData.lightLevel
                        );

                        oled_write_string(
                            4,
                            40,
                            lightText
                        );

                        printf(
                            "DisplayTask: "
                            "Light page = "
                            "%d %%\n",
                            sensorData.lightLevel
                        );

                        break;
                    }

                    case MOTION:
                    {
                        oled_write_string(
                            2,
                            40,
                            "MOTION"
                        );

                        if (sensorData.motionDetected)
                        {
                            oled_write_string(
                                4,
                                40,
                                "1"
                            );
                        }
                        else
                        {
                            oled_write_string(
                                4,
                                40,
                                "0"
                            );
                        }

                        printf(
                            "DisplayTask: "
                            "Motion page = "
                            "%d\n",
                            sensorData.motionDetected
                        );

                        break;
                    }
                }
            }
        }

        /*
         * Prevent DisplayTask from continuously
         * consuming CPU.
         */

        vTaskDelay(
            pdMS_TO_TICKS(20)
        );
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

    /* =====================================================
       STEP 30 - ALARM LOGIC TESTS
       ===================================================== */

    printf(
        "\n===== ALARM LOGIC TESTS =====\n"
    );

    printf(
        "Alarm Test 17.9 C = %d\n",
        evaluateTemperature(17.9f)
    );

    printf(
        "Alarm Test 18.0 C = %d\n",
        evaluateTemperature(18.0f)
    );

    printf(
        "Alarm Test 24.0 C = %d\n",
        evaluateTemperature(24.0f)
    );

    printf(
        "Alarm Test 30.0 C = %d\n",
        evaluateTemperature(30.0f)
    );

    printf(
        "Alarm Test 30.1 C = %d\n",
        evaluateTemperature(30.1f)
    );

    printf(
        "============================\n\n"
    );

    /* Initialize LDR */

    ldr_init();

    /* Configure DHT22 */

    gpio_set_direction(
        DHT_PIN,
        GPIO_MODE_INPUT
    );

    gpio_pullup_en(
        DHT_PIN
    );

    /* Configure rotary encoder */

    gpio_config_t encoder_config =
    {
        .pin_bit_mask =
            (1ULL << ENCODER_CLK_PIN) |
            (1ULL << ENCODER_DT_PIN) |
            (1ULL << ENCODER_SW_PIN),

        .mode =
            GPIO_MODE_INPUT,

        .pull_up_en =
            GPIO_PULLUP_ENABLE,

        .pull_down_en =
            GPIO_PULLDOWN_DISABLE,

        .intr_type =
            GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(
        gpio_config(
            &encoder_config
        )
    );

    /* Configure PIR */

    gpio_config_t pir_config =
    {
        .pin_bit_mask =
            (1ULL << PIR_PIN),

        .mode =
            GPIO_MODE_INPUT,

        .pull_up_en =
            GPIO_PULLUP_DISABLE,

        .pull_down_en =
            GPIO_PULLDOWN_DISABLE,

        .intr_type =
            GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(
        gpio_config(
            &pir_config
        )
    );

    /* =====================================================
       CREATE SENSOR QUEUE
       ===================================================== */

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

    /* =====================================================
       CREATE DISPLAY MODE QUEUE
       ===================================================== */

    displayModeQueue =
        xQueueCreate(
            5,
            sizeof(DisplayMode)
        );

    if (displayModeQueue == NULL)
    {
        printf(
            "Failed to create Display Mode Queue\n"
        );

        return;
    }

    printf(
        "Display Mode Queue created successfully\n"
    );

    /* =====================================================
       CREATE MOTION QUEUE
       ===================================================== */

    motionQueue =
        xQueueCreate(
            1,
            sizeof(bool)
        );

    if (motionQueue == NULL)
    {
        printf(
            "Failed to create Motion Queue\n"
        );

        return;
    }

    printf(
        "Motion Queue created successfully\n"
    );

    /* =====================================================
       CREATE SYSTEM STATE QUEUE
       ===================================================== */

    stateQueue =
        xQueueCreate(
            1,
            sizeof(SystemState)
        );

    if (stateQueue == NULL)
    {
        printf(
            "Failed to create System State Queue\n"
        );

        return;
    }

    printf(
        "System State Queue created successfully\n"
    );

    /* =====================================================
       CREATE EVENT GROUP - STEP 35
       ===================================================== */

    systemEventGroup =
        xEventGroupCreate();

    if (systemEventGroup == NULL)
    {
        printf(
            "Failed to create System Event Group\n"
        );

        return;
    }

    printf(
        "System Event Group created successfully\n"
    );

    printf(
        "EVENT_ACTIVE = BIT0\n"
    );

    printf(
        "EVENT_MOTION = BIT1\n"
    );

    printf(
        "EVENT_ALARM  = BIT2\n"
    );

    /* =====================================================
       CREATE FREERTOS TASKS
       ===================================================== */

    xTaskCreate(
        SensorTask,
        "SensorTask",
        4096,
        NULL,
        2,
        NULL
    );

    xTaskCreate(
        MotionTask,
        "MotionTask",
        4096,
        NULL,
        3,
        NULL
    );

    xTaskCreate(
        DisplayTask,
        "DisplayTask",
        4096,
        NULL,
        1,
        NULL
    );

    xTaskCreate(
        InputTask,
        "InputTask",
        4096,
        NULL,
        2,
        NULL
    );
}