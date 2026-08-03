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

LOG_MODULE_REGISTER(dual_protocol_rx, LOG_LEVEL_INF);

/* Maximum payload in a receive operation */
#define RX_PAYLOAD_LENGTH       (76U)

#define MODEM_MASK_SIZE_BYTES   (1U)
#define RSSI_SIZE_BYTES         (1U)
#define TIMESTAMP_SIZE_BYTES    (4U)

/* Total buffer: modem mask + header + payload + rssi + timestamp */
#define RX_PACKET_LENGTH    (MODEM_MASK_SIZE_BYTES + sizeof(TRX_PayloadHeader) + \
			     RX_PAYLOAD_LENGTH + RSSI_SIZE_BYTES + TIMESTAMP_SIZE_BYTES)

#define RX_CMD_SLOT             (0U)
#define RX_STREAM_ID            (0U)
#define RF_CONFIG_ID            (1U)

#define FE_CONFIG_REFERENCE     (0xFEFEFEFEU)
#define RF_CONFIG_REFERENCE     (0xF000000DU)

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

/* RCL settings for dual protocol OQPSK/2GFSK50kbps */
#include "settings/rcl_settings_dualrx_sun_oqpsk_2gfsk50kbps.h"
#define RF_CONFIG_SIZE (LRF_mainRegConfig_dualrxSunOqpsk2gfsk50kbps_byteCount)
#define RF_CONFIG_PTR  ((uint8_t *)LRF_mainRegConfig_dualrxSunOqpsk2gfsk50kbps)
#define FE_CONFIG_SIZE (LRF_frontendRegConfig_dualrxSunOqpsk2gfsk50kbps_byteCount)
#define FE_CONFIG_PTR  ((uint8_t *)LRF_frontendRegConfig_dualrxSunOqpsk2gfsk50kbps)

static K_SEM_DEFINE(config_sem,             0, 1);
static K_SEM_DEFINE(cmd_store_complete_sem, 0, 1);
static K_SEM_DEFINE(rx_sem,                 0, 1);

static TRX_Request_CommandStore cmd_dual_rx;
static uint8_t rx_buffer[RX_PACKET_LENGTH];
static uint32_t cnt_rx_packets;

/* Last received modem mask — written in callback, read by blink work item */
static uint8_t last_rx_modem;

/*
 * Single-LED blink runs in the system workqueue so the RX callback
 * (TRX dispatch thread) is not blocked during k_sleep calls.
 * 1 pulse (100ms) = OQPSK;  2 pulses (100ms + 150ms gap + 100ms) = FSK.
 */
#if HAS_LED0 && !HAS_LED1
static struct k_work blink_work;

static void do_led_blink(struct k_work *work)
{
	uint8_t modem = last_rx_modem;

	gpio_pin_set_dt(&led0, 1);
	k_sleep(K_MSEC(100));
	gpio_pin_set_dt(&led0, 0);
	if (modem & TRX_RadioCommand_Modem_FSK) {
		k_sleep(K_MSEC(150));
		gpio_pin_set_dt(&led0, 1);
		k_sleep(K_MSEC(100));
		gpio_pin_set_dt(&led0, 0);
	}
}
#endif

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

/* Signals completion (or failure) of the device configuration store request. */
static void storeDeviceConfigCallback(TRX_Host_Handle handle, uintptr_t pConfigData,
				      TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pConfigData);
	ARG_UNUSED(arg);

	if (TRX_EventLastStatusError & events) {
		TRX_Request_LastStatus *pLastStatus = (TRX_Request_LastStatus *)request;

		if (pLastStatus->status == TRX_STATUS_INVALID_PARAM)
			LOG_ERR("Device config: invalid param");
		else if (pLastStatus->status == TRX_STATUS_INVALID_STATE)
			LOG_ERR("Device config: invalid state");
		k_sem_give(&config_sem);
		return;
	}

	if (TRX_EventDeviceConfigStoreComplete & events)
		k_sem_give(&config_sem);
}

/* Handles dual-protocol RX events: command store, buffer overflow, and received-packet delivery for either modem. */
static void rx_callback(TRX_Host_Handle handle, uintptr_t pCmdStoreOrData,
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

	/* Overflow check before FinalStreamStoreReceived */
	if (TRX_EventStreamStoreOverflow & events) {
		LOG_ERR("RX buffer overflow — packet dropped");
		return;
	}

	if (TRX_EventFinalStreamStoreReceived & events) {
		if ((uintptr_t)NULL != pCmdStoreOrData) {
			/*
			 * Packet layout (SUN OQPSK):
			 *   [modemMask(1B)] [TRX_PayloadHeader(4B)] [payload] [rssi(1B)] [timestamp(4B)]
			 * Packet layout (Proprietary FSK):
			 *   [modemMask(1B)] [length(1B)] [payload] [rssi(1B)] [timestamp(4B)]
			 */
			uint8_t *pData = (uint8_t *)pCmdStoreOrData;
			uint8_t modemMask = pData[0];

			last_rx_modem = modemMask;
			cnt_rx_packets++;

			/* Dual-LED: instant toggle — safe to call directly in callback */
#if HAS_LED0 && HAS_LED1
			if (modemMask & TRX_RadioCommand_Modem_OQPSK) {
				gpio_pin_toggle_dt(&led0);
			} else if (modemMask & TRX_RadioCommand_Modem_FSK) {
				gpio_pin_toggle_dt(&led1);
			}
#endif
			/* Single-LED: offload timed blink to workqueue */
#if HAS_LED0 && !HAS_LED1
			k_work_submit(&blink_work);
#endif

			if (modemMask & TRX_RadioCommand_Modem_OQPSK) {
				TRX_PayloadHeader *hdr =
					(TRX_PayloadHeader *)(pData + MODEM_MASK_SIZE_BYTES);
				int8_t rssi = *(int8_t *)(pData + MODEM_MASK_SIZE_BYTES +
							  sizeof(TRX_PayloadHeader) + hdr->length);
				LOG_INF("RX #%u OQPSK 868 MHz: %u bytes, RSSI %d dBm",
					cnt_rx_packets, (uint8_t)hdr->length, rssi);
			} else if (modemMask & TRX_RadioCommand_Modem_FSK) {
				uint8_t length = pData[MODEM_MASK_SIZE_BYTES];
				int8_t rssi = *(int8_t *)(pData + MODEM_MASK_SIZE_BYTES + 1 + length);

				LOG_INF("RX #%u FSK 869 MHz: %u bytes, RSSI %d dBm",
					cnt_rx_packets, length, rssi);
			} else {
				LOG_WRN("RX #%u: unknown modem mask 0x%02x",
					cnt_rx_packets, modemMask);
			}
		}
	}

	if (TRX_EventCmdStatus & events) {
		TRX_Request_CommandStatus *pStatus = (TRX_Request_CommandStatus *)request;

		if (pStatus->status != TRX_CommandStatus_Finished) {
			LOG_WRN("RX command unexpected status: %d", pStatus->status);
			k_sem_give(&rx_sem);
		}
	}

	if (TRX_EventFinalCmdStatus & events) {
		k_sem_give(&rx_sem);
	}
}

int main(void)
{
	TRX_Host_Status status;

	LOG_INF("TI CC140xP Dual Protocol RX Sample");
	LOG_INF("Listening for OQPSK @ 868 MHz and 2GFSK @ 869 MHz");

#if !TI_CC140XP_ENABLED
	LOG_ERR("TI CC140xP device tree node not found");
	LOG_ERR("Please ensure your board overlay defines the ti_cc140xp node");
	return -1;
#endif

	if (!device_is_ready(ti_cc140xp_dev)) {
		LOG_ERR("TI CC140xP device not ready");
		return -1;
	}

	int ret;

#if HAS_LED0
	if (!gpio_is_ready_dt(&led0)) {
		LOG_ERR("LED0 GPIO device not ready");
		return -1;
	}
	ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED0 GPIO: %d", ret);
		return -1;
	}
	LOG_INF("LED0 (green) configured");
#endif

#if HAS_LED1
	if (!gpio_is_ready_dt(&led1)) {
		LOG_ERR("LED1 GPIO device not ready");
		return -1;
	}
	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED1 GPIO: %d", ret);
		return -1;
	}
	LOG_INF("LED1 (red) configured");
#endif

#if !HAS_LED0 && !HAS_LED1
	ARG_UNUSED(ret);
#endif

#if HAS_LED0 && !HAS_LED1
	k_work_init(&blink_work, do_led_blink);
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
			.dio6 = DIO6_UNCHANGED,
		},
		.clockConfig = TRX_ClockConfig_XOSC,
		.powerMode = {
			.dwellTimeUs = 0U,
			.powerPolicy = TRX_PowerPolicy_StandbyDisallow,
		},
		.rfMode = TRX_RfMode_PropFSKPeakAGC,
	};

	status = ti_cc140xp_store_device_config(ti_cc140xp_dev, device_config,
					       storeDeviceConfigCallback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store device config: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("Device config stored (PropFSKPeakAGC)");

	status = ti_cc140xp_erase_nv(ti_cc140xp_dev, config_callback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to erase NV: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("NV erased");

	/* Load and persist FE configuration */
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

	status = ti_cc140xp_persist_config(ti_cc140xp_dev, TRX_CONFIG_ID_FRONTEND);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to persist FE config: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("FE config stored");

	/* Load and persist PHY configuration */
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

	status = ti_cc140xp_persist_config(ti_cc140xp_dev, RF_CONFIG_ID);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to persist PHY config: %d", status);
		return -1;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("PHY config stored");

	/* Build the Dual RX command */
	cmd_dual_rx.slot              = RX_CMD_SLOT;
	cmd_dual_rx.cmd_id            = TRX_RadioCommand_DualReceive;
	cmd_dual_rx.enable_on_true    = false;
	cmd_dual_rx.enable_on_false   = false;
	cmd_dual_rx.enable_on_compare = false;
	cmd_dual_rx.trigger           = Command_Trigger_Immediate;
	cmd_dual_rx.conflict_policy   = TRX_ConflictPolicy_AlwaysInterrupt;
	cmd_dual_rx.allow_delay       = false;
	cmd_dual_rx.params.dualRx.phy0.config_id  = RF_CONFIG_ID;
	cmd_dual_rx.params.dualRx.stream_id       = RX_STREAM_ID;
	cmd_dual_rx.params.dualRx.frequency0      = 868000000U;  /* Hz, not kHz */
	cmd_dual_rx.params.dualRx.frequency1      = 869000000U;  /* Hz, not kHz */
	cmd_dual_rx.params.dualRx.modem           = TRX_RadioCommand_Modem_FSK |
						    TRX_RadioCommand_Modem_OQPSK;
	cmd_dual_rx.params.dualRx.enable_mdr      = false;
	cmd_dual_rx.params.dualRx.stream_early    = true;
	cmd_dual_rx.params.dualRx.timeout         = 0U;
	cmd_dual_rx.params.dualRx.repeat          = true;
	cmd_dual_rx.params.dualRx.search_strategy = TRX_Rx_SearchStrategy_Sync;

	/* Register the RX stream buffer */
	status = ti_cc140xp_register_rx_stream(ti_cc140xp_dev, RX_STREAM_ID,
					      rx_buffer, sizeof(rx_buffer),
					      rx_callback,
					      TRX_EventFinalStreamStoreReceived |
					      TRX_EventStreamStoreOverflow |
					      TRX_EventLastStatusError);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to register RX stream: %d", status);
		return -1;
	}

	/* Store command and auto-submit (4th arg = &cmd_dual_rx).
	 * repeat=true keeps the command running; main re-submits only on error.
	 */
	status = ti_cc140xp_store_cmds(ti_cc140xp_dev, &cmd_dual_rx, 1U, &cmd_dual_rx,
				      rx_callback,
				      TRX_EventCmdStatus       |
				      TRX_EventFinalCmdStatus   |
				      TRX_EventCmdStoreComplete |
				      TRX_EventCmdSubmitComplete |
				      TRX_EventLastStatusError);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store RX command: %d", status);
		return -1;
	}
	k_sem_take(&cmd_store_complete_sem, K_FOREVER);
	LOG_INF("Listening for dual protocol packets (OQPSK 868 MHz / FSK 869 MHz)...");

	/* repeat=true keeps the command running; rx_sem fires only on error/stop */
	while (1) {
		k_sem_take(&rx_sem, K_FOREVER);
		LOG_WRN("RX command stopped, re-submitting...");
		status = ti_cc140xp_submit_cmd(ti_cc140xp_dev, RX_CMD_SLOT);
		if (status != TRX_Host_Success) {
			k_busy_wait(1000U);
		}
	}

	return 0;
}
