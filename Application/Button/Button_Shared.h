#pragma once

#include "Button.h"

inline PortPinPair fingerprintTouchButtonPair(FingerprintModuleTouchSensor_GPIO_Port, FingerprintModuleTouchSensor_Pin);

// 指纹模块虚拟按钮
extern Button fingerprintTouchButton;
