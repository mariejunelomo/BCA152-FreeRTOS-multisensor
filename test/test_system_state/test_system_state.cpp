#include <unity.h>
#include "system_state.h"

extern "C" {
void setUp(void) {}
void tearDown(void) {}
}

void test_active_without_timeout_stays_active(void)
{
    TEST_ASSERT_EQUAL_INT(ACTIVE, evaluateSystemState(ACTIVE, false, false));
}

void test_active_with_timeout_becomes_inactive(void)
{
    TEST_ASSERT_EQUAL_INT(INACTIVE, evaluateSystemState(ACTIVE, false, true));
}

void test_inactive_without_motion_stays_inactive(void)
{
    TEST_ASSERT_EQUAL_INT(INACTIVE, evaluateSystemState(INACTIVE, false, false));
}

void test_inactive_with_motion_becomes_active(void)
{
    TEST_ASSERT_EQUAL_INT(ACTIVE, evaluateSystemState(INACTIVE, true, false));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_active_without_timeout_stays_active);
    RUN_TEST(test_active_with_timeout_becomes_inactive);
    RUN_TEST(test_inactive_without_motion_stays_inactive);
    RUN_TEST(test_inactive_with_motion_becomes_active);
    return UNITY_END();
}