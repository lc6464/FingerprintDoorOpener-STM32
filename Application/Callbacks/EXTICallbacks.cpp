#include "gpio.h"

#include "Button_Shared.h"

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (fingerprintTouchButton.HandleInterrupt(GPIO_Pin)) {
		return;
	}
}