#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <numeric>
#include <span>
#include <utility> // For std::pair

// --- 平台抽象层 ---
// 该驱动支持 STM32 HAL 和 ESP32 ESP-IDF 两种平台
// 通过条件编译实现跨平台兼容

// 优先使用 FreeRTOS (CMSIS OS V2)
#if defined(USE_CUBEMX_FREERTOS)
#include "cmsis_os.h"
#endif

#if defined(USE_HAL_DRIVER) // 检查是否为 STM32 HAL 环境
#include "gpio.h"
#include "usart.h"
#include "PortPinPair.h"

using UartHandle_t = UART_HandleTypeDef *;

#elif defined(ESP_PLATFORM) // 检查是否为 ESP32 ESP-IDF 环境
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

using UartHandle_t = uart_port_t;

// 为 ESP-IDF 实现兼容的 PortPinPair 类
using GPIO_TypeDef = void;
class PortPinPair {
public:
	GPIO_TypeDef *Port; // 在 ESP-IDF 中不使用，始终为 nullptr
	gpio_num_t Pin;
	constexpr explicit PortPinPair(gpio_num_t pin) : Port(nullptr), Pin(pin) { }
};
#else
#error "Unsupported platform. Please define USE_HAL_DRIVER or ESP_PLATFORM."
#endif


// FPM383C 指纹识别模块驱动类
class FPM383C {
public:

	// FPM383C 驱动操作状态
	enum class Status : uint8_t {
		OK,                // 操作成功
		ModuleError,       // 模块返回错误 (具体错误请看 ModuleErrorCode)
		Timeout,           // 等待响应超时
		InvalidResponse,   // 无效的响应包 (帧头/校验和错误)
		TransmitError,     // 发送失败
		ReceiveError,      // 接收失败
		Busy,              // 设备正忙于另一项操作
		AsyncInProgress,   // 异步操作已成功启动
		UnknownError       // 未知错误
	};

	/**
	 * @brief 模块返回的错误码
	 * @details 完整列表请查阅用户手册
	 */
	enum class ModuleErrorCode : uint32_t {
		None = 0x00,                        // 无错误，操作成功
		CmdInvalid = 0x01,                  // 命令码无效
		DataLengthInvalid = 0x02,           // 数据长度不正确
		FieldInvalid = 0x03,                // 命令字段参数无效
		SystemBusy = 0x04,                  // 系统繁忙，无法处理当前命令
		RequestNotMet = 0x05,               // 未发送命令请求就查询结果
		SoftwareError = 0x06,               // 软件错误
		HardwareError = 0x07,               // 硬件错误
		NoFinger = 0x08,                    // 传感器上无手指
		EnrollFailed = 0x09,                // 注册失败
		MatchFailedLibEmpty = 0x0A,         // 匹配失败或指纹库为空
		DatabaseIsFull = 0x0B,              // 指纹数据库已满
		StorageWriteFailed = 0x0C,          // 存储器写入失败
		StorageReadFailed = 0x0D,           // 存储器读取失败
		ImageQualityPoor = 0x0E,            // 图像质量差，无法提取特征
		FingerprintDuplicated = 0x0F,       // 指纹重复（启用重复检查时）
		ImageAreaTooSmall = 0x10,           // 图像有效面积过小
		MoveRangeTooLarge = 0x11,           // 手指移动范围过大
		MoveRangeTooSmall = 0x12,           // 手指移动范围过小
		IdOccupied = 0x13,                  // 指定的 ID 已被占用
		ModuleCaptureFailed = 0x14,         // 模块采集图像失败
		CmdAborted = 0x15,                  // 命令被中止
		FeatureNotNeedUpdate = 0x16,        // 特征值无需更新（自学习）
		IdNotExists = 0x17,                 // 指定的 ID 不存在
		GainAdjustFailed = 0x18,            // 增益调整失败
		BufferOverflow = 0x19,              // 缓冲区溢出
		SensorSleepReceiveCmd = 0x1A,       // 传感器休眠时收到命令
		ChecksumError = 0x1C,               // 校验和错误
		FlashWriteFailedOnEnroll = 0x22,    // 注册时 Flash 写入失败
		OtherError = 0xFF                   // 其他未定义错误
	};

	// 指纹比对结果
	struct MatchResult {
		bool IsSuccess = false;      // 是否成功匹配
		uint16_t FingerId = 0xFFFF;  // 匹配到的指纹 ID
		uint16_t MatchScore = 0;     // 匹配分数
	};

	// 自动注册过程中的状态
	struct EnrollStatus {
		bool IsComplete = false;                            // 本次注册流程是否已完成
		uint8_t Step = 0;                                   // 当前完成的步骤
		uint8_t Progress = 0;                               // 注册进度
		uint16_t FingerId = 0xFFFF;                         // 最终分配/使用的指纹 ID
		ModuleErrorCode ErrorCode = ModuleErrorCode::None;  // 注册过程中的错误码
	};

	/**
	 * @brief 系统策略配置
	 * @details Bit1: 重复指纹检查, Bit2: 自学习功能, Bit4: 360度识别
	 */
	struct SystemPolicy {
		bool EnableDuplicateCheck = false;
		bool EnableSelfLearning = false;
		bool Enable360Recognition = false;
	};

	/**
	 * @brief 命令执行结果的组合返回类型
	 * @details 可通过 auto [status, errCode] = ... 进行解构
	 */
	using CommandResult = std::pair<Status, ModuleErrorCode>;

	/**
	 * @brief 构造函数
	 * @param huart UART 句柄
	 * @param touchPin 触摸检测引脚
	 * @param powerPin 电源控制引脚 (可选)
	 */
	explicit FPM383C(UartHandle_t huart, PortPinPair touchPin, PortPinPair *powerPin = nullptr)
		: _huart(huart), _touchPin(touchPin), _powerPin(powerPin) { }

	/**
	 * @brief 初始化模块
	 * @return 操作状态和模块错误码
	 */
	CommandResult Init();

	/**
	 * @brief 检查手指是否按在传感器上
	 * @param isPressed [out] 手指是否按下
	 * @return 操作状态和模块错误码
	 */
	CommandResult IsFingerPressed(bool &isPressed);

	/**
	 * @brief 1:N 同步匹配指纹
	 * @param result [out] 匹配结果
	 * @return 操作状态和模块错误码
	 */
	CommandResult Match(MatchResult &result);

	/**
	 * @brief 自动注册指纹
	 * @param fingerId 要注册的指纹 ID。如果为 0xFFFF，模块将自动分配一个未使用的 ID
	 * @param requiredPresses 需要按下的次数 (通常为 6)
	 * @param finalStatus [out] 最终的注册状态
	 * @param progressCallback [optional] 进度回调函数
	 * @return 操作状态和模块错误码
	 */
	CommandResult AutoEnroll(EnrollStatus &finalStatus, uint16_t fingerId = 0xFF, uint8_t requiredPresses = 6,
		const std::function<void(const EnrollStatus &)> &progressCallback = nullptr);

	/**
	 * @brief 删除指定 ID 的指纹
	 * @param fingerId 要删除的指纹 ID
	 * @return 操作状态和模块错误码
	 */
	CommandResult DeleteFingerprint(uint16_t fingerId);

	/**
	 * @brief 删除所有指纹
	 * @return 操作状态和模块错误码
	 */
	CommandResult DeleteAllFingerprints();

	/**
	 * @brief 获取已注册的指纹数量
	 * @param count [out] 指纹数量
	 * @return 操作状态和模块错误码
	 */
	CommandResult GetFingerprintCount(uint16_t &count);

	/**
	 * @brief 设置模块通信密码
	 * @param password 新密码
	 * @param writeToFlash 是否写入 Flash
	 * @return 操作状态和模块错误码
	 */
	CommandResult SetPassword(uint32_t password, bool writeToFlash = true);

	/**
	 * @brief 更新指纹特征值 (自学习)
	 * @details 每次匹配成功后，可调用此函数更新对应 ID 的指纹模板，提高后续匹配成功率
	 * @param fingerId 刚刚匹配成功的指纹 ID
	 * @return 操作状态和模块错误码
	 */
	CommandResult UpdateFeatureAfterMatch(uint16_t fingerId);

	/**
	 * @brief 获取系统策略
	 * @return 返回一个包含操作结果和策略设置的 pair
	 */
	std::pair<CommandResult, SystemPolicy> GetSystemPolicy();

	/**
	 * @brief 配置系统策略
	 * @param policy 要设置的策略
	 * @return 操作状态和模块错误码
	 */
	CommandResult SetSystemPolicy(const SystemPolicy &policy);


	// --- 异步方法 ---
	/**
	 * @brief 开始异步匹配 (1:N)
	 * @details 发送匹配命令后立即返回，结果通过回调函数通知
	 * @return OK 表示命令已发送，Busy 表示已有异步操作在进行
	 */
	Status StartAsyncMatch();

	/**
	 * @brief 开始异步注册
	 * @details 发送注册命令后立即返回，进度和最终结果通过回调函数通知
	 * @param fingerId 要注册的指纹 ID
	 * @param requiredPresses 需要按下的次数
	 * @return OK 表示命令已发送，Busy 表示已有异步操作在进行
	 */
	Status StartAsyncEnroll(uint16_t fingerId = 0xFFFF, uint8_t requiredPresses = 6);


	// --- 回调注册 ---
	inline void RegisterMatchCallback(const std::function<void(const MatchResult &)> &callback) { _matchCallback = callback; }
	inline void RegisterEnrollProgressCallback(const std::function<void(const EnrollStatus &)> &callback) { _enrollProgressCallback = callback; }
	inline void RegisterEnrollCompleteCallback(const std::function<void(const EnrollStatus &)> &callback) { _enrollCompleteCallback = callback; }

	/**
	 * @brief UART 接收回调处理函数
	 * @details 在 STM32 的 HAL_UARTEx_RxEventCallback 或 ESP-IDF 的 UART 事件任务中调用
	 * @param size 接收到的数据长度
	 */
	void UartRxCallback(uint16_t size);

private:
	// --- 协议常量 (参考 FPM383C 用户手册 V1.2.0) ---
	static constexpr std::array<uint8_t, 8> FRAME_HEADER = { 0xF1, 0x1F, 0xE2, 0x2E, 0xB6, 0x6B, 0xA8, 0x8A };
	static constexpr uint32_t DEFAULT_PASSWORD = 0x00000000;
	static constexpr uint32_t DEFAULT_TIMEOUT_MS = 2000;
	static constexpr uint32_t AUTO_ENROLL_TIMEOUT_MS = 15000; // 注册操作较耗时，需要更长超时

	// --- 命令码 (根据手册整理) ---
	static constexpr uint16_t CMD_AUTO_ENROLL = 0x0118;          // 自动注册
	static constexpr uint16_t CMD_MATCH_SYNC = 0x0123;           // 同步 1:N 匹配
	static constexpr uint16_t CMD_MATCH_ASYNC = 0x0121;          // 异步 1:N 匹配
	static constexpr uint16_t CMD_QUERY_MATCH_RESULT = 0x0122;   // 查询匹配结果
	static constexpr uint16_t CMD_DELETE_FINGER = 0x0131;        // 删除指纹
	static constexpr uint16_t CMD_QUERY_FINGER_STATUS = 0x0135;  // 查询手指在位状态
	static constexpr uint16_t CMD_GET_FINGER_COUNT = 0x0203;     // 获取指纹数量
	static constexpr uint16_t CMD_HEARTBEAT = 0x0303;            // 心跳/初始化
	static constexpr uint16_t CMD_SET_PASSWORD = 0x0305;         // 设置密码（写入Flash）
	static constexpr uint16_t CMD_SET_PASSWORD_TEMP = 0x0201;    // 临时设置密码（掉电丢失）
	static constexpr uint16_t CMD_UPDATE_FEATURE = 0x0116;       // 更新特征值（自学习）
	static constexpr uint16_t CMD_GET_SYSTEM_POLICY = 0x02FB;    // 获取系统策略
	static constexpr uint16_t CMD_SET_SYSTEM_POLICY = 0x02FC;    // 设置系统策略

	// --- 缓冲区大小 ---
	static constexpr size_t RX_BUFFER_SIZE = 256;
	static constexpr size_t TX_BUFFER_SIZE = 256;
	static constexpr uint8_t LINK_LAYER_HEADER_LEN = 11; // 帧头(8) + 长度(2) + 校验和(1)

	// --- 内部状态 ---
	enum class CurrentOperation {
		None,        // 无进行中的操作
		AsyncMatch,  // 异步匹配中
		AsyncEnroll  // 异步注册中
	};

	// --- 私有方法 ---
	CommandResult _sendCommandAndGetResponse(uint16_t command, std::span<const uint8_t> payload, std::span<uint8_t> &responsePayload, uint32_t timeout);
	CommandResult _handleAutoEnrollment(uint16_t fingerId, uint8_t requiredPresses, EnrollStatus &finalStatus, const std::function<void(const EnrollStatus &)> &progressCallback);

	void _handleAsyncResponse();
	Status _startAsyncOperation(uint16_t command, std::span<const uint8_t> payload, CurrentOperation op);

	size_t _buildPacket(uint16_t command, std::span<const uint8_t> payload);
	bool _parsePacket(std::span<const uint8_t> rxData, uint16_t &ackCommand, ModuleErrorCode &errorCode, std::span<uint8_t> &responsePayload);

	// 校验和计算: 取反加一
	static inline constexpr uint8_t _calculateChecksum(std::span<const uint8_t> data) {
		uint8_t sum = std::accumulate(data.begin(), data.end(), static_cast<uint8_t>(0));
		return ~sum + 1;
	}

	// --- 平台抽象 ---
	// UART 发送函数（DMA 方式）
	inline bool _uartTransmit(uint16_t size) {
#if defined(USE_HAL_DRIVER)
		return HAL_UART_Transmit_DMA(_huart, _txBuffer.data(), size) == HAL_OK;
#elif defined(ESP_PLATFORM)
		return uart_write_bytes(_huart, _txBuffer.data(), size) == size;
#endif
	}

	// UART 接收函数（DMA 空闲中断方式）
	inline bool _uartReceive() {
#if defined(USE_HAL_DRIVER)
		return HAL_UARTEx_ReceiveToIdle_DMA(_huart, _rxBuffer.data(), _rxBuffer.size()) == HAL_OK;
#elif defined(ESP_PLATFORM)
		return true; // ESP-IDF DMA is event-driven
#endif
	}

	// 电源控制 (低电平有效)
	inline void _setPower(bool on) {
		if (!_powerPin) return;
#if defined(USE_HAL_DRIVER)
		HAL_GPIO_WritePin(_powerPin->Port, _powerPin->Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
#elif defined(ESP_PLATFORM)
		gpio_set_level(_powerPin->Pin, on ? 0 : 1);
#endif
	}

	// --- 成员变量 ---
	UartHandle_t _huart;         // UART 句柄
	PortPinPair _touchPin;       // 触摸感应引脚
	PortPinPair *_powerPin;      // 电源控制引脚 (可选)
	uint32_t _password = DEFAULT_PASSWORD; // 通信密码

	std::array<uint8_t, RX_BUFFER_SIZE> _rxBuffer; // 接收缓冲区
	std::array<uint8_t, TX_BUFFER_SIZE> _txBuffer; // 发送缓冲区

	// 同步调用状态
	bool _isResponseReady = false;  // 响应就绪标志
	uint16_t _lastRxSize = 0;       // 最后接收到的数据长度

	// 异步操作状态
	CurrentOperation _currentOperation = CurrentOperation::None;
	uint16_t _asyncEnrollFingerId = 0;         // 异步注册的指纹 ID
	uint8_t _asyncEnrollRequiredPresses = 0;   // 异步注册需要的按压次数

	// 异步回调函数
	std::function<void(const MatchResult &)> _matchCallback;           // 匹配完成回调
	std::function<void(const EnrollStatus &)> _enrollProgressCallback; // 注册进度回调
	std::function<void(const EnrollStatus &)> _enrollCompleteCallback; // 注册完成回调
};