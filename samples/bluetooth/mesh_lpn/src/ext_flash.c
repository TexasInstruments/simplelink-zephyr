/*
 * Copyright (c) 2020, Texas Instruments Incorporated
 * Copyright (c) 2020 Linaro Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define GPIO_PORT DT_NODELABEL(gpio0)

#define BOARD_EXT_FLASH_SPI_CLK DT_PROP_BY_IDX(DT_NODELABEL(spi0_sck_default), pinmux, 0)
#define BOARD_EXT_FLASH_SPI_PICO DT_PROP_BY_IDX(DT_NODELABEL(spi0_mosi_default), pinmux, 0)
#define BOARD_EXT_FLASH_SPI_POCI DT_PROP_BY_IDX(DT_NODELABEL(spi0_miso_default), pinmux, 0)
#define BOARD_EXT_FLASH_SPI_CS DT_PROP_BY_IDX(DT_NODELABEL(spi0_flash_cs), pinmux, 0)

/*
 *  ======== CC23xx_sendExtFlashByte ========
 */
void CC23xx_sendExtFlashByte(const struct device *dev,
					uint8_t byte)
{
	uint8_t i;

	/* SPI Flash CS */
	gpio_pin_set(dev, BOARD_EXT_FLASH_SPI_CS, 0);

	for (i = 0; i < 8; i++) {
		gpio_pin_set(dev, BOARD_EXT_FLASH_SPI_CLK, 0); /* SPI Flash CLK */

		/* SPI Flash MOSI */
		gpio_pin_set(dev, BOARD_EXT_FLASH_SPI_PICO, (byte >> (7 - i)) & 0x01);
		gpio_pin_set(dev, BOARD_EXT_FLASH_SPI_CLK, 1); /* SPI Flash CLK */

		k_busy_wait(1);
	}

	gpio_pin_set(dev, BOARD_EXT_FLASH_SPI_CLK, 0);   /* CLK */
	gpio_pin_set(dev, BOARD_EXT_FLASH_SPI_CS, 1);   /* CS */

	/*
	 * Keep CS high at least 40 us
	 * 3 cycles per loop: 700 loops @ 48 Mhz ~= 44 us
	 */
	k_busy_wait(44);
}

/*
 *  ======== CC23xx_wakeUpExtFlash ========
 */
void CC23xx_wakeUpExtFlash(const struct device *dev)
{
	/*
	 *  To wake up we need to toggle the chip select at
	 *  least 20 ns and ten wait at least 35 us.
	 */

	/* Toggle chip select for ~20ns to wake ext. flash */
	gpio_pin_set(dev, BOARD_EXT_FLASH_SPI_CS, 0);
	k_busy_wait(1);
	gpio_pin_set(dev, BOARD_EXT_FLASH_SPI_CS, 1);
	k_busy_wait(35);
}

/*
 *  ======== CC23xx_shutDownExtFlash ========
 */
void CC23xx_shutDownExtFlash(void)
{
	const struct device *dev;
	uint8_t extFlashShutdown = 0xB9;

	dev = DEVICE_DT_GET(GPIO_PORT);

	if (!device_is_ready(dev)) {
		printk("%s: device not ready.\n", dev->name);
		return;
	}

	/* Set SPI Flash CS pin as output */
	gpio_pin_configure(dev, BOARD_EXT_FLASH_SPI_CS, GPIO_OUTPUT);
	/* Set SPI Flash CLK pin as output */
	gpio_pin_configure(dev, BOARD_EXT_FLASH_SPI_CLK, GPIO_OUTPUT);
	/* Set SPI Flash MOSI pin as output */
	gpio_pin_configure(dev, BOARD_EXT_FLASH_SPI_PICO, GPIO_OUTPUT);
	/* Set SPI Flash MISO pin as input */
	gpio_pin_configure(dev, BOARD_EXT_FLASH_SPI_POCI, GPIO_INPUT | GPIO_PULL_DOWN);

	/*
	 *  To be sure we are putting the flash into sleep and not waking it,
	 *  we first have to make a wake up call
	 */
	CC23xx_wakeUpExtFlash(dev);

	CC23xx_sendExtFlashByte(dev, extFlashShutdown);
}
