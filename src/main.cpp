#include "stm32f1xx_hal.h"

extern "C" void app_main(void) {
    HAL_Init();
    while (1) {
    }
}

int main(void) {
    app_main();
    return 0;
}