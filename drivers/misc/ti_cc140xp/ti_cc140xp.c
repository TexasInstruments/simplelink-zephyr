/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_cc140xp_trx

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "ti_cc140xp.h"

#include "host/trx_host.h"

LOG_MODULE_REGISTER(ti_cc140xp, CONFIG_TI_CC140XP_LOG_LEVEL);

struct ti_cc140xp_config {
	struct spi_dt_spec spi;
	struct gpio_dt_spec reset_gpio;
	struct gpio_dt_spec int_gpio;
};

struct ti_cc140xp_data {
	struct k_sem lock;
	TRX_Host_Handle handle;
};

static int ti_cc140xp_init(const struct device *dev)
{
	const struct ti_cc140xp_config *config = dev->config;
	struct ti_cc140xp_data *data = dev->data;
	int ret;

	/* Initialize the lock semaphore */
	k_sem_init(&data->lock, 1, 1);

	/* Check if SPI device is ready */
	if (!spi_is_ready_dt(&config->spi)) {
		LOG_ERR("SPI device not ready");
		return -ENODEV;
	}

	/* CS GPIO is handled automatically by the SPI subsystem via the
	 * spi_dt_spec's cs control structure. No manual initialization needed.
	 */

	/* Initialize reset GPIO */
	if (!gpio_is_ready_dt(&config->reset_gpio)) {
		LOG_ERR("Reset GPIO device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure reset GPIO: %d", ret);
		return ret;
	}

	/* Hold reset for a short time, then release */
	k_sleep(K_MSEC(10));
	ret = gpio_pin_set_dt(&config->reset_gpio, 0);
	if (ret < 0) {
		LOG_ERR("Failed to release reset: %d", ret);
		return ret;
	}

	/* Wait for transceiver to come out of reset */
	k_sleep(K_MSEC(10));

	/* Note: Interrupt GPIO is configured by the platform_spi layer when
	 * TRX_Host_open() is called, so we don't configure it here
	 */

	LOG_INF("TI CC140xP transceiver initialized");

	return 0;
}

#define TI_CC140XP_DEFINE(inst)							\
	static struct ti_cc140xp_data ti_cc140xp_data_##inst;			\
										\
	static const struct ti_cc140xp_config ti_cc140xp_config_##inst = {	\
		.spi = SPI_DT_SPEC_INST_GET(inst,				\
					    SPI_OP_MODE_MASTER |		\
					    SPI_TRANSFER_MSB |			\
					    SPI_WORD_SET(8),			\
					    0),					\
		.reset_gpio = GPIO_DT_SPEC_INST_GET(inst, reset_gpios),	\
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, trx_host_spi_int_gpios), \
	};									\
										\
	DEVICE_DT_INST_DEFINE(inst,						\
			      ti_cc140xp_init,					\
			      NULL,						\
			      &ti_cc140xp_data_##inst,				\
			      &ti_cc140xp_config_##inst,				\
			      POST_KERNEL,					\
			      CONFIG_TI_CC140XP_INIT_PRIORITY,			\
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(TI_CC140XP_DEFINE)

/*****************************************************************************
 * Zephyr API Wrapper Functions
 *
 * These functions wrap the TRX Host driver APIs with Zephyr naming conventions.
 * They provide a Zephyr-style interface while calling the underlying host driver.
 *****************************************************************************/

int ti_cc140xp_host_open(const struct device *dev, TRX_Host_Params *params)
{
	struct ti_cc140xp_data *data = dev->data;

	data->handle = TRX_Host_open(params);
	if (data->handle == NULL) {
		return -ENODEV;
	}

	return 0;
}

void ti_cc140xp_host_close(const struct device *dev)
{
	struct ti_cc140xp_data *data = dev->data;

	TRX_Host_close(data->handle);
	data->handle = NULL;
}

TRX_Host_Status ti_cc140xp_register_rx_stream(const struct device *dev,
                                              uint8_t id,
                                              uint8_t *data,
                                              uint16_t len,
                                              TRX_Host_Callback callback,
                                              uint64_t subscribed_events)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_registerRxStream(drv_data->handle, id, data, len, callback,
					 subscribed_events);
}

TRX_Host_Status ti_cc140xp_store_config(const struct device *dev,
                                        uint8_t id,
                                        uint8_t *data,
                                        uint16_t len,
                                        uint32_t reference,
                                        bool force,
                                        TRX_Host_Callback callback,
                                        uint64_t subscribed_events)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_storeConfig(drv_data->handle, id, data, len, reference, force, callback,
				    subscribed_events);
}

TRX_Host_Status ti_cc140xp_store_stream(const struct device *dev,
                                        uint8_t id,
                                        uint8_t *data,
                                        uint16_t len,
                                        TRX_Stream_Retention retention,
                                        TRX_Host_Callback callback,
                                        uint64_t subscribed_events)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_storeStream(drv_data->handle, id, data, len, retention, callback,
				    subscribed_events);
}

TRX_Host_Status ti_cc140xp_continue_stream(const struct device *dev,
                                           uint8_t id,
                                           uint8_t *data,
                                           uint16_t len,
                                           bool end_stream)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_continueStream(drv_data->handle, id, data, len, end_stream);
}

TRX_Host_Status ti_cc140xp_store_cmds(const struct device *dev,
                                      TRX_Request_CommandStore *cmd_stores,
                                      uint8_t num_cmds,
                                      TRX_Request_CommandStore *cmd_to_submit,
                                      TRX_Host_Callback callback,
                                      uint64_t subscribed_events)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_storeCmds(drv_data->handle, cmd_stores, num_cmds, cmd_to_submit, callback,
				  subscribed_events);
}

TRX_Host_Status ti_cc140xp_submit_cmd(const struct device *dev,
                                      uint8_t slot)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_submitCmd(drv_data->handle, slot);
}

TRX_Host_Status ti_cc140xp_stop_cmd(const struct device *dev,
                                    uint8_t slot,
                                    TRX_Command_StopType stop_type)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_stopCmd(drv_data->handle, slot, stop_type);
}

TRX_Host_Status ti_cc140xp_ping(const struct device *dev,
                                uint8_t ping_data,
                                TRX_Host_Callback callback)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_ping(drv_data->handle, ping_data, callback);
}

TRX_Host_Status ti_cc140xp_persist_config(const struct device *dev,
                                          uint8_t id)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_persistConfig(drv_data->handle, id);
}

TRX_Host_Status ti_cc140xp_start_time_sync(const struct device *dev,
                                           TRX_Host_Callback callback)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_startTimeSync(drv_data->handle, callback);
}

TRX_Host_Status ti_cc140xp_util_get_rssi(const struct device *dev,
                                         TRX_Host_Callback callback)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_utilGetRssi(drv_data->handle, callback);
}

TRX_Host_Status ti_cc140xp_util_get_version(const struct device *dev,
                                            TRX_Host_Callback callback)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_utilGetVersion(drv_data->handle, callback);
}

TRX_Host_Status ti_cc140xp_util_get_mac_uuid(const struct device *dev,
                                             TRX_Host_Callback callback)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_utilGetMacUuid(drv_data->handle, callback);
}

TRX_Host_Status ti_cc140xp_util_get_device_info(const struct device *dev,
                                                TRX_Host_Callback callback)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_utilGetDeviceInfo(drv_data->handle, callback);
}

TRX_Host_Status ti_cc140xp_util_enter_ssbl(const struct device *dev,
                                           TRX_Host_Callback callback)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_utilEnterSsbl(drv_data->handle, callback);
}

TRX_Host_Status ti_cc140xp_util_set_power_mode(const struct device *dev,
                                               TRX_PowerMode mode,
                                               TRX_Host_Callback callback)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_utilSetPowerMode(drv_data->handle, mode, callback);
}

TRX_Host_Status ti_cc140xp_util_wake_trx(const struct device *dev)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_utilWakeTrx(drv_data->handle);
}

TRX_Host_Status ti_cc140xp_erase_nv(const struct device *dev,
                                    TRX_Host_Callback callback)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_eraseNv(drv_data->handle, callback);
}

TRX_Host_Status ti_cc140xp_util_set_rf_mode(const struct device *dev,
                                            TRX_RfMode rf_mode,
                                            TRX_Host_Callback callback)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_utilSetRfMode(drv_data->handle, rf_mode, callback);
}

TRX_Host_Status ti_cc140xp_util_dio_setup(const struct device *dev,
                                          TRX_DioConfig *dio_config,
                                          TRX_Host_Callback callback)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_utilDioSetup(drv_data->handle, *dio_config, callback);
}

TRX_Host_Status ti_cc140xp_store_device_config(const struct device *dev,
                                               TRX_DeviceConfigData data,
                                               TRX_Host_Callback callback)
{
	struct ti_cc140xp_data *drv_data = dev->data;

	return TRX_Host_storeDeviceConfig(drv_data->handle, data, callback);
}
