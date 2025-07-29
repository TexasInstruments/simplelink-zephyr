/*
 * Copyright (c) 2025 Conclusive Engineering Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_cc35xx_uart

#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>
#include <zephyr/dt-bindings/pinctrl/ti-cc35xx-pinctrl.h>

#include <inc/hw_memmap.h>
#include <inc/hw_ints.h>
#include <driverlib/uart.h>

LOG_MODULE_REGISTER(uart_cc35xx);

struct uart_cc35xx_dev_config {
	unsigned long base;
	uint32_t sys_clk_freq;
	const struct pinctrl_dev_config *pcfg;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_config_func_t irq_config_func;
#endif
};

struct uart_cc35xx_dev_data_t {
	uint32_t baud_rate;
	int id;
	struct k_spinlock lock;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_callback_user_data_t cb;
	void *cb_data;
#endif
};

static int uart_cc35xx_init(const struct device *dev)
{
	struct uart_cc35xx_dev_data_t *data = dev->data;
	const struct uart_cc35xx_dev_config *config = dev->config;
	int ret;

	ret = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		return ret;
	}

	UARTClockCtrl(config->base, true);
	UARTConfigSetExpClk(config->base, config->sys_clk_freq, data->baud_rate,
			    UART_CONFIG_WLEN_8 | UART_CONFIG_PAR_NONE | UART_CONFIG_STOP_ONE);
	UARTDisableCts(config->base);
	UARTDisableRts(config->base);
	UARTDisableInt(config->base, UART_INT_ALL);

	UARTClearInt(config->base, UART_INT_RX);
	UARTEnable(config->base);
	/* UARTEnable() enables the FIFO, but we do not use it in this implementation. */
	UARTDisableFifo(config->base);
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	config->irq_config_func(dev);
#endif
	return 0;
}

static int uart_cc35xx_fifo_fill(const struct device *dev, const uint8_t *tx_data, int size)
{
	const struct uart_cc35xx_dev_config *config = dev->config;
	int i = 0;

	for (i = 0; i < size && UARTSpaceAvailable(config->base); i++) {
		UARTPutCharNonBlocking(config->base, tx_data[i]);
	}

	return i;
}

static int uart_cc35xx_fifo_read(const struct device *dev, uint8_t *rx_data, int size)
{
	const struct uart_cc35xx_dev_config *config = dev->config;
	int i = 0;

	for (i = 0; i < size && UARTCharAvailable(config->base); i++) {
		rx_data[i] = UARTGetCharNonBlocking(config->base);
	}

	return i;
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

static void uart_cc35xx_irq_tx_enable(const struct device *dev)
{
	const struct uart_cc35xx_dev_config *config = dev->config;

	UARTEnableInt(config->base, UART_INT_TX);
}

static void uart_cc35xx_irq_tx_disable(const struct device *dev)
{
	const struct uart_cc35xx_dev_config *config = dev->config;

	UARTDisableInt(config->base, UART_INT_TX);
}

static int uart_cc35xx_irq_tx_ready(const struct device *dev)
{
	const struct uart_cc35xx_dev_config *config = dev->config;
	uint32_t status;

	status = UARTIntStatus(config->base, true);
	return !!(status & UART_INT_TX);
}

static void uart_cc35xx_irq_rx_enable(const struct device *dev)
{
	const struct uart_cc35xx_dev_config *config = dev->config;

	UARTEnableInt(config->base, UART_INT_RX);
}

static void uart_cc35xx_irq_rx_disable(const struct device *dev)
{
	const struct uart_cc35xx_dev_config *config = dev->config;

	UARTDisableInt(config->base, UART_INT_RX);
}

static int uart_cc35xx_irq_tx_complete(const struct device *dev)
{
	const struct uart_cc35xx_dev_config *config = dev->config;

	return UARTBusy(config->base);
}

static int uart_cc35xx_irq_rx_ready(const struct device *dev)
{
	const struct uart_cc35xx_dev_config *config = dev->config;
	uint32_t status;

	status = UARTIntStatus(config->base, true);
	return !!(status & UART_INT_RX);
}

static int uart_cc35xx_irq_is_pending(const struct device *dev)
{
	const struct uart_cc35xx_dev_config *config = dev->config;
	uint32_t status;

	status = UARTIntStatus(config->base, true);
	return !!(status & (UART_INT_TX | UART_INT_RX));
}

static int uart_cc35xx_irq_update(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 1;
}

static void uart_cc35xx_irq_callback_set(const struct device *dev, uart_irq_callback_user_data_t cb,
					 void *cb_data)
{
	struct uart_cc35xx_dev_data_t *data = dev->data;

	data->cb = cb;
	data->cb_data = cb_data;
}

static void uart_cc35xx_isr(const struct device *dev)
{
	struct uart_cc35xx_dev_data_t *data = dev->data;

	if (data->cb) {
		data->cb(dev, data->cb_data);
	}
}

#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

static int uart_cc35xx_poll_in(const struct device *dev, unsigned char *c)
{
	return uart_cc35xx_fifo_read(dev, c, 1);
}

static void uart_cc35xx_poll_out(const struct device *dev, unsigned char c)
{
	struct uart_cc35xx_dev_data_t *data = dev->data;

	K_SPINLOCK(&data->lock) {
		while (uart_cc35xx_fifo_fill(dev, &c, 1) == 0) {
		}
	}
}

static const struct uart_driver_api uart_cc35xx_driver_api = {
	.poll_in = uart_cc35xx_poll_in,
	.poll_out = uart_cc35xx_poll_out,
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.fifo_fill = uart_cc35xx_fifo_fill,
	.fifo_read = uart_cc35xx_fifo_read,
	.irq_tx_enable = uart_cc35xx_irq_tx_enable,
	.irq_tx_disable = uart_cc35xx_irq_tx_disable,
	.irq_tx_ready = uart_cc35xx_irq_tx_ready,
	.irq_rx_enable = uart_cc35xx_irq_rx_enable,
	.irq_rx_disable = uart_cc35xx_irq_rx_disable,
	.irq_tx_complete = uart_cc35xx_irq_tx_complete,
	.irq_rx_ready = uart_cc35xx_irq_rx_ready,
	.irq_is_pending = uart_cc35xx_irq_is_pending,
	.irq_update = uart_cc35xx_irq_update,
	.irq_callback_set = uart_cc35xx_irq_callback_set,

#endif
};

#define UART_35XX_DEVICE(idx)                                                                      \
	PINCTRL_DT_INST_DEFINE(idx);                                                               \
	IF_ENABLED(CONFIG_UART_INTERRUPT_DRIVEN, (                                                 \
	static void uart_cc35xx_cfg_func_##idx(const struct device *dev)                           \
	{                                                                                          \
		IF_ENABLED(CONFIG_UART_INTERRUPT_DRIVEN, (                                         \
			IRQ_CONNECT(DT_INST_IRQN(idx),                                             \
			    DT_INST_IRQ(idx, priority),                                            \
			    uart_cc35xx_isr, DEVICE_DT_INST_GET(idx),                              \
			    0);                                                                    \
			irq_enable(DT_INST_IRQN(idx)))                                             \
		);                                                                                 \
	}));                                                                                       \
	static const struct uart_cc35xx_dev_config uart_cc35xx_dev_cfg_##idx = {                   \
		.base = DT_INST_REG_ADDR(idx),                                                     \
		.sys_clk_freq = DT_INST_PROP_BY_PHANDLE(idx, clocks, clock_frequency),             \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(idx),                                       \
		IF_ENABLED(CONFIG_UART_INTERRUPT_DRIVEN,                                           \
		    (.irq_config_func = uart_cc35xx_cfg_func_##idx,)) };                           \
	static struct uart_cc35xx_dev_data_t uart_cc35xx_dev_data_##idx = {                        \
		.baud_rate = DT_INST_PROP(idx, current_speed),                                     \
		.id = idx,                                                                         \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(idx, uart_cc35xx_init, NULL, &uart_cc35xx_dev_data_##idx,            \
			      &uart_cc35xx_dev_cfg_##idx, PRE_KERNEL_1,                            \
			      CONFIG_SERIAL_INIT_PRIORITY, (void *)&uart_cc35xx_driver_api);

DT_INST_FOREACH_STATUS_OKAY(UART_35XX_DEVICE);
