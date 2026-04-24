/*
 * Copyright (c) 2025 Conclusive Engineering Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zephyr/sys/byteorder.h"
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/ztest.h>

#define LM75_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(lm75)
#define LM75_ADDRESS DT_REG_ADDR(LM75_NODE)
#define LM75_TEMP_REG 0
#define I2C_TEST_NODE DT_PARENT(LM75_NODE)
static const struct device *const i2c_device = DEVICE_DT_GET(I2C_TEST_NODE);

ZTEST(i2c_lm75, test_read_temp)
{
	uint8_t buf[2];
	int8_t temp;
	int ret;

	zassert_true(device_is_ready(i2c_device), "I2C not ready");

	ret = i2c_burst_read(i2c_device, LM75_ADDRESS, LM75_TEMP_REG, buf, sizeof(buf));
	TC_PRINT("Test read: Master: %s, address: 0x%x Value 0x%02x\n",
			i2c_device->name, LM75_ADDRESS, buf[0]);
	zassert_true(ret == 0, "Error reading from i2c bus");

	temp = buf[0];
	/* Check if temp is within reasonable value */
	zassert_true(temp > 0 && temp < 50, "Unexpected temp value, i2c may be malfunctioning");
}

ZTEST_SUITE(i2c_lm75, NULL, NULL, NULL, NULL, NULL);
