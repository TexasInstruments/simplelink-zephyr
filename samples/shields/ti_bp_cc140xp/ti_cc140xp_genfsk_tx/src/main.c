/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <ti_cc140xp/ti_cc140xp.h>

LOG_MODULE_REGISTER(genfsk_tx, LOG_LEVEL_INF);

/* Length of the payload to transmit */
#define TX_PAYLOAD_LENGTH   (20U)

/* Total length: GenFSK PHY uses 1-byte header carrying payload length */
#define TX_PACKET_LENGTH    (1 + TX_PAYLOAD_LENGTH)

/* Packet interval in ms */
#define PACKET_INTERVAL_MS  (500U)

/* Slot for the TX cmd */
#define TX_CMD_SLOT         (0U)

/* ID of the packet data payload on the TRX */
#define TX_STREAM_ID        (0U)

/* ID of the PHY configuration on the TRX */
#define RF_CONFIG_ID        (1U)

/* Reference of the Front End (FE) configuration on the TRX */
#define FE_CONFIG_REFERENCE (0xFEFEFEFE)

/* Reference of the PHY configuration on the TRX */
#define RF_CONFIG_REFERENCE (0xF000000D)

/* Transmission frequency in Hz */
#define FREQUENCY           (868000U)

/* Device tree nodes */
#define TI_CC140XP_NODE DT_NODELABEL(ti_cc140xp)

#if DT_NODE_EXISTS(TI_CC140XP_NODE)
#define TI_CC140XP_ENABLED 1
static const struct device *const ti_cc140xp_dev = DEVICE_DT_GET(TI_CC140XP_NODE);
#else
#define TI_CC140XP_ENABLED 0
#endif

/* LED configuration */
#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#define HAS_LED 1
#else
#define HAS_LED 0
#warning "LED not found in device tree, LED toggling will be disabled"
#endif

/* GenFSK 50 kbps PHY settings */
#include "settings/rcl_settings_2gfsk50kbps.h"
#define RF_CONFIG_SIZE (LRF_mainRegConfig_2gfsk50kbps_byteCount)
#define RF_CONFIG_PTR  ((uint8_t *)LRF_mainRegConfig_2gfsk50kbps)
#define FE_CONFIG_SIZE (LRF_frontendRegConfig_2gfsk50kbps_byteCount)
#define FE_CONFIG_PTR  ((uint8_t *)LRF_frontendRegConfig_2gfsk50kbps)

/*
 * One semaphore per asynchronous TRX Host completion event the main thread
 * needs to wait on: device config store, RF mode set, command store,
 * stream store, and TX command status.
 */
static K_SEM_DEFINE(config_sem, 0, 1);
static K_SEM_DEFINE(rfmode_complete_sem, 0, 1);
static K_SEM_DEFINE(cmd_store_complete_sem, 0, 1);
static K_SEM_DEFINE(stream_store_complete_sem, 0, 1);
static K_SEM_DEFINE(tx_cmd_status_sem, 0, 1);

static TRX_Request_CommandStore cmd_tx;
static uint8_t tx_packet[TX_PACKET_LENGTH];
static uint16_t seq_number;
static uint32_t cnt_tx_packets;

/* Handles device-wide events not tied to a specific command: NV corruption and transport errors. */
static void general_callback(TRX_Host_Handle handle, uintptr_t pConfigData,
			     TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pConfigData);
	ARG_UNUSED(request);
	ARG_UNUSED(arg);

	if (events & TRX_EventNvCorrupt)
		LOG_ERR("TRX NV corrupt");
	if (events & TRX_EventTransportError)
		LOG_ERR("TRX transport error");
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
		LOG_ERR("Config store error");
		k_sem_give(&config_sem);
		return;
	}

	if (TRX_EventNvEraseComplete & events)
		k_sem_give(&config_sem);
	if (TRX_EventConfigStoreComplete & events)
		k_sem_give(&config_sem);
	if (TRX_EventConfigPersistComplete & events)
		k_sem_give(&config_sem);
}

/* Signals completion (or failure) of the RF mode / device configuration store request. */
static void store_device_config_callback(TRX_Host_Handle handle, uintptr_t pConfigData,
					 TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pConfigData);
	ARG_UNUSED(arg);

	if (TRX_EventLastStatusError & events) {
		TRX_Request_LastStatus *pLastStatus = (TRX_Request_LastStatus *)request;

		if (pLastStatus->status == TRX_STATUS_INVALID_PARAM)
			LOG_ERR("Device config error: invalid param");
		else if (pLastStatus->status == TRX_STATUS_INVALID_STATE)
			LOG_ERR("Device config error: invalid state");
		k_sem_give(&rfmode_complete_sem);
		return;
	}

	if (TRX_EventDeviceConfigStoreComplete & events)
		k_sem_give(&rfmode_complete_sem);
}

/* Handles TX command completion events: command store, stream store, and per-packet TX status. */
static void transmit_callback(TRX_Host_Handle handle, uintptr_t pCmdStore,
			      TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pCmdStore);
	ARG_UNUSED(arg);

	if (TRX_EventLastStatusError & events) {
		LOG_ERR("Transmit error");
		k_sem_give(&tx_cmd_status_sem);
		return;
	}

	if (TRX_EventCmdStoreComplete & events)
		k_sem_give(&cmd_store_complete_sem);

	if (TRX_EventStreamStoreComplete & events)
		k_sem_give(&stream_store_complete_sem);

	if (TRX_EventCmdStatus & events) {
		TRX_Request_CommandStatus *p = (TRX_Request_CommandStatus *)request;

		if (p->status == TRX_CommandStatus_Finished) {
			LOG_INF("TX packet #%u sent", ++cnt_tx_packets);
#if HAS_LED
			gpio_pin_toggle_dt(&led);
#endif
		} else {
			LOG_ERR("TX command ended unexpectedly: status %d", p->status);
		}
		k_sem_give(&tx_cmd_status_sem);
	}
}

int main(void)
{
	TRX_Host_Status status;
	int ret;

	LOG_INF("TI CC140xP RF Packet TX GenFSK Sample");

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

#if HAS_LED
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

	TRX_Host_Params params = {
		.bitRate = 12000000U,
		.generalCb = general_callback,
		.arg = (uintptr_t)NULL
	};

	ret = ti_cc140xp_host_open(ti_cc140xp_dev, &params);
	if (ret != 0) {
		LOG_ERR("Failed to open TRX Host");
		return -1;
	}
	LOG_INF("TRX Host opened successfully");

	k_sleep(K_MSEC(100));

	LOG_INF("Erasing NV...");
	status = ti_cc140xp_erase_nv(ti_cc140xp_dev, config_callback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to erase NV: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("NV erased successfully");

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
			.powerPolicy = (TRX_PowerPolicy)0,
		},
		.rfMode = TRX_RfMode_PropFSK,
	};
	status = ti_cc140xp_store_device_config(ti_cc140xp_dev, deviceConfig,
					       store_device_config_callback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store device config: %d", status);
		return -1;
	}
	k_sem_take(&rfmode_complete_sem, K_FOREVER);
	LOG_INF("TRX device configured successfully");

	LOG_INF("Loading GenFSK FE configuration (%u bytes)...", FE_CONFIG_SIZE);
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

	LOG_INF("Persisting FE configuration...");
	status = ti_cc140xp_persist_config(ti_cc140xp_dev, TRX_CONFIG_ID_FRONTEND);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to persist FE config: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("FE configuration persisted successfully");

	LOG_INF("Loading GenFSK PHY configuration (%u bytes)...", RF_CONFIG_SIZE);
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

	LOG_INF("Persisting PHY configuration...");
	status = ti_cc140xp_persist_config(ti_cc140xp_dev, RF_CONFIG_ID);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to persist PHY config: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("PHY configuration persisted successfully");

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
	cmd_tx.params.tx.phy0.option_mask = TRX_PHY_FEATURE_WHITENING_DISABLED_2GFSK50KBPS;
	cmd_tx.params.tx.pa = TRX_TxPa_High;
	cmd_tx.params.tx.stream_id = TX_STREAM_ID;
	cmd_tx.params.tx.frequency = FREQUENCY;
	cmd_tx.params.tx.modem = TRX_RadioCommand_Modem_FSK;
	/* Use the last (highest) entry in the calibrated High-PA power table */
	cmd_tx.params.tx.power =
		TRX_txPowerLevelsHigh_2gfsk50kbps.powerLevels[
			TRX_txPowerLevelsHigh_2gfsk50kbps.numEntries - 1];

	LOG_INF("Storing TX command...");
	status = ti_cc140xp_store_cmds(ti_cc140xp_dev, &cmd_tx, 1U, NULL,
				      transmit_callback,
				      TRX_EventCmdStatus | TRX_EventCmdStoreComplete |
				      TRX_EventCmdSubmitComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store TX command: %d", status);
		return -1;
	}
	k_sem_take(&cmd_store_complete_sem, K_FOREVER);
	LOG_INF("TX command stored - transmitting on %u Hz", FREQUENCY);

	/* GenFSK 1-byte header carries payload length; set once before loop */
	tx_packet[0] = TX_PAYLOAD_LENGTH;

	while (1) {
		/* Packet: [length(1B)][seqHi][seqLo][rand x 18] */
		tx_packet[1] = (uint8_t)(seq_number >> 8);
		tx_packet[2] = (uint8_t)(seq_number++);
		for (int i = 3; i < TX_PACKET_LENGTH; i++) {
			tx_packet[i] = (uint8_t)rand();
		}

		status = ti_cc140xp_store_stream(ti_cc140xp_dev, TX_STREAM_ID,
						tx_packet, sizeof(tx_packet),
						Stream_Retention_Flush_Streaming,
						transmit_callback,
						TRX_EventStreamStoreComplete);
		if (status != TRX_Host_Success) {
			LOG_ERR("Failed to store stream: %d", status);
			continue;
		}
		k_sem_take(&stream_store_complete_sem, K_FOREVER);

		status = ti_cc140xp_submit_cmd(ti_cc140xp_dev, TX_CMD_SLOT);
		if (status != TRX_Host_Success) {
			LOG_ERR("Failed to submit TX command: %d", status);
			continue;
		}

		k_sem_take(&tx_cmd_status_sem, K_FOREVER);

		/* Wait between transmissions */
		k_sleep(K_MSEC(PACKET_INTERVAL_MS));
	}

	return 0;
}
