#pragma once

#include <algorithm> // 用于 std::copy
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "stm32f1xx_hal.h" // 用于 HAL 类型和 Flash 宏

/**
 * @class FlashConfig
 * @brief 管理 STM32F1xx MCU 内部 Flash 配置数据的读写
 *
 * 本类实现了一种磨损均衡策略以最小化擦除周期，它从指定 Flash 页的末尾开始向前顺序写入配置块，只有当整个页写满后才会执行擦除操作
 */
class FlashConfig {
public:
	/**
	 * @brief 定义配置项的结构体
	 */
	struct Config {
		uint16_t key;
		uint16_t value;
	};

	using ConfigList = std::span<Config>;

	/**
	 * @brief 操作的状态码枚举
	 */
	enum class Status : uint8_t {
		Ok,                  // 操作成功
		Error,               // 通用错误
		DataTooLarge,        // 数据对于单个页面来说太大
		FlashWriteError,     // HAL 写入操作失败
		FlashEraseError,     // HAL 擦除操作失败
		InvalidAddress,      // 地址不在有效范围内或不对齐
		InvalidConfigHeader, // 闪存配置无效（在魔法数字之前读取到非 0xFFFF）
		ConfigNotFound       // 没有找到有效的配置块
	};

	/**
	 * @brief 状态码字符串表示形式
	 */
	static constexpr std::array<std::string_view, 8> StatusStrings{
		"Ok",
		"Error",
		"DataTooLarge",
		"FlashWriteError",
		"FlashEraseError",
		"InvalidAddress",
		"InvalidConfigHeader",
		"ConfigNotFound"
	};

	/**
	 * @brief 当在配置中找不到指定键时返回的值
	 */
	static constexpr uint16_t INVALID_VALUE = uint16_t(-1);

	/**
	 * @brief 可存储的最大配置项数量
	 *        请根据您的应用需求和可用 RAM 调整此值
	 */
	static constexpr uint16_t MAX_CONFIG_ITEMS = 16;

private:
	// --- 编译期 Flash 几何信息配置 ---

	// 用于存储的 Flash 页面（从内存末尾倒数），1 = 最后一页，2 = 倒数第二页，以此类推
	static constexpr uint32_t PAGE_OFFSET_FROM_END = 1;

	// 使用 HAL 宏来定义 C++ 常量，保持代码中无宏定义
	static constexpr uint32_t FLASH_PAGE_SIZE_CONST = FLASH_PAGE_SIZE;
	static constexpr uint32_t FLASH_BASE_CONST = FLASH_BASE;
	// FLASH_BANK_END 是最后一个字节的地址，加 1 以获得相对于 FLASH_BASE 的总大小
	static constexpr uint32_t FLASH_END_ADDR_CONST = FLASH_BANK1_END;

	// 我们将用于配置存储的页面的起始地址
	static constexpr uint32_t CONFIG_PAGE_ADDRESS = (FLASH_END_ADDR_CONST + 1) - (PAGE_OFFSET_FROM_END * FLASH_PAGE_SIZE_CONST);

	// 魔术字 "Cf"，用于标识一个配置块的开始
	static constexpr uint16_t MAGIC_HEADER = 0x4643; // 'C' 'f'

public:
	/**
	 * @brief 默认构造函数
	 */
	FlashConfig() = default;

	/**
	 * @brief 初始化 FlashConfig 实例，通过搜索最新的有效配置
	 * @return 一个 Status 码，指示操作的结果
	 */
	Status Init();

	/**
	 * @brief 将一组新的配置数据写入 Flash
	 *
	 * 此函数实现了磨损均衡逻辑，它会尝试将新数据写在先前写入的块之前
	 * 如果页面已满，它将擦除页面并将新数据写入页面的末尾
	 *
	 * @param data 指向要写入的 Config 项数组的指针
	 * @param count 数组中 Config 项的数量
	 * @return 一个 Status 码，指示操作的结果
	 */
	Status WriteConfig(ConfigList configList);

	/**
	 * @brief 检索给定配置键的值
	 * @param key 要查找的键
	 * @return 如果找到，则返回对应的值，否则返回 INVALID_VALUE
	 */
	uint16_t GetValue(uint16_t key) const;

	/**
	 * @brief 获取当前加载的配置项数量
	 * @return 已加载项的数量
	 */
	uint16_t GetConfigCount() const { return _loadedCount; }

private:
	/**
	 * @brief 扫描配置页以找到第一个（因此也是最新的）有效配置块
	 * @return 最新配置块的魔术字地址，如果未找到则返回 0，如果魔术字异常则返回 0xFFFFFFFF
	 */
	uint32_t _findLatestConfig();

	/**
	 * @brief 擦除整个配置页
	 * @return 成功时返回 Status::Ok，失败时返回 Status::FlashEraseError
	 */
	Status _erasePage();

	/**
	 * @brief 将数据块写入指定的 Flash 地址
	 * @param address 要写入的起始地址，必须是字（32位）对齐的
	 * @param data 指向数据缓冲区的指针
	 * @param sizeInBytes 要写入的数据总大小（以字节为单位），必须是 4 的倍数
	 * @return 成功时返回 Status::Ok，失败时返回 Status::FlashWriteError
	 */
	Status _writeToFlash(uint32_t address, const uint8_t *data, uint32_t sizeInBytes);

	// 成员变量
	std::array<Config, MAX_CONFIG_ITEMS> _loadedConfig{};
	uint16_t _loadedCount = 0;

	// 上次找到的配置块的起始地址（指向 MAGIC_HEADER）
	// 如果没有找到配置，则设置为页尾，为首次写入做准备
	uint32_t _lastConfigAddress = 0;
};