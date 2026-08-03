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

LOG_MODULE_REGISTER(genfsk_rx, LOG_LEVEL_INF);

/* Packet format: [hdr(1B)][payload][rssi(1B)][timestamp(4B)] */
#define RSSI_SIZE_BYTES         (1U)
#define TIMESTAMP_SIZE_BYTES    (4U)
#define RX_PAYLOAD_LENGTH       (100U)
#define RX_PACKET_HEADER_LENGTH (1U)
#define RX_PACKET_LENGTH        (RX_PACKET_HEADER_LENGTH + RX_PAYLOAD_LENGTH + \
				 RSSI_SIZE_BYTES + TIMESTAMP_SIZE_BYTES)

#define RX_CMD_SLOT         (0U)
#define RX_STREAM_ID        (0U)
#define RF_CONFIG_ID        (1U)
#define FE_CONFIG_REFERENCE (0xFEFEFEFEU)
#define RF_CONFIG_REFERENCE (0xF000000DU)
#define FREQUENCY           (868000U)

/* GenFSK 50 kbps PHY settings */
#include "settings/rcl_settings_2gfsk50kbps.h"
#define RF_CONFIG_SIZE (LRF_mainRegConfig_2gfsk50kbps_byteCount)
#define RF_CONFIG_PTR  ((uint8_t *)LRF_mainRegConfig_2gfsk50kbps)
#define FE_CONFIG_SIZE (LRF_frontendRegConfig_2gfsk50kbps_byteCount)
#define FE_CONFIG_PTR  ((uint8_t *)LRF_frontendRegConfig_2gfsk50kbps)

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

/*
 * Three semaphores:
 *   configurationSem        -> config_sem
 *   setRfModeCompleteSem    -> rfmode_complete_sem
 *   cmdStoreCompleteSem     -> cmd_store_complete_sem
 */
static K_SEM_DEFINE(config_sem, 0, 1);
static K_SEM_DEFINE(rfmode_complete_sem, 0, 1);
static K_SEM_DEFINE(cmd_store_complete_sem, 0, 1);

static TRX_Request_CommandStore cmd_rx;
static uint8_t rx_buffer[RX_PACKET_LENGTH];
static uint32_t cnt_rx_packets;

/* Parsed fields from the most recently received packet */
static uint8_t rx_header[RX_PACKET_HEADER_LENGTH];
static uint8_t rx_payload[RX_PAYLOAD_LENGTH];
static uint8_t rx_rssi[RSSI_SIZE_BYTES];
static uint8_t rx_timestamp[TIMESTAMP_SIZE_BYTES];

/* Per-status counters for unexpected RX command stops */
typedef struct {
	bool crc_not_ok;
	bool graceful_stop_timeout;
	bool graceful_stop_api;
	bool graceful_stop_scheduling;
	bool hard_stop_api;
	bool hard_stop_scheduling;
	bool rx_err;
	bool rx_buffer_corruption;
	bool rx_fifo_err;
	bool error;
} rx_debug_t;

static rx_debug_t rx_debug;

static void read_packet(const uint8_t *data)
{
	const uint8_t *header = data;

	memcpy(rx_header, header, RX_PACKET_HEADER_LENGTH);
	const uint8_t *payload  = header + RX_PACKET_HEADER_LENGTH;
	const uint8_t *end      = payload + rx_header[0];
	const int8_t  *rssi     = (const int8_t *)end;
	const uint32_t *ts      = (const uint32_t *)(end + sizeof(int8_t));

	memcpy(rx_payload,   payload, rx_header[0]);
	memcpy(rx_rssi,      rssi,    RSSI_SIZE_BYTES);
	memcpy(rx_timestamp, ts,      TIMESTAMP_SIZE_BYTES);
}

static void debug_cmd_failure(const TRX_Request_CommandStatus *p)
{
	const TRX_Request_CommandStatus_Params_Rx *rx_params =
		(const TRX_Request_CommandStatus_Params_Rx *)&p->params;

	if (rx_params->num_crc_ok == 0U) {
		rx_debug.crc_not_ok = true;
	}

	switch (p->status) {
	case TRX_CommandStatus_GracefulStopTimeout:
		rx_debug.graceful_stop_timeout = true;
		break;
	case TRX_CommandStatus_GracefulStopApi:
		rx_debug.graceful_stop_api = true;
		break;
	case TRX_CommandStatus_GracefulStopScheduling:
		rx_debug.graceful_stop_scheduling = true;
		break;
	case TRX_CommandStatus_HardStopApi:
		rx_debug.hard_stop_api = true;
		break;
	case TRX_CommandStatus_HardStopScheduling:
		rx_debug.hard_stop_scheduling = true;
		break;
	case TRX_CommandStatus_RxErr:
		rx_debug.rx_err = true;
		break;
	case TRX_CommandStatus_Error_RxBufferCorruption:
		rx_debug.rx_buffer_corruption = true;
		break;
	case TRX_CommandStatus_Error_RxFifo:
		rx_debug.rx_fifo_err = true;
		break;
	default:
		rx_debug.error = true;
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

/* Handles RX command completion events: command store, buffer overflow, and received-packet delivery. */
static void receive_callback(TRX_Host_Handle handle, uintptr_t pCmdStoreOrData,
			     TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(arg);

	if (TRX_EventLastStatusError & events) {
		TRX_Request_LastStatus *pLastStatus = (TRX_Request_LastStatus *)request;

		if (pLastStatus->status == TRX_STATUS_INVALID_PARAM)
			LOG_ERR("RX command error: invalid param");
		else if (pLastStatus->status == TRX_STATUS_INVALID_STATE)
			LOG_ERR("RX command error: invalid state");
		k_sem_give(&cmd_store_complete_sem);
		return;
	}

	if (TRX_EventCmdStoreComplete & events)
		k_sem_give(&cmd_store_complete_sem);

	if (TRX_EventCmdStatus & events) {
		TRX_Request_CommandStatus *p = (TRX_Request_CommandStatus *)request;

		if (p->status != TRX_CommandStatus_Finished) {
			LOG_ERR("RX command ended unexpectedly: status %d", p->status);
			debug_cmd_failure(p);
		}
	}

	/* Overflow must be checked before FinalStreamStoreReceived */
	if (TRX_EventStreamStoreOverflow & events) {
		LOG_ERR("RX buffer overflow - packet dropped");
		return;
	}

	if (TRX_EventFinalStreamStoreReceived & events) {
		if ((uintptr_t)NULL != pCmdStoreOrData) {
			read_packet((const uint8_t *)pCmdStoreOrData);
#if HAS_LED
			gpio_pin_toggle_dt(&led);
#endif
			cnt_rx_packets++;
			LOG_INF("RX packet #%u (%u bytes, RSSI: %d dBm)",
				cnt_rx_packets, rx_header[0], (int8_t)rx_rssi[0]);
		}
	}
}

int main(void)
{
	TRX_Host_Status status;
	int ret;

	LOG_INF("TI CC140xP RF Packet RX GenFSK Sample");

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

	memset(&rx_debug, 0, sizeof(rx_debug));

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

	LOG_INF("Erasing NV...");
	status = ti_cc140xp_erase_nv(ti_cc140xp_dev, config_callback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to erase NV: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("NV erased successfully");

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

	LOG_INF("Creating RX command...");
	cmd_rx.slot = RX_CMD_SLOT;
	cmd_rx.cmd_id = TRX_RadioCommand_Receive;
	cmd_rx.enable_on_true = false;
	cmd_rx.enable_on_false = false;
	cmd_rx.enable_on_compare = false;
	cmd_rx.trigger = Command_Trigger_Immediate;
	cmd_rx.conflict_policy = TRX_ConflictPolicy_AlwaysInterrupt;
	cmd_rx.allow_delay = false;
	cmd_rx.params.rx.phy0.config_id = RF_CONFIG_ID;
	cmd_rx.params.rx.phy0.option_mask = TRX_PHY_FEATURE_WHITENING_DISABLED_2GFSK50KBPS;
	cmd_rx.params.rx.stream_id = RX_STREAM_ID;
	cmd_rx.params.rx.frequency = FREQUENCY;
	cmd_rx.params.rx.modem = TRX_RadioCommand_Modem_FSK;
	cmd_rx.params.rx.enable_mdr = false;
	cmd_rx.params.rx.stream_early = true;
	cmd_rx.params.rx.timeout = 0U;
	cmd_rx.params.rx.repeat = true;
	cmd_rx.params.rx.search_strategy = TRX_Rx_SearchStrategy_Sync;

	LOG_INF("Registering RX buffer...");
	status = ti_cc140xp_register_rx_stream(ti_cc140xp_dev, RX_STREAM_ID,
					      rx_buffer, sizeof(rx_buffer),
					      receive_callback,
					      TRX_EventFinalStreamStoreReceived);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to register RX stream: %d", status);
		return -1;
	}
	LOG_INF("RX buffer registered successfully");

	/* Store and auto-submit: 4th arg = &cmd_rx triggers immediate submit after store */
	LOG_INF("Storing and submitting RX command...");
	status = ti_cc140xp_store_cmds(ti_cc140xp_dev, &cmd_rx, 1U, &cmd_rx,
				      receive_callback,
				      TRX_EventCmdStatus | TRX_EventCmdStoreComplete |
				      TRX_EventCmdSubmitComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store RX command: %d", status);
		return -1;
	}
	k_sem_take(&cmd_store_complete_sem, K_FOREVER);
	LOG_INF("Listening on %u kHz", FREQUENCY);

	/* RX repeats automatically — sleep forever */
	while (1) {
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
