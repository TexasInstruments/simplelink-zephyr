/*
 * Copyright (c) 2025 Conclusive Engineering Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>

#include <ti/drivers/xmem/flash/FlashWFF3.h>
#include <driverlib/cpu.h>

#define DT_DRV_COMPAT        ti_cc35xx_nv_flash
#define CC35XX_ERASE_OPCODE  0x20
#define CC35XX_ERASE_TIMEOUT 200

struct flash_cc35xx_config {
	mem_addr_t base;
	mem_addr_t phys_addr;
	size_t size;
	size_t erase_size;
	const struct flash_pages_layout *layout;
	const struct flash_parameters *parameters;
};
static int flash_cc35xx_initialized;
static struct k_mutex flash_cc35xx_mutex;

static mem_addr_t flash_cc35xx_offset_to_phys_addr(const struct device *dev, off_t offset)
{
	const struct flash_cc35xx_config *config = dev->config;

	return config->phys_addr + offset;
}

static void *flash_cc35xx_offset_to_logic_addr(const struct device *dev, off_t offset)
{
	const struct flash_cc35xx_config *config = dev->config;
	uintptr_t addr = config->base + offset;

	return (void *)addr;
}

static bool flash_cc35xx_is_range_valid(const struct device *dev, off_t offset, size_t size)
{
	const struct flash_cc35xx_config *config = dev->config;

	return ((size_t)offset < config->size) && (size < config->size - offset);
}

static int flash_cc35xx_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	if (flash_cc35xx_initialized == 0) {
		k_mutex_init(&flash_cc35xx_mutex);
		flash_cc35xx_initialized = 1;
	}

	return 0;
}

__ramfunc static int flash_cc35xx_erase(const struct device *dev, off_t offset, size_t size)
{
	const struct flash_cc35xx_config *config = dev->config;
	size_t erase_size = config->erase_size;
	mem_addr_t addr = flash_cc35xx_offset_to_phys_addr(dev, offset);
	int ret;
	unsigned int key;

	if (offset % erase_size) {
		return -EINVAL;
	}
	if (size % erase_size) {
		return -EINVAL;
	}
	if (!flash_cc35xx_is_range_valid(dev, offset, size)) {
		return -EINVAL;
	}

	k_mutex_lock(&flash_cc35xx_mutex, K_FOREVER);
	for (; size; size -= erase_size, addr += erase_size) {
		key = irq_lock();
		ret = FlashSectorErase(addr, CC35XX_ERASE_OPCODE, CC35XX_ERASE_TIMEOUT);
		/* BUG: FlashSectorErase() is supposed to wait for flash busy flag to
		 * go off after erasing sector. But it appears there is a bug somewhere
		 * with the logic, and if we unlock interrupts right after we exit that
		 * function, chip - most often than not - will go into lockup state
		 * due to XIP operation on busy flash. Adding busy-loop that is long
		 * enough works around that problem. Value has been chosen arbitrarily
		 * based on tests
		 */
		CPUDelay(2097152l);
		irq_unlock(key);
		if (ret) {
			break;
		}
	}
	k_mutex_unlock(&flash_cc35xx_mutex);

	return ret ? -ETIMEDOUT : 0;
}

static int flash_cc35xx_write(const struct device *dev, off_t offset, const void *buf, size_t size)
{
	const struct flash_cc35xx_config *config = dev->config;
	uint32_t *addr = flash_cc35xx_offset_to_logic_addr(dev, offset);
	const uint32_t *buffer = buf;

	if (size % config->parameters->write_block_size) {
		return -EINVAL;
	}
	if ((uintptr_t)buffer % config->parameters->write_block_size) {
		return -EINVAL;
	}
	if (!flash_cc35xx_is_range_valid(dev, offset, size)) {
		return -EINVAL;
	}

	size /= sizeof(uint32_t);
	k_mutex_lock(&flash_cc35xx_mutex, K_FOREVER);
	for (size_t i = 0; i < size; i++) {
		addr[i] = buffer[i];
	}
	k_mutex_unlock(&flash_cc35xx_mutex);

	return 0;
}

static int flash_cc35xx_read(const struct device *dev, off_t offset, void *buf, size_t size)
{
	void *addr = flash_cc35xx_offset_to_logic_addr(dev, offset);

	if (!flash_cc35xx_is_range_valid(dev, offset, size)) {
		return -EINVAL;
	}

	k_mutex_lock(&flash_cc35xx_mutex, K_FOREVER);
	memcpy(buf, addr, size);
	k_mutex_unlock(&flash_cc35xx_mutex);

	return 0;
}

static const struct flash_parameters *flash_cc35xx_get_parameters(const struct device *dev)
{
	const struct flash_cc35xx_config *config = dev->config;

	return config->parameters;
}

#if defined(CONFIG_FLASH_PAGE_LAYOUT)
static void flash_cc35xx_layout(const struct device *dev, const struct flash_pages_layout **layout,
				size_t *layout_size)
{
	const struct flash_cc35xx_config *config = dev->config;

	*layout = config->layout;
	*layout_size = 1;
}
#endif /* CONFIG_FLASH_PAGE_LAYOUT */

static const struct flash_driver_api flash_cc35xx_api = {
	.erase = flash_cc35xx_erase,
	.write = flash_cc35xx_write,
	.read = flash_cc35xx_read,
	.get_parameters = flash_cc35xx_get_parameters,
#if defined(CONFIG_FLASH_PAGE_LAYOUT)
	.page_layout = flash_cc35xx_layout,
#endif
};

#define FLASH_CC35XX_DEVICE(n)                                                                     \
	static const struct flash_pages_layout flash_cc35xx_layout_##n = {                         \
		.pages_size = DT_INST_PROP(n, write_block_size),                                   \
		.pages_count = DT_INST_REG_SIZE_BY_IDX(n, 0) / DT_INST_PROP(n, write_block_size),  \
	};                                                                                         \
	static const struct flash_parameters flash_cc35xx_parameters_##n = {                       \
		.write_block_size = DT_INST_PROP(n, write_block_size),                             \
		.erase_value = 0xff,                                                               \
	};                                                                                         \
	static const struct flash_cc35xx_config flash_cc35xx_config_##n = {                        \
		.base = DT_INST_REG_ADDR_BY_IDX(n, 0),                                             \
		.size = DT_INST_REG_SIZE_BY_IDX(n, 0),                                             \
		.phys_addr = DT_INST_REG_ADDR_BY_IDX(n, 1),                                        \
		.erase_size = DT_INST_PROP(n, erase_block_size),                                   \
		.layout = &flash_cc35xx_layout_##n,                                                \
		.parameters = &flash_cc35xx_parameters_##n,                                        \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, flash_cc35xx_init, NULL, NULL, &flash_cc35xx_config_##n,          \
			      POST_KERNEL, CONFIG_FLASH_INIT_PRIORITY, &flash_cc35xx_api);

DT_INST_FOREACH_STATUS_OKAY(FLASH_CC35XX_DEVICE)
