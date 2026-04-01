/*
 * Copyright (c) 2017 comsuisse AG
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "i2s_speed_test.h"

#if !IS_ENABLED(CONFIG_I2S_TEST_USE_I2S_DIR_BOTH)
static void *test_i2s_speed_setup(void)
{
	/* Configure I2S TX transfer. */
	int ret;

	dev_i2s_tx = DEVICE_DT_GET_OR_NULL(I2S_DEV_NODE_TX);
	zassert_not_null(dev_i2s_tx, "transfer device not found");
	zassert(device_is_ready(dev_i2s_tx), "transfer device not ready");

	ret = configure_stream(dev_i2s_tx, I2S_DIR_TX);
	zassert_equal(ret, TC_PASS);

	/* Configure I2S RX transfer. */
	dev_i2s_rx = DEVICE_DT_GET_OR_NULL(I2S_DEV_NODE_RX);
	zassert_not_null(dev_i2s_rx, "receive device not found");
	zassert(device_is_ready(dev_i2s_rx), "receive device not ready");

	ret = configure_stream(dev_i2s_rx, I2S_DIR_RX);
	zassert_equal(ret, TC_PASS);

	return NULL;
}
#endif /* !IS_ENABLED(CONFIG_I2S_TEST_USE_I2S_DIR_BOTH) */

static void *test_i2s_speed_rxtx_setup(void)
{
	int ret;

	/* Configure I2S Dir Both transfer. */
	dev_i2s_rxtx = DEVICE_DT_GET_OR_NULL(I2S_DEV_NODE_RX);
	zassert_not_null(dev_i2s_rxtx, "receive device not found");
	zassert(device_is_ready(dev_i2s_rxtx), "receive device not ready");

	ret = configure_stream(dev_i2s_rxtx, I2S_DIR_BOTH);
	zassert_equal(ret, TC_PASS);

	/* Check if the tested driver supports the I2S_DIR_BOTH value.
	 * Use the DROP trigger for this, as in the current state of the driver
	 * (READY, both TX and RX queues empty) it is actually a no-op.
	 */
	ret = i2s_trigger(dev_i2s_rxtx, I2S_DIR_BOTH, I2S_TRIGGER_DROP);
	dir_both_supported = (ret == 0);

	if (IS_ENABLED(CONFIG_I2S_TEST_USE_I2S_DIR_BOTH)) {
		zassert_true(dir_both_supported,
			     "I2S_DIR_BOTH value is supposed to be supported.");
	}

	return NULL;
}

#if !IS_ENABLED(CONFIG_I2S_TEST_USE_I2S_DIR_BOTH)
ZTEST_SUITE(drivers_i2s_speed, NULL, test_i2s_speed_setup, NULL, NULL, NULL);
#endif

ZTEST_SUITE(drivers_i2s_speed_both_rxtx, NULL, test_i2s_speed_rxtx_setup, NULL, NULL, NULL);
