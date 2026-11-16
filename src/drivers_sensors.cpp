#include "stm32f1xx_hal.h"

#include "drivers_sensors.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stddef.h>

extern UART_HandleTypeDef huart1;
extern "C" void EXTI0_IRQHandler(void);

namespace {

ADC_HandleTypeDef hadc1;
TIM_HandleTypeDef htim2;
volatile int16_t encoderQuarterSteps = 0;
uint8_t encoderState = 0;
volatile bool buzzerEnabled = false;

bool WaitForDhtLevel(GPIO_PinState level)
{
    uint32_t timeout = 0;
    while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) != level) {
        if (++timeout >= 12000U) {
            return false;
        }
    }
    return true;
}

void DhtInputMode(void)
{
    GPIO_InitTypeDef gpio = {};
    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);
}

}

void Delay_us(uint32_t us)
{
    volatile uint32_t count = us * 2U;
    while (count--) {
        __NOP();
    }
}

void Serial_Print(const char *text)
{
    if (text == NULL) {
        return;
    }
    while (*text != '\0') {
        const char *start = text;
        uint16_t length = 0;
        while (*text != '\0' && length < 64U) {
            ++text;
            ++length;
        }
        if (HAL_UART_Transmit(&huart1, reinterpret_cast<uint8_t *>(const_cast<char *>(start)),
                              length, 250U) != HAL_OK) {
            return;
        }
    }
}

bool DHT22_Read(float *temperature, float *humidity)
{
    if (temperature == NULL || humidity == NULL) {
        return false;
    }

    uint8_t data[5] = {};
    GPIO_InitTypeDef gpio = {};
    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
    Delay_us(1200U);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
    Delay_us(30U);
    DhtInputMode();

    if (!WaitForDhtLevel(GPIO_PIN_RESET) ||
        !WaitForDhtLevel(GPIO_PIN_SET) ||
        !WaitForDhtLevel(GPIO_PIN_RESET)) {
        DhtInputMode();
        return false;
    }

    for (uint8_t bit = 0; bit < 40U; ++bit) {
        if (!WaitForDhtLevel(GPIO_PIN_SET)) {
            DhtInputMode();
            return false;
        }
        Delay_us(40U);
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) {
            data[bit / 8U] |= static_cast<uint8_t>(1U << (7U - bit % 8U));
        }
        if (!WaitForDhtLevel(GPIO_PIN_RESET)) {
            DhtInputMode();
            return false;
        }
    }

    const uint8_t checksum = static_cast<uint8_t>(data[0] + data[1] + data[2] + data[3]);
    if (checksum != data[4]) {
        return false;
    }
    const uint16_t rawHumidity = static_cast<uint16_t>((data[0] << 8U) | data[1]);
    const uint16_t rawTemperature = static_cast<uint16_t>(((data[2] & 0x7FU) << 8U) | data[3]);
    *humidity = rawHumidity / 10.0f;
    *temperature = rawTemperature / 10.0f;
    if ((data[2] & 0x80U) != 0U) {
        *temperature = -*temperature;
    }
    return true;
}

int LDR_ReadPercent(void)
{
    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        return -1;
    }
    if (HAL_ADC_PollForConversion(&hadc1, 10U) != HAL_OK) {
        (void)HAL_ADC_Stop(&hadc1);
        return -1;
    }
    const uint32_t raw = HAL_ADC_GetValue(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);
    return static_cast<int>((raw * 100U + 2047U) / 4095U);
}

bool PIR_ReadMotion(void)
{
    return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_SET;
}

bool Sensors_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};
    gpio.Pin = GPIO_PIN_4;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

    gpio.Pin = GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        return false;
    }
    ADC_ChannelConfTypeDef channel = {};
    channel.Channel = ADC_CHANNEL_1;
    channel.Rank = ADC_REGULAR_RANK_1;
    channel.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &channel) != HAL_OK ||
        HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK) {
        return false;
    }

    __HAL_RCC_TIM2_CLK_ENABLE();
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = (SystemCoreClock / 1000000U) - 1U;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 499U;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
        return false;
    }
    HAL_NVIC_SetPriority(TIM2_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    return true;
}

void Encoder_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {};
    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &gpio);

    encoderState = static_cast<uint8_t>(
        (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_SET ? 2U : 0U) |
        (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_SET ? 1U : 0U));
    HAL_NVIC_SetPriority(EXTI0_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

int Encoder_GetDelta(void)
{
    taskENTER_CRITICAL();
    const int16_t steps = encoderQuarterSteps;
    const int16_t detents = steps / 4;
    encoderQuarterSteps = static_cast<int16_t>(steps - detents * 4);
    taskEXIT_CRITICAL();
    return detents;
}

bool Encoder_ReadButton(void)
{
    return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_RESET;
}

bool Buzzer_Set(bool on)
{
    if (on == buzzerEnabled) {
        return true;
    }
    if (on) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
        buzzerEnabled = true;
        if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) {
            buzzerEnabled = false;
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
            return false;
        }
        return true;
    }

    buzzerEnabled = false;
    const HAL_StatusTypeDef status = HAL_TIM_Base_Stop_IT(&htim2);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    return status == HAL_OK;
}

extern "C" void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

extern "C" void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *timer)
{
    if (timer->Instance == TIM2) {
        if (buzzerEnabled) {
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        } else {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
        }
    }
}

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin != GPIO_PIN_0) {
        return;
    }
    static const int8_t transitions[16] = {
        0, -1, 1, 0, 1, 0, 0, -1,
        -1, 0, 0, 1, 0, 1, -1, 0
    };
    const uint8_t current = static_cast<uint8_t>(
        (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_SET ? 2U : 0U) |
        (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_SET ? 1U : 0U));
    const uint8_t index = static_cast<uint8_t>((encoderState << 2U) | current);
    encoderQuarterSteps = static_cast<int16_t>(encoderQuarterSteps + transitions[index]);
    encoderState = current;
}
