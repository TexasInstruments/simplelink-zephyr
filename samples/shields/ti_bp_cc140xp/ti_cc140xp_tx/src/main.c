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

/* Length of the payload to transmit */
#define TX_PAYLOAD_LENGTH (76U)

/* Total length of the HEADER + PAYLOAD to transmit */
#define TX_PACKET_LENGTH (sizeof(TRX_PayloadHeader) + TX_PAYLOAD_LENGTH)

/* Set packet interval to 500ms */
#define PACKET_INTERVAL_MS 500

/* Slot for the TX cmd */
#define TX_CMD_SLOT      (0U)

/* ID of the packet data payload on the TRX */
#define TX_STREAM_ID     (0U)

/* ID of the PHY configuration on the TRX */
#define RF_CONFIG_ID     (1U)

/* Reference of the Front End (FE) configuration on the TRX */
#define FE_CONFIG_REFERENCE (0xFEFEFEFE)

/* Reference of the PHY configuration on the TRX */
#define RF_CONFIG_REFERENCE (0xF000000D)

/* Transmission frequency in Hz */
#define FREQUENCY        (920600U)

/* Device tree nodes */
#define TI_CC140XP_NODE DT_NODELABEL(ti_cc140xp)

#if DT_NODE_EXISTS(TI_CC140XP_NODE)
#define TI_CC140XP_ENABLED 1
static const struct device *const ti_cc140xp_dev = DEVICE_DT_GET(TI_CC140XP_NODE);
#else
#define TI_CC140XP_ENABLED 0
#endif

/* LED configuration - check if LED exists in device tree */
#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#define HAS_LED 1
#else
#define HAS_LED 0
#warning "LED not found in device tree, LED toggling will be disabled"
#endif

/* Semaphores for synchronization */
static K_SEM_DEFINE(config_sem, 0, 1);
static K_SEM_DEFINE(cmd_store_complete_sem, 0, 1);
static K_SEM_DEFINE(stream_store_complete_sem, 0, 1);
static K_SEM_DEFINE(cmd_submit_complete_sem, 0, 1);

/* Global Transmit command */
static TRX_Request_CommandStore cmd_tx;

/* TX packet counter */
static uint32_t cnt_tx_packets;

/* Transmit packet buffer */
static uint8_t tx_packet[TX_PACKET_LENGTH] = {
	0, 0, 0, 0, /* Reserved for common header */
	75, 74, 73, 72, 71,
	70, 69, 68, 67, 66, 65, 64, 63,
	62, 61, 60, 59, 58, 57, 56, 55,
	54, 53, 52, 51, 50, 49, 48, 47,
	46, 45, 44, 43, 42, 41, 40, 39,
	38, 37, 36, 35, 34, 33, 32, 31,
	30, 29, 28, 27, 26, 25, 24, 23,
	22, 21, 20, 19, 18, 17, 16, 15,
	14, 13, 12, 11, 10,  9,  8,  7,
	6,  5,  4,  3,  2,  1,  0,
};

/* RCL settings for Wi-SUN */
#include "settings/rcl_settings_wisun.h"
#define RF_CONFIG_SIZE (LRF_mainRegConfig_wisun_byteCount)
#define RF_CONFIG_PTR  ((uint8_t *)LRF_mainRegConfig_wisun)

#define FE_CONFIG_SIZE (LRF_frontendRegConfig_wisun_byteCount)
#define FE_CONFIG_PTR  ((uint8_t *)LRF_frontendRegConfig_wisun)

/* Callback functions */

/* Handles device-wide events not tied to a specific command: NV corruption and transport errors. */
static void general_callback(TRX_Host_Handle handle, uintptr_t pConfigData,
			     TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pConfigData);
	ARG_UNUSED(request);
	ARG_UNUSED(arg);

	/* If NV is corrupt, erase it */
	if (events & TRX_EventNvCorrupt) {
		LOG_ERR("TRX NV corrupt - needs erase");
	}

	/* If transport error occurred, reinitialize */
	if (events & TRX_EventTransportError) {
		LOG_ERR("TRX transport error - needs reinitialization");
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

	/* Check for errors */
	if (TRX_EventLastStatusError & events) {
		LOG_ERR("Config store error");
		return;
	}

	if (TRX_EventNvEraseComplete & events) {
		LOG_DBG("NV erase complete");
		k_sem_give(&config_sem);
	}

	if (TRX_EventConfigStoreComplete & events) {
		LOG_DBG("Config store complete");
		k_sem_give(&config_sem);
	}

	if (TRX_EventConfigPersistComplete & events) {
		LOG_DBG("Config persist complete");
		k_sem_give(&config_sem);
	}
}

/* Signals completion (or failure) of the device configuration store request. */
static void storeDeviceConfigCallback(TRX_Host_Handle handle, uintptr_t pConfigData,
				       TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pConfigData);
	ARG_UNUSED(arg);

	if (TRX_EventLastStatusError & events) {
		TRX_Request_LastStatus *pLastStatus = (TRX_Request_LastStatus *)request;

		if (pLastStatus->status == TRX_STATUS_INVALID_PARAM) {
			LOG_ERR("Device config error: invalid param");
		} else if (pLastStatus->status == TRX_STATUS_INVALID_STATE) {
			LOG_ERR("Device config error: invalid state");
		}
		k_sem_give(&config_sem);
		return;
	}

	if (TRX_EventDeviceConfigStoreComplete & events) {
		LOG_DBG("Device config store complete");
		k_sem_give(&config_sem);
	}
}

/* Handles TX command completion events: command store, stream store, and per-packet TX status. */
static void transmit_callback(TRX_Host_Handle handle, uintptr_t pCmdStore,
			      TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pCmdStore);
	ARG_UNUSED(arg);

	/* Check for errors */
	if (TRX_EventLastStatusError & events) {
		LOG_ERR("Transmit error");
		return;
	}

	if (TRX_EventCmdStatus & events) {
		TRX_Request_CommandStatus *pCmdStatusRequest = (TRX_Request_CommandStatus *)request;

		/* Check if TX command finished successfully */
		if (TRX_CommandStatus_Finished != pCmdStatusRequest->status) {
			LOG_WRN("TX command exited unexpectedly with status %d",
				pCmdStatusRequest->status);
			return;
		}

		k_sem_give(&cmd_submit_complete_sem);
		cnt_tx_packets++;
#if HAS_LED
		gpio_pin_toggle_dt(&led);
#endif
		LOG_INF("TX packet #%u sent successfully", cnt_tx_packets);
	}

	if (TRX_EventCmdStoreComplete & events) {
		LOG_DBG("Command store complete");
		k_sem_give(&cmd_store_complete_sem);
	}

	if (TRX_EventStreamStoreComplete & events) {
		LOG_DBG("Stream store complete");
		k_sem_give(&stream_store_complete_sem);
	}
}

int main(void)
{
	TRX_Host_Status status;

	LOG_INF("TI CC140xP RF Packet TX Sample");

#if !TI_CC140XP_ENABLED
	LOG_ERR("TI CC140xP device tree node not found");
	LOG_ERR("Please ensure your board overlay defines the ti_cc140xp node");
	return -1;
#endif

	if (!device_is_ready(ti_cc140xp_dev)) {
		LOG_ERR("TI CC140xP device not ready");
		return -1;
	}

	LOG_INF("TI CC140xP device is ready");

	int ret;

#if HAS_LED
	/* Configure LED */
	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED GPIO device not ready");
		return -1;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED GPIO: %d", ret);
		return -1;
	}
	LOG_INF("LED configured successfully");
#endif

	/* Initialize the TRX Host driver */
	TRX_Host_Params params = {
		.bitRate = 12000000, /* 12 MHz SPI */
		.generalCb = general_callback,
		.arg = 0
	};

	ret = ti_cc140xp_host_open(ti_cc140xp_dev, &params);
	if (ret != 0) {
		LOG_ERR("Failed to open TRX Host driver");
		return -1;
	}
	LOG_INF("TRX Host driver opened successfully");

	/* Configure TRX device */
	LOG_INF("Configuring TRX device...");
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
		.powerMode = {
			.dwellTimeUs = 0U,
			.powerPolicy = TRX_PowerPolicy_StandbyDisallow,
		},
		.rfMode = TRX_RfMode_SUN,
	};
	status = ti_cc140xp_store_device_config(ti_cc140xp_dev, deviceConfig,
					       storeDeviceConfigCallback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store device config: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("TRX device configured successfully");

	/* Erase NV */
	LOG_INF("Erasing NV...");
	status = ti_cc140xp_erase_nv(ti_cc140xp_dev, config_callback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to erase NV: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("NV erased successfully");

	/* Load the PHY configuration */
	LOG_INF("Loading Wi-SUN FE configuration (%u bytes)...", FE_CONFIG_SIZE);
	status = ti_cc140xp_store_config(ti_cc140xp_dev, TRX_CONFIG_ID_FRONTEND,
					FE_CONFIG_PTR, FE_CONFIG_SIZE,
					FE_CONFIG_REFERENCE, true, config_callback,
					TRX_EventConfigStoreComplete |
					TRX_EventConfigPersistComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store FE config: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("FE configuration loaded successfully");

	/* Persist the configuration */
	LOG_INF("Persisting FE configuration...");
	status = ti_cc140xp_persist_config(ti_cc140xp_dev, TRX_CONFIG_ID_FRONTEND);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to persist FE config: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("FE configuration persisted successfully");

	/* Load the PHY configuration */
	LOG_INF("Loading Wi-SUN PHY configuration (%u bytes)...", RF_CONFIG_SIZE);
	status = ti_cc140xp_store_config(ti_cc140xp_dev, RF_CONFIG_ID,
					RF_CONFIG_PTR, RF_CONFIG_SIZE,
					RF_CONFIG_REFERENCE, false, config_callback,
					TRX_EventConfigStoreComplete |
					TRX_EventConfigPersistComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store PHY config: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("PHY configuration loaded successfully");

	/* Persist the configuration */
	LOG_INF("Persisting PHY configuration...");
	status = ti_cc140xp_persist_config(ti_cc140xp_dev, RF_CONFIG_ID);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to persist config: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("PHY configuration persisted successfully");

	/* Create a TX command */
	LOG_INF("Creating TX command...");
	cmd_tx.slot = TX_CMD_SLOT;
	cmd_tx.cmd_id = TRX_RadioCommand_Transmit;
	cmd_tx.enable_on_true = false;
	cmd_tx.enable_on_false = false;
	cmd_tx.enable_on_compare = false;
	cmd_tx.trigger = Command_Trigger_Immediate;
	cmd_tx.conflict_policy = TRX_ConflictPolicy_AlwaysInterrupt;
	cmd_tx.allow_delay = false;
	cmd_tx.params.tx.phy0.config_id = RF_CONFIG_ID;
	cmd_tx.params.tx.phy0.option_mask = TRX_PHY_FEATURE_FSK_MODE_2B_WISUN;
	cmd_tx.params.tx.pa = TRX_TxPa_High;
	cmd_tx.params.tx.stream_id = TX_STREAM_ID;
	cmd_tx.params.tx.frequency = FREQUENCY;
	cmd_tx.params.tx.modem = TRX_RadioCommand_Modem_FSK;
	cmd_tx.params.tx.power = TRX_txPowerLevelsHigh_wisun.powerLevels[TRX_txPowerLevelsHigh_wisun.numEntries - 1];

	/* Store the TX command to the TRX */
	status = ti_cc140xp_store_cmds(ti_cc140xp_dev, &cmd_tx, 1U, NULL,
				      transmit_callback,
				      TRX_EventCmdStatus | TRX_EventCmdStoreComplete |
				      TRX_EventCmdSubmitComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store TX command: %d", status);
		return -1;
	}
	k_sem_take(&cmd_store_complete_sem, K_FOREVER);
	LOG_INF("TX command stored successfully");

	/* Update the PHY header */
	if (TRX_RadioCommand_Modem_FSK == cmd_tx.params.tx.modem) {
		((TRX_PayloadHeader_SunFSK *)tx_packet)->length = TX_PAYLOAD_LENGTH;
		((TRX_PayloadHeader_SunFSK *)tx_packet)->modulation =
			TRX_PayloadHeader_Modulation_FSK;
		((TRX_PayloadHeader_SunFSK *)tx_packet)->mode_switch = 0U;
		((TRX_PayloadHeader_SunFSK *)tx_packet)->fcs_mode = 0U;
		((TRX_PayloadHeader_SunFSK *)tx_packet)->whitening = 1U;
	} else if (TRX_RadioCommand_Modem_OFDM == cmd_tx.params.tx.modem) {
		((TRX_PayloadHeader_SunOFDM *)tx_packet)->length = TX_PAYLOAD_LENGTH;
		((TRX_PayloadHeader_SunOFDM *)tx_packet)->modulation =
			TRX_PayloadHeader_Modulation_OFDM;
		((TRX_PayloadHeader_SunOFDM *)tx_packet)->rate =
			TRX_PayloadHeader_SunOFDM_Rate_MCS6;
		((TRX_PayloadHeader_SunOFDM *)tx_packet)->scrambler = 0U;
		((TRX_PayloadHeader_SunOFDM *)tx_packet)->newPhyId = 0U;
	} else {
		LOG_ERR("Unsupported modem type");
		return -1;
	}

	LOG_INF("Starting TX packet transmission loop...");
	LOG_INF("Transmitting on frequency %u kHz", cmd_tx.params.tx.frequency);

	/* Main transmission loop */
	while (1) {
		/* Wait for packet interval */
		k_sleep(K_MSEC(PACKET_INTERVAL_MS));

		/* Load the stream of data to transmit */
		status = ti_cc140xp_store_stream(ti_cc140xp_dev, TX_STREAM_ID,
						tx_packet, sizeof(tx_packet),
						Stream_Retention_Flush_Streaming,
						transmit_callback,
						TRX_EventLastStatusError |
						TRX_EventStreamStoreComplete);
		if (status != TRX_Host_Success) {
			LOG_ERR("Failed to store stream: %d", status);
			continue;
		}
		k_sem_take(&stream_store_complete_sem, K_FOREVER);

		/* Issue the TX command to the radio */
		status = ti_cc140xp_submit_cmd(ti_cc140xp_dev, TX_CMD_SLOT);
		if (status != TRX_Host_Success) {
			LOG_ERR("Failed to submit TX command: %d", status);
			continue;
		}
		k_sem_take(&cmd_submit_complete_sem, K_FOREVER);
	}

	return 0;
}
