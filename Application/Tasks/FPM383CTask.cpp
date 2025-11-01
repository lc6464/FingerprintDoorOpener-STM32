#include "cmsis_os.h"
#include "gpio.h"

#include "FPM383C_Shared.h"

#include "UARTMessage.h"
#include "ServoMessage.h"

// static bool pressedLastState = false;

void FPM383CTask() {
	osDelay(300);

	// 为什么死都没法关灯啊

	// bool isPressed = false;
	// fpm383c.IsFingerPressed(isPressed);

	// auto [ledControlStatus, ledControlErrorCode] = fpm383c.SetLEDControl(FPM383C::LEDControl::ControlInfo(
	// 	FPM383C::LEDControl::Mode::Off
	// ));
	// UARTMessage ledMsg{
	// 	.type = UARTMessageType::LEDControl,
	// 	.data1 = static_cast<uint8_t>(ledControlStatus),
	// 	.data2 = static_cast<uint16_t>(ledControlErrorCode)
	// };
	// osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&ledMsg), 0, 50);

	osDelay(100);

	auto [enterSleepModeStatus, enterSleepModeErrorCode] = fpm383c.EnterSleepMode();
	UARTMessage sleepMsg{
		.type = UARTMessageType::FingerprintEnterSleepMode,
		.data1 = static_cast<uint8_t>(enterSleepModeStatus),
		.data2 = static_cast<uint16_t>(enterSleepModeErrorCode)
	};
	osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&sleepMsg), 0, 50);

	// fpm383c.SetLEDControl(FPM383C::LEDControl::ControlInfo(
	// 	FPM383C::LEDControl::Mode::Off
	// ));

	osDelay(100);

	while (true) {
		if (HAL_GPIO_ReadPin(FingerprintModuleTouchSensor_GPIO_Port, FingerprintModuleTouchSensor_Pin) == GPIO_PIN_RESET) {
			// pressedLastState = false;
			osDelay(50);
			continue;
		}

		// pressedLastState = true;
		bool isPressed = false;
		auto [status, ModuleErrorCode] = fpm383c.IsFingerPressed(isPressed);
		if (status != FPM383C::Status::OK) {
			// 通信存在问题，发送错误消息
			UARTMessage msg{
				.type = UARTMessageType::FingerprintError,
				.errorCode = static_cast<uint8_t>(status),
				.moduleErrorCode = static_cast<uint16_t>(ModuleErrorCode)
			};
			osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&msg), 0, 50);

			osDelay(200);
			continue;
		} else if (!isPressed) {
			// 手指未按下，继续等待

			auto [ledControlStatus, ledControlErrorCode] = fpm383c.SetLEDControl(FPM383C::LEDControl::ControlInfo(
				FPM383C::LEDControl::Mode::Off
			));
			UARTMessage ledMsg{
				.type = UARTMessageType::LEDControl,
				.data1 = static_cast<uint8_t>(ledControlStatus),
				.data2 = static_cast<uint16_t>(ledControlErrorCode)
			};
			osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&ledMsg), 0, 50);

			auto [enterSleepModeStatus, enterSleepModeErrorCode] = fpm383c.EnterSleepMode();
			UARTMessage sleepMsg{
				.type = UARTMessageType::FingerprintEnterSleepMode,
				.data1 = static_cast<uint8_t>(enterSleepModeStatus),
				.data2 = static_cast<uint16_t>(enterSleepModeErrorCode)
			};
			osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&sleepMsg), 0, 50);

			osDelay(100);
			continue;
		}

		// 手指已按下，执行匹配
		UARTMessage startMsg{
			.type = UARTMessageType::FingerprintMatchStart
		};
		osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&startMsg), 0, 50);

		FPM383C::MatchResult matchResult;
		auto [matchStatus, matchErrCode] = fpm383c.Match(matchResult);
		if (matchStatus != FPM383C::Status::OK) {
			// 匹配过程中出现错误，发送错误消息
			UARTMessage msg{
				.type = UARTMessageType::FingerprintError,
				.errorCode = static_cast<uint8_t>(matchStatus),
				.moduleErrorCode = static_cast<uint16_t>(matchErrCode)
			};
			osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&msg), 0, 50);

			osDelay(250);
			continue;
		}

		ServoMessage openDoorMsg{
			.type = matchResult.IsSuccess ? ServoMessageType::MoveToUnlockPosition : ServoMessageType::MoveToResetPosition
		};
		osMessageQueuePut(ServoQueueHandle, reinterpret_cast<uint32_t *>(&openDoorMsg), 0, 100); // Servo 队列多等待一些时间

		UARTMessage msg{
			.type = UARTMessageType::FingerprintMatchComplete,
			.fingerprintMatchResult = matchResult.IsSuccess,
			.fingerprintId = matchResult.FingerId
		};
		osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&msg), 0, 50);

		if (matchResult.IsSuccess) {
			// 匹配成功，自学习
			auto [updateStatus, updateErrCode] = fpm383c.UpdateFeatureAfterMatch(matchResult.FingerId);
			if (updateStatus == FPM383C::Status::OK || updateErrCode == FPM383C::ModuleErrorCode::FeatureNotNeedUpdate) {
				// 自学习成功，发送成功消息
				UARTMessage updateSuccessMsg{
					.type = UARTMessageType::FingerprintUpdateFeatureAfterMatch,
					.data1 = static_cast<uint8_t>(updateStatus),
					.data2 = static_cast<uint16_t>(updateErrCode)
				};
				osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&updateSuccessMsg), 0, 50);
			} else {
				// 自学习过程中出现错误，发送错误消息
				UARTMessage updateMsg{
					.type = UARTMessageType::FingerprintError,
					.errorCode = static_cast<uint8_t>(updateStatus),
					.moduleErrorCode = static_cast<uint16_t>(updateErrCode)
				};
				osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&updateMsg), 0, 50);
			}
		}

		osDelay(400);  // 400ms 后关灯

		auto [ledControlStatus, ledControlErrorCode] = fpm383c.SetLEDControl(FPM383C::LEDControl::ControlInfo(
			FPM383C::LEDControl::Mode::Off
		));
		UARTMessage ledMsg{
			.type = UARTMessageType::LEDControl,
			.data1 = static_cast<uint8_t>(ledControlStatus),
			.data2 = static_cast<uint16_t>(ledControlErrorCode)
		};
		osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&ledMsg), 0, 50);

		auto [enterSleepModeStatus, enterSleepModeErrorCode] = fpm383c.EnterSleepMode();
		UARTMessage sleepMsg{
			.type = UARTMessageType::FingerprintEnterSleepMode,
			.data1 = static_cast<uint8_t>(enterSleepModeStatus),
			.data2 = static_cast<uint16_t>(enterSleepModeErrorCode)
		};
		osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&sleepMsg), 0, 50);

		osDelay(600); // 识别后延迟久一点
	}
}