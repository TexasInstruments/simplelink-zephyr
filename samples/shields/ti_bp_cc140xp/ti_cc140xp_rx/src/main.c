/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <ti_cc140xp/ti_cc140xp.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* The data received will contain both timestamp (4B) and RSSI (1B) information. */
#define RSSI_SIZE_BYTES         (1U)
#define TIMESTAMP_SIZE_BYTES    (4U)

/* Maximum payload in a receive operation */
#define RX_PAYLOAD_LENGTH   (76U)

/* Total size of data that can be received including HEADER + PAYLOAD + RSSI + TIMESTAMP */
#define RX_PACKET_LENGTH    (sizeof(TRX_PayloadHeader) + RX_PAYLOAD_LENGTH + RSSI_SIZE_BYTES + TIMESTAMP_SIZE_BYTES)

/* Slot for the RX cmd */
#define RX_CMD_SLOT      (0U)

/* ID of the packet data payload on the TRX */
#define RX_STREAM_ID     (0U)

/* ID of the PHY configuration on the TRX */
#define RF_CONFIG_ID     (1U)

/* Reference of the Front End (FE) configuration on the TRX */
#define FE_CONFIG_REFERENCE (0xFEFEFEFE)

/* Reference of the PHY configuration on the TRX */
#define RF_CONFIG_REFERENCE (0xF000000D)

/* Reception frequency in Hz */
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

/* RCL settings for Wi-SUN */
#include "settings/rcl_settings_wisun.h"
#define RF_CONFIG_SIZE (LRF_mainRegConfig_wisun_byteCount)
#define RF_CONFIG_PTR  ((uint8_t *)LRF_mainRegConfig_wisun)

#define FE_CONFIG_SIZE (LRF_frontendRegConfig_wisun_byteCount)
#define FE_CONFIG_PTR  ((uint8_t *)LRF_frontendRegConfig_wisun)

/* Debug structure for RX failures */
typedef struct rxDebugging {
	bool crcNotOk;
	bool gracefulStopTimeout;
	bool gracefulStopApi;
	bool gracefulStopScheduling;
	bool hardStopApi;
	bool hardStopScheduling;
	bool rxErr;
	bool rxBufferCorruption;
	bool rxFifoErr;
	bool error;
} rxDebugging_t;

/* Semaphores for synchronization */
static K_SEM_DEFINE(config_sem, 0, 1);
static K_SEM_DEFINE(cmd_store_complete_sem, 0, 1);

/* Global Receive command */
static TRX_Request_CommandStore cmdRx;

/* Receive buffer */
static uint8_t rxBuffer[RX_PACKET_LENGTH];

/* RX packet counter */
static uint32_t cntRxPackets;

/* Debug variable for RX failures */
static rxDebugging_t rxDebug;

/* Buffer where the received packet header will be copied */
static uint8_t rxHeader[sizeof(TRX_PayloadHeader)];

/* Buffer where the received packet payload will be copied */
static uint8_t rxPayload[RX_PAYLOAD_LENGTH];

/* Buffer where the RSSI of a received packet will be copied */
static uint8_t rxRssi[RSSI_SIZE_BYTES];

/* Buffer where the timestamp of a received packet will be copied */
static uint8_t rxTimestamp[TIMESTAMP_SIZE_BYTES];

static void read_packet(uint8_t *data)
{
	TRX_PayloadHeader *header = (TRX_PayloadHeader *)data;
	uint8_t *start_of_payload = (uint8_t *)header + sizeof(TRX_PayloadHeader);
	uint8_t *end_of_payload = (uint8_t *)header + header->length + sizeof(TRX_PayloadHeader);
	int8_t *rssi = (int8_t *)end_of_payload;
	uint32_t *timestamp = (uint32_t *)(end_of_payload + sizeof(int8_t));

	memcpy(rxHeader, header, sizeof(TRX_PayloadHeader));
	memcpy(rxPayload, start_of_payload, header->length);
	memcpy(rxRssi, rssi, RSSI_SIZE_BYTES);
	memcpy(rxTimestamp, timestamp, TIMESTAMP_SIZE_BYTES);
}

static void debug_cmd_failure(TRX_Request_CommandStatus *p_req_cmd_status)
{
	TRX_CommandStatus cmd_status = p_req_cmd_status->status;
	TRX_Request_CommandStatus_Params_Rx *p_rx =
		(TRX_Request_CommandStatus_Params_Rx *)&p_req_cmd_status->params;

	if (p_rx->num_crc_ok == 0U) {
		rxDebug.crcNotOk = true;
	}

	switch (cmd_status) {
	case TRX_CommandStatus_GracefulStopTimeout:
		rxDebug.gracefulStopTimeout = true;
		break;
	case TRX_CommandStatus_GracefulStopApi:
		rxDebug.gracefulStopApi = true;
		break;
	case TRX_CommandStatus_GracefulStopScheduling:
		rxDebug.gracefulStopScheduling = true;
		break;
	case TRX_CommandStatus_HardStopApi:
		rxDebug.hardStopApi = true;
		break;
	case TRX_CommandStatus_HardStopScheduling:
		rxDebug.hardStopScheduling = true;
		break;
	case TRX_CommandStatus_RxErr:
		rxDebug.rxErr = true;
		break;
	case TRX_CommandStatus_Error_RxBufferCorruption:
		rxDebug.rxBufferCorruption = true;
		break;
	case TRX_CommandStatus_Error_RxFifo:
		rxDebug.rxFifoErr = true;
		break;
	default:
		rxDebug.error = true;
		break;
	}
}

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
		LOG_ERR("Config store error");
		k_sem_give(&config_sem);
		return;
	}

	if (TRX_EventNvEraseComplete & events) {
		LOG_INF("NV erase complete");
		k_sem_give(&config_sem);
	}

	if (TRX_EventConfigStoreComplete & events) {
		LOG_INF("Config store complete");
		k_sem_give(&config_sem);
	}

	if (TRX_EventConfigPersistComplete & events) {
		LOG_INF("Config persist complete");
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
			LOG_ERR("Device config error: invalid param");
		} else if (pLastStatus->status == TRX_STATUS_INVALID_STATE) {
			LOG_ERR("Device config error: invalid state");
		}
		k_sem_give(&config_sem);
		return;
	}

	if (TRX_EventDeviceConfigStoreComplete & events) {
		LOG_INF("Device config store complete");
		k_sem_give(&config_sem);
	}
}

/* Handles RX command completion events: command store, buffer overflow, and received-packet delivery. */
static void receive_callback(TRX_Host_Handle handle, uintptr_t pCmdStoreOrData,
			     TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(arg);

	if (TRX_EventCmdStoreComplete & events) {
		LOG_INF("Command store complete");
		k_sem_give(&cmd_store_complete_sem);
	}

	if (TRX_EventCmdStatus & events) {
		debug_cmd_failure((TRX_Request_CommandStatus *)request);
	}

	/* Handle buffer overflow event - must be checked independently, not nested */
	if (TRX_EventStreamStoreOverflow & events) {
		LOG_ERR("RX buffer overflow - packet dropped");
		return;
	}

	if (TRX_EventFinalStreamStoreReceived & events) {
		if ((uintptr_t)NULL != pCmdStoreOrData) {
			read_packet((uint8_t *)pCmdStoreOrData);
#if HAS_LED
			gpio_pin_toggle_dt(&led);
#endif
			cntRxPackets++;
			LOG_INF("RX packet #%u (%u bytes, RSSI: %d dBm)",
				cntRxPackets, rxHeader[0], (int8_t)rxRssi[0]);
		}
	}
}

int main(void)
{
	TRX_Host_Status status;

	LOG_INF("TI CC140xP RF Packet RX Sample");

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

	memset(&rxDebug, 0, sizeof(rxDebugging_t));

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
					       store_device_config_callback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store device config: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("TRX device configured successfully");

	LOG_INF("Erasing NV...");
	status = ti_cc140xp_erase_nv(ti_cc140xp_dev, config_callback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to erase NV: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("NV erased successfully");

	/* Load the FE configuration */
	LOG_INF("Loading Wi-SUN FE configuration (%u bytes)...", FE_CONFIG_SIZE);
	status = ti_cc140xp_store_config(ti_cc140xp_dev, TRX_CONFIG_ID_FRONTEND,
					FE_CONFIG_PTR, FE_CONFIG_SIZE,
					FE_CONFIG_REFERENCE, false, config_callback,
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

	LOG_INF("Creating RX command...");
	cmdRx.slot = RX_CMD_SLOT;
	cmdRx.cmd_id = TRX_RadioCommand_Receive;
	cmdRx.enable_on_true = false;
	cmdRx.enable_on_false = false;
	cmdRx.enable_on_compare = false;
	cmdRx.trigger = Command_Trigger_Immediate;
	cmdRx.conflict_policy = TRX_ConflictPolicy_AlwaysInterrupt;
	cmdRx.allow_delay = false;
	cmdRx.params.rx.phy0.config_id = RF_CONFIG_ID;
	cmdRx.params.rx.phy0.option_mask = TRX_PHY_FEATURE_FSK_MODE_2B_WISUN;
	cmdRx.params.rx.stream_id = RX_STREAM_ID;
	cmdRx.params.rx.frequency = FREQUENCY;
	cmdRx.params.rx.modem = TRX_RadioCommand_Modem_FSK;
	cmdRx.params.rx.enable_mdr = false;
	cmdRx.params.rx.stream_early = true;
	cmdRx.params.rx.timeout = 0U;
	cmdRx.params.rx.repeat = true;
	cmdRx.params.rx.search_strategy = TRX_Rx_SearchStrategy_Sync;

	LOG_INF("Registering RX buffer...");
	status = ti_cc140xp_register_rx_stream(ti_cc140xp_dev, RX_STREAM_ID, rxBuffer,
					      sizeof(rxBuffer), receive_callback,
					      TRX_EventFinalStreamStoreReceived);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to register RX stream: %d", status);
		return -1;
	}
	LOG_INF("RX buffer registered successfully");

	LOG_INF("Storing RX command...");
	status = ti_cc140xp_store_cmds(ti_cc140xp_dev, &cmdRx, 1U, &cmdRx, receive_callback,
				      TRX_EventCmdStatus | TRX_EventCmdStoreComplete |
				      TRX_EventCmdSubmitComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store RX command: %d", status);
		return -1;
	}
	k_sem_take(&cmd_store_complete_sem, K_FOREVER);
	LOG_INF("RX command stored successfully - listening for packets on frequency 920600 Hz");

	while (1) {
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
