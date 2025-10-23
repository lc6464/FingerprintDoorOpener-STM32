#include "cmsis_os.h"
#include "usart.h"

#include <array>
#include <cstring>

#include "UARTMessage.h"
#include "strings.h"

bool uart1TxComplete = true;
bool uart1RxComplete = false;

// static std::array<uint8_t, 128> uart1TxBuffer{};
std::array<uint8_t, 128> uart1TxBuffer{};
std::array<uint8_t, 128> uart1RxBuffer{};

void StartReceiveDMA() {
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart1RxBuffer.data(), uart1RxBuffer.size());
}

void UARTTask() {
    while (true) {
        if (uart1RxComplete) {
            uart1RxComplete = false;
            HAL_UART_Transmit_DMA(&huart1, reinterpret_cast<const uint8_t *>("UART RX Complete\n"), 18);
            StartReceiveDMA();
            osDelay(50);
        }

        if (!uart1TxComplete) {
            // 等待 DMA 传输完成
            osDelay(5); // 避免忙等待，适当延时
            continue;
        }
        uint32_t msg;
        auto status = osMessageQueueGet(UARTQueueHandle, &msg, nullptr, osWaitForever);
        if (status == osOK) {
            uart1TxComplete = false;
            UARTMessage message = *reinterpret_cast<UARTMessage *>(&msg);

            if (message.type == UARTMessageType::FingerprintMatchComplete && message.fingerprintMatchResult) {
                // 匹配成功，发送详细信息
                std::string_view prefix = "Open the door, ID=";
                auto buffer = reinterpret_cast<char *>(std::copy(prefix.begin(), prefix.end(), uart1TxBuffer.begin()));
                buffer += uint16ToString(message.fingerprintId, buffer);
                *buffer++ = '\n';
                *buffer = '\0';
                HAL_UART_Transmit_DMA(&huart1, uart1TxBuffer.data(), strlen(reinterpret_cast<char *>(uart1TxBuffer.data())));
                osDelay(5);
                continue;
            }

            auto messageTypeStr = to_string(message.type);
            auto buffer = reinterpret_cast<char *>(std::copy(messageTypeStr.begin(), messageTypeStr.end(), uart1TxBuffer.begin()));
            *buffer++ = ' ';
            buffer += uint8ToString(message.data1, buffer);
            *buffer++ = ' ';
            buffer += uint16ToString(message.data2, buffer);
            *buffer++ = '\n';
            *buffer = '\0';
            HAL_UART_Transmit_DMA(&huart1, uart1TxBuffer.data(), strlen(reinterpret_cast<char *>(uart1TxBuffer.data())));
        } else {
            uart1TxComplete = false;
            HAL_UART_Transmit_DMA(&huart1, reinterpret_cast<const uint8_t *>("UART Queue Get Error\n"), 21);
        }
        osDelay(5);
    }
}