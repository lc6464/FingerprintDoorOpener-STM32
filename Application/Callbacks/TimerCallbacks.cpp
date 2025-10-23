#include "tim.h"

#include "Button_Shared.h"

extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM6) { // 1kHz 定时器
		fingerprintTouchButton.Tick();
		return;
	}

	if (htim->Instance == TIM7) { // 分配给 HAL 的定时器
		HAL_IncTick();
		return;
	}
}