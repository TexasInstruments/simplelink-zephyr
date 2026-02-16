/*
 * Copyright (c) 2026 Conclusive Engineering Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#if defined(CONFIG_SENSOR)
#include <zephyr/drivers/sensor.h>
#endif
#if defined(CONFIG_SPI)
#include <zephyr/drivers/spi.h>
#endif
#include <zephyr/sys/printk.h>

#define LED_NODE DT_NODELABEL(red_led)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

#if defined(CONFIG_SENSOR)
static const struct device *const tmp1075 =
	DEVICE_DT_GET(DT_NODELABEL(tmp1075_temperature));
#endif

#if defined(CONFIG_SPI)
#define SPI_NODE DT_NODELABEL(spi0)
static const struct device *const spi_dev = DEVICE_DT_GET(SPI_NODE);

static const struct spi_config spi_cfg = {
	.frequency = 1000000,
	.operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
	.slave = 0,
};
#endif

int main(void)
{
#if defined(CONFIG_SPI)
	uint8_t tx_buf[4] = { 0xAA, 0x55, 0xDE, 0xAD };
	struct spi_buf tx_spi_buf = { .buf = tx_buf, .len = sizeof(tx_buf) };
	struct spi_buf_set tx_spi_set = { .buffers = &tx_spi_buf, .count = 1 };
#endif

	if (!gpio_is_ready_dt(&led)) {
		printk("pm sample: red LED not ready\n");
		return 0;
	}

	if (gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE) < 0) {
		printk("pm sample: red LED configure failed\n");
		return 0;
	}

	printk("pm sample: boot\n");

	int tick = 0;

	while (1) {
#if defined(CONFIG_SENSOR)
		struct sensor_value temp = { 0 };
#endif
		int rc;

		gpio_pin_set_dt(&led, 1);
		printk("pm sample: tick %d (pre-sleep)\n", tick++);
#if defined(CONFIG_SPI)
		if (device_is_ready(spi_dev)) {
			rc = spi_write(spi_dev, &spi_cfg, &tx_spi_set);
			printk("pm sample: spi rc=%d\n", rc);
		}
#endif
#if defined(CONFIG_SENSOR)
		if (device_is_ready(tmp1075)) {
			rc = sensor_sample_fetch(tmp1075);
			if (rc == 0) {
				rc = sensor_channel_get(tmp1075, SENSOR_CHAN_AMBIENT_TEMP, &temp);
			}
			if (rc == 0) {
				printk("pm sample: tmp1075 %d.%06d C\n",
				       temp.val1, temp.val2 < 0 ? -temp.val2 : temp.val2);
			} else {
				printk("pm sample: tmp1075 rc=%d\n", rc);
			}
		}
#endif
		k_busy_wait(1000);
		gpio_pin_set_dt(&led, 0);
		printk("pm sample: sleep enter\n");
		k_msleep(5000);
		printk("pm sample: sleep exit\n");
	}
	return 0;
}
