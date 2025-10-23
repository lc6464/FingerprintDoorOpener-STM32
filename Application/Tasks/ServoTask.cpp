#include "cmsis_os.h"
#include "Servo_Shared.h"
#include "usart.h"

#include "ServoMessage.h"
#include "UARTMessage.h"

inline constexpr int16_t ServoUnlockAngle = -40;  // 解锁位置角度
inline constexpr int16_t ServoResetAngle = 40;  // 复位位置角度

inline constexpr size_t ServoMoveNeedTimeMs = 1000;    // 每次舵机移动所需时间，单位毫秒
inline constexpr size_t ServoUnlockKeepTimeMs = 2000;  // 解锁保持时间，单位毫秒

// 舵机状态枚举
enum class ServoState : uint8_t {
	Idle = 0,                    // 空闲状态
	MovingToUnlock,              // 正在移动到解锁位置
	UnlockReleased,              // 已到达解锁位置并释放
	MovingToReset,               // 正在移动到复位位置
	ResetReleased,               // 已到达复位位置并释放
};

// 状态机变量
ServoState currentState = ServoState::Idle;
uint32_t stateStartTick = 0;  // 当前状态开始的时刻

static void SendUARTMessage(UARTMessageType type) {
	UARTMessage msg{
		.type = type
	};
	osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&msg), 0, 50);
}

void ServoTask() {
	ServoMessage msg;
	osStatus_t status;

	while (true) {
		// 检查队列消息（非阻塞）
		status = osMessageQueueGet(ServoQueueHandle, &msg, nullptr, 0);

		if (status == osOK) {
			// 收到消息，处理
			switch (msg.type) {
			case ServoMessageType::MoveToUnlockPosition:
				// 立即移动到解锁位置
				servo.SetAngle(ServoUnlockAngle);
				SendUARTMessage(UARTMessageType::ServoMovingToUnlockPosition);
				currentState = ServoState::MovingToUnlock;
				stateStartTick = osKernelGetTickCount();
				break;

			case ServoMessageType::MoveToResetPosition:
				// 立即移动到复位位置，放弃当前状态
				servo.SetAngle(ServoResetAngle);
				SendUARTMessage(UARTMessageType::ServoMovingToResetPosition);
				currentState = ServoState::MovingToReset;
				stateStartTick = osKernelGetTickCount();
				break;

			case ServoMessageType::ReleaseServo:
				// 立即释放舵机
				servo.Release();
				SendUARTMessage(UARTMessageType::ServoRelease);
				currentState = ServoState::Idle;
				break;

			default:
				break;
			}
		}

		// 状态机逻辑
		uint32_t currentTick = osKernelGetTickCount();
		uint32_t elapsedTime = currentTick - stateStartTick;

		switch (currentState) {
		case ServoState::MovingToUnlock:
			// 移动到解锁位置中
			if (elapsedTime >= ServoMoveNeedTimeMs) {
				// 移动时间到，释放舵机
				servo.Release();
				SendUARTMessage(UARTMessageType::ServoRelease);
				currentState = ServoState::UnlockReleased;
				stateStartTick = currentTick;
			}
			break;

		case ServoState::UnlockReleased:
			// 在解锁位置已释放
			if (elapsedTime >= ServoUnlockKeepTimeMs) {
				// 保持时间到，移动回复位位置
				servo.SetAngle(ServoResetAngle);
				SendUARTMessage(UARTMessageType::ServoMovingToResetPosition);
				currentState = ServoState::MovingToReset;
				stateStartTick = currentTick;
			}
			break;

		case ServoState::MovingToReset:
			// 移动到复位位置中
			if (elapsedTime >= ServoMoveNeedTimeMs) {
				// 移动时间到，释放舵机
				servo.Release();
				SendUARTMessage(UARTMessageType::ServoRelease);
				currentState = ServoState::ResetReleased;
				stateStartTick = currentTick;
			}
			break;

		case ServoState::ResetReleased:
			// 在复位位置已释放，可以清除状态
			currentState = ServoState::Idle;
			break;

		case ServoState::Idle:
			// 空闲状态，等待消息
			break;

		default:
			break;
		}

		osDelay(50);  // 延时 50ms，防止任务占用过多 CPU 时间
	}
}