/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <ti_cc140xp/ti_cc140xp.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/*
 * Byte pattern for TRX_TxTestMode_Modulated only.
 * 0x00 = all zeros (tone at -deviation), 0xFF = all ones (tone at +deviation)
 * 0x55/0xAA = alternating bits (max occupied bandwidth)
 */
#define TX_TEST_PATTERN (0x55U)

#define TX_CMD_SLOT         (0U)
#define RF_CONFIG_ID        (1U)
#define FE_CONFIG_REFERENCE (0xFEFEFEFEU)
#define RF_CONFIG_REFERENCE (0xF000000DU)
#define FREQUENCY           (920600U)

#define TI_CC140XP_NODE DT_NODELABEL(ti_cc140xp)

#if DT_NODE_EXISTS(TI_CC140XP_NODE)
#define TI_CC140XP_ENABLED 1
static const struct device *const ti_cc140xp_dev = DEVICE_DT_GET(TI_CC140XP_NODE);
#else
#define TI_CC140XP_ENABLED 0
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#define HAS_LED 1
#else
#define HAS_LED 0
#warning "LED not found in device tree, LED toggling will be disabled"
#endif

static K_SEM_DEFINE(config_sem, 0, 1);
static K_SEM_DEFINE(cmd_store_complete_sem, 0, 1);

static TRX_Request_CommandStore cmd_tx_test;

#include "settings/rcl_settings_wisun.h"
#define RF_CONFIG_SIZE (LRF_mainRegConfig_wisun_byteCount)
#define RF_CONFIG_PTR  ((uint8_t *)LRF_mainRegConfig_wisun)
#define FE_CONFIG_SIZE (LRF_frontendRegConfig_wisun_byteCount)
#define FE_CONFIG_PTR  ((uint8_t *)LRF_frontendRegConfig_wisun)

/* Handles device-wide events not tied to a specific command: NV corruption and transport errors. */
static void general_callback(TRX_Host_Handle handle, uintptr_t pConfigData,
			     TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pConfigData);
	ARG_UNUSED(request);
	ARG_UNUSED(arg);

	if (events & TRX_EventNvCorrupt) {
		LOG_ERR("TRX NV corrupt");
	}
	if (events & TRX_EventTransportError) {
		LOG_ERR("TRX transport error");
	}
}

/* Signals completion of NV erase, config store, and config persist requests. */
static void config_callback(TRX_Host_Handle handle, uintptr_t pConfigData,
			    TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pConfigData);
	ARG_UNUSED(request);
	ARG_UNUSED(arg);

	if (TRX_EventLastStatusError & events) {
		LOG_ERR("Config error");
		return;
	}
	if (TRX_EventNvEraseComplete & events) {
		k_sem_give(&config_sem);
	}
	if (TRX_EventConfigStoreComplete & events) {
		k_sem_give(&config_sem);
	}
	if (TRX_EventConfigPersistComplete & events) {
		k_sem_give(&config_sem);
	}
}

/* Signals completion (or failure) of the device configuration store request. */
static void store_device_config_callback(TRX_Host_Handle handle, uintptr_t pConfigData,
					 TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pConfigData);
	ARG_UNUSED(arg);

	if (TRX_EventLastStatusError & events) {
		TRX_Request_LastStatus *pLastStatus = (TRX_Request_LastStatus *)request;

		if (pLastStatus->status == TRX_STATUS_INVALID_PARAM) {
			LOG_ERR("Device config: invalid param");
		} else if (pLastStatus->status == TRX_STATUS_INVALID_STATE) {
			LOG_ERR("Device config: invalid state");
		}
		k_sem_give(&config_sem);
		return;
	}
	if (TRX_EventDeviceConfigStoreComplete & events) {
		k_sem_give(&config_sem);
	}
}

/* Handles TX test command completion: command store and unexpected-stop status. */
static void transmit_test_callback(TRX_Host_Handle handle, uintptr_t pCmdStore,
				   TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pCmdStore);
	ARG_UNUSED(arg);

	if (TRX_EventLastStatusError & events) {
		LOG_ERR("Transmit test error");
		return;
	}
	if (TRX_EventCmdStoreComplete & events) {
		k_sem_give(&cmd_store_complete_sem);
	}
	if (TRX_EventCmdStatus & events) {
		TRX_Request_CommandStatus *pStatus = (TRX_Request_CommandStatus *)request;

		LOG_WRN("TX test command stopped with status %d", pStatus->status);
	}
}

int main(void)
{
	TRX_Host_Status status;
	int ret;

	LOG_INF("TI CC140xP RF Carrier Wave Sample");

#if !TI_CC140XP_ENABLED
	LOG_ERR("TI CC140xP device tree node not found");
	return -1;
#endif

	if (!device_is_ready(ti_cc140xp_dev)) {
		LOG_ERR("TI CC140xP device not ready");
		return -1;
	}

#if HAS_LED
	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED GPIO not ready");
		return -1;
	}
	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED: %d", ret);
		return -1;
	}
#endif

	TRX_Host_Params params = {
		.bitRate   = 12000000,
		.generalCb = general_callback,
		.arg       = 0
	};

	ret = ti_cc140xp_host_open(ti_cc140xp_dev, &params);
	if (ret != 0) {
		LOG_ERR("Failed to open TRX Host driver");
		return -1;
	}
	LOG_INF("TRX Host opened");

	TRX_DeviceConfigData deviceConfig = {
		.dioConfig = {
			.dio0 = DIO0_UNCHANGED,
			.dio2 = DIO2_HIGH_PA,
			.dio3 = DIO3_LOW_PA_AND_LNA,
			.dio4 = DIO4_UNCHANGED,
			.dio5 = DIO5_UNCHANGED,
			.dio6 = DIO6_UNCHANGED,
		},
		.clockConfig = TRX_ClockConfig_XOSC,
		.powerMode   = {
			.dwellTimeUs = 0U,
			.powerPolicy = TRX_PowerPolicy_StandbyDisallow,
		},
		.rfMode = TRX_RfMode_SUN,
	};
	status = ti_cc140xp_store_device_config(ti_cc140xp_dev, deviceConfig,
					       store_device_config_callback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store device config");
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);

	status = ti_cc140xp_erase_nv(ti_cc140xp_dev, config_callback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to erase NV");
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("NV erased");

	status = ti_cc140xp_store_config(ti_cc140xp_dev, TRX_CONFIG_ID_FRONTEND,
					FE_CONFIG_PTR, FE_CONFIG_SIZE,
					FE_CONFIG_REFERENCE, true, config_callback,
					TRX_EventConfigStoreComplete |
					TRX_EventConfigPersistComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store FE config");
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);

	status = ti_cc140xp_persist_config(ti_cc140xp_dev, TRX_CONFIG_ID_FRONTEND);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to persist FE config");
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("FE config stored");

	status = ti_cc140xp_store_config(ti_cc140xp_dev, RF_CONFIG_ID,
					RF_CONFIG_PTR, RF_CONFIG_SIZE,
					RF_CONFIG_REFERENCE, false, config_callback,
					TRX_EventConfigStoreComplete |
					TRX_EventConfigPersistComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store PHY config");
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);

	status = ti_cc140xp_persist_config(ti_cc140xp_dev, RF_CONFIG_ID);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to persist PHY config");
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("PHY config stored");

	cmd_tx_test.slot              = TX_CMD_SLOT;
	cmd_tx_test.cmd_id            = TRX_RadioCommand_TransmitTest;
	cmd_tx_test.enable_on_true    = false;
	cmd_tx_test.enable_on_false   = false;
	cmd_tx_test.enable_on_compare = false;
	cmd_tx_test.trigger           = Command_Trigger_Immediate;
	cmd_tx_test.conflict_policy   = TRX_ConflictPolicy_AlwaysInterrupt;
	cmd_tx_test.allow_delay       = false;
	cmd_tx_test.params.txTest.phy0.config_id   = RF_CONFIG_ID;
	cmd_tx_test.params.txTest.phy0.option_mask = TRX_PHY_FEATURE_FSK_MODE_2B_WISUN;
	cmd_tx_test.params.tx.pa                   = TRX_TxPa_High;
	cmd_tx_test.params.txTest.frequency        = FREQUENCY;
	cmd_tx_test.params.txTest.modem            = TRX_RadioCommand_Modem_FSK;
	cmd_tx_test.params.txTest.power.rawValue   = TRX_MAX_POWER;
	cmd_tx_test.params.txTest.mode             = TRX_TxTestMode_Unmodulated;
	cmd_tx_test.params.txTest.pattern          = TX_TEST_PATTERN;

	LOG_INF("Mode: Unmodulated CW at %u kHz", FREQUENCY);

	status = ti_cc140xp_store_cmds(ti_cc140xp_dev, &cmd_tx_test, 1U, &cmd_tx_test,
				      transmit_test_callback,
				      TRX_EventCmdStoreComplete | TRX_EventCmdStatus);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store transmit test command");
		return -1;
	}
	k_sem_take(&cmd_store_complete_sem, K_FOREVER);
	LOG_INF("Transmitting -- press RESET to stop");

#if HAS_LED
	gpio_pin_set_dt(&led, 1);
#endif

	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
