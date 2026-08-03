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

#include "settings/rcl_settings_wisun.h"
#include "wisun/wisun_delta_tables_JP.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/*
 * MDR Header PHY: FSK Mode 2B at channel 10 (~924.9 MHz, ChannelPlanID 22).
 * Valid channel range for MDR_HEADER_OPTION_MASK = FSK_MODE_2B_WISUN: 3 < ch < 18
 */
#define MDR_HEADER_OPTION_MASK  TRX_PHY_FEATURE_FSK_MODE_2B_WISUN
#define MDR_HEADER_MODEM        TRX_RadioCommand_Modem_FSK
#define MDR_HEADER_CHANNEL      10

/* Packet buffer layout: [hdr(4B) | payload | rssi(1B) | timestamp(4B)] */
#define RX_PAYLOAD_LENGTH       (10U)
#define RSSI_SIZE_BYTES         (1U)
#define TIMESTAMP_SIZE_BYTES    (4U)
#define RX_PACKET_LENGTH        (sizeof(TRX_PayloadHeader) + RX_PAYLOAD_LENGTH + \
				 RSSI_SIZE_BYTES + TIMESTAMP_SIZE_BYTES)

/* TRX configuration IDs and magic references */
#define RF_CONFIG_ID            (1U)
#define DELTA_TABLE_CONFIG_ID   (2U)
#define FE_CONFIG_REFERENCE     (0xFEFEFEFEU)
#define RF_CONFIG_REFERENCE     (0xF000000DU)
#define DELTA_CONFIG_REFERENCE  (0xDE1ADE1AU)

/* Stream ID and command slot */
#define MDR_PACKET_STREAM_ID    (0U)
#define CMD_RX_MDR_PACKET_SLOT  (0U)

#define RF_CONFIG_SIZE  (LRF_mainRegConfig_wisun_byteCount)
#define RF_CONFIG_PTR   ((uint8_t *)LRF_mainRegConfig_wisun)
#define FE_CONFIG_SIZE  (LRF_frontendRegConfig_wisun_byteCount)
#define FE_CONFIG_PTR   ((uint8_t *)LRF_frontendRegConfig_wisun)

/* Device tree nodes */
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

static K_SEM_DEFINE(config_sem,           0, 1);
static K_SEM_DEFINE(device_config_sem,    0, 1);
static K_SEM_DEFINE(rx_sem,               0, 1);
static K_SEM_DEFINE(cmd_store_complete_sem, 0, 1);

static TRX_Request_CommandStore cmd_mdr_rx;
static uint8_t rx_buffer[RX_PACKET_LENGTH];

static uint8_t  rx_header[sizeof(TRX_PayloadHeader)];
static uint8_t  rx_payload[RX_PAYLOAD_LENGTH];
static uint8_t  rx_rssi[RSSI_SIZE_BYTES];
static uint8_t  rx_timestamp[TIMESTAMP_SIZE_BYTES];
static uint32_t cnt_rx_packets;

/* Decoded MDR fields from the most recently received packet */
static uint8_t                mdr_byte;
static uint8_t                mdr_rate;
static uint16_t               mdr_option_mask;
static TRX_RadioCommand_Modem mdr_modem;

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
		LOG_ERR("Config error");
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

/* Signals completion (or failure) of the device configuration store request. */
static void device_config_callback(TRX_Host_Handle handle, uintptr_t pConfigData,
				   TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pConfigData);
	ARG_UNUSED(arg);

	if (TRX_EventLastStatusError & events) {
		TRX_Request_LastStatus *p = (TRX_Request_LastStatus *)request;

		if (p->status == TRX_STATUS_INVALID_PARAM)
			LOG_ERR("Device config: invalid param");
		else if (p->status == TRX_STATUS_INVALID_STATE)
			LOG_ERR("Device config: invalid state");
		k_sem_give(&device_config_sem);
		return;
	}

	if (TRX_EventDeviceConfigStoreComplete & events)
		k_sem_give(&device_config_sem);
}

/* Handles MDR RX command completion events: command store, buffer overflow, and received-packet delivery. */
static void rx_callback(TRX_Host_Handle handle, uintptr_t pCmdOrData,
			TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(arg);

	if (TRX_EventLastStatusError & events) {
		LOG_ERR("RX error");
		k_sem_give(&rx_sem);
		return;
	}

	if (TRX_EventCmdStoreComplete & events)
		k_sem_give(&cmd_store_complete_sem);

	/* Overflow arrives alongside FinalStreamStoreReceived — check first */
	if (TRX_EventStreamStoreOverflow & events) {
		LOG_ERR("RX buffer overflow - packet dropped");
		return;
	}

	if (TRX_EventFinalStreamStoreReceived & events) {
		if ((uintptr_t)NULL != pCmdOrData) {
			TRX_PayloadHeader *hdr    = (TRX_PayloadHeader *)pCmdOrData;
			uint8_t payload_len       = (uint8_t)hdr->length;
			uint8_t *start_of_payload = (uint8_t *)pCmdOrData + sizeof(TRX_PayloadHeader);
			uint8_t *end_of_payload   = start_of_payload + payload_len;
			int8_t  *rssi             = (int8_t *)end_of_payload;
			uint32_t *timestamp       = (uint32_t *)(end_of_payload + sizeof(int8_t));

			memcpy(rx_header,    hdr,              sizeof(TRX_PayloadHeader));
			memcpy(rx_payload,   start_of_payload, MIN(payload_len, RX_PAYLOAD_LENGTH));
			memcpy(rx_rssi,      rssi,             RSSI_SIZE_BYTES);
			memcpy(rx_timestamp, timestamp,        TIMESTAMP_SIZE_BYTES);

			/* Decode MDR modem type, mdrByte, and optionMask from packet header */
			uint32_t *pData = (uint32_t *)RF_CONFIG_PTR;
			TRX_WisunMdrMappingTable *map =
				(TRX_WisunMdrMappingTable *)&pData[12];

			mdr_rate = 0U;
			if (hdr->common.modulation == TRX_PayloadHeader_Modulation_FSK) {
				mdr_byte  = (uint8_t)hdr->sunfsk.newPhyId;
				mdr_modem = TRX_RadioCommand_Modem_FSK;
			} else if (hdr->common.modulation == TRX_PayloadHeader_Modulation_OFDM) {
				mdr_byte  = (uint8_t)hdr->ofdm.newPhyId;
				mdr_rate  = (uint8_t)hdr->ofdm.rate;
				mdr_modem = TRX_RadioCommand_Modem_OFDM;
				mdr_byte ^= mdr_rate;
			}

			if (mdr_byte != 0U) {
				for (uint8_t i = 0U; i < map->numEntries; i++) {
					if (mdr_byte == map->mappingTable[i].mdrByte) {
						mdr_option_mask = map->mappingTable[i].optionMask;
						break;
					}
				}
			}

#if HAS_LED
			gpio_pin_toggle_dt(&led);
#endif
			cnt_rx_packets++;
			LOG_INF("RX MDR #%u (%u bytes, RSSI: %d dBm, %s MCS%u)",
				cnt_rx_packets, payload_len, (int8_t)rx_rssi[0],
				(mdr_modem == TRX_RadioCommand_Modem_OFDM) ? "OFDM" : "FSK",
				mdr_rate);
		}
	}

	if (TRX_EventFinalCmdStatus & events) {
		TRX_Request_CommandStatus *p = (TRX_Request_CommandStatus *)request;

		switch (p->status) {
		case TRX_CommandStatus_GracefulStopTimeout:
			LOG_INF("RX: graceful stop (timeout)");
			break;
		case TRX_CommandStatus_GracefulStopApi:
			LOG_INF("RX: graceful stop (API)");
			break;
		case TRX_CommandStatus_GracefulStopScheduling:
			LOG_INF("RX: graceful stop (scheduling)");
			break;
		case TRX_CommandStatus_HardStopApi:
			LOG_WRN("RX: hard stop (API)");
			break;
		case TRX_CommandStatus_HardStopScheduling:
			LOG_WRN("RX: hard stop (scheduling)");
			break;
		default:
			break;
		}

		k_sem_give(&rx_sem);
	}
}

int main(void)
{
	TRX_Host_Status status;
	int ret;

	LOG_INF("TI CC140xP Wi-SUN MDR RX Sample");

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
#else
	ARG_UNUSED(ret);
#endif

	TRX_Host_Params params = {
		.bitRate   = 12000000U,
		.generalCb = general_callback,
		.arg       = (uintptr_t)NULL
	};

	ret = ti_cc140xp_host_open(ti_cc140xp_dev, &params);
	if (ret != 0) {
		LOG_ERR("Failed to open TRX Host driver");
		return -1;
	}
	LOG_INF("TRX Host opened");

	TRX_DeviceConfigData device_config = {
		.dioConfig = {
			.dio0 = DIO0_UNCHANGED,
			.dio2 = DIO2_HIGH_PA,
			.dio3 = DIO3_LOW_PA_AND_LNA,
			.dio4 = DIO4_UNCHANGED,
			.dio5 = DIO5_UNCHANGED,
			/* DIO6_SFD would enable the sync-frame-detected signal on DIO6
			 * of CC1407P (useful as a scope trigger). Not wired in this overlay.
			 */
			.dio6 = DIO6_UNCHANGED,
		},
		.clockConfig = TRX_CLOCK_CONFIG_WISUN,
		.powerMode   = {
			.dwellTimeUs = 0U,
			.powerPolicy = TRX_PowerPolicy_StandbyDisallow,
		},
		.rfMode = TRX_RF_MODE_WISUN,
	};

	status = ti_cc140xp_store_device_config(ti_cc140xp_dev, device_config,
					       device_config_callback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store device config");
		return -1;
	}
	k_sem_take(&device_config_sem, K_FOREVER);

	status = ti_cc140xp_erase_nv(ti_cc140xp_dev, config_callback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to erase NV");
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("NV erased");

	/* Store and persist FE (PA table) config */
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

	/* Store and persist PHY (Wi-SUN multi-PHY) config */
	status = ti_cc140xp_store_config(ti_cc140xp_dev, RF_CONFIG_ID,
					RF_CONFIG_PTR, RF_CONFIG_SIZE,
					RF_CONFIG_REFERENCE, true, config_callback,
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

	/* Locate MDR mapping table at word offset 12 of LRF_mainRegConfig_wisun */
	uint32_t *pData = (uint32_t *)RF_CONFIG_PTR;
	TRX_WisunMdrMappingTable *map = (TRX_WisunMdrMappingTable *)&pData[12];

	uint8_t mdr_header_idx = 0U;

	for (; mdr_header_idx < map->numEntries; mdr_header_idx++) {
		if (map->mappingTable[mdr_header_idx].modem == MDR_HEADER_MODEM &&
		    map->mappingTable[mdr_header_idx].optionMask == MDR_HEADER_OPTION_MASK) {
			break;
		}
	}

	int16_t header_channel_plan_id = get_wisun_channelPlanId_from_channelPlanIdIdx_JP(
		map->mappingTable[mdr_header_idx].channelPlanIdIdx);
	int32_t header_rx_freq = get_wisun_frequency_from_channel_JP(
		header_channel_plan_id, MDR_HEADER_CHANNEL);

	LOG_INF("Header: ChannelPlanID=%d ch=%d freq=%d kHz",
		header_channel_plan_id, MDR_HEADER_CHANNEL, header_rx_freq);

	/* Store delta table for the header channel plan (no persist needed for RX) */
	const uint32_t *delta_config = get_wisun_delta_table_JP(header_channel_plan_id);
	uint16_t delta_len = (uint16_t)(((delta_config[0] & 0x0FFFU) * sizeof(uint32_t)) +
				       sizeof(uint32_t));

	status = ti_cc140xp_store_config(ti_cc140xp_dev, DELTA_TABLE_CONFIG_ID,
					(uint8_t *)delta_config, delta_len,
					DELTA_CONFIG_REFERENCE, true, config_callback,
					TRX_EventConfigStoreComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store delta table");
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("Delta table stored");

	/* Base index links the header channel to its delta table entry */
	int16_t base_idx = get_wisun_delta_table_baseindex_JP(header_channel_plan_id,
							      MDR_HEADER_CHANNEL);

	/* Build the RX command */
	memset(&cmd_mdr_rx, 0, sizeof(cmd_mdr_rx));
	cmd_mdr_rx.slot              = CMD_RX_MDR_PACKET_SLOT;
	cmd_mdr_rx.cmd_id            = TRX_RadioCommand_Receive;
	cmd_mdr_rx.enable_on_true    = false;
	cmd_mdr_rx.enable_on_false   = false;
	cmd_mdr_rx.enable_on_compare = false;
	cmd_mdr_rx.trigger           = Command_Trigger_Immediate;
	cmd_mdr_rx.conflict_policy   = TRX_ConflictPolicy_AlwaysInterrupt;
	cmd_mdr_rx.allow_delay       = false;
	/* phy0: FSK header PHY */
	cmd_mdr_rx.params.rx.phy0.config_id   = RF_CONFIG_ID;
	cmd_mdr_rx.params.rx.phy0.option_mask = MDR_HEADER_OPTION_MASK;
	/* phy1: delta table config + base index for OFDM packet retuning */
	cmd_mdr_rx.params.rx.phy1.config_id   = DELTA_TABLE_CONFIG_ID;
	cmd_mdr_rx.params.rx.phy1.option_mask = (uint16_t)base_idx;
	cmd_mdr_rx.params.rx.phy2.config_id   = 0U;
	cmd_mdr_rx.params.rx.phy2.option_mask = 0U;
	cmd_mdr_rx.params.rx.stream_id        = MDR_PACKET_STREAM_ID;
	cmd_mdr_rx.params.rx.frequency        = header_rx_freq;
	cmd_mdr_rx.params.rx.modem            = MDR_HEADER_MODEM;
	cmd_mdr_rx.params.rx.enable_mdr       = true;
	cmd_mdr_rx.params.rx.stream_early     = true;
	cmd_mdr_rx.params.rx.timeout          = 0U;
	cmd_mdr_rx.params.rx.repeat           = true;
	cmd_mdr_rx.params.rx.search_strategy  = TRX_Rx_SearchStrategy_Sync;

	/* Register the RX stream buffer */
	status = ti_cc140xp_register_rx_stream(ti_cc140xp_dev, MDR_PACKET_STREAM_ID,
					      rx_buffer, sizeof(rx_buffer),
					      rx_callback,
					      TRX_EventFinalStreamStoreReceived |
					      TRX_EventStreamStoreOverflow |
					      TRX_EventLastStatusError);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to register RX stream: %d", status);
		return -1;
	}

	/* Store command without auto-submit (NULL 4th arg); submit manually in loop */
	status = ti_cc140xp_store_cmds(ti_cc140xp_dev, &cmd_mdr_rx, 1U, NULL,
				      rx_callback,
				      TRX_EventCmdStatus      |
				      TRX_EventFinalCmdStatus  |
				      TRX_EventCmdStoreComplete |
				      TRX_EventCmdSubmitComplete |
				      TRX_EventLastStatusError);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store RX command");
		return -1;
	}
	k_sem_take(&cmd_store_complete_sem, K_FOREVER);
	LOG_INF("Listening for Wi-SUN MDR packets on ch %d (%d kHz)...",
		MDR_HEADER_CHANNEL, header_rx_freq);

	/* Submit → wait for command to end → repeat (handles error recovery) */
	while (1) {
		status = ti_cc140xp_submit_cmd(ti_cc140xp_dev, CMD_RX_MDR_PACKET_SLOT);
		if (status != TRX_Host_Success) {
			k_busy_wait(1000U);
			continue;
		}
		k_sem_take(&rx_sem, K_FOREVER);
	}

	return 0;
}
