#include "usart.h"

#include "FPM383C_Shared.h"

extern bool uart1TxComplete;
extern bool uart1RxComplete;
extern std::array<uint8_t, 128> uart1TxBuffer;
extern std::array<uint8_t, 128> uart1RxBuffer;

// UART DMA 传输完成回调处理
extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        uart1TxComplete = true;
    }
}

// UART DMA 空闲中断回调处理
extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart->Instance == USART1) {
        uart1RxComplete = true;
        return;
    }

    if (huart->Instance == USART2) {
        // 调用 FPM383C 模块的接收回调处理函数
        fpm383c.UartRxCallback(Size);
    }
}