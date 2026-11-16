#include "app_logic.h"

Measurement_t Measurement_Move(Measurement_t current, int delta)
{
    int index = static_cast<int>(current);
    if (index < 0 || index >= MEASUREMENT_COUNT) {
        index = 0;
    }
    const int count = static_cast<int>(MEASUREMENT_COUNT);
    int next = (index + delta) % count;
    if (next < 0) {
        next += count;
    }
    return static_cast<Measurement_t>(next);
}

TemperatureStatus_t TemperatureStatus_Evaluate(float temperature, bool valid)
{
    if (!valid) {
        return TEMPERATURE_INVALID;
    }
    if (temperature < 18.0f) {
        return TEMPERATURE_LOW;
    }
    if (temperature > 30.0f) {
        return TEMPERATURE_HIGH;
    }
    return TEMPERATURE_NORMAL;
}

SystemState_t SystemState_Update(SystemState_t current, uint32_t now,
                                 uint32_t *lastActivity, bool activity,
                                 uint32_t inactivityTimeout)
{
    if (lastActivity == nullptr) {
        return current;
    }
    if (activity) {
        *lastActivity = now;
        return SYSTEM_ACTIVE;
    }
    if (current == SYSTEM_ACTIVE &&
        static_cast<uint32_t>(now - *lastActivity) >= inactivityTimeout) {
        return SYSTEM_INACTIVE;
    }
    return current;
}
