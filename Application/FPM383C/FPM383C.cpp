#include "FPM383C.h"
#include <algorithm> // for std::copy

// Only for Debugging
// #include <cstdio>

// ============================================================================
// 平台抽象层辅助函数
// ============================================================================

/**
 * @brief 跨平台延时函数
 * @param ms 延时毫秒数
 * @details 根据不同平台选择合适的延时实现:
 *          - FreeRTOS CMSIS: 使用 osDelay (任务挂起)
 *          - ESP32: 使用 vTaskDelay (任务挂起)
 *          - STM32 HAL (无OS): 使用 HAL_Delay (阻塞延时)
 */
static inline void platform_delay(uint32_t ms) {
#if defined(osCMSIS_FreeRTOS)
	osDelay(ms);
#elif defined(ESP_PLATFORM)
	vTaskDelay(pdMS_TO_TICKS(ms));
#elif defined(USE_HAL_DRIVER)
	HAL_Delay(ms);
#endif
}

/**
 * @brief 跨平台系统滴答获取函数
 * @return 当前系统滴答数 (单位: 毫秒)
 * @details 用于超时判断，根据不同平台获取系统运行时间
 */
static inline uint32_t platform_get_tick() {
#if defined(osCMSIS_FreeRTOS)
	return osKernelGetTickCount();
#elif defined(ESP_PLATFORM)
	return pdTICKS_TO_MS(xTaskGetTickCount());
#elif defined(USE_HAL_DRIVER)
	return HAL_GetTick();
#endif
}

/**
 * @brief 初始化指纹模块
 * @return 命令执行结果 (状态 + 错误码)
 * @details 执行流程:
 *          1. 打开模块电源
 *          2. 等待 300ms 让模块上电稳定
 *          3. 发送心跳命令验证通信是否正常
 */
FPM383C::CommandResult FPM383C::Init() {
	// _setPower(true);
	// platform_delay(300); // 等待模块上电稳定，必要的初始化时间
	std::span<uint8_t> response;
	return _sendCommandAndGetResponse(CMD_HEARTBEAT, {}, response, DEFAULT_TIMEOUT_MS);
}

/**
 * @brief 查询手指是否按在传感器上
 * @param isPressed [out] 返回手指是否按下 (true=按下, false=未按下)
 * @return 命令执行结果
 * @details 响应负载格式: [状态字节] (1=按下, 0=未按下)
 */
FPM383C::CommandResult FPM383C::IsFingerPressed(bool &isPressed) {
	isPressed = false;
	std::span<uint8_t> response;
	CommandResult result = _sendCommandAndGetResponse(CMD_QUERY_FINGER_STATUS, {}, response, DEFAULT_TIMEOUT_MS);

	auto &[status, errCode] = result;
	if (status == Status::OK) {
		if (!response.empty()) {
			isPressed = (response[0] == 1);
		} else {
			status = Status::InvalidResponse;
		}
	}
	return result;
}

/**
 * @brief 执行同步 1:N 指纹匹配
 * @param result [out] 匹配结果结构体 (成功标志 + 指纹ID + 匹配分数)
 * @return 命令执行结果
 * @details 响应负载格式 (5字节):
 *          [0-1]: 匹配结果 (big-endian, uint16, 1=成功, 0=失败)
 *          [2-3]: 匹配分数 (big-endian, uint16)
 *          [4-5]: 指纹 ID (big-endian, uint16)
 *          该函数会阻塞等待用户按下手指并完成匹配
 */
FPM383C::CommandResult FPM383C::Match(MatchResult &result) {
	result = { false, 0, 0 };
	std::span<uint8_t> response;
	CommandResult cmdResult = _sendCommandAndGetResponse(CMD_MATCH_SYNC, {}, response, DEFAULT_TIMEOUT_MS);

	auto &[status, errCode] = cmdResult;
	if (status == Status::OK) {
		if (response.size() >= 5) {
			result.IsSuccess = (response[1] == 1);
			if (result.IsSuccess) {
				// 大端序解析匹配分数和指纹 ID
				result.MatchScore = (static_cast<uint16_t>(response[2]) << 8) | response[3];
				result.FingerId = (static_cast<uint16_t>(response[4]) << 8) | response[5];
			}
		} else if (errCode == ModuleErrorCode::None) {
			result.IsSuccess = false;
		} else {
			result.IsSuccess = false;
		}
	}
	return cmdResult;
}

FPM383C::CommandResult FPM383C::AutoEnroll(EnrollStatus &finalStatus, uint16_t fingerId/* = 0xFF*/, uint8_t requiredPresses/* = 6*/,
	const std::function<void(const EnrollStatus &)> &progressCallback/* = nullptr*/) {
	return _handleAutoEnrollment(fingerId, requiredPresses, finalStatus, progressCallback);
}

FPM383C::Status FPM383C::StartAsyncMatch() {
	return _startAsyncOperation(CMD_MATCH_ASYNC, {}, CurrentOperation::AsyncMatch);
}

FPM383C::Status FPM383C::StartAsyncEnroll(uint16_t fingerId, uint8_t requiredPresses) {
	_asyncEnrollFingerId = fingerId;
	_asyncEnrollRequiredPresses = requiredPresses;
	// 构造自动注册命令负载
	const std::array<uint8_t, 4> payload = {
		0x01, // 手指抬起检测标志: 0=不检测, 1=检测 (要求用户每次采集后抬起手指)
		_asyncEnrollRequiredPresses,
		static_cast<uint8_t>(_asyncEnrollFingerId >> 8),
		static_cast<uint8_t>(_asyncEnrollFingerId & 0xFF)
	};
	return _startAsyncOperation(CMD_AUTO_ENROLL, payload, CurrentOperation::AsyncEnroll);
}

FPM383C::CommandResult FPM383C::DeleteFingerprint(uint16_t fingerId) {
	const std::array<uint8_t, 5> payload = {
		0x00, // 删除单个指纹
		static_cast<uint8_t>(fingerId >> 8),
		static_cast<uint8_t>(fingerId & 0xFF),
		0x00, 0x00
	};
	std::span<uint8_t> response;
	return _sendCommandAndGetResponse(CMD_DELETE_FINGER, payload, response, DEFAULT_TIMEOUT_MS);
}

FPM383C::CommandResult FPM383C::DeleteAllFingerprints() {
	static constexpr std::array<uint8_t, 5> payload = {
		0x01, // 删除所有指纹
		0x00, 0x00, 0x00, 0x00
	};
	std::span<uint8_t> response;
	return _sendCommandAndGetResponse(CMD_DELETE_FINGER, payload, response, DEFAULT_TIMEOUT_MS);
}

FPM383C::CommandResult FPM383C::GetFingerprintCount(uint16_t &count) {
	count = 0;
	std::span<uint8_t> response;
	CommandResult result = _sendCommandAndGetResponse(CMD_GET_FINGER_COUNT, {}, response, DEFAULT_TIMEOUT_MS);

	auto &[status, errCode] = result;
	if (status == Status::OK) {
		if (response.size() >= 2) {
			count = (static_cast<uint16_t>(response[0]) << 8) | response[1];
		} else {
			status = Status::InvalidResponse;
		}
	}
	return result;
}

FPM383C::CommandResult FPM383C::SetPassword(uint32_t password, bool writeToFlash/* = true*/) {
	std::array<uint8_t, 4> payload = {
		static_cast<uint8_t>(password >> 24),
		static_cast<uint8_t>(password >> 16),
		static_cast<uint8_t>(password >> 8),
		static_cast<uint8_t>(password & 0xFF),
	};
	std::span<uint8_t> response;
	CommandResult result = _sendCommandAndGetResponse(writeToFlash ? CMD_SET_PASSWORD : CMD_SET_PASSWORD_TEMP,
		payload, response, DEFAULT_TIMEOUT_MS);

	if (result.first == Status::OK) {
		_password = password; // 同步更新当前会话密码
	}
	return result;
}

FPM383C::CommandResult FPM383C::UpdateFeatureAfterMatch(uint16_t fingerId) {
	const std::array<uint8_t, 2> payload = {
		static_cast<uint8_t>(fingerId >> 8),
		static_cast<uint8_t>(fingerId & 0xFF),
	};
	std::span<uint8_t> response;
	return _sendCommandAndGetResponse(CMD_UPDATE_FEATURE, payload, response, DEFAULT_TIMEOUT_MS);
}

std::pair<FPM383C::CommandResult, FPM383C::SystemPolicy> FPM383C::GetSystemPolicy() {
	SystemPolicy policy;
	std::span<uint8_t> response;
	CommandResult result = _sendCommandAndGetResponse(CMD_GET_SYSTEM_POLICY, {}, response, DEFAULT_TIMEOUT_MS);

	auto &[status, errCode] = result;
	if (status == Status::OK) {
		if (response.size() >= 4) {
			// 根据手册 V1.2.0，策略配置位于响应数据的第4个字节 (little-endian)
			const uint8_t policyByte = response[3];
			policy.EnableDuplicateCheck = (policyByte & (1 << 1)) != 0;
			policy.EnableSelfLearning = (policyByte & (1 << 2)) != 0;
			policy.Enable360Recognition = (policyByte & (1 << 4)) != 0;
		} else {
			status = Status::InvalidResponse;
		}
	}
	return { result, policy };
}

// FPM383C::CommandResult FPM383C::SetSystemPolicy(const FPM383C::SystemPolicy &policy) {
// 	// 将策略结构体转换为位掩码
// 	uint32_t policyValue = 0;
// 	if (policy.EnableDuplicateCheck) policyValue |= (1 << 1);
// 	if (policy.EnableSelfLearning)   policyValue |= (1 << 2);
// 	if (policy.Enable360Recognition) policyValue |= (1 << 4);

// 	const std::array<uint8_t, 4> payload = {
// 		static_cast<uint8_t>(policyValue & 0xFF),
// 		static_cast<uint8_t>((policyValue >> 8) & 0xFF),
// 		static_cast<uint8_t>((policyValue >> 16) & 0xFF),
// 		static_cast<uint8_t>((policyValue >> 24) & 0xFF)
// 	};
// 	std::span<uint8_t> response;
// 	return _sendCommandAndGetResponse(CMD_SET_SYSTEM_POLICY, payload, response, DEFAULT_TIMEOUT_MS);
// }

FPM383C::CommandResult FPM383C::EnterSleepMode(bool isDeepSleep/* = false*/) {
	const std::array<uint8_t, 1> payload = {
		isDeepSleep ? static_cast<uint8_t>(0x01) : static_cast<uint8_t>(0x00)
	};
	std::span<uint8_t> response;
	return _sendCommandAndGetResponse(CMD_ENTER_SLEEP_MODE, payload, response, DEFAULT_TIMEOUT_MS);
}

FPM383C::CommandResult FPM383C::SetLEDControl(const FPM383C::LEDControl::ControlInfo &controlInfo) {
	// 由于联合体内存布局一致，直接访问 Raw 即可获取所有参数
	const auto &rawParams = controlInfo.GetRawParams();

	std::array<uint8_t, 5> payload = {
		static_cast<uint8_t>(controlInfo.ControlMode),
		static_cast<uint8_t>(controlInfo.LightColor),
		rawParams[0],
		rawParams[1],
		rawParams[2]
	};

	std::span<uint8_t> response;
	return _sendCommandAndGetResponse(CMD_SET_LED_CONTROL, payload, response, DEFAULT_TIMEOUT_MS);
}

void FPM383C::UartRxCallback(uint16_t size) {
	_lastRxSize = size;

	if (_currentOperation != CurrentOperation::None) {
		// 异步操作模式: 在中断中直接处理响应，避免阻塞主循环
		_handleAsyncResponse();
	} else {
		// 同步操作模式: 仅设置标志位，由 _sendCommandAndGetResponse() 处理
		_isResponseReady = true;
	}
}

// ============================================================================
// 私有方法实现
// ============================================================================

/**
 * @brief 发送命令并等待响应 (同步阻塞模式)
 * @param command 命令码
 * @param payload 命令负载数据
 * @param responsePayload [out] 返回的响应负载 (引用，指向内部缓冲区)
 * @param timeout 超时时间 (毫秒)
 * @return 命令执行结果
 * @details 执行流程:
 *          1. 检查是否有异步操作正在进行
 *          2. 启动 UART DMA 接收
 *          3. 构造并发送命令包
 *          4. 阻塞等待响应或超时
 *          5. 解析响应包并返回结果
 *          注意: 该函数会占用 CPU 进行轮询等待
 */
FPM383C::CommandResult FPM383C::_sendCommandAndGetResponse(uint16_t command, std::span<const uint8_t> payload,
	std::span<uint8_t> &responsePayload, uint32_t timeout) {
	// 检查是否有异步操作正在进行
	if (_currentOperation != CurrentOperation::None) {
		return { Status::Busy, ModuleErrorCode::None };
	}

	_isResponseReady = false;
	_lastRxSize = 0;

	// 启动 UART DMA 接收 (空闲中断模式)
	if (!_uartReceive()) {
		return { Status::ReceiveError, ModuleErrorCode::None };
	}

	// 构造并发送命令包
	size_t packetSize = _buildPacket(command, payload);
	if (!_uartTransmit(packetSize)) {
		return { Status::TransmitError, ModuleErrorCode::None };
	}

	// 阻塞等待响应或超时
	uint32_t startTime = platform_get_tick();
	while (!_isResponseReady) {
		if (platform_get_tick() - startTime > timeout) {
			// 超时，中止接收操作
#if defined(USE_HAL_DRIVER)
			HAL_UART_AbortReceive_IT(_huart);
#elif defined(ESP_PLATFORM)
			uart_flush(_huart);
#endif
			return { Status::Timeout, ModuleErrorCode::None };
		}
		platform_delay(5); // 短暂延时，避免 CPU 空转
	}

	// 解析响应包
	uint16_t ackCommand;
	ModuleErrorCode errorCode;
	if (_parsePacket({ _rxBuffer.data(), _lastRxSize }, ackCommand, errorCode, responsePayload)) {
		if (errorCode == ModuleErrorCode::None) {
			return { Status::OK, ModuleErrorCode::None };
		}
		// 模块返回了错误码
		return { Status::ModuleError, errorCode };
	}

	// 响应包格式错误 (帧头或校验和不匹配)
	return { Status::InvalidResponse, ModuleErrorCode::None };
}

/**
 * @brief 处理自动注册流程 (同步阻塞模式)
 * @param fingerId 要注册的指纹 ID (0xFFFF 表示自动分配)
 * @param requiredPresses 需要采集的次数 (通常为 6)
 * @param finalStatus [out] 最终注册状态
 * @param progressCallback [可选] 进度回调函数
 * @return 命令执行结果
 * @details 注册过程说明:
 *          1. 模块需要多次采集同一手指的指纹
 *          2. 每次采集完成后返回一次响应，包含当前步骤和进度
 *          3. Step=0xFF 时表示注册完成
 *          4. 抬起手指检测标志设置为 1，要求用户每次采集后抬起手指
 *          5. 超时时间较长 (15秒)，因为用户可能需要时间调整手指位置
 *          响应负载格式 (5字节):
 *          [0]: 步骤号 (1-N, 0xFF=完成)
 *          [1-2]: 指纹 ID (big-endian)
 *          [3]: 保留
 *          [4]: 进度 (0-100)
 */
FPM383C::CommandResult FPM383C::_handleAutoEnrollment(uint16_t fingerId, uint8_t requiredPresses, EnrollStatus &finalStatus,
	const std::function<void(const EnrollStatus &)> &progressCallback) {
	if (_currentOperation != CurrentOperation::None) {
		return { Status::Busy, ModuleErrorCode::None };
	}

	// 构造注册命令负载
	const std::array<uint8_t, 4> payload = {
		0x01, // 手指抬起检测标志: 0=不检测, 1=检测 (要求用户每次采集后抬起手指)
		requiredPresses,
		static_cast<uint8_t>(fingerId >> 8),
		static_cast<uint8_t>(fingerId & 0xFF)
	};

	_isResponseReady = false;
	if (!_uartReceive()) {
		return { Status::ReceiveError, ModuleErrorCode::None };
	}
	size_t packetSize = _buildPacket(CMD_AUTO_ENROLL, payload);
	if (!_uartTransmit(packetSize)) {
		return { Status::TransmitError, ModuleErrorCode::None };
	}

	uint32_t startTime = platform_get_tick();

	// 循环接收多次响应，直到注册完成
	while (true) {
		// 等待本次响应
		while (!_isResponseReady) {
			if (platform_get_tick() - startTime > AUTO_ENROLL_TIMEOUT_MS) {
				// 超时，中止操作
#if defined(USE_HAL_DRIVER)
				HAL_UART_AbortReceive_IT(_huart);
#elif defined(ESP_PLATFORM)
				uart_flush(_huart);
#endif
				return { Status::Timeout, ModuleErrorCode::None };
			}
			platform_delay(1);
		}

		uint16_t ackCmd;
		ModuleErrorCode errCode;
		std::span<uint8_t> respPayload;

		if (_parsePacket({ _rxBuffer.data(), _lastRxSize }, ackCmd, errCode, respPayload)) {
			if (errCode != ModuleErrorCode::None) {
				// 注册过程中发生错误，流程终止
				finalStatus.IsComplete = true;
				finalStatus.ErrorCode = errCode;
				if (progressCallback) progressCallback(finalStatus);
				return { Status::ModuleError, errCode };
			}

			// 解析注册进度响应
			if (respPayload.size() >= 5) {
				finalStatus.Step = respPayload[0];
				finalStatus.FingerId = (static_cast<uint16_t>(respPayload[1]) << 8) | respPayload[2];
				finalStatus.Progress = respPayload[4];
				finalStatus.ErrorCode = ModuleErrorCode::None;

				// Step=0xFF 表示注册完成
				finalStatus.IsComplete = (finalStatus.Step == 0xFF);

				// 调用进度回调，通知上层应用当前状态
				if (progressCallback) {
					progressCallback(finalStatus);
				}

				if (finalStatus.IsComplete) {
					return { Status::OK, ModuleErrorCode::None };
				}
			}
		} else {
			return { Status::InvalidResponse, ModuleErrorCode::None };
		}

		// 准备接收下一次响应
		_isResponseReady = false;
		if (!_uartReceive()) return { Status::ReceiveError, ModuleErrorCode::None };
		// 重置超时计时器，避免多步操作累积超时
		startTime = platform_get_tick();
	}
}

/**
 * @brief 处理异步操作的响应
 * @details 该函数在 UART 接收中断回调中被调用
 *          根据当前异步操作类型 (_currentOperation) 解析响应并调用对应的用户回调函数
 *          支持两种异步操作:
 *          1. 异步匹配: 单次响应，完成后重置状态
 *          2. 异步注册: 多次响应，直到注册完成才重置状态
 */
void FPM383C::_handleAsyncResponse() {
	uint16_t ackCmd;
	ModuleErrorCode errCode;
	std::span<uint8_t> respPayload;

	// 解析响应包
	if (!_parsePacket({ _rxBuffer.data(), _lastRxSize }, ackCmd, errCode, respPayload)) {
		// 解析失败，重置状态以允许后续操作
		_currentOperation = CurrentOperation::None;
		return;
	}

	switch (_currentOperation) {
	case CurrentOperation::AsyncMatch:
	{
		// 处理异步匹配响应
		MatchResult result = { false, 0, 0 };
		if (errCode == ModuleErrorCode::None && respPayload.size() >= 5) {
			result.IsSuccess = (respPayload[0] == 1);
			if (result.IsSuccess) {
				// 解析匹配分数和指纹 ID
				result.MatchScore = (static_cast<uint16_t>(respPayload[1]) << 8) | respPayload[2];
				result.FingerId = (static_cast<uint16_t>(respPayload[3]) << 8) | respPayload[4];
			}
		} else if (errCode == ModuleErrorCode::MatchFailedLibEmpty) {
			result.IsSuccess = false;
		}

		// 调用匹配回调函数通知结果
		if (_matchCallback) {
			_matchCallback(result);
		}
		_currentOperation = CurrentOperation::None; // 匹配操作完成，重置状态
		break;
	}

	case CurrentOperation::AsyncEnroll:
	{
		// 处理异步注册响应
		EnrollStatus enrollStatus;
		enrollStatus.ErrorCode = errCode;

		if (errCode == ModuleErrorCode::None && respPayload.size() >= 5) {
			enrollStatus.Step = respPayload[0];
			enrollStatus.FingerId = (static_cast<uint16_t>(respPayload[1]) << 8) | respPayload[2];
			enrollStatus.Progress = respPayload[4];
			enrollStatus.IsComplete = (enrollStatus.Step == 0xFF);
		} else {
			// 发生错误，注册流程终止
			enrollStatus.IsComplete = true;
		}

		// 调用进度回调
		if (_enrollProgressCallback) {
			_enrollProgressCallback(enrollStatus);
		}

		if (enrollStatus.IsComplete) {
			// 注册完成，调用完成回调
			if (_enrollCompleteCallback) {
				_enrollCompleteCallback(enrollStatus);
			}
			_currentOperation = CurrentOperation::None; // 注册完成，重置状态
		} else {
			// 注册未完成，继续等待下一次响应
			if (!_uartReceive()) {
				_currentOperation = CurrentOperation::None; // 接收启动失败，终止流程
			}
		}
		break;
	}
	case CurrentOperation::None:
	default:
		break;
	}
}

/**
 * @brief 启动异步操作
 * @param command 命令码
 * @param payload 命令负载
 * @param op 操作类型 (AsyncMatch 或 AsyncEnroll)
 * @return 操作状态
 * @details 执行流程:
 *          1. 检查当前是否有异步操作在进行
 *          2. 启动 UART DMA 接收
 *          3. 构造并发送命令包
 *          4. 立即返回，不等待响应
 *          响应将在 UART 中断回调中由 _handleAsyncResponse() 处理
 */
FPM383C::Status FPM383C::_startAsyncOperation(uint16_t command, std::span<const uint8_t> payload, CurrentOperation op) {
	if (_currentOperation != CurrentOperation::None) {
		return Status::Busy;
	}
	_currentOperation = op;

	if (!_uartReceive()) {
		_currentOperation = CurrentOperation::None;
		return Status::ReceiveError;
	}

	size_t packetSize = _buildPacket(command, payload);
	if (!_uartTransmit(packetSize)) {
		_currentOperation = CurrentOperation::None;
		return Status::TransmitError;
	}

	return Status::AsyncInProgress;
}

/**
 * @brief 构造完整的命令数据包
 * @param command 命令码
 * @param payload 命令负载数据
 * @return 构造的数据包总长度
 * @details 数据包结构 (参考 FPM383C 用户手册 V1.2.0):
 *
 *          链路层 (11字节):
 *          [0-7]:  帧头 (固定: F1 1F E2 2E B6 6B A8 8A)
 *          [8-9]:  应用层数据长度 (big-endian, uint16)
 *          [10]:   链路层校验和 (前10字节的校验和)
 *
 *          应用层 (可变长度):
 *          [0-3]:  通信密码 (big-endian, uint32)
 *          [4-5]:  命令码 (big-endian, uint16)
 *          [6-N]:  命令负载 (可选)
 *          [N+1]:  应用层校验和 (应用层数据的校验和)
 *
 *          校验和算法: 累加所有字节，取反加一 (~sum + 1)
 */
size_t FPM383C::_buildPacket(uint16_t command, std::span<const uint8_t> payload) {
	// 计算应用层数据长度: 密码(4) + 命令(2) + 负载(N) + 校验和(1)
	const uint16_t appDataLen = 4 + 2 + payload.size() + 1;

	// 填充帧头 (8字节固定值)
	std::copy(FRAME_HEADER.begin(), FRAME_HEADER.end(), _txBuffer.begin());

	// 填充应用层数据长度 (big-endian)
	_txBuffer[8] = static_cast<uint8_t>(appDataLen >> 8);
	_txBuffer[9] = static_cast<uint8_t>(appDataLen & 0xFF);

	// 计算并填充链路层校验和
	_txBuffer[10] = _calculateChecksum({ _txBuffer.data(), 10 });

	// 填充应用层数据
	size_t offset = 11;

	// 通信密码 (big-endian, 4字节)
	_txBuffer[offset++] = static_cast<uint8_t>(_password >> 24);
	_txBuffer[offset++] = static_cast<uint8_t>(_password >> 16);
	_txBuffer[offset++] = static_cast<uint8_t>(_password >> 8);
	_txBuffer[offset++] = static_cast<uint8_t>(_password & 0xFF);

	// 命令码 (big-endian, 2字节)
	_txBuffer[offset++] = static_cast<uint8_t>(command >> 8);
	_txBuffer[offset++] = static_cast<uint8_t>(command & 0xFF);

	// 命令负载 (可选)
	if (!payload.empty()) {
		std::copy(payload.begin(), payload.end(), _txBuffer.begin() + offset);
		offset += payload.size();
	}

	// 计算并填充应用层校验和
	_txBuffer[offset] = _calculateChecksum({ _txBuffer.data() + 11, appDataLen - 1u });
	offset++;

	// Only for Debugging: Print the constructed packet
	// char debugStr[256];
	// size_t debugLen = 0;
	// for (size_t i = 0; i < offset; ++i) {
	// 	debugLen += snprintf(debugStr + debugLen, sizeof(debugStr) - debugLen, "%02X ", _txBuffer[i]);
	// }
	// debugStr[debugLen] = '\0'; // 结束字符串
	// HAL_UART_Transmit(&huart1, reinterpret_cast<uint8_t *>(debugStr), static_cast<uint16_t>(debugLen), 100);

	return offset;
}

/**
 * @brief 解析接收到的响应数据包
 * @param rxData 接收到的原始数据
 * @param ackCommand [out] 返回的命令码
 * @param errorCode [out] 返回的错误码
 * @param responsePayload [out] 返回的响应负载 (引用，指向 rxData 内部)
 * @return true=解析成功, false=解析失败
 * @details 解析流程:
 *          1. 验证数据长度是否足够
 *          2. 验证帧头是否匹配
 *          3. 验证链路层校验和
 *          4. 提取应用层数据长度
 *          5. 验证应用层校验和
 *          6. 提取命令码、错误码和响应负载
 *
 *          响应包结构 (参考 FPM383C 用户手册 V1.2.0):
 *
 *          链路层 (11字节):
 *          [0-7]:  帧头 (固定: F1 1F E2 2E B6 6B A8 8A)
 *          [8-9]:  应用层数据长度 (big-endian, uint16)
 *          [10]:   链路层校验和
 *
 *          应用层 (可变长度):
 *          [0-3]:  通信密码 (big-endian, uint32)
 *          [4-5]:  命令码 (big-endian, uint16)
 *          [6-9]:  错误码 (big-endian, uint32)
 *          [10-N]: 响应负载 (可选)
 *          [N+1]:  应用层校验和
 */
bool FPM383C::_parsePacket(std::span<const uint8_t> rxData, uint16_t &ackCommand, ModuleErrorCode &errorCode,
	std::span<uint8_t> &responsePayload) {
	// 1. 检查最小长度 (至少要有链路层头)
	if (rxData.size() < LINK_LAYER_HEADER_LEN) return false;

	// 2. 验证帧头
	if (!std::equal(rxData.begin(), rxData.begin() + 8, FRAME_HEADER.begin())) return false;

	// 3. 验证链路层校验和
	const uint8_t headerChecksum = _calculateChecksum({ rxData.data(), 10 });
	if (headerChecksum != rxData[10]) return false;

	// 4. 提取应用层数据长度 (big-endian)
	const uint16_t appDataLen = (static_cast<uint16_t>(rxData[8]) << 8) | static_cast<uint16_t>(rxData[9]);
	if (rxData.size() < static_cast<size_t>(LINK_LAYER_HEADER_LEN + appDataLen)) return false;

	// 5. 获取应用层数据
	const std::span<const uint8_t> appData = { rxData.data() + LINK_LAYER_HEADER_LEN, appDataLen };

	// 6. 验证应用层校验和 (最后一个字节)
	const uint8_t appChecksum = _calculateChecksum({ appData.data(), appData.size() - 1u });
	if (appChecksum != appData.back()) return false;

	// 7. 检查应用层最小长度: 密码(4) + 命令(2) + 错误码(4) = 10字节 (不含校验和)
	if (appData.size() < 10) return false;

	// 8. 提取命令码 (big-endian)
	ackCommand = (static_cast<uint16_t>(appData[4]) << 8) | static_cast<uint16_t>(appData[5]);

	// 9. 提取错误码 (big-endian, 4字节)
	errorCode = static_cast<ModuleErrorCode>(
		(static_cast<uint32_t>(appData[6]) << 24) |
		(static_cast<uint32_t>(appData[7]) << 16) |
		(static_cast<uint32_t>(appData[8]) << 8) |
		(static_cast<uint32_t>(appData[9]))
		);

	// 10. 提取响应负载 (如果有)
	size_t payloadOffset = 10;
	if (appData.size() > payloadOffset + 1) { // +1 是因为最后一字节是校验和
		// 响应负载长度 = 应用层数据长度 - (密码4 + 命令2 + 错误码4 + 校验和1)
		responsePayload = { const_cast<uint8_t *>(appData.data() + payloadOffset), appData.size() - payloadOffset - 1 };
	} else {
		responsePayload = {};
	}

	return true;
}