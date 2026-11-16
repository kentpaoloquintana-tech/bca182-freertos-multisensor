#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "semphr.h"

#include "app_logic.h"

typedef struct {
    float temperature;
    float humidity;
    int lightPct;
    bool motionDetected;
    bool dhtValid;
} SensorData_t;

extern QueueHandle_t xSensorQueue;
extern SemaphoreHandle_t xConfigMutex;
extern EventGroupHandle_t xSystemEvents;
extern Measurement_t selectedMeasurement;
extern bool alarmMuted;

bool AppTasks_Init(void);

#endif
