/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>

#define SENSOR_ID_REG		0x0f
#define SENSOR_ID_VAL		0x33
#define SENSOR_RD_BIT		BIT(7)
#define SENSOR_AI_BIT		BIT(6)
#define SENSOR_ID_RD_CMD 	(SENSOR_RD_BIT | SENSOR_ID_REG)

#define SPI_BUF_SIZE 2

//static const struct gpio_dt_spec cs = GPIO_DT_SPEC_GET(DT_NODELABEL(spi0), cs_gpios);

int main(void)
{
	const struct device *spi_bus = DEVICE_DT_GET(DT_NODELABEL(spi0));
	struct spi_config spi_cfg = { 0 };
	uint8_t tx_buf[SPI_BUF_SIZE] = { 0 };
	uint8_t rx_buf[SPI_BUF_SIZE] = { 0 };
	const struct spi_buf tx_spi_buf = { tx_buf, SPI_BUF_SIZE };
	const struct spi_buf rx_spi_buf = { rx_buf, SPI_BUF_SIZE };
	const struct spi_buf_set tx_spi_buf_set = { &tx_spi_buf, 1 };
	const struct spi_buf_set rx_spi_buf_set = { &rx_spi_buf, 1 };
	int ret, i;

	printf("---------- TEST_SPI ----------\n");

	spi_cfg.frequency = 1000000;
	spi_cfg.operation = SPI_OP_MODE_MASTER |
			    SPI_MODE_CPOL |
			    SPI_MODE_CPHA |
			    SPI_WORD_SET(8) |
			    SPI_TRANSFER_MSB |
			    SPI_FULL_DUPLEX |
			    SPI_FRAME_FORMAT_MOTOROLA;
	//spi_cfg.cs.gpio = cs;

	tx_buf[0] = SENSOR_ID_RD_CMD;

	while (1) {
		ret = spi_transceive(spi_bus, &spi_cfg, &tx_spi_buf_set, &rx_spi_buf_set);
		if (ret) {
			printf("Error: failed to read SPI register (%d)\n", ret);
			return ret;
		}

		printf("reg = 0x%02x , data = ", SENSOR_ID_REG);
		for (i = 0 ; i < SPI_BUF_SIZE ; i++)
			printf("0x%02x ", rx_buf[i]);
		printf("\n");

		if (rx_buf[0] == SENSOR_ID_VAL)
			printf("SPI slave detected: ID = 0x%02x\n", SENSOR_ID_VAL);

		k_msleep(1000);
	}

	return 0;
}
