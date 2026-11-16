#include "stm32f1xx_hal.h"

#include "app_tasks.h"
#include "drivers_oled.h"
#include "drivers_sensors.h"
#include "task.h"

UART_HandleTypeDef huart1;

extern "C" void HardFault_Handler(void)
{
    static const char message[] = "HARDFAULT\r\n";
    for (const char *cursor = message; *cursor != '\0'; ++cursor) {
        uint32_t timeout = 1000000U;
        while ((USART1->SR & USART_SR_TXE) == 0U && timeout > 0U) {
            --timeout;
        }
        USART1->DR = static_cast<uint8_t>(*cursor);
    }
    for (;;) {
        __NOP();
    }
}

extern "C" void vApplicationTickHook(void)
{
    HAL_IncTick();
}

namespace {

bool USART1_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};
    gpio.Pin = GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200U;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    return HAL_UART_Init(&huart1) == HAL_OK;
}

void ShowFatalError(const char *message)
{
    Serial_Print(message);
    OLED_Clear();
    OLED_DrawString(0, 2, "STARTUP ERROR");
    OLED_DrawString(0, 4, message);
    OLED_Update();
    for (;;) {
    }
}

}

int main(void)
{
    HAL_Init();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    SystemCoreClockUpdate();

    if (!USART1_Init()) {
        for (;;) {
        }
    }
    Serial_Print("BOOT: USART1 ready\r\n");
    if (!Sensors_Init()) {
        ShowFatalError("SENSOR INIT FAILED");
    }
    Serial_Print("BOOT: sensors ready\r\n");
    Encoder_Init();
    OLED_Init();
    OLED_DrawString(0U, 2U, "BCA182 STARTING");
    OLED_DrawString(0U, 4U, "PLEASE WAIT");
    OLED_Update();
    Serial_Print("BOOT: OLED ready\r\n");
    if (!AppTasks_Init()) {
        ShowFatalError("RTOS ALLOC FAILED");
    }

    Serial_Print("BOOT: scheduler starting\r\n");
    vTaskStartScheduler();
    ShowFatalError("SCHEDULER FAILED");
}
