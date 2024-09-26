/*
 * Copyright (c) 2022 Libre Solar Technologies GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include <string.h>

/* change this to any other UART peripheral if desired */
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)

#define MSG_SIZE 32

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];

#ifdef CONFIG_UART_CC23X0_DMA_DRIVEN

#define MSG_SIZE_DMA (MSG_SIZE - 1)

static char rx_buf_rsp[MSG_SIZE];
static char *new_buf;

/* serial_cb_async used only if CONFIG_UART_EXCLUSIVE_API_CALLBACKS=n */
void serial_cb_async(const struct device *dev, struct uart_event *evt, void *user_data)
{
	switch (evt->type) {
	case UART_TX_DONE:
		printk("\nUART_TX_DONE (p = %p ; len = %d)\n", evt->data.tx.buf, evt->data.tx.len);
		break;
	case UART_TX_ABORTED:
		printk("\nUART_TX_ABORTED (p = %p ; len = %d)\n", evt->data.tx.buf, evt->data.tx.len);
		break;
	case UART_RX_RDY:
		printk("\nUART_RX_RDY (p = %p ; len = %d ; offset = %d)\n", evt->data.rx.buf, evt->data.rx.len, evt->data.rx.offset);
		evt->data.rx.buf[evt->data.rx.offset + evt->data.rx.len] = '\0';
		k_msgq_put(&uart_msgq, &evt->data.rx.buf[evt->data.rx.offset], K_NO_WAIT);
		break;
	case UART_RX_BUF_REQUEST:
		printk("\nUART_RX_BUF_REQUEST\n");
		new_buf = (new_buf == rx_buf) ? rx_buf_rsp : rx_buf;
		uart_rx_buf_rsp(uart_dev, new_buf, MSG_SIZE_DMA);
		break;
	case UART_RX_BUF_RELEASED:
		printk("\nUART_RX_BUF_RELEASED (p = %p)\n", evt->data.rx.buf);
		break;
	case UART_RX_DISABLED:
		printk("\nUART_RX_DISABLED\n");
		break;
	case UART_RX_STOPPED:
		printk("\nUART_RX_STOPPED\n");
		break;
	default:
		printk("\nUART_RX_INVALID\n");
	}
}

#else

static int rx_buf_pos;

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}
}

#endif /* CONFIG_UART_CC23X0_DMA_DRIVEN */

/*
 * Print a null-terminated string character by character to the UART interface
 */
void print_uart(char *buf)
{
	int msg_len = strlen(buf);

#ifdef CONFIG_UART_CC23X0_DMA_DRIVEN
	int ret;

	do {
		ret = uart_tx(uart_dev, buf, msg_len, SYS_FOREVER_US);
	} while (ret == -EBUSY);
#else
	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart_dev, buf[i]);
	}
#endif /* CONFIG_UART_CC23X0_DMA_DRIVEN */
}

int main(void)
{
	char tx_buf[MSG_SIZE];
	int ret;

	if (!device_is_ready(uart_dev)) {
		printk("UART device not found!");
		return 0;
	}

#ifdef CONFIG_UART_CC23X0_DMA_DRIVEN
	/* configure interrupt and callback to handle events */
	ret = uart_callback_set(uart_dev, serial_cb_async, NULL);
	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Async UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support async API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return 0;
	}

	new_buf = rx_buf;
	uart_rx_enable(uart_dev, new_buf, MSG_SIZE_DMA, SYS_FOREVER_US);

	print_uart("Hello! I'm your echo bot.\r\n");
	print_uart("Type 31 characters.\r\n");
	print_uart("[Tip] echo -n [...] > /dev/ttyUSB0\r\n");
#else
	/* configure interrupt and callback to receive data */
	ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return 0;
	}
	uart_irq_rx_enable(uart_dev);

	print_uart("Hello! I'm your echo bot.\r\n");
	print_uart("Tell me something and press enter:\r\n");
#endif /* CONFIG_UART_CC23X0_DMA_DRIVEN */

	/* indefinitely wait for input from the user */
	while (k_msgq_get(&uart_msgq, &tx_buf, K_FOREVER) == 0) {
		print_uart("Echo: ");
		print_uart(tx_buf);
		print_uart("\r\n");
	}
	return 0;
}
