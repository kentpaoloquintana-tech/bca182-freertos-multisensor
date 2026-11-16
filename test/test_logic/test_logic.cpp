#include <unity.h>

#include "app_logic.h"

void setUp(void) {}
void tearDown(void) {}

void test_invalid_temperature_has_no_alarm(void)
{
    TEST_ASSERT_EQUAL(TEMPERATURE_INVALID,
                      TemperatureStatus_Evaluate(24.0f, false));
}

void test_temperature_below_lower_limit_is_low(void)
{
    TEST_ASSERT_EQUAL(TEMPERATURE_LOW,
                      TemperatureStatus_Evaluate(17.9f, true));
}

void test_temperature_at_lower_limit_is_normal(void)
{
    TEST_ASSERT_EQUAL(TEMPERATURE_NORMAL,
                      TemperatureStatus_Evaluate(18.0f, true));
}

void test_temperature_inside_range_is_normal(void)
{
    TEST_ASSERT_EQUAL(TEMPERATURE_NORMAL,
                      TemperatureStatus_Evaluate(24.0f, true));
}

void test_temperature_at_upper_limit_is_normal(void)
{
    TEST_ASSERT_EQUAL(TEMPERATURE_NORMAL,
                      TemperatureStatus_Evaluate(30.0f, true));
}

void test_temperature_above_upper_limit_is_high(void)
{
    TEST_ASSERT_EQUAL(TEMPERATURE_HIGH,
                      TemperatureStatus_Evaluate(30.1f, true));
}

void test_next_measurement_moves_forward(void)
{
    TEST_ASSERT_EQUAL(MEASUREMENT_HUMIDITY,
                      Measurement_Move(MEASUREMENT_TEMPERATURE, 1));
}

void test_previous_measurement_moves_backward(void)
{
    TEST_ASSERT_EQUAL(MEASUREMENT_TEMPERATURE,
                      Measurement_Move(MEASUREMENT_HUMIDITY, -1));
}

void test_forward_navigation_wraps_after_motion(void)
{
    TEST_ASSERT_EQUAL(MEASUREMENT_TEMPERATURE,
                      Measurement_Move(MEASUREMENT_MOTION, 1));
}

void test_reverse_navigation_wraps_before_temperature(void)
{
    TEST_ASSERT_EQUAL(MEASUREMENT_MOTION,
                      Measurement_Move(MEASUREMENT_TEMPERATURE, -1));
}

void test_active_state_remains_active_before_timeout(void)
{
    uint32_t lastActivity = 100U;
    TEST_ASSERT_EQUAL(SYSTEM_ACTIVE,
                      SystemState_Update(SYSTEM_ACTIVE, 14999U, &lastActivity,
                                         false, 15000U));
    TEST_ASSERT_EQUAL_UINT32(100U, lastActivity);
}

void test_active_state_becomes_inactive_at_timeout(void)
{
    uint32_t lastActivity = 100U;
    TEST_ASSERT_EQUAL(SYSTEM_INACTIVE,
                      SystemState_Update(SYSTEM_ACTIVE, 15100U, &lastActivity,
                                         false, 15000U));
}

void test_inactive_state_stays_inactive_without_activity(void)
{
    uint32_t lastActivity = 100U;
    TEST_ASSERT_EQUAL(SYSTEM_INACTIVE,
                      SystemState_Update(SYSTEM_INACTIVE, 20000U, &lastActivity,
                                         false, 15000U));
}

void test_motion_or_input_reactivates_inactive_state(void)
{
    uint32_t lastActivity = 100U;
    TEST_ASSERT_EQUAL(SYSTEM_ACTIVE,
                      SystemState_Update(SYSTEM_INACTIVE, 20000U, &lastActivity,
                                         true, 15000U));
    TEST_ASSERT_EQUAL_UINT32(20000U, lastActivity);
}

void test_state_timeout_handles_tick_wraparound(void)
{
    uint32_t lastActivity = 0xFFFFFF00U;
    TEST_ASSERT_EQUAL(SYSTEM_INACTIVE,
                      SystemState_Update(SYSTEM_ACTIVE, 0x00003A98U,
                                         &lastActivity, false, 15000U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_invalid_temperature_has_no_alarm);
    RUN_TEST(test_temperature_below_lower_limit_is_low);
    RUN_TEST(test_temperature_at_lower_limit_is_normal);
    RUN_TEST(test_temperature_inside_range_is_normal);
    RUN_TEST(test_temperature_at_upper_limit_is_normal);
    RUN_TEST(test_temperature_above_upper_limit_is_high);
    RUN_TEST(test_next_measurement_moves_forward);
    RUN_TEST(test_previous_measurement_moves_backward);
    RUN_TEST(test_forward_navigation_wraps_after_motion);
    RUN_TEST(test_reverse_navigation_wraps_before_temperature);
    RUN_TEST(test_active_state_remains_active_before_timeout);
    RUN_TEST(test_active_state_becomes_inactive_at_timeout);
    RUN_TEST(test_inactive_state_stays_inactive_without_activity);
    RUN_TEST(test_motion_or_input_reactivates_inactive_state);
    RUN_TEST(test_state_timeout_handles_tick_wraparound);
    return UNITY_END();
}
