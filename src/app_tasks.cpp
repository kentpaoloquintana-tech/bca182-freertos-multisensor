#include "stm32f1xx_hal.h"

#include "app_tasks.h"
#include "drivers_oled.h"
#include "drivers_sensors.h"

QueueHandle_t xSensorQueue = NULL;
SemaphoreHandle_t xConfigMutex = NULL;
EventGroupHandle_t xSystemEvents = NULL;
Measurement_t selectedMeasurement = MEASUREMENT_TEMPERATURE;
bool alarmMuted = false;

namespace {

const EventBits_t kInputActivityBit = 1U << 0;
const EventBits_t kMotionActivityBit = 1U << 1;
const EventBits_t kActiveStateBit = 1U << 2;
const EventBits_t kActivityBits = kInputActivityBit | kMotionActivityBit;
const uint32_t kInactivityTimeoutTicks = pdMS_TO_TICKS(15000U);

SemaphoreHandle_t xSensorDataMutex = NULL;
SensorData_t latestSensorData = {};

void SensorTask(void *argument)
{
    (void)argument;
    Serial_Print("TASK: Sensor started\r\n");
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        float temperature = 0.0f;
        float humidity = 0.0f;
        const bool dhtValid = DHT22_Read(&temperature, &humidity);
        const int lightPct = LDR_ReadPercent();
        SensorData_t reading = {};

        if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY) == pdTRUE) {
            latestSensorData.temperature = temperature;
            latestSensorData.humidity = humidity;
            latestSensorData.dhtValid = dhtValid;
            latestSensorData.lightPct = lightPct;
            reading = latestSensorData;
            if (xQueueOverwrite(xSensorQueue, &reading) != pdPASS) {
                Serial_Print("ERROR: sensor queue overwrite failed\r\n");
            }
            xSemaphoreGive(xSensorDataMutex);
        }

        char line[32];
        if (dhtValid) {
            Format_Fixed1(line, "TEMP=", temperature, " C, HUM=");
            Serial_Print(line);
            Format_Fixed1(line, "", humidity, " %");
            Serial_Print(line);
        } else {
            Serial_Print("DHT22=INVALID");
        }
        if (lightPct >= 0) {
            Format_Int(line, ", LIGHT=", lightPct, "%");
            Serial_Print(line);
        } else {
            Serial_Print(", LIGHT=ERROR");
        }
        Serial_Print(reading.motionDetected ? ", MOTION=YES\r\n" : ", MOTION=NO\r\n");
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(2000U));
    }
}

void InputTask(void *argument)
{
    (void)argument;
    Serial_Print("TASK: Input started\r\n");
    bool candidatePressed = Encoder_ReadButton();
    bool stablePressed = candidatePressed;
    TickType_t candidateSince = xTaskGetTickCount();

    for (;;) {
        const int delta = Encoder_GetDelta();
        if (delta != 0) {
            if (xSemaphoreTake(xConfigMutex, portMAX_DELAY) == pdTRUE) {
                selectedMeasurement = Measurement_Move(selectedMeasurement, delta);
                xSemaphoreGive(xConfigMutex);
            }
            (void)xEventGroupSetBits(xSystemEvents, kInputActivityBit);
        }

        const TickType_t now = xTaskGetTickCount();
        const bool pressed = Encoder_ReadButton();
        if (pressed != candidatePressed) {
            candidatePressed = pressed;
            candidateSince = now;
        } else if (pressed != stablePressed &&
                   (now - candidateSince) >= pdMS_TO_TICKS(30U)) {
            stablePressed = pressed;
            if (stablePressed) {
                (void)xEventGroupSetBits(xSystemEvents, kInputActivityBit);
            } else {
                if (xSemaphoreTake(xConfigMutex, portMAX_DELAY) == pdTRUE) {
                    alarmMuted = !alarmMuted;
                    xSemaphoreGive(xConfigMutex);
                }
                (void)xEventGroupSetBits(xSystemEvents, kInputActivityBit);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}

void AlarmTask(void *argument)
{
    (void)argument;
    Serial_Print("TASK: Alarm started\r\n");
    TickType_t lastWake = xTaskGetTickCount();
    uint8_t phase = 0;
    bool buzzerFailureReported = false;
    for (;;) {
        const bool active =
            (xEventGroupGetBits(xSystemEvents) & kActiveStateBit) != 0U;
        SensorData_t reading = {};
        if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY) == pdTRUE) {
            reading = latestSensorData;
            xSemaphoreGive(xSensorDataMutex);
        }
        bool muted = false;
        if (xSemaphoreTake(xConfigMutex, portMAX_DELAY) == pdTRUE) {
            muted = alarmMuted;
            xSemaphoreGive(xConfigMutex);
        }

        const TemperatureStatus_t status =
            TemperatureStatus_Evaluate(reading.temperature, reading.dhtValid);
        bool buzzerOn = false;
        if (active && !muted && status == TEMPERATURE_LOW) {
            buzzerOn = phase == 0U;
        } else if (active && !muted && status == TEMPERATURE_HIGH) {
            buzzerOn = (phase % 2U) == 0U;
        }
        if (!Buzzer_Set(buzzerOn)) {
            if (!buzzerFailureReported) {
                Serial_Print("ERROR: buzzer timer control failed\r\n");
            }
            buzzerFailureReported = true;
        } else {
            buzzerFailureReported = false;
        }
        phase = static_cast<uint8_t>((phase + 1U) % 4U);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(250U));
    }
}

void StateTask(void *argument)
{
    (void)argument;
    Serial_Print("TASK: State started\r\n");
    SystemState_t state = SYSTEM_ACTIVE;
    uint32_t lastActivity = static_cast<uint32_t>(xTaskGetTickCount());
    TickType_t lastWake = xTaskGetTickCount();
    bool previousMotion = false;
    (void)xEventGroupSetBits(xSystemEvents, kActiveStateBit);

    for (;;) {
        const bool motion = PIR_ReadMotion();
        if (motion != previousMotion &&
            xSemaphoreTake(xSensorDataMutex, portMAX_DELAY) == pdTRUE) {
            latestSensorData.motionDetected = motion;
            SensorData_t reading = latestSensorData;
            if (xQueueOverwrite(xSensorQueue, &reading) != pdPASS) {
                Serial_Print("ERROR: motion queue update failed\r\n");
            }
            xSemaphoreGive(xSensorDataMutex);
            previousMotion = motion;
        }
        if (motion) {
            (void)xEventGroupSetBits(xSystemEvents, kMotionActivityBit);
        }
        const EventBits_t events = xEventGroupClearBits(xSystemEvents, kActivityBits);
        const bool activity = (events & kActivityBits) != 0U;
        const uint32_t now = static_cast<uint32_t>(xTaskGetTickCount());
        const SystemState_t nextState = SystemState_Update(
            state, now, &lastActivity, activity, kInactivityTimeoutTicks);
        if (nextState != state) {
            state = nextState;
            if (state == SYSTEM_ACTIVE) {
                (void)xEventGroupSetBits(xSystemEvents, kActiveStateBit);
            } else {
                (void)xEventGroupClearBits(xSystemEvents, kActiveStateBit);
            }
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(50U));
    }
}

void DrawMeasurement(const SensorData_t &reading, Measurement_t measurement,
                     bool muted)
{
    char value[24];
    OLED_Clear();
    OLED_DrawString(0, 0, "BCA182 ACTIVE");

    switch (measurement) {
    case MEASUREMENT_TEMPERATURE:
        OLED_DrawString(0, 1, "TEMPERATURE");
        if (reading.dhtValid) {
            Format_Fixed1(value, "", reading.temperature, " C");
            OLED_DrawString(0, 3, value);
        } else {
            OLED_DrawString(0, 3, "DHT ERROR");
        }
        switch (TemperatureStatus_Evaluate(reading.temperature, reading.dhtValid)) {
        case TEMPERATURE_LOW:
            OLED_DrawString(0, 5, "ALARM: LOW");
            break;
        case TEMPERATURE_HIGH:
            OLED_DrawString(0, 5, "ALARM: HIGH");
            break;
        case TEMPERATURE_NORMAL:
            OLED_DrawString(0, 5, "ALARM: NORMAL");
            break;
        default:
            OLED_DrawString(0, 5, "ALARM: SENSOR ERR");
            break;
        }
        break;
    case MEASUREMENT_HUMIDITY:
        OLED_DrawString(0, 1, "HUMIDITY");
        if (reading.dhtValid) {
            Format_Fixed1(value, "", reading.humidity, " %");
            OLED_DrawString(0, 3, value);
        } else {
            OLED_DrawString(0, 3, "DHT ERROR");
        }
        break;
    case MEASUREMENT_LIGHT:
        OLED_DrawString(0, 1, "LIGHT LEVEL");
        if (reading.lightPct >= 0) {
            Format_Int(value, "", reading.lightPct, " %");
            OLED_DrawString(0, 3, value);
        } else {
            OLED_DrawString(0, 3, "ADC ERROR");
        }
        break;
    case MEASUREMENT_MOTION:
        OLED_DrawString(0, 1, "MOTION");
        OLED_DrawString(0, 3, reading.motionDetected ? "DETECTED" : "CLEAR");
        break;
    default:
        OLED_DrawString(0, 1, "UNKNOWN MEASUREMENT");
        break;
    }
    OLED_DrawString(0, 7, muted ? "BUZZER MUTED" : "PRESS SW: MUTE");
    OLED_Update();
}

void DisplayTask(void *argument)
{
    (void)argument;
    Serial_Print("TASK: Display started\r\n");
    SensorData_t latest = {};
    Measurement_t lastMeasurement = MEASUREMENT_COUNT;
    bool lastMuted = false;
    bool lastActive = false;
    bool redraw = true;
    bool displayEnabled = false;

    for (;;) {
        SensorData_t incoming;
        if (xQueueReceive(xSensorQueue, &incoming, pdMS_TO_TICKS(250U)) == pdPASS) {
            latest = incoming;
            redraw = true;
        }

        const bool active = (xEventGroupGetBits(xSystemEvents) & kActiveStateBit) != 0U;
        if (active != lastActive) {
            lastActive = active;
            if (active) {
                OLED_SetDisplay(true);
                displayEnabled = true;
                redraw = true;
            } else if (displayEnabled) {
                OLED_Clear();
                OLED_Update();
                OLED_SetDisplay(false);
                displayEnabled = false;
            }
        }
        if (!active) {
            continue;
        }

        Measurement_t measurement;
        bool muted;
        if (xSemaphoreTake(xConfigMutex, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        measurement = selectedMeasurement;
        muted = alarmMuted;
        xSemaphoreGive(xConfigMutex);
        if (measurement != lastMeasurement || muted != lastMuted) {
            redraw = true;
        }
        if (redraw) {
            DrawMeasurement(latest, measurement, muted);
            lastMeasurement = measurement;
            lastMuted = muted;
            redraw = false;
        }
    }
}

}

bool AppTasks_Init(void)
{
    xSensorQueue = xQueueCreate(1U, sizeof(SensorData_t));
    xConfigMutex = xSemaphoreCreateMutex();
    xSensorDataMutex = xSemaphoreCreateMutex();
    xSystemEvents = xEventGroupCreate();
    if (xSensorQueue == NULL || xConfigMutex == NULL ||
        xSensorDataMutex == NULL || xSystemEvents == NULL) {
        return false;
    }

    if (xTaskCreate(SensorTask, "Sensor", 256U, NULL, 2U, NULL) != pdPASS ||
        xTaskCreate(InputTask, "Input", 256U, NULL, 3U, NULL) != pdPASS ||
        xTaskCreate(AlarmTask, "Alarm", 256U, NULL, 2U, NULL) != pdPASS ||
        xTaskCreate(StateTask, "State", 256U, NULL, 2U, NULL) != pdPASS ||
        xTaskCreate(DisplayTask, "Display", 256U, NULL, 1U, NULL) != pdPASS) {
        return false;
    }
    return true;
}
