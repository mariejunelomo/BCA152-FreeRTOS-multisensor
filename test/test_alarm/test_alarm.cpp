#include <unity.h>
#include "alarm.h"

extern "C" {
void setUp(void) {}
void tearDown(void) {}
}

void test_below_lower_threshold_is_low(void)
{
    TEST_ASSERT_EQUAL_INT(LOW_TEMPERATURE, evaluateTemperature(17.9f));
}

void test_exactly_lower_threshold_is_normal(void)
{
    TEST_ASSERT_EQUAL_INT(NORMAL, evaluateTemperature(18.0f));
}

void test_middle_value_is_normal(void)
{
    TEST_ASSERT_EQUAL_INT(NORMAL, evaluateTemperature(24.0f));
}

void test_exactly_upper_threshold_is_normal(void)
{
    TEST_ASSERT_EQUAL_INT(NORMAL, evaluateTemperature(30.0f));
}

void test_above_upper_threshold_is_high(void)
{
    TEST_ASSERT_EQUAL_INT(HIGH_TEMPERATURE, evaluateTemperature(30.1f));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_below_lower_threshold_is_low);
    RUN_TEST(test_exactly_lower_threshold_is_normal);
    RUN_TEST(test_middle_value_is_normal);
    RUN_TEST(test_exactly_upper_threshold_is_normal);
    RUN_TEST(test_above_upper_threshold_is_high);
    return UNITY_END();
}