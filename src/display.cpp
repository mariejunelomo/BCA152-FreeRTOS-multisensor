#include "display.h"
#include "system_state.h"
#include "rtos_objects.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_err.h"

#define OLED_SDA_PIN        GPIO_NUM_21
#define OLED_SCL_PIN        GPIO_NUM_22
#define OLED_I2C_PORT       I2C_NUM_0
#define OLED_ADDR           0x3C
#define OLED_WIDTH          128
#define OLED_HEIGHT         64

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
    i2c_config_t config = {};

    config.mode = I2C_MODE_MASTER;
    config.sda_io_num = OLED_SDA_PIN;
    config.scl_io_num = OLED_SCL_PIN;
    config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    config.scl_pullup_en = GPIO_PULLUP_ENABLE;
    config.master.clk_speed = 400000;

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

void DisplayTask(void *pvParameters)
{
    SensorData sensorData = {};

    DisplayMode currentMode =
        DisplayMode::TEMPERATURE;

    SystemState currentState =
        ACTIVE;

    bool oledEnabled = true;

    EventBits_t lastEventBits =
        0;

    safe_printf(
        "DisplayTask started\n"
    );

    oled_init();
    oled_clear();
    oled_set_power(true);

    while (1)
    {
        /* STEP 35:
         * DisplayTask consumes the Event Group.
         */

        EventBits_t eventBits =
            xEventGroupGetBits(
                systemEventGroup
            );

        if (eventBits != lastEventBits)
        {
            if (eventBits & EVENT_ACTIVE)
            {
                if (!(lastEventBits & EVENT_ACTIVE))
                {
                    safe_printf(
                        "DisplayTask: "
                        "EVENT_ACTIVE received\n"
                    );
                }
            }
            else
            {
                if (lastEventBits & EVENT_ACTIVE)
                {
                    safe_printf(
                        "DisplayTask: "
                        "EVENT_ACTIVE cleared\n"
                    );
                }
            }

            if (eventBits & EVENT_MOTION)
            {
                if (!(lastEventBits & EVENT_MOTION))
                {
                    safe_printf(
                        "DisplayTask: "
                        "EVENT_MOTION received\n"
                    );
                }
            }
            else
            {
                if (lastEventBits & EVENT_MOTION)
                {
                    safe_printf(
                        "DisplayTask: "
                        "EVENT_MOTION cleared\n"
                    );
                }
            }

            if (eventBits & EVENT_ALARM)
            {
                if (!(lastEventBits & EVENT_ALARM))
                {
                    safe_printf(
                        "DisplayTask: "
                        "EVENT_ALARM received\n"
                    );
                }
            }
            else
            {
                if (lastEventBits & EVENT_ALARM)
                {
                    safe_printf(
                        "DisplayTask: "
                        "EVENT_ALARM cleared\n"
                    );
                }
            }

            lastEventBits =
                eventBits;
        }

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
                    oled_clear();
                    oled_set_power(false);
                    oledEnabled = false;

                    safe_printf(
                        "DisplayTask: "
                        "OLED OFF - "
                        "system INACTIVE\n"
                    );
                }
                else
                {
                    oled_set_power(true);
                    oledEnabled = true;

                    safe_printf(
                        "DisplayTask: "
                        "OLED ON - "
                        "system ACTIVE\n"
                    );
                }
            }
        }

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
                /* Discard sensor data while inactive. */
            }

            vTaskDelay(
                pdMS_TO_TICKS(100)
            );

            continue;
        }

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

            safe_printf(
                "DisplayTask: "
                "Switched to DisplayMode %d\n",
                currentMode
            );
        }

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

        SensorData newSensorData;

        if (
            xQueueReceive(
                sensorQueue,
                &newSensorData,
                pdMS_TO_TICKS(100)
            ) == pdPASS
        )
        {
            // Update sensor readings without overwriting motion state.
            sensorData.temperature = newSensorData.temperature;
            sensorData.humidity = newSensorData.humidity;
            sensorData.lightLevel = newSensorData.lightLevel;
            sensorData.temperatureValid = newSensorData.temperatureValid;
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

            if (
                oledEnabled &&
                currentState == ACTIVE
            )
            {
                oled_clear();

                oled_write_string(
                    0,
                    28,
                    "ROOM MONITOR"
                );

                switch (currentMode)
                {
                    case DisplayMode::TEMPERATURE:
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

                        safe_printf(
                            "DisplayTask: "
                            "Temperature page = "
                            "%.2f C\n",
                            sensorData.temperature
                        );

                        break;
                    }

                    case DisplayMode::HUMIDITY:
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

                        safe_printf(
                            "DisplayTask: "
                            "Humidity page = "
                            "%.2f %%\n",
                            sensorData.humidity
                        );

                        break;
                    }

                    case DisplayMode::LIGHT:
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

                        safe_printf(
                            "DisplayTask: "
                            "Light page = "
                            "%d %%\n",
                            sensorData.lightLevel
                        );

                        break;
                    }

                    case DisplayMode::MOTION:
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

                        safe_printf(
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
        vTaskDelay(
            pdMS_TO_TICKS(20)
        );
    }
}
