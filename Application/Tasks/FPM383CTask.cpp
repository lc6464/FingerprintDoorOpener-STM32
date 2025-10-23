#include "cmsis_os.h"
#include "gpio.h"

#include "FPM383C_Shared.h"
#include "UARTMessage.h"

void FPM383CTask() {
    while (true) {
        bool isPressed = false;
        auto [status, ModuleErrorCode] = fpm383c.IsFingerPressed(isPressed);
        if (status != FPM383C::Status::OK) {
            // 通信存在问题，发送错误消息
            UARTMessage msg{
                .type = UARTMessageType::Error,
                .errorCode = static_cast<uint8_t>(status),
                .moduleErrorCode = static_cast<uint16_t>(ModuleErrorCode)
            };
            osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&msg), 0, 100);

            osDelay(200);
            continue;
        } else if (!isPressed) {
            // 手指未按下，继续等待
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
                .type = UARTMessageType::Error,
                .errorCode = static_cast<uint8_t>(matchStatus),
                .moduleErrorCode = static_cast<uint16_t>(matchErrCode)
            };
            osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&msg), 0, 100);

            osDelay(250);
            continue;
        }

        UARTMessage msg{
            .type = UARTMessageType::FingerprintMatchComplete,
            .fingerprintMatchResult = matchResult.IsSuccess,
            .fingerprintId = matchResult.FingerId
        };
        osMessageQueuePut(UARTQueueHandle, reinterpret_cast<uint32_t *>(&msg), 0, 100);

        osDelay(500);
    }
}