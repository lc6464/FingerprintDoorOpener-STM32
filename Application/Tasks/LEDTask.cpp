#include "cmsis_os.h"
#include "gpio.h"

// #include "UARTMessage.h"

void LEDTask() {
	while (true) {
		// HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET); // 点亮 LED
		// osDelay(75); // 延时 75ms
		// HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);   // 熄灭 LED
		// osDelay(175); // 延时 175ms
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, HAL_GPIO_ReadPin(FingerprintModuleTouchSensor_GPIO_Port, FingerprintModuleTouchSensor_Pin));
		osDelay(20); // 延时 20ms
	}
}