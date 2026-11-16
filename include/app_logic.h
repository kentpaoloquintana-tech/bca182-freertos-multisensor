#ifndef APP_LOGIC_H
#define APP_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MEASUREMENT_TEMPERATURE = 0,
    MEASUREMENT_HUMIDITY,
    MEASUREMENT_LIGHT,
    MEASUREMENT_MOTION,
    MEASUREMENT_COUNT
} Measurement_t;

typedef enum {
    TEMPERATURE_INVALID = 0,
    TEMPERATURE_NORMAL,
    TEMPERATURE_LOW,
    TEMPERATURE_HIGH
} TemperatureStatus_t;

typedef enum {
    SYSTEM_INACTIVE = 0,
    SYSTEM_ACTIVE
} SystemState_t;

Measurement_t Measurement_Move(Measurement_t current, int delta);
TemperatureStatus_t TemperatureStatus_Evaluate(float temperature, bool valid);
SystemState_t SystemState_Update(SystemState_t current, uint32_t now,
                                 uint32_t *lastActivity, bool activity,
                                 uint32_t inactivityTimeout);

#endif
