#include "cmsis_os.h"
#include "gpio.h"

void LEDTask() {
    while (true) {
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET); // 点亮 LED
        osDelay(100); // 延时 100ms
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);   // 熄灭 LED
        osDelay(400); // 延时 400ms
    }
}