/*
 * 
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>

#define I2C_EXP_ADDR 0x76
#define I2C_EXP_REG  0x02
#define I2C_EXP_BUF_SIZE 16

int main(void)
{
	const struct device*const i2c_bus = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	uint8_t i2c_buf_tx[3] = { I2C_EXP_REG, 0xF3 };
//	uint8_t i2c_buf_rx[3] = { };
	int ret;

	printk("---------- TEST_I2C bme280 ----------\n");

	while (1) {
		printk("Single write [0x76][0xD0]\n");
		ret = i2c_write(i2c_bus, i2c_buf_tx, 2, I2C_EXP_ADDR);
		if (ret) {
			printk("Error: failed to write I2C register (%d)\n", ret);
			//return ret;
		}
/*
		ret = i2c_read(i2c_bus, i2c_buf_rx, 3, I2C_EXP_ADDR);
		printk("Single read[%x][%x]\n", i2c_buf_rx[0], i2c_buf_rx[1]);
		if (ret) {
			printk("Error: failed to read I2C register (%d)\n", ret);
			return ret;
		}
*/
		k_msleep(3000);
	}

	return 0;
}