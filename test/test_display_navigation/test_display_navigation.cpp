#include <unity.h>
#include "display_mode.h"

extern "C" {
void setUp(void) {}
void tearDown(void) {}
}

void test_forward_temperature_to_humidity(void)
{
    TEST_ASSERT_EQUAL_INT(
        (int)DisplayMode::HUMIDITY,
        (int)nextDisplayMode(DisplayMode::TEMPERATURE));
}

void test_forward_wraps_motion_to_temperature(void)
{
    TEST_ASSERT_EQUAL_INT(
        (int)DisplayMode::TEMPERATURE,
        (int)nextDisplayMode(DisplayMode::MOTION));
}

void test_reverse_humidity_to_temperature(void)
{
    TEST_ASSERT_EQUAL_INT(
        (int)DisplayMode::TEMPERATURE,
        (int)previousDisplayMode(DisplayMode::HUMIDITY));
}

void test_reverse_wraps_temperature_to_motion(void)
{
    TEST_ASSERT_EQUAL_INT(
        (int)DisplayMode::MOTION,
        (int)previousDisplayMode(DisplayMode::TEMPERATURE));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_forward_temperature_to_humidity);
    RUN_TEST(test_forward_wraps_motion_to_temperature);
    RUN_TEST(test_reverse_humidity_to_temperature);
    RUN_TEST(test_reverse_wraps_temperature_to_motion);
    return UNITY_END();
}