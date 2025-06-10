/*
 * Copyright (c) 2025 Conclusive Engineering Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_cc35xx_gpio_port_bank

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/irq.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_utils.h>

#include <GPIO.h>
#include <inc/hw_types.h>
#include <inc/hw_memmap.h>
#include <inc/hw_hostmcu_aon.h>
#include <inc/hw_soc_aon.h>
#include <inc/hw_ints.h>

/* Missing defines from hw_hostmcu_aon.h */
#define HOSTMCU_AON_CFGWICSNS_GPIO_AND_EN (1U << 1)
#define HOSTMCU_AON_CFGWICSNS_GPIO_OR_EN  (1U << 2)
#define HOSTMCU_AON_CFGWUTP_GPIO_AND_FAST (1U << 1)
#define HOSTMCU_AON_CFGWUTP_GPIO_OR_FAST  (1U << 2)

struct cc35xx_gpio_port_pin_config {
	/* gpio_driver_config needs to be first */
	struct gpio_driver_config common;
	uint32_t gpio_index;
};

struct cc35xx_gpio_port_pin_data {
	/* gpio_driver_data needs to be first */
	struct gpio_driver_data common;
	const struct device *self;
	sys_slist_t callbacks;
};

struct cc35xx_gpio_port_bank_config {
	/* gpio_driver_config needs to be first */
	struct gpio_driver_config common;
	uint32_t ngpios;
	struct cc35xx_gpio_port_pin_config **port_pin_config;
};

struct cc35xx_gpio_port_bank_data {
	/* gpio_driver_data needs to be first */
	struct gpio_driver_data common;
	struct cc35xx_gpio_port_pin_data **port_pin_data;
};

static int cc35xx_gpio_port_get_raw(const struct device *dev, gpio_port_value_t *value)
{
	const struct cc35xx_gpio_port_pin_config *const dev_cfg = dev->config;

	*value = GPIO_read(dev_cfg->gpio_index);
	return 0;
}

static int cc35xx_gpio_port_set_masked_raw(const struct device *dev, gpio_port_pins_t mask,
					   gpio_port_value_t value)
{
	const struct cc35xx_gpio_port_pin_config *const dev_cfg = dev->config;

	GPIO_write(dev_cfg->gpio_index, value & mask);
	return 0;
}

static int cc35xx_gpio_port_set_bits_raw(const struct device *dev, gpio_port_pins_t mask)
{
	const struct cc35xx_gpio_port_pin_config *const dev_cfg = dev->config;

	GPIO_write(dev_cfg->gpio_index, BIT(0) & mask);
	return 0;
}

static int cc35xx_gpio_port_clear_bits_raw(const struct device *dev, gpio_port_pins_t mask)
{
	const struct cc35xx_gpio_port_pin_config *const dev_cfg = dev->config;

	GPIO_write(dev_cfg->gpio_index, ~(BIT(0) & mask));
	return 0;
}

static int cc35xx_gpio_port_toggle_bits(const struct device *dev, gpio_port_pins_t mask)
{
	const struct cc35xx_gpio_port_pin_config *const dev_cfg = dev->config;

	if (!(mask & BIT(0))) {
		return -ENOTSUP;
	}

	GPIO_toggle(dev_cfg->gpio_index);
	return 0;
}

static int cc35xx_gpio_pin_configure(const struct device *dev, gpio_pin_t pin, gpio_flags_t flags)
{
	const struct cc35xx_gpio_port_pin_config *const dev_cfg = dev->config;
	uint32_t config = 0;

	if (pin > 0) {
		return -ENOTSUP;
	}

	if ((flags & GPIO_DIR_MASK) == GPIO_DIR_MASK) {
		return -ENOTSUP;
	}

	if ((flags & GPIO_OUTPUT) && (flags & GPIO_LINE_OPEN_DRAIN)) {
		return -ENOTSUP;
	}

	if ((flags & GPIO_OUTPUT) && (flags & GPIO_LINE_OPEN_SOURCE)) {
		return -ENOTSUP;
	}

	if (flags & GPIO_OUTPUT) {
		config |= GPIO_CFG_OUT_STD;
		if (flags & GPIO_OUTPUT_INIT_HIGH) {
			config |= GPIO_CFG_OUT_HIGH;
		} else if (flags & GPIO_OUTPUT_INIT_LOW) {
			config |= GPIO_CFG_OUT_LOW;
		}
	} else if (flags & GPIO_INPUT) {
		if (flags & GPIO_PULL_UP) {
			config |= GPIO_CFG_IN_PU;
		} else if (flags & GPIO_PULL_DOWN) {
			config |= GPIO_CFG_IN_PD;
		} else {
			config |= GPIO_CFG_IN_NOPULL;
		}
	} else {
		config |= GPIO_CFG_NO_DIR;
	}

	GPIO_setConfig(dev_cfg->gpio_index, config);

	return 0;
}

static int cc35xx_gpio_pin_interrupt_configure(const struct device *dev, gpio_pin_t pin,
					       enum gpio_int_mode mode, enum gpio_int_trig trig)
{
	uint32_t config = 0;
	const struct cc35xx_gpio_port_pin_config *const dev_cfg = dev->config;

	if (mode == GPIO_INT_MODE_LEVEL) {
		return -ENOTSUP;
	}

	if (trig == GPIO_INT_TRIG_BOTH) {
		return -ENOTSUP;
	}

	switch (mode) {
	case GPIO_INT_MODE_DISABLED:
		config |= GPIO_CFG_INT_DISABLE;
		break;
	case GPIO_INT_MODE_EDGE:
		config |= GPIO_CFG_INT_ENABLE;
		config |= (trig == GPIO_INT_TRIG_HIGH) ? GPIO_CFG_IN_INT_RISING
						       : GPIO_CFG_IN_INT_FALLING;
		break;
	default:
		return -ENOTSUP;
	}

	GPIO_setInterruptConfig(dev_cfg->gpio_index, config);

	return 0;
}

static int cc35xx_gpio_manage_callback(const struct device *dev, struct gpio_callback *callback,
				       bool set)
{
	struct cc35xx_gpio_port_pin_data *dev_data = dev->data;

	return gpio_manage_callback(&dev_data->callbacks, callback, set);
}

static inline void cc35xx_event_mask_get(const struct device *dev, uint32_t event_mask,
					 bool low_interrupt)
{
	uint32_t flag_index;
	struct cc35xx_gpio_port_pin_data *port_pin_data;
	const struct cc35xx_gpio_port_bank_data *const dev_data = dev->data;
	const struct cc35xx_gpio_port_bank_config *const dev_cfg = dev->config;

	while (event_mask) {
		/* MASK_TO_PIN only detects the highest set bit */
		flag_index = GPIO_MASK_TO_PIN(event_mask);
		if (unlikely(flag_index > dev_cfg->ngpios)) {
			return;
		}

		/* So it's safe to use PIN_TO_MASK to clear that bit */
		event_mask &= ~GPIO_PIN_TO_MASK(flag_index);
		GPIO_clearInt(flag_index + (low_interrupt ? 0 : 32));
		port_pin_data = dev_data->port_pin_data[flag_index + (low_interrupt ? 0 : 32)];

		if (likely(port_pin_data != NULL && port_pin_data->self != NULL)) {
			/* BIT(0) - every port has only one pin */
			gpio_fire_callbacks(&port_pin_data->callbacks, port_pin_data->self, BIT(0));
		}
	}
}

static void cc35xx_gpio_port_bank_isr(const struct device *dev)
{
	uint32_t event_mask_0_31;
	uint32_t event_mask_32_44;

	/* Get the masked interrupt status from the MIS registers */
	event_mask_0_31 = HWREG(SOC_AON_BASE + SOC_AON_O_GPIOMIS0S);
	event_mask_32_44 = HWREG(SOC_AON_BASE + SOC_AON_O_GPIOMIS1S);

	cc35xx_event_mask_get(dev, event_mask_0_31, true);
	cc35xx_event_mask_get(dev, event_mask_32_44, false);
}

static int cc35xx_gpio_port_pin_init(const struct device *dev)
{
	struct cc35xx_gpio_port_pin_data *const dev_data = dev->data;
	const struct cc35xx_gpio_port_pin_config *const dev_cfg = dev->config;

	dev_data->self = dev;
	GPIO_setConfig(dev_cfg->gpio_index, GPIO_CFG_NO_DIR);

	return 0;
}

static int cc35xx_gpio_port_bank_init(const struct device *dev)
{
	/*
	 * Make sure no GPIOs are wakeup sources. If the bit for a given GPIO is set
	 * in the mask, it means the event will not propagate to the wakeup
	 * interrupt. When interrupts are enabled/disabled for individual GPIOs, the
	 * corresponding bit in relevant mask will be cleared/set.
	 */
	HWREG(HOSTMCU_AON_BASE + HOSTMCU_AON_O_GPWUAND) = 0xFFFFFFFF;
	HWREG(HOSTMCU_AON_BASE + HOSTMCU_AON_O_GPWUAND1) = 0xFFFFFFFF;
	HWREG(HOSTMCU_AON_BASE + HOSTMCU_AON_O_GPWUOR) = 0xFFFFFFFF;
	HWREG(HOSTMCU_AON_BASE + HOSTMCU_AON_O_GPWUOR1) = 0xFFFFFFFF;

	/* Disallow all GPIO interrupt sources to pass from RIS to MIS */
	HWREG(SOC_AON_BASE + SOC_AON_O_GPIOFNC0S) = 0;
	HWREG(SOC_AON_BASE + SOC_AON_O_GPIOFNC1S) = 0;

	/* Enable IRQ */
	IRQ_CONNECT(DT_IRQN(DT_DRV_INST(0)), DT_IRQ(DT_DRV_INST(0), priority),
		    cc35xx_gpio_port_bank_isr, DEVICE_DT_GET(DT_INST(0, DT_DRV_COMPAT)), 0);

	irq_enable(DT_IRQN(DT_DRV_INST(0)));

	/* Configure the GPIO OR event as a sleep wakeup source */
	HWREG(HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGWICSNS) |= HOSTMCU_AON_CFGWICSNS_GPIO_OR_EN;

	/* Disable GPIO AND event as a sleep wakeup source */
	HWREG(HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGWICSNS) &= ~(HOSTMCU_AON_CFGWICSNS_GPIO_AND_EN);

	/* Configure the GPIO OR event to be a fast wakeup source */
	HWREG(HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGWUTP) |= HOSTMCU_AON_CFGWUTP_GPIO_OR_FAST;

	return 0;
}

static const struct gpio_driver_api cc35xx_gpio_drv_api = {
	.pin_configure = cc35xx_gpio_pin_configure,
	.port_get_raw = cc35xx_gpio_port_get_raw,
	.port_set_masked_raw = cc35xx_gpio_port_set_masked_raw,
	.port_set_bits_raw = cc35xx_gpio_port_set_bits_raw,
	.port_clear_bits_raw = cc35xx_gpio_port_clear_bits_raw,
	.port_toggle_bits = cc35xx_gpio_port_toggle_bits,
	.pin_interrupt_configure = cc35xx_gpio_pin_interrupt_configure,
	.manage_callback = cc35xx_gpio_manage_callback,
};

#define CC35XX_GPIO_PORT_PIN_DEFINE(child_node_id)                                                 \
	static struct cc35xx_gpio_port_pin_data cc35xx_gpio_port_pin_data_##child_node_id;         \
	static struct cc35xx_gpio_port_pin_config cc35xx_gpio_port_pin_config_##child_node_id = {  \
		.gpio_index = DT_PROP(child_node_id, gpio_index),                                  \
		.common.port_pin_mask = 1,                                                         \
	};                                                                                         \
	DEVICE_DT_DEFINE(child_node_id, cc35xx_gpio_port_pin_init, NULL,                           \
			 &cc35xx_gpio_port_pin_data_##child_node_id,                               \
			 &cc35xx_gpio_port_pin_config_##child_node_id, PRE_KERNEL_1,               \
			 CONFIG_GPIO_INIT_PRIORITY, &cc35xx_gpio_drv_api)

#define CC35XX_PIN_CONFIG_ENTRY(child)                                                             \
	[DT_PROP(child, gpio_index)] = &cc35xx_gpio_port_pin_config_##child,

#define CC35XX_PIN_DATA_ENTRY(child)                                                               \
	[DT_PROP(child, gpio_index)] = &cc35xx_gpio_port_pin_data_##child,

#define CC35XX_GPIO_PORT_BANK_DEFINE(inst)                                                         \
	DT_FOREACH_CHILD_STATUS_OKAY(DT_INST(inst, DT_DRV_COMPAT), CC35XX_GPIO_PORT_PIN_DEFINE);   \
	static struct cc35xx_gpio_port_pin_data                                                    \
		*cc35xx_gpio_port_pin_data_array_##inst[DT_PROP(DT_DRV_INST(inst), ngpios)] = {    \
			DT_FOREACH_CHILD_STATUS_OKAY(DT_INST(inst, DT_DRV_COMPAT),                 \
						     CC35XX_PIN_DATA_ENTRY)};                      \
	static struct cc35xx_gpio_port_pin_config                                                  \
		*cc35xx_gpio_port_pin_config_array_##inst[DT_PROP(DT_DRV_INST(inst), ngpios)] = {  \
			DT_FOREACH_CHILD_STATUS_OKAY(DT_INST(inst, DT_DRV_COMPAT),                 \
						     CC35XX_PIN_CONFIG_ENTRY)};                    \
	static struct cc35xx_gpio_port_bank_data cc35xx_gpio_port_bank_data_##inst = {             \
		.port_pin_data = cc35xx_gpio_port_pin_data_array_##inst,                           \
	};                                                                                         \
	static const struct cc35xx_gpio_port_bank_config cc35xx_gpio_port_bank_config_##inst = {   \
		.ngpios = DT_PROP(DT_DRV_INST(inst), ngpios),                                      \
		.port_pin_config = cc35xx_gpio_port_pin_config_array_##inst,                       \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(inst, cc35xx_gpio_port_bank_init, NULL,                              \
			      &cc35xx_gpio_port_bank_data_##inst,                                  \
			      &cc35xx_gpio_port_bank_config_##inst, PRE_KERNEL_1,                  \
			      CONFIG_GPIO_INIT_PRIORITY, NULL)

DT_INST_FOREACH_STATUS_OKAY(CC35XX_GPIO_PORT_BANK_DEFINE)
