/*
 * Wireless UART Bridge for CC140xP Shield + CC2745 Host MCU
 *
 * UART (115200 8N1) <-> RF (Wi-SUN FSK) Bridge
 *
 * Architecture:
 * - UART RX Thread: Reads UART via interrupt-driven ring buffer, packages on
 *   255 bytes or 20ms quiet timeout, then drives the TX command.
 * - RX command runs continuously (repeat=true) in the background, exactly as
 *   in the ti_cc140xp_rx sample. Before each TX, the host explicitly stops it
 *   (ti_cc140xp_stop_cmd + wait for TRX_EventStopStatus) rather than relying
 *   on TX's conflict_policy to implicitly preempt it - submitting TX while RX
 *   is still consuming the SPI buffer pool races the TX stream load and fails
 *   with TRX_CommandStatus_Error_MissingTxBuffer. After each TX completes,
 *   the host re-submits the already-stored RX command slot, mirroring the
 *   resubmit-on-stop loop in ti_cc140xp_dual_protocol_rx.
 * - UART TX Thread: Drains the RF-received-packet queue to the UART FIFO.
 * - LED0 (Green): Toggles on RF TX. LED1 (Red): Toggles on RF RX.
 * - Fallback: If only one LED is present, it blinks once for TX, twice for RX.
 *
 * Copyright (c) 2024 Texas Instruments Incorporated
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log_backend.h>

#include <ti_cc140xp/ti_cc140xp.h>

LOG_MODULE_REGISTER(ti_cc140xp_uart_bridge, LOG_LEVEL_DBG);

/* RCL settings for Wi-SUN (same configuration used by ti_cc140xp_rx/_tx) */
#include "settings/rcl_settings_wisun.h"
#define RF_CONFIG_SIZE (LRF_mainRegConfig_wisun_byteCount)
#define RF_CONFIG_PTR  ((uint8_t *)LRF_mainRegConfig_wisun)
#define FE_CONFIG_SIZE (LRF_frontendRegConfig_wisun_byteCount)
#define FE_CONFIG_PTR  ((uint8_t *)LRF_frontendRegConfig_wisun)

/* ========================================================================
 * Configuration Constants
 * ======================================================================== */

#define MAX_RF_PAYLOAD        255
#define UART_BUF_SIZE         512
#define UART_QUIET_TIMEOUT_MS 20
#define RF_FREQUENCY_KHZ      920600U

#define TX_CMD_SLOT      (0U)
#define RX_CMD_SLOT      (1U)
#define TX_STREAM_ID     (0U)
#define RX_STREAM_ID     (1U)
#define RF_CONFIG_ID     (1U)

#define FE_CONFIG_REFERENCE (0xFEFEFEFEU)
#define RF_CONFIG_REFERENCE (0xF000000DU)

/* The data received will contain both timestamp (4B) and RSSI (1B) information. */
#define RSSI_SIZE_BYTES      (1U)
#define TIMESTAMP_SIZE_BYTES (4U)
#define RX_PACKET_LENGTH     (sizeof(TRX_PayloadHeader) + MAX_RF_PAYLOAD + \
			       RSSI_SIZE_BYTES + TIMESTAMP_SIZE_BYTES)

/* Thread Stack Sizes */
#define UART_RX_STACK_SIZE    2048
#define UART_TX_STACK_SIZE    2048

/* Thread Priorities */
#define UART_RX_PRIORITY      5
#define UART_TX_PRIORITY      5 //TODO recheck prio

/* ========================================================================
 * Data Structures
 * ======================================================================== */

struct rf_packet {
	void *fifo_reserved; /* 1st word reserved by k_fifo */
	uint8_t data[MAX_RF_PAYLOAD];
	uint8_t len;
};

struct uart_tx_packet {
	void *fifo_reserved; /* 1st word reserved by k_fifo */
	uint8_t data[MAX_RF_PAYLOAD];
	uint8_t len;
};

/* Fixed-size memory slabs for both packet types. receive_callback() runs in
 * SPI transport completion context (effectively ISR context, invoked
 * directly from the Zephyr SPI driver's completion path) - k_malloc()/k_free()
 * are not safe to call there (the heap allocator's internal locking assumes
 * thread context), and doing so under rapid back-to-back RX events corrupts
 * the heap and hard-faults. Slabs allocate/free in O(1) with ISR-safe
 * primitives, so all packet handoff uses them instead of the heap. */
#define RF_PACKET_SLAB_COUNT   16
#define UART_PACKET_SLAB_COUNT 16

K_MEM_SLAB_DEFINE(rf_packet_slab, sizeof(struct rf_packet), RF_PACKET_SLAB_COUNT, 4);
K_MEM_SLAB_DEFINE(uart_packet_slab, sizeof(struct uart_tx_packet), UART_PACKET_SLAB_COUNT, 4);

/* ========================================================================
 * Device Tree
 * ======================================================================== */

static const struct device *const uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

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

/* Button sw0 toggles log output on/off at runtime, so the UART terminal can
 * show a clean echo of bytes forwarded from the other board without log
 * lines interleaved. Logs are enabled by default on every boot/reset. */
#if DT_NODE_HAS_STATUS(DT_ALIAS(sw0), okay)
#define SW0_NODE DT_ALIAS(sw0)
static const struct gpio_dt_spec btn0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
#define HAS_SW0 1
#else
#define HAS_SW0 0
#warning "SW0 not found in device tree, log mute button will be disabled"
#endif

/* ========================================================================
 * Global State
 * ======================================================================== */

/* UART RX ring buffer + quiet timer */
static struct ring_buf uart_rx_rb;
static uint8_t uart_rx_rb_buf[UART_BUF_SIZE];
static struct k_timer uart_quiet_timer;

/* Thread communication queues */
static K_FIFO_DEFINE(rf_tx_fifo);   /* UART RX -> TX thread (bytes to send over RF) */
static K_FIFO_DEFINE(uart_tx_fifo); /* RF RX -> UART TX thread (bytes received over RF) */

/* Semaphores, named after the reference ti_cc140xp_tx / ti_cc140xp_rx samples */
static K_SEM_DEFINE(config_sem, 0, 1);
static K_SEM_DEFINE(tx_cmd_store_sem, 0, 1);
static K_SEM_DEFINE(tx_stream_store_sem, 0, 1);
static K_SEM_DEFINE(tx_cmd_submit_sem, 0, 1);
static K_SEM_DEFINE(rx_cmd_store_sem, 0, 1);
static K_SEM_DEFINE(rx_stop_sem, 0, 1);

/* RF command structures (configured once in rf_init(), reused per packet) */
static TRX_Request_CommandStore cmd_tx;
static TRX_Request_CommandStore cmd_rx;

/* RX stream buffer (header + payload + RSSI + timestamp) */
static uint8_t rx_buffer[RX_PACKET_LENGTH];

/* Statistics */
static uint32_t stat_uart_rx_bytes;
static uint32_t stat_rf_tx_packets;
static uint32_t stat_rf_rx_packets;
static uint32_t stat_uart_tx_bytes;

/* ========================================================================
 * LED Control
 * ======================================================================== */

static void led_init(void)
{
#if HAS_LED0
	gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
	LOG_INF("LED0 (Green) configured");
#else
	LOG_WRN("LED0 (Green) not available");
#endif

#if HAS_LED1
	gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
	LOG_INF("LED1 (Red) configured");
#else
	LOG_WRN("LED1 (Red) not available");
#endif

#if !HAS_LED0 && !HAS_LED1
	LOG_WRN("No LEDs available - status indication disabled");
#endif
}

static void led_tx_toggle(void)
{
#if HAS_LED0
	gpio_pin_toggle_dt(&led0);
#elif HAS_LED1
	/* Fallback: single LED - blink once for TX */
	gpio_pin_set_dt(&led1, 1);
	k_busy_wait(50000);
	gpio_pin_set_dt(&led1, 0);
#endif
}

static void led_rx_toggle(void)
{
#if HAS_LED1
	gpio_pin_toggle_dt(&led1);
#elif HAS_LED0
	/* Fallback: single LED - blink twice for RX */
	gpio_pin_set_dt(&led0, 1);
	k_busy_wait(50000);
	gpio_pin_set_dt(&led0, 0);
	k_busy_wait(50000);
	gpio_pin_set_dt(&led0, 1);
	k_busy_wait(50000);
	gpio_pin_set_dt(&led0, 0);
#endif
}

/* ========================================================================
 * Log Mute Button (SW0)
 *
 * Toggles the UART log backend on/off at runtime so the serial terminal can
 * show a clean echo of bytes forwarded from the other board, with no log
 * lines interleaved. log_backend_activate()/deactivate() just flip a flag on
 * the backend - no locking, no Kconfig dependency - and are explicitly safe
 * to call directly from this GPIO interrupt callback (ISR context), so no
 * deferral to a thread/workqueue is needed. Logs are enabled by default on
 * every boot/reset; this state is intentionally not persisted.
 * ======================================================================== */

#if HAS_SW0
#define LOG_MUTE_DEBOUNCE_MS 200

static struct gpio_callback btn0_cb_data;
static bool logs_muted = true;
static int64_t last_toggle_uptime_ms;

static void set_logs_muted(bool muted)
{
	logs_muted = muted;
	for (size_t i = 0; i < log_backend_count_get(); i++) {
		const struct log_backend *backend = log_backend_get(i);

		if (logs_muted) {
			log_backend_deactivate(backend);
		} else {
			log_backend_activate(backend, backend->cb->ctx);
		}
	}
}

static void button_pressed_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	int64_t now = k_uptime_get();

	/* Debounce: ignore re-triggers from mechanical bounce within the window */
	if ((now - last_toggle_uptime_ms) < LOG_MUTE_DEBOUNCE_MS) {
		return;
	}
	last_toggle_uptime_ms = now;

	set_logs_muted(!logs_muted);
}

static void log_mute_button_init(void)
{
	if (!gpio_is_ready_dt(&btn0)) {
		LOG_WRN("SW0 GPIO device not ready - log mute button disabled");
		return;
	}

	gpio_pin_configure_dt(&btn0, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&btn0, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&btn0_cb_data, button_pressed_isr, BIT(btn0.pin));
	gpio_add_callback(btn0.port, &btn0_cb_data);

	LOG_INF("Press SW0 to toggle log output (logs OFF by default)");

	/* Apply the muted-by-default state now that the "press SW0..." notice
	 * above has had a chance to print. */
	set_logs_muted(true);
}
#else
static void log_mute_button_init(void)
{
	LOG_WRN("SW0 not available - log mute button disabled");
}
#endif

/* ========================================================================
 * UART RX - Interrupt Handler & Quiet Timer
 * ======================================================================== */

static void uart_quiet_timer_expiry(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);

	uint32_t len = ring_buf_size_get(&uart_rx_rb);

	if (len > 0 && len <= MAX_RF_PAYLOAD) {
		struct rf_packet *pkt;

		/* ISR-safe: this runs in k_timer expiry (interrupt) context,
		 * and is also called directly from uart_rx_isr() - never touch
		 * the heap here. K_NO_WAIT never blocks; if the slab is
		 * exhausted (TX thread falling behind), drop this chunk rather
		 * than stall the UART RX interrupt path. */
		if (k_mem_slab_alloc(&rf_packet_slab, (void **)&pkt, K_NO_WAIT) != 0) {
			LOG_WRN("RF packet slab exhausted - UART chunk dropped");
			return;
		}

		uint32_t read = ring_buf_get(&uart_rx_rb, pkt->data, len);

		if (read == len) {
			pkt->len = (uint8_t)len;
			k_fifo_put(&rf_tx_fifo, pkt);
			LOG_DBG("UART quiet timeout: packaged %u bytes for RF TX", len);
		} else {
			k_mem_slab_free(&rf_packet_slab, (void *)pkt);
		}
	}
}

static void uart_rx_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_rx_ready(dev)) {
		uint8_t buf[64];
		int read = uart_fifo_read(dev, buf, sizeof(buf));

		if (read <= 0) {
			break;
		}

		uint32_t written = ring_buf_put(&uart_rx_rb, buf, read);

		if (written != (uint32_t)read) {
			LOG_WRN("UART RX ring buffer overflow: %u/%d bytes", written, read);
		}
		stat_uart_rx_bytes += written;

		/* Reset quiet timer - new data arrived */
		k_timer_start(&uart_quiet_timer, K_MSEC(UART_QUIET_TIMEOUT_MS), K_NO_WAIT);

		/* Hit max payload size - package immediately */
		if (ring_buf_size_get(&uart_rx_rb) >= MAX_RF_PAYLOAD) {
			k_timer_stop(&uart_quiet_timer);
			uart_quiet_timer_expiry(NULL);
		}
	}
}

/* ========================================================================
 * TRX Host Callbacks
 * ======================================================================== */

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

	if (events & TRX_EventLastStatusError) {
		LOG_ERR("Config store error");
		k_sem_give(&config_sem);
		return;
	}

	if (events & TRX_EventNvEraseComplete) {
		k_sem_give(&config_sem);
	}
	if (events & TRX_EventConfigStoreComplete) {
		k_sem_give(&config_sem);
	}
	if (events & TRX_EventConfigPersistComplete) {
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

	if (events & TRX_EventLastStatusError) {
		TRX_Request_LastStatus *pLastStatus = (TRX_Request_LastStatus *)request;

		if (pLastStatus->status == TRX_STATUS_INVALID_PARAM) {
			LOG_ERR("Device config error: invalid param");
		} else if (pLastStatus->status == TRX_STATUS_INVALID_STATE) {
			LOG_ERR("Device config error: invalid state");
		}
		k_sem_give(&config_sem);
		return;
	}

	if (events & TRX_EventDeviceConfigStoreComplete) {
		k_sem_give(&config_sem);
	}
}

/*
 * Handles TX command events: command store, stream store, and per-packet TX status.
 * Mirrors transmit_callback() in ti_cc140xp_tx/src/main.c.
 */
static void transmit_callback(TRX_Host_Handle handle, uintptr_t pCmdStore,
			       TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(pCmdStore);
	ARG_UNUSED(arg);

	if (events & TRX_EventLastStatusError) {
		LOG_ERR("Transmit error");
		return;
	}

	if (events & TRX_EventCmdStatus) {
		TRX_Request_CommandStatus *pCmdStatusRequest = (TRX_Request_CommandStatus *)request;

		if (pCmdStatusRequest->status == TRX_CommandStatus_Finished) {
			k_sem_give(&tx_cmd_submit_sem);
			stat_rf_tx_packets++;
			led_tx_toggle();
			LOG_INF(">> TX #%u sent (%u pkts total)", stat_rf_tx_packets, stat_rf_tx_packets);
		} else {
			LOG_WRN("TX command exited unexpectedly with status %d",
				pCmdStatusRequest->status);
		}
	}

	if (events & TRX_EventCmdStoreComplete) {
		k_sem_give(&tx_cmd_store_sem);
	}

	if (events & TRX_EventStreamStoreComplete) {
		k_sem_give(&tx_stream_store_sem);
	}
}

/*
 * Handles RX command events: command store, stop acknowledgment, buffer
 * overflow, and received-packet delivery. Mirrors receive_callback() in
 * ti_cc140xp_rx/src/main.c, with one addition: the UART RX thread explicitly
 * stops this RX command before every TX (see transmit_packet()) rather than
 * relying on TX's conflict_policy to implicitly preempt it - submitting TX
 * while RX is still actively consuming the SPI buffer pool races the TX
 * stream load and fails with TRX_CommandStatus_Error_MissingTxBuffer.
 *
 * The stop acknowledgment arrives as TRX_EventStopStatus, whose associated
 * request is a TRX_Request_LastStatus (status in params[0]) - NOT a
 * TRX_Request_CommandStatus. Casting it as the latter reads past the
 * structure and faults, so stop acknowledgment is only ever signalled via
 * TRX_EventStopStatus, never by inspecting the request on TRX_EventCmdStatus.
 */
static void receive_callback(TRX_Host_Handle handle, uintptr_t pCmdStoreOrData,
			      TRX_Request *request, uint64_t events, uintptr_t arg)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(request);
	ARG_UNUSED(arg);

	if (events & TRX_EventCmdStoreComplete) {
		k_sem_give(&rx_cmd_store_sem);
	}

	if (events & TRX_EventStopStatus) {
		LOG_DBG("RX command stop acknowledged");
		k_sem_give(&rx_stop_sem);
	}

	if (events & TRX_EventStreamStoreOverflow) {
		LOG_WRN("RX buffer overflow - packet dropped");
		return;
	}

	if (events & TRX_EventFinalStreamStoreReceived) {
		uint8_t *data = (uint8_t *)pCmdStoreOrData;

		if (!data) {
			return;
		}

		TRX_PayloadHeader *header = (TRX_PayloadHeader *)data;

		if (header->length > MAX_RF_PAYLOAD) {
			LOG_WRN("RX payload too large: %u", header->length);
			return;
		}

		uint8_t payload_len = (uint8_t)header->length;
		uint8_t *payload_start = data + sizeof(TRX_PayloadHeader);

		struct uart_tx_packet *pkt;

		/* ISR-safe: K_NO_WAIT never blocks. If the slab is exhausted
		 * (consumer thread falling behind), drop the packet rather
		 * than touch the heap or block the SPI completion path. */
		if (k_mem_slab_alloc(&uart_packet_slab, (void **)&pkt, K_NO_WAIT) != 0) {
			LOG_WRN("UART TX packet slab exhausted - packet dropped");
			return;
		}

		memcpy(pkt->data, payload_start, payload_len);
		pkt->len = payload_len;
		k_fifo_put(&uart_tx_fifo, pkt);
		stat_rf_rx_packets++;
		led_rx_toggle();
		LOG_INF("<< RX #%u received (%u bytes, %u pkts total)",
			stat_rf_rx_packets, payload_len, stat_rf_rx_packets);
	}
}

/* ========================================================================
 * RF Initialization
 * ======================================================================== */

static int rf_init(void)
{
	TRX_Host_Status status;

	LOG_INF("TI CC140xP TRX initialization");

#if !TI_CC140XP_ENABLED
	LOG_ERR("TI CC140xP device tree node not found");
	return -ENODEV;
#endif

	if (!device_is_ready(ti_cc140xp_dev)) {
		LOG_ERR("TI CC140xP device not ready");
		return -ENODEV;
	}

	TRX_Host_Params params = {
		.bitRate = 12000000U,
		.generalCb = general_callback,
		.arg = (uintptr_t)NULL,
	};

	status = ti_cc140xp_host_open(ti_cc140xp_dev, &params);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to open TRX Host: %d", status);
		return -EIO;
	}
	LOG_INF("TRX Host opened");

	k_sleep(K_MSEC(100));

	LOG_INF("Configuring TRX device...");
	TRX_DeviceConfigData deviceConfig = {
		.dioConfig = {
			.dio0 = DIO0_UNCHANGED,
			.dio2 = DIO2_HIGH_PA,        /* Must not be changed when using the boosterpack */
			.dio3 = DIO3_LOW_PA_AND_LNA, /* Must not be changed when using the boosterpack */
			.dio4 = DIO4_UNCHANGED,
			.dio5 = DIO5_UNCHANGED,
			.dio6 = DIO6_UNCHANGED,
			.dio7 = DIO7_UNCHANGED,
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
		return -EIO;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("TRX device configured");

	LOG_INF("Erasing NV...");
	status = ti_cc140xp_erase_nv(ti_cc140xp_dev, config_callback);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to erase NV: %d", status);
		return -EIO;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("NV erased");

	LOG_INF("Loading Wi-SUN FE configuration (%u bytes)...", FE_CONFIG_SIZE);
	status = ti_cc140xp_store_config(ti_cc140xp_dev, TRX_CONFIG_ID_FRONTEND,
					  FE_CONFIG_PTR, FE_CONFIG_SIZE,
					  FE_CONFIG_REFERENCE, true, config_callback,
					  TRX_EventConfigStoreComplete |
					  TRX_EventConfigPersistComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store FE config: %d", status);
		return -EIO;
	}
	k_sem_take(&config_sem, K_FOREVER);

	status = ti_cc140xp_persist_config(ti_cc140xp_dev, TRX_CONFIG_ID_FRONTEND);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to persist FE config: %d", status);
		return -EIO;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("FE configuration loaded and persisted");

	LOG_INF("Loading Wi-SUN PHY configuration (%u bytes)...", RF_CONFIG_SIZE);
	status = ti_cc140xp_store_config(ti_cc140xp_dev, RF_CONFIG_ID,
					  RF_CONFIG_PTR, RF_CONFIG_SIZE,
					  RF_CONFIG_REFERENCE, false, config_callback,
					  TRX_EventConfigStoreComplete |
					  TRX_EventConfigPersistComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store PHY config: %d", status);
		return -EIO;
	}
	k_sem_take(&config_sem, K_FOREVER);

	status = ti_cc140xp_persist_config(ti_cc140xp_dev, RF_CONFIG_ID);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to persist PHY config: %d", status);
		return -EIO;
	}
	k_sem_take(&config_sem, K_FOREVER);
	LOG_INF("PHY configuration loaded and persisted");

	/* TX command - configured once, reused for every outgoing packet. The
	 * running RX command is explicitly stopped before each TX submission
	 * (see transmit_packet()); AlwaysInterrupt is kept as a safety net in
	 * case the explicit stop didn't complete in time. */
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
	cmd_tx.params.tx.frequency = RF_FREQUENCY_KHZ;
	cmd_tx.params.tx.modem = TRX_RadioCommand_Modem_FSK;
	cmd_tx.params.tx.power.dBm = 21;
	cmd_tx.params.tx.power.fraction = 1; /* +0.5 dBm. Total power = 21.5 dBm */

	status = ti_cc140xp_store_cmds(ti_cc140xp_dev, &cmd_tx, 1U, NULL,
					transmit_callback,
					TRX_EventCmdStatus | TRX_EventCmdStoreComplete |
					TRX_EventCmdSubmitComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store TX command: %d", status);
		return -EIO;
	}
	k_sem_take(&tx_cmd_store_sem, K_FOREVER);
	LOG_INF("TX command stored");

	/* RX command - repeat=true keeps it running continuously in the
	 * background, auto-submitted via the 4th store_cmds argument. */
	cmd_rx.slot = RX_CMD_SLOT;
	cmd_rx.cmd_id = TRX_RadioCommand_Receive;
	cmd_rx.enable_on_true = false;
	cmd_rx.enable_on_false = false;
	cmd_rx.enable_on_compare = false;
	cmd_rx.trigger = Command_Trigger_Immediate;
	cmd_rx.conflict_policy = TRX_ConflictPolicy_AlwaysInterrupt;
	cmd_rx.allow_delay = false;
	cmd_rx.params.rx.phy0.config_id = RF_CONFIG_ID;
	cmd_rx.params.rx.phy0.option_mask = TRX_PHY_FEATURE_FSK_MODE_2B_WISUN;
	cmd_rx.params.rx.stream_id = RX_STREAM_ID;
	cmd_rx.params.rx.frequency = RF_FREQUENCY_KHZ;
	cmd_rx.params.rx.modem = TRX_RadioCommand_Modem_FSK;
	cmd_rx.params.rx.enable_mdr = false;
	cmd_rx.params.rx.stream_early = true;
	cmd_rx.params.rx.timeout = 0U;
	cmd_rx.params.rx.repeat = true;
	cmd_rx.params.rx.search_strategy = TRX_Rx_SearchStrategy_Sync;

	status = ti_cc140xp_register_rx_stream(ti_cc140xp_dev, RX_STREAM_ID, rx_buffer,
						sizeof(rx_buffer), receive_callback,
						TRX_EventFinalStreamStoreReceived |
						TRX_EventStreamStoreOverflow);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to register RX stream: %d", status);
		return -EIO;
	}

	status = ti_cc140xp_store_cmds(ti_cc140xp_dev, &cmd_rx, 1U, &cmd_rx,
					receive_callback,
					TRX_EventCmdStoreComplete |
					TRX_EventCmdSubmitComplete |
					TRX_EventStopStatus);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store RX command: %d", status);
		return -EIO;
	}
	k_sem_take(&rx_cmd_store_sem, K_FOREVER);
	LOG_INF("RX command stored and listening on %u kHz", RF_FREQUENCY_KHZ);

	return 0;
}

/* ========================================================================
 * Thread: UART RX -> RF TX
 * ======================================================================== */

static void resume_rx(void)
{
	TRX_Host_Status status = ti_cc140xp_submit_cmd(ti_cc140xp_dev, RX_CMD_SLOT);

	if (status != TRX_Host_Success) {
		LOG_WRN("Failed to resume RX: %d", status);
	}
}

/* Staging buffer for header + payload, built fresh for every outgoing packet.
 * The SUN FSK PHY requires a 4-byte TRX_PayloadHeader_SunFSK prefix describing
 * the payload (length/modulation/whitening/fcs_mode) - without it the TRX
 * firmware cannot stage the stream into a TX buffer and the submit fails with
 * TRX_CommandStatus_Error_MissingTxBuffer. Mirrors the header construction in
 * ti_cc140xp_tx/src/main.c, sized dynamically here since UART payloads vary
 * (1-255 bytes) rather than using a single fixed-length test packet. */
static uint8_t tx_staging_buf[sizeof(TRX_PayloadHeader) + MAX_RF_PAYLOAD];

static void transmit_packet(struct rf_packet *pkt)
{
	TRX_Host_Status status;

	/* Explicitly stop the continuously-running RX command and wait for the
	 * stop acknowledgment before touching the TX stream buffer. Relying on
	 * TX's conflict_policy to implicitly preempt RX races the TX stream
	 * load against RX still consuming the SPI buffer pool, which fails
	 * with TRX_CommandStatus_Error_MissingTxBuffer. */
	status = ti_cc140xp_stop_cmd(ti_cc140xp_dev, RX_CMD_SLOT, TRX_Command_StopType_Graceful);
	if (status != TRX_Host_Success) {
		LOG_WRN("Failed to stop RX before TX: %d", status);
	} else if (k_sem_take(&rx_stop_sem, K_SECONDS(2)) != 0) {
		LOG_WRN("RX stop acknowledgment timeout");
	}

	/* Build SUN FSK payload header + copy payload into staging buffer */
	TRX_PayloadHeader_SunFSK *header = (TRX_PayloadHeader_SunFSK *)tx_staging_buf;

	header->length = pkt->len;
	header->modulation = TRX_PayloadHeader_Modulation_FSK;
	header->mode_switch = 0U;
	header->fcs_mode = 0U;
	header->whitening = 1U;
	memcpy(tx_staging_buf + sizeof(TRX_PayloadHeader), pkt->data, pkt->len);

	status = ti_cc140xp_store_stream(ti_cc140xp_dev, TX_STREAM_ID,
					  tx_staging_buf, sizeof(TRX_PayloadHeader) + pkt->len,
					  Stream_Retention_Flush_Streaming,
					  transmit_callback,
					  TRX_EventLastStatusError |
					  TRX_EventStreamStoreComplete);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to store TX stream: %d", status);
		return;
	}
	if (k_sem_take(&tx_stream_store_sem, K_SECONDS(2)) != 0) {
		LOG_WRN("TX stream store timeout");
		return;
	}

	status = ti_cc140xp_submit_cmd(ti_cc140xp_dev, TX_CMD_SLOT);
	if (status != TRX_Host_Success) {
		LOG_ERR("Failed to submit TX command: %d", status);
		return;
	}
	if (k_sem_take(&tx_cmd_submit_sem, K_SECONDS(5)) != 0) {
		LOG_WRN("TX command timeout");
	}
}

static void uart_rx_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("UART RX thread started");

	while (1) {
		struct rf_packet *pkt = k_fifo_get(&rf_tx_fifo, K_FOREVER);

		if (!pkt) {
			continue;
		}

		if (pkt->len == 0 || pkt->len > MAX_RF_PAYLOAD) {
			k_mem_slab_free(&rf_packet_slab, (void *)pkt);
			continue;
		}

		LOG_DBG("UART->RF: sending %u bytes", pkt->len);
		transmit_packet(pkt);
		k_mem_slab_free(&rf_packet_slab, (void *)pkt);

		/* RX was interrupted by the TX above - resume it. */
		resume_rx();
	}
}

/* ========================================================================
 * Thread: RF RX -> UART TX
 * ======================================================================== */

static void uart_tx_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("UART TX thread started");

	while (1) {
		struct uart_tx_packet *pkt = k_fifo_get(&uart_tx_fifo, K_FOREVER);

		if (!pkt) {
			continue;
		}

		if (pkt->len == 0) {
			k_mem_slab_free(&uart_packet_slab, (void *)pkt);
			continue;
		}

		LOG_DBG("RF->UART: writing %u bytes", pkt->len);

		uint32_t written = 0;

		while (written < pkt->len) {
			int ret = uart_fifo_fill(uart_dev, pkt->data + written,
						  pkt->len - written);

			if (ret <= 0) {
				k_yield();
				continue;
			}
			written += ret;
		}
		stat_uart_tx_bytes += written;

		k_mem_slab_free(&uart_packet_slab, (void *)pkt);
	}
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void)
{
	int ret;

	LOG_INF("============================================");
	LOG_INF("Wireless UART Bridge Starting");
	LOG_INF("CC2745 + CC140xP BoosterPack");
	LOG_INF("UART: 115200 8N1 <-> RF: Wi-SUN FSK @ %u kHz", RF_FREQUENCY_KHZ);
	LOG_INF("============================================");

	led_init();
	log_mute_button_init();

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device not ready");
		return 0;
	}

	ring_buf_init(&uart_rx_rb, UART_BUF_SIZE, uart_rx_rb_buf);
	k_timer_init(&uart_quiet_timer, uart_quiet_timer_expiry, NULL);

	uart_irq_callback_user_data_set(uart_dev, uart_rx_isr, NULL);
	uart_irq_rx_enable(uart_dev);
	LOG_INF("UART RX interrupt enabled");

	ret = rf_init();
	if (ret != 0) {
		LOG_ERR("RF initialization failed: %d", ret);
		return 0;
	}

	LOG_INF("All subsystems initialized - starting threads");

	static K_THREAD_STACK_DEFINE(uart_rx_stack, UART_RX_STACK_SIZE);
	static struct k_thread uart_rx_thread_data;

	k_thread_create(&uart_rx_thread_data, uart_rx_stack,
			K_THREAD_STACK_SIZEOF(uart_rx_stack),
			uart_rx_thread, NULL, NULL, NULL,
			UART_RX_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&uart_rx_thread_data, "UART RX");

	static K_THREAD_STACK_DEFINE(uart_tx_stack, UART_TX_STACK_SIZE);
	static struct k_thread uart_tx_thread_data;

	k_thread_create(&uart_tx_thread_data, uart_tx_stack,
			K_THREAD_STACK_SIZEOF(uart_tx_stack),
			uart_tx_thread, NULL, NULL, NULL,
			UART_TX_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&uart_tx_thread_data, "UART TX");

	LOG_INF("Bridge operational");

	while (1) {
		k_sleep(K_SECONDS(30));
		LOG_INF("Stats: UART RX=%u B, RF TX=%u pkts, RF RX=%u pkts, UART TX=%u B",
			stat_uart_rx_bytes, stat_rf_tx_packets,
			stat_rf_rx_packets, stat_uart_tx_bytes);
	}

	return 0;
}
