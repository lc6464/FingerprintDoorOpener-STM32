#pragma once

#include "FPM383C.h"

#include "Button_Shared.h"

// 指纹模块全局实例
inline FPM383C fpm383c(&huart2, fingerprintTouchButtonPair);
