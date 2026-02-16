/*
 * Copyright (c) 2025-2026 Conclusive Engineering sp. z o. o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/bluetooth.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <ble_if.h>
#include <ble_transport.h>
#include <hci_transport.h>

/* Needs to happen after CC3xxx Wi-Fi driver init. */
#define HCI_CC3XXX_INIT_PRIORITY	81

#define DT_DRV_COMPAT ti_cc3xxx_bt_hci

LOG_MODULE_REGISTER(DT_DRV_COMPAT, CONFIG_BT_HCI_DRIVER_LOG_LEVEL);

#ifdef CONFIG_DT_HAS_TI_CC33XX_UART_ENABLED

#define TI_CC33XX_HCI_RX_BUF_SIZE     (128)
#define TI_CC33XX_HCI_TX_BUF_SIZE     (128)
#define TI_CC33XX_HCI_ISR_CHUNK_LEN   (4)
#define TI_CC33XX_HCI_WAIT_TIMEOUT_MS (1000)

#define BT_NODE DT_CHOSEN(zephyr_bt_hci)

#define ti_cc33xx_hci_uart_signal k_sem_give
#define ti_cc33xx_hci_uart_wait   k_sem_take

struct ti_hci_uart_ctx {
	struct ring_buf *rx_ring;
	struct k_sem *rx_sem;
	struct ring_buf *tx_ring;
	struct k_sem *tx_sem;
};

static int ti_cc33xx_hci_uart_read(uint8_t *buf, uint16_t len);
static int ti_cc33xx_hci_uart_write(uint8_t *buf, uint16_t len);

const hciTransport_t uartForHci = {
	&ti_cc33xx_hci_uart_read,
	&ti_cc33xx_hci_uart_write
};

K_SEM_DEFINE(ti_hci_uart_rx_sem, 0, 1);
K_SEM_DEFINE(ti_hci_uart_tx_sem, 0, 1);

static const struct device *ti_hci_uart_dev = DEVICE_DT_GET(DT_PARENT(BT_NODE));
static uint8_t ti_hci_uart_rx_buf[TI_CC33XX_HCI_RX_BUF_SIZE];
static uint8_t ti_hci_uart_tx_buf[TI_CC33XX_HCI_TX_BUF_SIZE];
static struct ring_buf ti_hci_uart_rx_ring;
static struct ring_buf ti_hci_uart_tx_ring;
static struct ti_hci_uart_ctx hci_uart_ctx;

static void ti_cc33xx_hci_uart_isr(const struct device *dev, void *user_data)
{
	uint8_t *dst;
	uint32_t len, available, skip;
	struct ti_hci_uart_ctx *ctx = user_data;

	if (!uart_irq_update(dev)) {
		return;
	}

	if (uart_irq_rx_ready(dev)) {
		available = ring_buf_put_claim(ctx->rx_ring, &dst, TI_CC33XX_HCI_ISR_CHUNK_LEN);
		if (available == 0) {
			uart_fifo_read(dev, (uint8_t *)&skip, sizeof(skip));
			return;
		}

		len = uart_fifo_read(dev, dst, available);

		ring_buf_put_finish(ctx->rx_ring, len > 0 ? len : 0);
		ti_cc33xx_hci_uart_signal(ctx->rx_sem);
		return;
	}

	if (uart_irq_tx_ready(dev)) {
		available = ring_buf_get_claim(ctx->tx_ring, &dst, TI_CC33XX_HCI_ISR_CHUNK_LEN);
		if (available == 0) {
			uart_irq_tx_disable(dev);
			return;
		}

		len = uart_fifo_fill(dev, dst, available);

		ring_buf_get_finish(ctx->tx_ring, len > 0 ? len : 0);
		ti_cc33xx_hci_uart_signal(ctx->tx_sem);
		return;
	}
}

void UartHciOpen(void)
{
	if (!device_is_ready(ti_hci_uart_dev)) {
		return;
	}

	ring_buf_init(&ti_hci_uart_rx_ring, sizeof(ti_hci_uart_rx_buf), ti_hci_uart_rx_buf);
	ring_buf_init(&ti_hci_uart_tx_ring, sizeof(ti_hci_uart_tx_buf), ti_hci_uart_tx_buf);

	hci_uart_ctx.rx_ring = &ti_hci_uart_rx_ring;
	hci_uart_ctx.rx_sem  = &ti_hci_uart_rx_sem;
	hci_uart_ctx.tx_ring = &ti_hci_uart_tx_ring;
	hci_uart_ctx.tx_sem  = &ti_hci_uart_tx_sem;

	uart_irq_callback_user_data_set(ti_hci_uart_dev, ti_cc33xx_hci_uart_isr, &hci_uart_ctx);
	uart_irq_rx_enable(ti_hci_uart_dev);
}

void UartHciClose(void)
{
	if (!device_is_ready(ti_hci_uart_dev)) {
		return;
	}

	uart_irq_rx_disable(ti_hci_uart_dev);
	uart_irq_tx_disable(ti_hci_uart_dev);
}

static int ti_cc33xx_hci_uart_read(uint8_t *buf, uint16_t len)
{
	size_t read = 0;
	uint32_t got;

	if (!device_is_ready(ti_hci_uart_dev)) {
		return -1;
	}

	while (read < len) {
		got = ring_buf_get(&ti_hci_uart_rx_ring, buf + read, len - read);

		if (got == 0) {
			ti_cc33xx_hci_uart_wait(&ti_hci_uart_rx_sem, K_FOREVER);
		}
		read += got;
	}

	return 0;
}

static int ti_cc33xx_hci_uart_write(uint8_t *buf, uint16_t len)
{
	int ret;
	uint32_t put;
	size_t sent = 0;

	if (!device_is_ready(ti_hci_uart_dev)) {
		return -1;
	}

	while (sent < len) {
		put = ring_buf_put(&ti_hci_uart_tx_ring, buf + sent, len - sent);

		uart_irq_tx_enable(ti_hci_uart_dev);

		if (put == 0) {
			ret = ti_cc33xx_hci_uart_wait(&ti_hci_uart_tx_sem,
						      K_MSEC(TI_CC33XX_HCI_WAIT_TIMEOUT_MS));
			if (ret < 0) {
				return -1;
			}
		}
		sent += put;
	}

	return 0;
}

#endif /* CONFIG_DT_HAS_TI_CC33XX_UART_ENABLED */

struct hci_cc3xxx_priv {
	bt_hci_recv_t recv;
};

static int hci_cc3xxx_setup(const struct device *dev,
			    const struct bt_hci_setup_params *params)
{
	const bt_addr_t *addr = &params->public_addr;

	if (!bt_addr_eq(addr, BT_ADDR_ANY)) {
		BleIf_SetBdAddr(addr->val);
	}

	return 0;
}

static int hci_cc3xxx_open(const struct device *dev, bt_hci_recv_t hci_recv)
{
	struct hci_cc3xxx_priv *priv = dev->data;

	priv->recv = hci_recv;

	return BleIf_EnableBLE();
}

static int hci_cc3xxx_close(const struct device *dev)
{
	struct hci_cc3xxx_priv *priv = dev->data;

	priv->recv = NULL;

	return 0;
}

static int hci_cc3xxx_send(const struct device *dev, struct net_buf *buf)
{
	int ret;

	switch (bt_buf_get_type(buf)) {
	case BT_BUF_ACL_OUT:
		net_buf_push_u8(buf, BT_HCI_H4_ACL);
		break;
	case BT_BUF_CMD:
		net_buf_push_u8(buf, BT_HCI_H4_CMD);
		break;
	default:
		LOG_ERR("Unsupported BT type");
		ret = -EINVAL;
		goto out;
	}

	ret = BleIf_SendCommand(buf->data, buf->len);

out:
	net_buf_unref(buf);

	return ret;
}

static const struct bt_hci_driver_api drv = {
	.setup = hci_cc3xxx_setup,
	.open = hci_cc3xxx_open,
	.close = hci_cc3xxx_close,
	.send = hci_cc3xxx_send,
};

static bool hci_cc3xxx_is_evt_discardable(uint8_t *data)
{
	struct bt_hci_evt_hdr *evt = (void *)data;

	if (evt->evt != BT_HCI_EVT_LE_META_EVENT || !evt->len)
		return false;

	switch (data[BT_HCI_EVT_HDR_SIZE]) {
	case BT_HCI_EVT_LE_ADVERTISING_REPORT:
	/* Fall-through. */
	case BT_HCI_EVT_LE_EXT_ADVERTISING_REPORT:
		return true;
	default:
		return false;
	}
}

static int hci_cc3xxx_evt_recv(uint8_t *data, uint16_t len)
{
	const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_bt_hci));
	struct hci_cc3xxx_priv *priv = dev->data;
	struct net_buf *buf;
	uint8_t pkt_type;
	bool discardable;

	if (len == 0) {
		return -EINVAL;
	}

	pkt_type = data[0];

	/* Skip over the HCI packet indicator byte. */
	data++;
	len--;

	switch (pkt_type) {
	case BT_HCI_H4_EVT:
		if (len < BT_HCI_EVT_HDR_SIZE) {
			LOG_ERR("Event header is missing");
			return -EINVAL;
		}

		discardable = hci_cc3xxx_is_evt_discardable(data);
		buf = bt_buf_get_evt(data[0], discardable,
				     discardable ? K_NO_WAIT : K_FOREVER);
		break;
	case BT_HCI_H4_ACL:
		buf = bt_buf_get_rx(BT_BUF_ACL_IN, K_FOREVER);
		break;
	default:
		LOG_ERR("Unknown HCI packet type: %d", pkt_type);
		return -ENOTSUP;
	}

	if (!buf) {
		return -ENOMEM;
	}

	if (len > net_buf_tailroom(buf)) {
		LOG_ERR("Not enough space in RX buffer");
		net_buf_unref(buf);
		return -EINVAL;
	}

	net_buf_add_mem(buf, data, len);
	priv->recv(dev, buf);

	return 0;
}

static int hci_cc3xxx_init(const struct device *dev)
{
	BleIf_OpenTransport();
	BleIf_EventCbRegister(hci_cc3xxx_evt_recv);

	return 0;
}

#define	HCI_DEVICE_INIT(inst)						     \
	static struct hci_cc3xxx_priv hci_data_##inst = {		     \
	};								     \
	DEVICE_DT_INST_DEFINE(inst, hci_cc3xxx_init, NULL, &hci_data_##inst, \
			      NULL, POST_KERNEL, HCI_CC3XXX_INIT_PRIORITY,   \
			      &drv)

HCI_DEVICE_INIT(0)
