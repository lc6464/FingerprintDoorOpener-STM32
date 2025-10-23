#include <algorithm>
#include <cstdint>
#include <cmath>

#include "tim.h"

#include "UnitConvertor.h"

class Servo {
private:
	TIM_HandleTypeDef *htim;
	uint32_t channel;
public:
	Servo(TIM_HandleTypeDef &htim, uint32_t channel) : htim(&htim), channel(channel) { }

	void SetAngle(int16_t angle) {
		uint16_t compareValue = UnitConvertor::AngleToCompare(angle);
		__HAL_TIM_SetCompare(htim, channel, compareValue);
	}

	void SetRadian(float radian) {
		float degree = UnitConvertor::RadianToDegree(radian);
		SetAngle(static_cast<int16_t>(std::round(degree)));
	}

	void Release() {
		__HAL_TIM_SetCompare(htim, channel, 0);
	}
};