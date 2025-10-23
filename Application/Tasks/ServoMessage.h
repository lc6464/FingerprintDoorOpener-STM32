#pragma once

#include <cstdint>
#include <string_view>

#include "cmsis_os.h"

extern osMessageQueueId_t ServoQueueHandle;

enum class ServoMessageType : uint8_t {
	None = 0,
	MoveToUnlockPosition,
	MoveToResetPosition,
	ReleaseServo,
	Error
};

// 8bit + 8bit
struct ServoMessage {
	ServoMessageType type;
	uint8_t reserved = 0; // 保留字节，供将来扩展使用
};

inline constexpr size_t ServoMessageSize = sizeof(ServoMessage);

inline constexpr std::string_view to_string(ServoMessageType type) {
	switch (type) {
	case ServoMessageType::None:
		return "None";
	case ServoMessageType::MoveToUnlockPosition:
		return "MoveToUnlockPosition";
	case ServoMessageType::MoveToResetPosition:
		return "MoveToResetPosition";
	case ServoMessageType::ReleaseServo:
		return "ReleaseServo";
	case ServoMessageType::Error:
		return "Error";
	default:
		return "Unknown";
	}
}