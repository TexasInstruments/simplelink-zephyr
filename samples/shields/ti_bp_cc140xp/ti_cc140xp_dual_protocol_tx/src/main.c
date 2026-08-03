/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <ti_cc140xp/ti_cc140xp.h>

LOG_MODULE_REGISTER(dual_protocol_tx, LOG_LEVEL_INF);

/* Length of the payload to transmit */
#define PAYLOAD_LENGTH (20U)

/* Total length of the HEADER + PAYLOAD to transmit */
#define PACKET_LENGTH_OQPSK (sizeof(TRX_PayloadHeader) + PAYLOAD_LENGTH)
#define PACKET_LENGTH_FSK   (1 + PAYLOAD_LENGTH)

/* Set packet interval to 2s */
#define PACKET_INTERVAL_MS     2000

/* Slot for the TX cmds */
#define TX_CMD_SLOT_OQPSK   (0U)
#define TX_CMD_SLOT_FSK     (1U)

/* ID of the packet data payload on the TRX */
#define TX_STREAM_ID     (0U)

/* ID of the PHY configuration on the TRX */
#define RF_CONFIG_ID     (1U)

/* Reference of the Front End (FE) configuration on the TRX */
#define FE_CONFIG_REFERENCE (0xFEFEFEFE)

/* Reference of the PHY configuration on the TRX */
#define RF_CONFIG_REFERENCE (0xF000000D)

/* Device tree nodes */
#define TI_CC140XP_NODE DT_NODELABEL(ti_cc140xp)

#if DT_NODE_EXISTS(TI_CC140XP_NODE)
#define TI_CC140XP_ENABLED 1
static const struct device *const ti_cc140xp_dev = DEVICE_DT_GET(TI_CC140XP_NODE);
#else
#define TI_CC140XP_ENABLED 0
#endif

/* LED configuration - check if LEDs exist in device tree */
#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#define HAS_LED0 1
#else
#define HAS_LED0 0
#warning "LED0 not found in device tree, green LED toggling will be disabled"
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(led1), okay)
#define LED1_NODE DT_ALIAS(led1)
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
#define HAS_LED1 1
#else
#define HAS_LED1 0
#warning "LED1 not found in device tree, red LED toggling will be disabled"
#endif

/* Slot that completed last TX — written in callback, read in main */
static uint8_t last_tx_slot = TX_CMD_SLOT_OQPSK;

/* Semaphores for synchronization */
static K_SEM_DEFINE(config_sem, 0, 1);
static K_SEM_DEFINE(cmd_store_complete_sem, 0, 1);
static K_SEM_DEFINE(stream_store_complete_sem, 0, 1);
static K_SEM_DEFINE(tx_cmd_status_available_sem, 0, 1);

/* Global Transmit command */
static TRX_Request_CommandStore cmd_tx;

/* Transmit packet buffer */
static uint8_t tx_packet[PACKET_LENGTH_OQPSK];

/* RCL settings for dual protocol OQPSK/2GFSK50kbps */
#include "settings/rcl_settings_dualrx_sun_oqpsk_2gfsk50kbps.h"
#define RF_CONFIG_SIZE (LRF_mainRegConfig_dualrxSunOqpsk2gfsk50kbps_byteCount)
#define RF_CONFIG_PTR  ((uint8_t *)LRF_mainRegConfig_dualrxSunOqpsk2gfsk50kbps)

#define FE_CONFIG_SIZE (LRF_frontendRegConfig_dualrxSunOqpsk2gfsk50kbps_byteCount)
#define FE_CONFIG_PTR  ((uint8_t *)LRF_frontendRegConfig_dualrxSunOqpsk2gfsk50kbps)

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

/* Handles TX command completion events for both PHY slots: command store, stream store, and per-packet TX status. */
static void tx_callback(TRX_Host_Handle handle, uintptr_t pCmdStore,
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

		/* Record slot before signalling so main() can read it */
		last_tx_slot = pCmdStatusRequest->slot;

		/* TX completed successfully - signal completion */
		k_sem_give(&tx_cmd_status_available_sem);

		/* Dual-LED: toggle each LED independently */
#if HAS_LED0 && HAS_LED1
		if (pCmdStatusRequest->slot == TX_CMD_SLOT_OQPSK) {
			gpio_pin_toggle_dt(&led0);
		} else {
			gpio_pin_toggle_dt(&led1);
		}
#endif

		if (pCmdStatusRequest->slot == TX_CMD_SLOT_OQPSK) {
			LOG_INF("OQPSK TX packet sent successfully (868 MHz)");
		} else {
			LOG_INF("FSK TX packet sent successfully (869 MHz)");
		}
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

	LOG_INF("TI CC140xP Dual Protocol Packet TX Sample");
	LOG_INF("Alternates between OQPSK @ 868 MHz and 2GFSK @ 869 MHz");

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

#if HAS_LED0
	/* Configure LED0 (green) */
	if (!gpio_is_ready_dt(&led0)) {
		LOG_ERR("LED0 GPIO device not ready");
		return -1;
	}

	ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED0 GPIO: %d", ret);
		return -1;
	}
	LOG_INF("LED0 (green) configured successfully");
#endif

#if HAS_LED1
	/* Configure LED1 (red) */
	if (!gpio_is_ready_dt(&led1)) {
		LOG_ERR("LED1 GPIO device not ready");
		return -1;
	}

	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED1 GPIO: %d", ret);
		return -1;
	}
	LOG_INF("LED1 (red) configured successfully");
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
		.rfMode = TRX_RfMode_PropFSKPeakAGC,
	};
	status = ti_cc140xp_store_device_config(ti_cc140xp_dev, deviceConfig,
					       storeDeviceConfigCallback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store device config: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("TRX device configured successfully (PropFSKPeakAGC mode)");

	/* Erase NV */
	LOG_INF("Erasing NV...");
	status = ti_cc140xp_erase_nv(ti_cc140xp_dev, config_callback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to erase NV: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("NV erased successfully");

	/* Load the front end configuration */
	LOG_INF("Loading dual RX FE configuration (%u bytes)...", FE_CONFIG_SIZE);
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
	LOG_INF("Loading dual protocol PHY configuration (%u bytes)...", RF_CONFIG_SIZE);
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

	/* Create the OQPSK TX command */
	LOG_INF("Creating OQPSK TX command...");
	cmd_tx.slot = TX_CMD_SLOT_OQPSK;
	cmd_tx.cmd_id = TRX_RadioCommand_Transmit;
	cmd_tx.enable_on_true = false;
	cmd_tx.enable_on_false = false;
	cmd_tx.enable_on_compare = false;
	cmd_tx.trigger = Command_Trigger_Immediate;
	cmd_tx.conflict_policy = TRX_ConflictPolicy_AlwaysInterrupt;
	cmd_tx.allow_delay = false;
	cmd_tx.params.tx.phy0.config_id = RF_CONFIG_ID;
	cmd_tx.params.tx.pa = TRX_TxPa_High;
	cmd_tx.params.tx.stream_id = TX_STREAM_ID;
	cmd_tx.params.tx.frequency = 868000;
	cmd_tx.params.tx.modem = TRX_RadioCommand_Modem_OQPSK;
	cmd_tx.params.tx.power.rawValue = TRX_MAX_POWER;

	/* Store the OQPSK TX command to the TRX */
	status = ti_cc140xp_store_cmds(ti_cc140xp_dev, &cmd_tx, 1U, NULL,
				      tx_callback,
				      TRX_EventCmdStatus | TRX_EventCmdStoreComplete |
				      TRX_EventCmdSubmitComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store OQPSK TX command: %d", status);
		return -1;
	}
	k_sem_take(&cmd_store_complete_sem, K_FOREVER);
	LOG_INF("OQPSK TX command stored successfully");

	/* Create the FSK TX command - only modify the delta */
	LOG_INF("Creating FSK TX command...");
	cmd_tx.slot = TX_CMD_SLOT_FSK;
	cmd_tx.params.tx.stream_id = TX_STREAM_ID;
	cmd_tx.params.tx.frequency = 869000;
	cmd_tx.params.tx.modem = TRX_RadioCommand_Modem_FSK;

	/* Store the FSK TX command to the TRX */
	status = ti_cc140xp_store_cmds(ti_cc140xp_dev, &cmd_tx, 1U, NULL,
				      tx_callback,
				      TRX_EventCmdStatus | TRX_EventCmdStoreComplete |
				      TRX_EventCmdSubmitComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store FSK TX command: %d", status);
		return -1;
	}
	k_sem_take(&cmd_store_complete_sem, K_FOREVER);
	LOG_INF("FSK TX command stored successfully");

	/* Alternate between OQPSK and FSK packets */
	int iteration = 0;

	while (1) {
		uint8_t cmdSlot = iteration % 2 == 0 ? TX_CMD_SLOT_OQPSK : TX_CMD_SLOT_FSK;
		uint8_t packetLength = iteration % 2 == 0 ? PACKET_LENGTH_OQPSK : PACKET_LENGTH_FSK;

		/* Update the PHY header based on the selected modulation */
		if (cmdSlot == TX_CMD_SLOT_OQPSK) {
			/* Clear the packet header and fill out the required fields */
			memset(tx_packet, 0, sizeof(TRX_PayloadHeader));
			((TRX_PayloadHeader_SUNOQPSK *)tx_packet)->length = PAYLOAD_LENGTH;
			((TRX_PayloadHeader_SUNOQPSK *)tx_packet)->rate_mode = TRX_PayloadHeader_SunOQPSK_RateMode3_50kbps;
			((TRX_PayloadHeader_SUNOQPSK *)tx_packet)->modulation = TRX_PayloadHeader_Modulation_OQPSK;

			/* Payload starts after 4 bytes of header */
			memset(&tx_packet[sizeof(TRX_PayloadHeader)], 0xAA, PAYLOAD_LENGTH);
		} else {
			/* FSK - Update the PHY header (1 byte) */
			tx_packet[0] = PAYLOAD_LENGTH;

			/* Payload starts after 1 byte of header */
			memset(&tx_packet[1], 0xAA, PAYLOAD_LENGTH);
		}

		/* Load the stream of data to transmit */
		status = ti_cc140xp_store_stream(ti_cc140xp_dev,
						TX_STREAM_ID,
						tx_packet,
						packetLength,
						Stream_Retention_Flush_Streaming,
						tx_callback,
						TRX_EventStreamStoreComplete);
		if (status != TRX_Host_Success) {
			LOG_ERR("Failed to store stream: %d", status);
			k_busy_wait(100U);
			continue;
		}
		k_sem_take(&stream_store_complete_sem, K_FOREVER);

		/* Submit the TX command to the TRX */
		status = ti_cc140xp_submit_cmd(ti_cc140xp_dev, cmdSlot);
		if (status != TRX_Host_Success) {
			LOG_ERR("Failed to submit TX command: %d", status);
			k_busy_wait(100U);
			continue;
		}

		/* Wait for TX to complete and status to be available */
		k_sem_take(&tx_cmd_status_available_sem, K_FOREVER);

		/* Single-LED: 1 pulse = OQPSK, 2 pulses = FSK */
#if HAS_LED0 && !HAS_LED1
		gpio_pin_set_dt(&led0, 1);
		k_sleep(K_MSEC(100));
		gpio_pin_set_dt(&led0, 0);
		if (last_tx_slot == TX_CMD_SLOT_FSK) {
			k_sleep(K_MSEC(150));
			gpio_pin_set_dt(&led0, 1);
			k_sleep(K_MSEC(100));
			gpio_pin_set_dt(&led0, 0);
		}
#endif

		/* Wait for packet interval */
		k_sleep(K_MSEC(PACKET_INTERVAL_MS));
		iteration = (iteration + 1) % 2;
	}

	return 0;
}
