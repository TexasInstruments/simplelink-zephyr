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
 * Valid channel range for FSK_MODE_2B_WISUN: 3 < ch < 18
 */
#define MDR_HEADER_OPTION_MASK  TRX_PHY_FEATURE_FSK_MODE_2B_WISUN
#define MDR_HEADER_MODEM        TRX_RadioCommand_Modem_FSK
#define MDR_HEADER_CHANNEL      10

/*
 * MDR Packet PHY: OFDM Option 2 MCS5 (16-QAM, 1/2 FEC) at the nearest OFDM
 * channel to the header frequency.
 */
#define MDR_PACKET_OPTION_MASK  TRX_PHY_FEATURE_OFDM_OPTION_2_WISUN
#define MDR_PACKET_MODEM        TRX_RadioCommand_Modem_OFDM
#define MDR_PACKET_RATE         TRX_PayloadHeader_SunOFDM_Rate_MCS5

/* RSSI threshold for CCA — same for both header (FSK) and packet (OFDM) PHYs */
#define RSSI_THRESHOLD          (-80)

/* Application parameters */
#define TX_PAYLOAD_LENGTH       (10U)
#define NUMBER_OF_PACKETS       (100U)
#define PACKET_INTERVAL_US      (100000U)   /* 100 ms between MDR CCA pairs */

#define TX_PACKET_LENGTH        (sizeof(TRX_PayloadHeader) + TX_PAYLOAD_LENGTH)

/* TRX configuration IDs and magic references */
#define RF_CONFIG_ID            (1U)
#define DELTA_TABLE_CONFIG_ID   (2U)
#define FE_CONFIG_REFERENCE     (0xFEFEFEFEU)
#define RF_CONFIG_REFERENCE     (0xF000000DU)
#define DELTA_CONFIG_REFERENCE  (0xDE1ADE1AU)

/* Stream IDs */
#define MDR_HEADER_STREAM_ID    (0U)
#define MDR_PACKET_STREAM_ID    (1U)

/*
 * Command slots.  Chain: CS_HDR(0) → TX_HDR(1) → CS_PKT(2) → TX_PKT(3)
 * CS fires first; on channel idle it enables the matching TX slot.
 */
#define CMD_CS_MDR_HEADER_SLOT  (0U)
#define CMD_TX_MDR_HEADER_SLOT  (1U)
#define CMD_CS_MDR_PACKET_SLOT  (2U)
#define CMD_TX_MDR_PACKET_SLOT  (3U)

/* PHY register config accessors */
#define RF_CONFIG_SIZE  (LRF_mainRegConfig_wisun_byteCount)
#define RF_CONFIG_PTR   ((uint8_t *)LRF_mainRegConfig_wisun)
#define FE_CONFIG_SIZE  (LRF_frontendRegConfig_wisun_byteCount)
#define FE_CONFIG_PTR   ((uint8_t *)LRF_frontendRegConfig_wisun)

/* --- Device tree nodes --- */

#define TI_CC140XP_NODE DT_NODELABEL(ti_cc140xp)

#if DT_NODE_EXISTS(TI_CC140XP_NODE)
#define TI_CC140XP_ENABLED 1
static const struct device *const ti_cc140xp_dev = DEVICE_DT_GET(TI_CC140XP_NODE);
#else
#define TI_CC140XP_ENABLED 0
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#define HAS_LED 1
#else
#define HAS_LED 0
#warning "LED not found in device tree, LED toggling will be disabled"
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(sw0), okay)
static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static struct gpio_callback btn_cb_data;
#define HAS_BTN 1
#else
#define HAS_BTN 0
#warning "Button sw0 not found in device tree, auto-transmitting every 5 seconds"
#endif

/* --- Types --- */

typedef struct {
	TRX_Request_CommandStore cmdCsMdrHeader;
	TRX_Request_CommandStore cmdTxMdrHeader;
	TRX_Request_CommandStore cmdCsMdrPacket;
	TRX_Request_CommandStore cmdTxMdrPacket;
} MDR_CCA_cmds;

/* --- Globals --- */

static uint8_t txMdrHeader[sizeof(TRX_PayloadHeader)] = {0, 0, 0, 0};
static uint8_t txMdrPacket[TX_PACKET_LENGTH] = {0, 0, 0, 0};

static K_SEM_DEFINE(config_sem,           0, 1);
static K_SEM_DEFINE(cs_tx_sem,            0, 1);
static K_SEM_DEFINE(cmd_store_complete_sem, 0, 1);
static K_SEM_DEFINE(button_sem,           0, 1);

static MDR_CCA_cmds mdr_cca_cmds;

/* --- Callbacks --- */

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

/* Handles the TRX firmware version query response. */
static void version_callback(TRX_Host_Handle handle, uintptr_t pConfigData,
			      TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pConfigData);
	ARG_UNUSED(arg);

	if (TRX_EventLastStatusError & events) {
		LOG_ERR("Version request error");
		k_sem_give(&config_sem);
		return;
	}
	if (TRX_EventGetVersionComplete & events) {
		TRX_Request_LastStatus *pLastStatus = (TRX_Request_LastStatus *)request;
		TRX_Request_LastStatus_Params *pParams =
			(TRX_Request_LastStatus_Params *)pLastStatus->params;

		LOG_INF("TRX firmware v%d.%d",
			pParams->getVersionRsp.major, pParams->getVersionRsp.minor);
		k_sem_give(&config_sem);
	}
}

/* Handles the CS->TX command chain: command store and per-slot status, including channel-busy skips. */
static void cs_tx_callback(TRX_Host_Handle handle, uintptr_t pCmdOrData,
			    TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pCmdOrData);
	ARG_UNUSED(arg);

	if (TRX_EventLastStatusError & events) {
		LOG_ERR("CS/TX error");
		k_sem_give(&cs_tx_sem);
		return;
	}
	if (TRX_EventCmdStoreComplete & events) {
		k_sem_give(&cmd_store_complete_sem);
	}
	if (TRX_EventCmdStatus & events) {
		TRX_Request_CommandStatus *pStatus = (TRX_Request_CommandStatus *)request;

		if (pStatus->slot == CMD_CS_MDR_HEADER_SLOT ||
		    pStatus->slot == CMD_CS_MDR_PACKET_SLOT) {
			if (pStatus->status == TRX_CommandStatus_ChannelBusy) {
				LOG_WRN("CS slot %u: channel busy, skipping TX", pStatus->slot);
			}
			if (TRX_EventFinalCmdStatus & events) {
				/* Channel busy — chain terminated at CS */
				k_sem_give(&cs_tx_sem);
			}
		} else if (pStatus->slot == CMD_TX_MDR_HEADER_SLOT ||
			   pStatus->slot == CMD_TX_MDR_PACKET_SLOT) {
			if (TRX_EventFinalCmdStatus & events) {
				k_sem_give(&cs_tx_sem);
			}
		}
	}
}

#if HAS_BTN
/* GPIO ISR for BTN1 — signals main() to start a burst of MDR CCA TX pairs. */
static void button_callback(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	k_sem_give(&button_sem);
}
#endif

/* --- MDR frequency/index helpers --- */

static uint8_t getMdrHeaderIdx(TRX_WisunMdrMappingTable *map)
{
	uint8_t idx;

	for (idx = 0; idx < map->numEntries; idx++) {
		if (map->mappingTable[idx].modem == MDR_HEADER_MODEM &&
		    map->mappingTable[idx].optionMask == MDR_HEADER_OPTION_MASK) {
			break;
		}
	}
	return idx;
}

static uint8_t getMdrPacketIdx(TRX_WisunMdrMappingTable *map)
{
	uint8_t idx;

	for (idx = 0; idx < map->numEntries; idx++) {
		if (map->mappingTable[idx].modem == MDR_PACKET_MODEM &&
		    map->mappingTable[idx].optionMask == MDR_PACKET_OPTION_MASK) {
			break;
		}
	}
	return idx;
}

static int16_t getMdrHeaderChannelPlanId(TRX_WisunMdrMappingTable *map, uint8_t hdrIdx)
{
	uint8_t cpIdIdx = map->mappingTable[hdrIdx].channelPlanIdIdx;

	return get_wisun_channelPlanId_from_channelPlanIdIdx_JP(cpIdIdx);
}

static int16_t getMdrPacketChannelPlanId(TRX_WisunMdrMappingTable *map, uint8_t pktIdx)
{
	uint8_t cpIdIdx = map->mappingTable[pktIdx].channelPlanIdIdx;

	return get_wisun_channelPlanId_from_channelPlanIdIdx_JP(cpIdIdx);
}

static uint32_t getMdrPacketTxFrequency(int16_t pktChannelPlanId, int32_t hdrFrequency)
{
	uint16_t minChan = 0U;
	uint16_t maxChan = 0U;

	if (pktChannelPlanId == 21) {
		minChan = 9U;
		maxChan = 37U;
	} else if (pktChannelPlanId == 22) {
		minChan = 4U;
		maxChan = 17U;
	} else if (pktChannelPlanId == 23) {
		minChan = 3U;
		maxChan = 11U;
	} else if (pktChannelPlanId == 24) {
		minChan = 2U;
		maxChan = 8U;
	}

	uint32_t newFreq = WISUN_FREQ_INVALID;

	for (int16_t i = (int16_t)minChan; i <= (int16_t)maxChan; i++) {
		newFreq = get_wisun_frequency_from_channel_JP(pktChannelPlanId, i);
		if ((hdrFrequency < (int32_t)newFreq) && (WISUN_FREQ_INVALID != newFreq)) {
			uint32_t prevFreq =
				get_wisun_frequency_from_channel_JP(pktChannelPlanId, i - 1);

			if ((newFreq - (uint32_t)hdrFrequency) >
			    ((uint32_t)hdrFrequency - prevFreq)) {
				newFreq = prevFreq;
			}
			break;
		}
	}
	return newFreq;
}

/*
 * CS command for the MDR header (FSK Mode 2B).
 * On channel idle  → enables TX_HDR slot.
 * On channel busy  → chain terminates; TRX_EventFinalCmdStatus fires.
 */
static void setupCmdMdrCsHdr(TRX_Request_CommandStore *pCmd, int32_t hdrFrequency)
{
	memset(pCmd, 0, sizeof(TRX_Request_CommandStore));

	pCmd->slot                       = CMD_CS_MDR_HEADER_SLOT;
	pCmd->cmd_id                     = TRX_RadioCommand_CarrierSense;
	pCmd->slot_on_false              = CMD_TX_MDR_HEADER_SLOT;
	pCmd->enable_on_false            = true;
	pCmd->enable_on_true             = false;
	pCmd->enable_on_compare          = false;
	pCmd->trigger                    = Command_Trigger_Immediate;
	pCmd->conflict_policy            = TRX_ConflictPolicy_AlwaysInterrupt;
	pCmd->allow_delay                = false;
	pCmd->params.cs.modem            = MDR_HEADER_MODEM;
	pCmd->params.cs.frequency        = hdrFrequency;
	pCmd->params.cs.phy0.config_id   = RF_CONFIG_ID;
	pCmd->params.cs.phy0.option_mask = MDR_HEADER_OPTION_MASK;
	pCmd->params.cs.phy1.config_id   = 0U;
	pCmd->params.cs.phy1.option_mask = 0U;
	pCmd->params.cs.phy2.config_id   = 0U;
	pCmd->params.cs.phy2.option_mask = 0U;
	pCmd->params.cs.mode             = TRX_CarrierSense_Mode_Energy;
	pCmd->params.cs.rssi_override    = RSSI_THRESHOLD;
	pCmd->params.cs.rssi_window      = 5U;
	pCmd->params.cs.rssi_count       = 5U;
	pCmd->params.cs.exit_condition   = TRX_CarrierSense_ExitCondition_WaitForBusy;
	/* FSK Mode 2B timeout: 51.3 + 96 + 128 = 275.3 µs (same as rfDiagnostics) */
	pCmd->params.cs.timeout          = 276U;
}

/*
 * TX command for the MDR header (FSK, zero-length, mode_switch=1).
 * frequency=0 reuses the CS calibration — no recalibration needed.
 * On TX done → enables CS_PKT slot.
 */
static void setupCmdMdrTxHdr(TRX_Request_CommandStore *pCmd, uint16_t newPhyId)
{
	memset(pCmd, 0, sizeof(TRX_Request_CommandStore));

	pCmd->slot                       = CMD_TX_MDR_HEADER_SLOT;
	pCmd->cmd_id                     = TRX_RadioCommand_Transmit;
	pCmd->enable_on_true             = true;
	pCmd->slot_on_true               = CMD_CS_MDR_PACKET_SLOT;
	pCmd->enable_on_false            = false;
	pCmd->enable_on_compare          = false;
	pCmd->chain_trigger              = Command_Trigger_Immediate;
	pCmd->chain_conflict_policy      = TRX_ConflictPolicy_AlwaysInterrupt;
	pCmd->allow_delay                = false;
	pCmd->params.tx.stream_id        = MDR_HEADER_STREAM_ID;
	pCmd->params.tx.phy0.config_id   = RF_CONFIG_ID;
	pCmd->params.tx.phy0.option_mask = MDR_HEADER_OPTION_MASK;
	pCmd->params.tx.phy1.config_id   = 0U;
	pCmd->params.tx.phy1.option_mask = 0U;
	pCmd->params.tx.phy2.config_id   = 0U;
	pCmd->params.tx.phy2.option_mask = 0U;
	pCmd->params.tx.pa               = TRX_TxPa_High;
	pCmd->params.tx.frequency        = 0U;
	pCmd->params.tx.modem            = MDR_HEADER_MODEM;
	pCmd->params.tx.power.dBm        = 10;
	pCmd->params.tx.power.fraction   = 0;

	((TRX_PayloadHeader_SunFSK *)txMdrHeader)->length      = 0U;
	((TRX_PayloadHeader_SunFSK *)txMdrHeader)->modulation  = TRX_PayloadHeader_Modulation_FSK;
	((TRX_PayloadHeader_SunFSK *)txMdrHeader)->mode_switch = 1U;
	((TRX_PayloadHeader_SunFSK *)txMdrHeader)->fcs_mode    = 0U;
	((TRX_PayloadHeader_SunFSK *)txMdrHeader)->whitening   = 1U;
	((TRX_PayloadHeader_SunFSK *)txMdrHeader)->newPhyId    = (newPhyId | MDR_PACKET_RATE);
}

/*
 * CS command for the MDR packet (OFDM Option 2).
 * frequency = delta from header (avoids OFDM recalibration).
 * On channel idle → enables TX_PKT slot.
 */
static void setupCmdMdrCsPacket(TRX_Request_CommandStore *pCmd, int32_t pktDeltaFrequency)
{
	memset(pCmd, 0, sizeof(TRX_Request_CommandStore));

	pCmd->slot                       = CMD_CS_MDR_PACKET_SLOT;
	pCmd->cmd_id                     = TRX_RadioCommand_CarrierSense;
	pCmd->slot_on_false              = CMD_TX_MDR_PACKET_SLOT;
	pCmd->enable_on_false            = true;
	pCmd->enable_on_true             = false;
	pCmd->enable_on_compare          = false;
	pCmd->chain_trigger              = Command_Trigger_Immediate;
	pCmd->chain_conflict_policy      = TRX_ConflictPolicy_AlwaysInterrupt;
	pCmd->allow_delay                = false;
	pCmd->params.cs.modem            = MDR_PACKET_MODEM;
	pCmd->params.cs.frequency        = pktDeltaFrequency;
	pCmd->params.cs.phy0.config_id   = RF_CONFIG_ID;
	pCmd->params.cs.phy0.option_mask = MDR_PACKET_OPTION_MASK;
	pCmd->params.cs.phy1.config_id   = 0U;
	pCmd->params.cs.phy1.option_mask = 0U;
	pCmd->params.cs.phy2.config_id   = 0U;
	pCmd->params.cs.phy2.option_mask = 0U;
	pCmd->params.cs.mode             = TRX_CarrierSense_Mode_Energy;
	pCmd->params.cs.rssi_override    = RSSI_THRESHOLD;
	pCmd->params.cs.corr_window      = 0U;
	pCmd->params.cs.corr_count       = 0U;
	pCmd->params.cs.exit_condition   = TRX_CarrierSense_ExitCondition_WaitForBusy;
	/* OFDM timeout: 193 µs (same as rfDiagnostics) */
	pCmd->params.cs.rssi_window      = 3U;
	pCmd->params.cs.rssi_count       = 3U;
	pCmd->params.cs.timeout          = 193U;
}

/* TX command for the MDR packet (OFDM Option 2 MCS5, delta frequency). */
static void setupCmdMdrTxPacket(TRX_Request_CommandStore *pCmd, int32_t pktDeltaFrequency)
{
	memset(pCmd, 0, sizeof(TRX_Request_CommandStore));

	pCmd->slot                       = CMD_TX_MDR_PACKET_SLOT;
	pCmd->cmd_id                     = TRX_RadioCommand_Transmit;
	pCmd->enable_on_true             = false;
	pCmd->enable_on_false            = false;
	pCmd->enable_on_compare          = false;
	pCmd->chain_trigger              = Command_Trigger_Immediate;
	pCmd->chain_conflict_policy      = TRX_ConflictPolicy_AlwaysInterrupt;
	pCmd->allow_delay                = false;
	pCmd->params.tx.stream_id        = MDR_PACKET_STREAM_ID;
	pCmd->params.tx.phy0.config_id   = RF_CONFIG_ID;
	pCmd->params.tx.phy0.option_mask = MDR_PACKET_OPTION_MASK;
	pCmd->params.tx.phy1.config_id   = 0U;
	pCmd->params.tx.phy1.option_mask = 0U;
	pCmd->params.tx.phy2.config_id   = 0U;
	pCmd->params.tx.phy2.option_mask = 0U;
	pCmd->params.tx.pa               = TRX_TxPa_High;
	pCmd->params.tx.frequency        = pktDeltaFrequency;
	pCmd->params.tx.modem            = MDR_PACKET_MODEM;
	pCmd->params.tx.power.dBm        = 10;
	pCmd->params.tx.power.fraction   = 0;

	if (pCmd->params.tx.modem == TRX_RadioCommand_Modem_FSK) {
		((TRX_PayloadHeader_SunFSK *)txMdrPacket)->length      = TX_PAYLOAD_LENGTH;
		((TRX_PayloadHeader_SunFSK *)txMdrPacket)->modulation  =
			TRX_PayloadHeader_Modulation_FSK;
		((TRX_PayloadHeader_SunFSK *)txMdrPacket)->mode_switch = 0U;
		((TRX_PayloadHeader_SunFSK *)txMdrPacket)->fcs_mode    = 0U;
		((TRX_PayloadHeader_SunFSK *)txMdrPacket)->whitening   = 1U;
		((TRX_PayloadHeader_SunFSK *)txMdrPacket)->newPhyId    = 0U;
	} else {
		((TRX_PayloadHeader_SunOFDM *)txMdrPacket)->length     = TX_PAYLOAD_LENGTH;
		((TRX_PayloadHeader_SunOFDM *)txMdrPacket)->modulation =
			TRX_PayloadHeader_Modulation_OFDM;
		((TRX_PayloadHeader_SunOFDM *)txMdrPacket)->rate       = MDR_PACKET_RATE;
		((TRX_PayloadHeader_SunOFDM *)txMdrPacket)->scrambler  = 0U;
		((TRX_PayloadHeader_SunOFDM *)txMdrPacket)->newPhyId   = 0U;
	}
}

/* --- main --- */

int main(void)
{
	TRX_Host_Status status;
	int ret;
	uint16_t sequence_number;
	uint8_t i;

	LOG_INF("TI CC140xP Wi-SUN MDR TX with CCA Sample");

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

#if HAS_BTN
	if (!gpio_is_ready_dt(&btn)) {
		LOG_ERR("Button GPIO not ready");
		return -1;
	}
	ret = gpio_pin_configure_dt(&btn, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("Failed to configure button: %d", ret);
		return -1;
	}
	ret = gpio_pin_interrupt_configure_dt(&btn, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure button interrupt: %d", ret);
		return -1;
	}
	gpio_init_callback(&btn_cb_data, button_callback, BIT(btn.pin));
	gpio_add_callback(btn.port, &btn_cb_data);
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
		.powerMode = {
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

	status = ti_cc140xp_util_get_version(ti_cc140xp_dev, version_callback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to get TRX version");
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);

	/*
	 * MDR mapping table is embedded at word offset 12 of LRF_mainRegConfig_wisun.
	 */
	uint32_t *pData = (uint32_t *)RF_CONFIG_PTR;
	TRX_WisunMdrMappingTable *map = (TRX_WisunMdrMappingTable *)&pData[12];

	uint8_t hdrIdx          = getMdrHeaderIdx(map);
	int16_t hdrChannelPlanId = getMdrHeaderChannelPlanId(map, hdrIdx);
	int32_t hdrFrequency     = (int32_t)get_wisun_frequency_from_channel_JP(hdrChannelPlanId,
									     MDR_HEADER_CHANNEL);

	uint8_t pktIdx          = getMdrPacketIdx(map);
	int16_t pktChannelPlanId = getMdrPacketChannelPlanId(map, pktIdx);
	uint32_t pktFrequency   = getMdrPacketTxFrequency(pktChannelPlanId, hdrFrequency);
	int32_t pktDeltaFrequency = (int32_t)(pktFrequency - (uint32_t)hdrFrequency);

	LOG_INF("Header: ChannelPlanID=%d ch=%d freq=%d kHz",
		hdrChannelPlanId, MDR_HEADER_CHANNEL, hdrFrequency);
	LOG_INF("Packet: ChannelPlanID=%d freq=%u kHz (delta=%d kHz)",
		pktChannelPlanId, pktFrequency, pktDeltaFrequency);

	const uint32_t *deltaConfig = get_wisun_delta_table_JP(hdrChannelPlanId);
	uint16_t deltaLen = (uint16_t)(((uint16_t)(deltaConfig[0] & 0x0FFF) * sizeof(uint32_t))
				       + sizeof(uint32_t));

	status = ti_cc140xp_store_config(ti_cc140xp_dev, DELTA_TABLE_CONFIG_ID,
					(uint8_t *)deltaConfig, deltaLen,
					DELTA_CONFIG_REFERENCE, false, config_callback,
					TRX_EventConfigStoreComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store delta table");
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("Delta table stored");

	/* Build command chain: CS_HDR(0) → TX_HDR(1) → CS_PKT(2) → TX_PKT(3) */
	uint16_t newPhyId = map->mappingTable[pktIdx].mdrByte;

	memset(&mdr_cca_cmds, 0, sizeof(MDR_CCA_cmds));
	setupCmdMdrCsHdr(&mdr_cca_cmds.cmdCsMdrHeader, hdrFrequency);
	setupCmdMdrTxHdr(&mdr_cca_cmds.cmdTxMdrHeader, newPhyId);
	setupCmdMdrCsPacket(&mdr_cca_cmds.cmdCsMdrPacket, pktDeltaFrequency);
	setupCmdMdrTxPacket(&mdr_cca_cmds.cmdTxMdrPacket, pktDeltaFrequency);

	uint64_t cmdEvents = TRX_EventCmdStatus | TRX_EventFinalCmdStatus |
			     TRX_EventCmdStoreComplete | TRX_EventCmdSubmitComplete |
			     TRX_EventLastStatusError;

	status = ti_cc140xp_store_cmds(ti_cc140xp_dev,
				      (TRX_Request_CommandStore *)&mdr_cca_cmds,
				      4U, NULL, cs_tx_callback, cmdEvents);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store MDR CCA commands");
		return -1;
	}
	k_sem_take(&cmd_store_complete_sem, K_FOREVER);

#if HAS_BTN
	LOG_INF("Ready -- press BTN1 to send %u MDR CCA pairs", NUMBER_OF_PACKETS);
#else
	LOG_INF("Ready -- auto-transmitting %u MDR CCA pairs every 5 seconds", NUMBER_OF_PACKETS);
#endif

	while (1) {
#if HAS_BTN
		k_sem_take(&button_sem, K_FOREVER);
#else
		k_sleep(K_SECONDS(5));
#endif

		for (i = 6U; i < TX_PACKET_LENGTH; i++) {
			txMdrPacket[i] = i - 5U;
		}
		sequence_number = 0U;

		while (sequence_number < NUMBER_OF_PACKETS) {
			txMdrPacket[4U] = (uint8_t)(sequence_number >> 8U);
			txMdrPacket[5U] = (uint8_t)(sequence_number++);

			/*
			 * Stream stores are fire-and-forget: the CS command calibrates the
			 * header frequency and the TX reuses it, so the streams just need to
			 * be in the buffer before submit.  No semaphore wait between stores.
			 */
			while (ti_cc140xp_store_stream(ti_cc140xp_dev, MDR_HEADER_STREAM_ID,
						      txMdrHeader, sizeof(txMdrHeader),
						      Stream_Retention_Flush_Streaming,
						      cs_tx_callback, 0U) != TRX_Host_Success) {
				k_busy_wait(100U);
			}

			while (ti_cc140xp_store_stream(ti_cc140xp_dev, MDR_PACKET_STREAM_ID,
						      txMdrPacket, sizeof(txMdrPacket),
						      Stream_Retention_Flush_Streaming,
						      cs_tx_callback, 0U) != TRX_Host_Success) {
				k_busy_wait(100U);
			}

			/* Submit CS chain; cs_tx_sem released on TRX_EventFinalCmdStatus */
			while (ti_cc140xp_submit_cmd(ti_cc140xp_dev,
						    CMD_CS_MDR_HEADER_SLOT) != TRX_Host_Success) {
				k_busy_wait(100U);
			}

			k_sem_take(&cs_tx_sem, K_FOREVER);

#if HAS_LED
			gpio_pin_toggle_dt(&led);
#endif
			LOG_INF("TX MDR CCA pair #%u sent", sequence_number);
			k_busy_wait(PACKET_INTERVAL_US);
		}

		LOG_INF("Burst complete: %u MDR CCA pairs sent", NUMBER_OF_PACKETS);
	}

	return 0;
}
