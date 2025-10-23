#include "FlashConfig.h"

#include <limits> // 用于 std::numeric_limits

FlashConfig::Status FlashConfig::Init() {
	_lastConfigAddress = _findLatestConfig();

	if (_lastConfigAddress == std::numeric_limits<uint32_t>::max()) {
		// 找到了无效的配置块
		_loadedCount = 0;
		// 将地址设置为最大值，视为无效 Magic Header
		_lastConfigAddress = std::numeric_limits<uint32_t>::max();

		return Status::InvalidConfigHeader;
	}

	if (_lastConfigAddress != 0) {
		// 找到了一个配置，读取它的内容
		// 将地址重新解释为字节指针以进行偏移计算
		const auto *ptr = reinterpret_cast<const uint8_t *>(_lastConfigAddress);

		// 跳过魔术字 (在 _findLatestConfig 中已验证)
		ptr += sizeof(MAGIC_HEADER);

		// 使用 reinterpret_cast 直接读取计数值，无需内存拷贝
		const uint16_t count = *reinterpret_cast<const uint16_t *>(ptr);
		ptr += sizeof(count);

		if (count > 0 && count <= MAX_CONFIG_ITEMS) {
			_loadedCount = count;
			// 将 Flash 中的数据拷贝到 RAM 缓存中
			// 源指针被重新解释为 Config 指针，以便 std::copy 正确工作
			const auto *source = reinterpret_cast<const Config *>(ptr);
			std::copy(source, source + count, _loadedConfig.begin());
			return Status::Ok;
		}
	}

	// 没有找到有效的配置
	_loadedCount = 0;
	// 将地址设置为页尾，为首次写入做准备
	_lastConfigAddress = CONFIG_PAGE_ADDRESS + FLASH_PAGE_SIZE_CONST;
	return Status::ConfigNotFound;
}

FlashConfig::Status FlashConfig::WriteConfig(ConfigList configList) {
	uint16_t count = configList.size();

	if (count > MAX_CONFIG_ITEMS) {
		return Status::DataTooLarge;
	}

	if (_lastConfigAddress == std::numeric_limits<uint32_t>::max()) {
		// 如果 Magic Header 无效，无法写入
		return Status::InvalidConfigHeader;
	}

	// 计算新配置块所需的总大小
	// 大小 = 魔术字(2) + 计数值(2) + N * 项目 (N * 4)
	const uint32_t totalSizeBytes = sizeof(MAGIC_HEADER) + sizeof(count) + (count * sizeof(Config));

	if (totalSizeBytes > FLASH_PAGE_SIZE_CONST) {
		return Status::DataTooLarge;
	}

	uint32_t writeAddress = _lastConfigAddress - totalSizeBytes;

	// 检查新数据是否能在不擦除的情况下放入剩余空间
	if (writeAddress < CONFIG_PAGE_ADDRESS) {
		// 空间不足，需要擦除
		Status eraseStatus = _erasePage();
		if (eraseStatus != Status::Ok) {
			return eraseStatus;
		}
		// 擦除后，新的写入地址位于页面的最末端
		writeAddress = CONFIG_PAGE_ADDRESS + FLASH_PAGE_SIZE_CONST - totalSizeBytes;
	}

	// 在栈上准备数据缓冲区，其大小由编译期常量确定，避免动态分配
	// 缓冲区的大小与数据结构严格对齐，以便进行32位写入
	std::array<uint8_t, sizeof(uint32_t) + MAX_CONFIG_ITEMS * sizeof(Config)> buffer;

	// 组合写入魔术字和计数值，打包成一个32位数，以利用更快的字写入
	// 在小端系统中，MAGIC_HEADER 会被存放在较低的地址
	const uint32_t headerAndCount = (static_cast<uint32_t>(count) << 16) | MAGIC_HEADER;
	*reinterpret_cast<uint32_t *>(buffer.data()) = headerAndCount;

	// 写入配置项数据
	if (count > 0) {
		// 使用 std::copy 将用户数据拷贝到缓冲区
		// 目标地址是缓冲区中头部之后的位置
		auto *dest = reinterpret_cast<Config *>(buffer.data() + sizeof(headerAndCount));
		std::copy(configList.begin(), configList.end(), dest);
	}

	// 将完整的缓冲区写入 Flash
	// 即使 count 为 0，仍然会写入魔术字和计数值，以覆盖先前的配置
	Status writeStatus = _writeToFlash(writeAddress, buffer.data(), totalSizeBytes);

	if (writeStatus == Status::Ok) {
		// 写入成功后，更新内部状态
		_lastConfigAddress = writeAddress;
		_loadedCount = count;
		if (count > 0) {
			std::copy(configList.begin(), configList.end(), _loadedConfig.begin());
		}
	}

	return writeStatus;
}

uint16_t FlashConfig::GetValue(uint16_t key) const {
	for (uint16_t i = 0; i < _loadedCount; ++i) {
		if (_loadedConfig[i].key == key) {
			return _loadedConfig[i].value;
		}
	}
	return INVALID_VALUE;
}

// --- 私有方法 ---

uint32_t FlashConfig::_findLatestConfig() {
	// 逐个半字（16位）扫描整个页面，以找到第一个魔术字
	// 由于反向写入策略，找到的第一个就是最新写入的那个
	for (uint32_t addr = CONFIG_PAGE_ADDRESS; addr < CONFIG_PAGE_ADDRESS + FLASH_PAGE_SIZE_CONST; addr += 2) {
		uint16_t content = *reinterpret_cast<const volatile uint16_t *>(addr);
		if (content == MAGIC_HEADER) {
			return addr; // 找到了
		}
		if (content != std::numeric_limits<uint16_t>::max()) {
			// 找到了无效的配置块
			return std::numeric_limits<uint32_t>::max(); // 返回最大值表示无效配置
		}
	}
	return 0; // 未找到
}

FlashConfig::Status FlashConfig::_erasePage() {
	HAL_StatusTypeDef status;
	FLASH_EraseInitTypeDef eraseInitStruct;
	uint32_t pageError = 0;

	eraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
	eraseInitStruct.PageAddress = CONFIG_PAGE_ADDRESS;
	eraseInitStruct.NbPages = 1;

	HAL_FLASH_Unlock();
	status = HAL_FLASHEx_Erase(&eraseInitStruct, &pageError);
	HAL_FLASH_Lock();

	// 如果发生错误，pageError 会包含错误的页面地址，否则为 0xFFFFFFFF
	return (status == HAL_OK && pageError == std::numeric_limits<uint32_t>::max()) ? Status::Ok : Status::FlashEraseError;
}

FlashConfig::Status FlashConfig::_writeToFlash(uint32_t address, const uint8_t *data, uint32_t sizeInBytes) {
	if (address < CONFIG_PAGE_ADDRESS || address + sizeInBytes > CONFIG_PAGE_ADDRESS + FLASH_PAGE_SIZE_CONST) {
		return Status::InvalidAddress;
	}

	// 确保地址是 4 字节对齐的
	if (address % 4 != 0) {
		return Status::InvalidAddress;
	}

	HAL_StatusTypeDef status = HAL_OK;

	HAL_FLASH_Unlock();

	// STM32F1 支持按字（32位）进行编程，效率更高
	// 数据结构设计保证了总长度是4字节的倍数
	for (uint32_t i = 0; i < sizeInBytes; i += 4) {
		// 从字节缓冲区中重新解释出一个32位数
		uint32_t word = *reinterpret_cast<const uint32_t *>(data + i);
		status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i, word);

		if (status != HAL_OK) {
			break; // 出现第一个错误就退出
		}
	}

	HAL_FLASH_Lock();

	return (status == HAL_OK) ? Status::Ok : Status::FlashWriteError;
}