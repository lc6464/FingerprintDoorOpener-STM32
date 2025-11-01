#pragma once

#include <cstdint>
#include <string_view>

#include "cmsis_os.h"

extern osMessageQueueId_t UARTQueueHandle;

enum class UARTMessageType : uint8_t {
	None = 0,
	FingerprintEnrollStart,
	FingerprintEnrollStep,
	FingerprintEnrollComplete,
	FingerprintEnterSleepMode,
	FingerprintMatchStart,
	FingerprintMatchComplete,
	FingerprintUpdateFeatureAfterMatch,
	FingerprintError,
	ServoMovingToUnlockPosition,
	ServoMovingToResetPosition,
	ServoRelease,
	LEDControl,
};

// 8bit + 8bit + 16bit
struct UARTMessage {
	UARTMessageType type;
	union {
		uint8_t data1;                  // 通用数据字段1
		uint8_t fingerprintEnrollStep;  // 用于 FingerprintEnrollStep
		bool fingerprintMatchResult;    // 用于 FingerprintMatchComplete
		uint8_t errorCode;              // 用于 Error
	};

	union {
		uint16_t data2;                 // 通用数据字段2
		uint16_t fingerprintId;         // 用于 FingerprintEnrollComplete 和 FingerprintMatchComplete
		uint16_t moduleErrorCode;       // 用于 Error
	};
};

inline constexpr size_t UARTMessageSize = sizeof(UARTMessage);

inline constexpr std::string_view to_string(UARTMessageType type) {
	switch (type) {
	case UARTMessageType::None:
		return "None";
	case UARTMessageType::FingerprintEnrollStart:
		return "FingerprintEnrollStart";
	case UARTMessageType::FingerprintEnrollStep:
		return "FingerprintEnrollStep";
	case UARTMessageType::FingerprintEnrollComplete:
		return "FingerprintEnrollComplete";
	case UARTMessageType::FingerprintEnterSleepMode:
		return "FingerprintEnterSleepMode";
	case UARTMessageType::FingerprintMatchStart:
		return "FingerprintMatchStart";
	case UARTMessageType::FingerprintMatchComplete:
		return "FingerprintMatchComplete";
	case UARTMessageType::FingerprintUpdateFeatureAfterMatch:
		return "FingerprintUpdateFeatureAfterMatch";
	case UARTMessageType::FingerprintError:
		return "FingerprintError";
	case UARTMessageType::ServoMovingToUnlockPosition:
		return "ServoMovingToUnlockPosition";
	case UARTMessageType::ServoMovingToResetPosition:
		return "ServoMovingToResetPosition";
	case UARTMessageType::ServoRelease:
		return "ServoRelease";
	case UARTMessageType::LEDControl:
		return "LEDControl";
	default:
		return "Unknown";
	}
}