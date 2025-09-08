/*
 * Copyright (c) 2025 Conclusive Engineering Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_cc35xx_spi

#define LOG_LEVEL CONFIG_SPI_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(spi_cc35xx);

#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>

#include <driverlib/spi.h>
#include <ti/devices/cc35xx/inc/hw_spi.h>

#include "spi_context.h"

#define SPI_INT_ALL                                                                                \
	(SPI_MIS_TX_SET | SPI_MIS_RX_SET | SPI_MIS_RXOVF_SET | SPI_MIS_IDLE_SET |                  \
	 SPI_MIS_TXEMPTY_SET | SPI_MIS_PER_SET | SPI_MIS_RTOUT_SET | SPI_MIS_DMARX_SET |           \
	 SPI_MIS_DMATX_SET)
#define IDLE_CHAR 0x00
#define SPI_SLAVE_BUS_MAX_FREQ 40000000l

struct spi_cc35xx_config {
	uint32_t base;
	const struct pinctrl_dev_config *pcfg;
	uint32_t sys_clk_freq;
};

struct spi_cc35xx_data {
	struct spi_context ctx;
	size_t rxleft;
};

static void spi_cc35xx_read_rx_fifo(const struct device *dev)
{
	const struct spi_cc35xx_config *cfg = dev->config;
	struct spi_cc35xx_data *data = dev->data;
	struct spi_context *ctx = &data->ctx;
	uint32_t rxd;

	while (SPIGetDataNonBlocking(cfg->base, &rxd)) {
		if (spi_context_rx_buf_on(ctx)) {
			*ctx->rx_buf = rxd;
		}
		spi_context_update_rx(ctx, 1, 1);
	}
}

static void spi_cc35xx_fill_tx_fifo(const struct device *dev)
{
	const struct spi_cc35xx_config *cfg = dev->config;
	struct spi_cc35xx_data *data = dev->data;
	struct spi_context *ctx = &data->ctx;
	uint32_t txd;

	while (sys_read32(cfg->base + SPI_O_STA) & SPI_STA_TNF_NOT_FULL) {
		if (!spi_context_tx_on(ctx) && !data->rxleft) {
			return;
		}

		if (data->rxleft) {
			data->rxleft--;
		}

		/* Fill TX fifo with idle chars if we have data to read,
		 * but nothing to send
		 */
		txd = IDLE_CHAR;
		if (spi_context_tx_buf_on(ctx)) {
			txd = *ctx->tx_buf;
		}
		spi_context_update_tx(ctx, 1, 1);

		SPIPutData(cfg->base, txd);
	}
}

static void spi_cc35xx_flush_fifo(mem_addr_t base)
{
	sys_write32(sys_read32(base + SPI_O_CTL0) | SPI_CTL0_FIFORST_RST_TRIG |
		SPI_CTL0_IDLEPOCI_IDLE_ONE, base + SPI_O_CTL0);
	while (sys_read32(base + SPI_O_CTL0) & SPI_CTL0_FIFORST) {
	}
}

static int spi_cc35xx_configure(const struct device *dev, const struct spi_config *config)
{
	const struct spi_cc35xx_config *cfg = dev->config;
	struct spi_cc35xx_data *data = dev->data;
	struct spi_context *ctx = &data->ctx;
	int is_master = SPI_OP_MODE_GET(config->operation) == SPI_OP_MODE_MASTER;
	uint32_t prot, freq;
	int ret, mode;

	if (spi_context_configured(ctx, config)) {
		return 0;
	}

	SPIDisable(cfg->base);
	SPIDisableInt(cfg->base, SPI_INT_ALL);
	SPIClearInt(cfg->base, SPI_INT_ALL);

	if (config->operation & SPI_HALF_DUPLEX) {
		LOG_ERR("Half-duplex not supported");
		return -ENOTSUP;
	}

	if (SPI_WORD_SIZE_GET(config->operation) != 8) {
		LOG_ERR("Word sizes other than 8 bits are not supported");
		return -ENOTSUP;
	}

	ret = pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		LOG_ERR("Applying SPI pinctrl state failed");
		return ret;
	}

	mode = is_master ? SPI_MODE_CONTROLLER : SPI_MODE_PERIPHERAL;
	prot = is_master ? SPI_CTL0_FRF_MOTOROLA_3WIRE : SPI_CTL0_FRF_MOTOROLA_4WIRE;
	freq = is_master ? config->frequency : SPI_SLAVE_BUS_MAX_FREQ;
	prot |= SPI_MODE_GET(config->operation) & SPI_MODE_CPOL ?
		SPI_CTL0_SPO_HIGH : SPI_CTL0_SPO_LOW;
	prot |= SPI_MODE_GET(config->operation) & SPI_MODE_CPHA ?
		SPI_CTL0_SPH_SECOND : SPI_CTL0_SPH_FIRST;

	SPIConfigSetExpClk(cfg->base, cfg->sys_clk_freq, prot, mode, freq, 8);
	sys_write32(SPI_IFLS_RXSEL_LVL_1_2 | SPI_IFLS_TXSEL_LVL_1_2, cfg->base + SPI_O_IFLS);
	if (config->operation & SPI_TRANSFER_LSB) {
		sys_write32((sys_read32(cfg->base + SPI_O_CTL1) & ~SPI_CTL1_MSB_M) |
				SPI_CTL1_MSB_LSB,
			cfg->base + SPI_O_CTL1);
	}

	SPIEnable(cfg->base);
	sys_write32(BIT(0), cfg->base + SPI_O_CLKCFG);
	spi_cc35xx_flush_fifo(cfg->base);

	ctx->config = config;
	return 0;
}

static void spi_cc35xx_master_transceive(const struct device *dev)
{
	const struct spi_cc35xx_config *cfg = dev->config;
	struct spi_cc35xx_data *data = dev->data;
	struct spi_context *ctx = &data->ctx;

	spi_cc35xx_flush_fifo(cfg->base);
	spi_context_cs_control(ctx, true);

	spi_cc35xx_fill_tx_fifo(dev);
	SPIEnableInt(cfg->base, SPI_MIS_TX_SET);
}

#ifdef CONFIG_SPI_SLAVE

static void spi_cc35xx_slave_transceive(const struct device *dev)
{
	const struct spi_cc35xx_config *cfg = dev->config;

	spi_cc35xx_flush_fifo(cfg->base);
	spi_cc35xx_fill_tx_fifo(dev);
	SPIEnableInt(cfg->base, SPI_MIS_RX_SET);
}
#endif

static int spi_cc35xx_transceive(const struct device *dev,
				 const struct spi_config *config,
				 const struct spi_buf_set *tx_bufs,
				 const struct spi_buf_set *rx_bufs,
				 spi_callback_t cb, void *userdata, int async)
{
	struct spi_cc35xx_data *data = dev->data;
	struct spi_context *ctx = &data->ctx;
	int err;

	spi_context_lock(ctx, async, cb, userdata, config);

	err = spi_cc35xx_configure(dev, config);
	if (err) {
		goto done;
	}

	spi_context_buffers_setup(ctx, tx_bufs, rx_bufs, 1);
	data->rxleft = spi_context_total_rx_len(ctx);

#ifdef CONFIG_SPI_SLAVE
	if (spi_context_is_slave(ctx)) {
		spi_cc35xx_slave_transceive(dev);
	} else
#endif
	{
		spi_cc35xx_master_transceive(dev);
	}

done:
	spi_context_wait_for_completion(ctx);
	spi_context_release(ctx, err);
#ifdef CONFIG_SPI_SLAVE
	if (spi_context_is_slave(ctx) && !err) {
		err = ctx->recv_frames;
	}
#endif

	return err;

}

static int spi_cc35xx_release(const struct device *dev, const struct spi_config *config)
{
	const struct spi_cc35xx_config *cfg = dev->config;
	struct spi_cc35xx_data *data = dev->data;
	struct spi_context *ctx = &data->ctx;

	if (!spi_context_configured(ctx, config)) {
		return -EINVAL;
	}

	if (SPIBusy(cfg->base)) {
		return -EBUSY;
	}

	spi_context_unlock_unconditionally(ctx);

	return 0;
}

static void spi_cc35xx_isr(const struct device *dev)
{
	const struct spi_cc35xx_config *cfg = dev->config;
	struct spi_cc35xx_data *data = dev->data;
	struct spi_context *ctx = &data->ctx;
	uint32_t txrx_irq = SPIIntStatus(cfg->base, true) & (SPI_MIS_RX | SPI_MIS_TX);

	if (txrx_irq) {
		SPIClearInt(cfg->base, txrx_irq);
		spi_cc35xx_read_rx_fifo(dev);

		if (!spi_context_rx_on(ctx) && !spi_context_tx_on(ctx)) {
			SPIDisableInt(cfg->base, txrx_irq);

			if (txrx_irq & SPI_MIS_RX) {
				spi_context_complete(ctx, dev, 0);
			} else {
				SPIClearInt(cfg->base, SPI_MIS_IDLE_SET);
				SPIEnableInt(cfg->base, SPI_MIS_IDLE_SET);
			}
			return;
		}

		spi_cc35xx_fill_tx_fifo(dev);
	}

	if (SPIIntStatus(cfg->base, true) & SPI_MIS_IDLE) {
		SPIDisableInt(cfg->base, SPI_MIS_IDLE);
		SPIClearInt(cfg->base, SPI_MIS_IDLE);
		spi_context_cs_control(ctx, false);
		spi_context_complete(ctx, dev, 0);
	}
}

#ifdef CONFIG_SPI_ASYNC
static int spi_cc35xx_transceive_async(const struct device *dev,
				       const struct spi_config *config,
				       const struct spi_buf_set *tx_bufs,
				       const struct spi_buf_set *rx_bufs,
				       spi_callback_t cb, void *userdata)
{
	return spi_cc35xx_transceive(dev, config, tx_bufs, rx_bufs, cb, userdata, 1);
}
#endif

static int spi_cc35xx_transceive_sync(const struct device *dev,
				      const struct spi_config *config,
				      const struct spi_buf_set *tx_bufs,
				      const struct spi_buf_set *rx_bufs)
{
	return spi_cc35xx_transceive(dev, config, tx_bufs, rx_bufs, NULL, NULL, 0);
}

static const struct spi_driver_api spi_cc35xx_driver_api = {
	.transceive = spi_cc35xx_transceive_sync,
#ifdef CONFIG_SPI_ASYNC
	.transceive_async = spi_cc35xx_transceive_async,
#endif
	.release = spi_cc35xx_release,
};

#define SPI_CC35XX_DEVICE_INIT(n)                                                                  \
	DEVICE_DT_INST_DEFINE(n, spi_cc35xx_init_##n, PM_DEVICE_DT_INST_GET(n),                    \
			      &spi_cc35xx_data_##n, &spi_cc35xx_config_##n, POST_KERNEL,           \
			      CONFIG_SPI_INIT_PRIORITY, &spi_cc35xx_driver_api)

#define SPI_CC35XX_INIT_FUNC(n)                                                                    \
	static int spi_cc35xx_init_##n(const struct device *dev)                                   \
	{                                                                                          \
		struct spi_cc35xx_data *data = dev->data;                                          \
		const struct spi_cc35xx_config *cfg = dev->config;                                 \
		int err;                                                                           \
                                                                                                   \
		data->ctx.config = NULL;                                                           \
		err = spi_context_cs_configure_all(&data->ctx);                                    \
		if (err < 0)                                                                       \
			return err;                                                                \
                                                                                                   \
		spi_context_unlock_unconditionally(&data->ctx);                                    \
		SPIDisable(cfg->base);                                                             \
		SPIDisableInt(cfg->base, SPI_INT_ALL);                                             \
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority), spi_cc35xx_isr,             \
			    DEVICE_DT_INST_GET(n), 0);                                             \
		irq_enable(DT_INST_IRQN(n));                                                       \
                                                                                                   \
		return 0;                                                                          \
	}

#define SPI_CC35XX_INIT(n)                                                                         \
	PINCTRL_DT_INST_DEFINE(n);                                                                 \
	SPI_CC35XX_INIT_FUNC(n)                                                                    \
                                                                                                   \
	static const struct spi_cc35xx_config spi_cc35xx_config_##n = {                            \
		.base = DT_INST_REG_ADDR(n),                                                       \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                                         \
		.sys_clk_freq = DT_INST_PROP_BY_PHANDLE(n, clocks, clock_frequency),               \
	};                                                                                         \
                                                                                                   \
	static struct spi_cc35xx_data spi_cc35xx_data_##n = {                                      \
		SPI_CONTEXT_INIT_LOCK(spi_cc35xx_data_##n, ctx),                                   \
		SPI_CONTEXT_INIT_SYNC(spi_cc35xx_data_##n, ctx),                                   \
		SPI_CONTEXT_CS_GPIOS_INITIALIZE(DT_DRV_INST(n), ctx)};                             \
                                                                                                   \
	SPI_CC35XX_DEVICE_INIT(n);

DT_INST_FOREACH_STATUS_OKAY(SPI_CC35XX_INIT)
