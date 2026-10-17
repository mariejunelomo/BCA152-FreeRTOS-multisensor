#include "motion.h"
#include "system_state.h"
#include "rtos_objects.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"

#define PIR_PIN GPIO_NUM_27

void MotionTask(void *pvParameters)
{
    bool motionDetected = false;
    bool timeoutReported = false;

    SystemState currentState =
        ACTIVE;

    TickType_t lastMotionTime =
        xTaskGetTickCount();

    const TickType_t inactivityTimeout =
        pdMS_TO_TICKS(15000);

    safe_printf(
        "MotionTask started\n"
    );

    xQueueOverwrite(
        stateQueue,
        &currentState
    );

    xEventGroupSetBits(
        systemEventGroup,
        EVENT_ACTIVE
    );

    safe_printf(
        "EVENT_ACTIVE: SET\n"
    );

    while (1)
    {
        int pirLevel =
            gpio_get_level(PIR_PIN);

        if (pirLevel == 1)
        {
            if (!motionDetected)
            {
                safe_printf(
                    "MotionTask: MOTION DETECTED\n"
                );

                xEventGroupSetBits(
                    systemEventGroup,
                    EVENT_MOTION
                );

                safe_printf(
                    "EVENT_MOTION: SET\n"
                );
            }

            motionDetected = true;
            timeoutReported = false;

            lastMotionTime =
                xTaskGetTickCount();

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

                if (
                    currentState !=
                    previousState
                )
                {
                    safe_printf(
                        "SystemState: "
                        "INACTIVE -> ACTIVE\n"
                    );

                    xQueueOverwrite(
                        stateQueue,
                        &currentState
                    );

                    xEventGroupSetBits(
                        systemEventGroup,
                        EVENT_ACTIVE
                    );

                    safe_printf(
                        "EVENT_ACTIVE: SET\n"
                    );
                }
            }
        }
        else
        {
            if (motionDetected)
            {
                safe_printf(
                    "MotionTask: NO MOTION\n"
                );

                xEventGroupClearBits(
                    systemEventGroup,
                    EVENT_MOTION
                );

                safe_printf(
                    "EVENT_MOTION: CLEARED\n"
                );
            }

            motionDetected = false;

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

                safe_printf(
                    "MotionTask: "
                    "NO MOTION - "
                    "15 SECOND TIMEOUT\n"
                );

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

                    if (
                        currentState !=
                        previousState
                    )
                    {
                        safe_printf(
                            "SystemState: "
                            "ACTIVE -> INACTIVE\n"
                        );

                        xQueueOverwrite(
                            stateQueue,
                            &currentState
                        );

                        xEventGroupClearBits(
                            systemEventGroup,
                            EVENT_ACTIVE
                        );

                        safe_printf(
                            "EVENT_ACTIVE: CLEARED\n"
                        );
                    }
                }
            }
        }

        xQueueOverwrite(
            motionQueue,
            &motionDetected
        );

        vTaskDelay(
            pdMS_TO_TICKS(100)
        );
    }
}
