/*
 * Copyright (c) 2017 comsuisse AG
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "i2s_speed_test.h"

/** @brief Short I2S transfer using I2S_DIR_BOTH.
 *
 * - START trigger starts both the transmission and reception.
 * - Sending / receiving a short sequence of data returns success.
 * - DRAIN trigger empties the transmit queue and stops both streams.
 */
ZTEST(drivers_i2s_speed_both_rxtx, test_i2s_dir_both_transfer_short)
{
	if (!dir_both_supported) {
		TC_PRINT("I2S_DIR_BOTH value is not supported.\n");
		ztest_test_skip();
		return;
	}

	void *rx_block[3];
	void *tx_block;
	size_t rx_size;
	int ret;

	/* Prefill TX queue */
	for (int i = 0; i < 3; i++) {
		ret = k_mem_slab_alloc(&tx_0_mem_slab, &tx_block, K_FOREVER);
		zassert_equal(ret, 0);
		fill_buf((uint16_t *)tx_block, i);

		ret = i2s_write(dev_i2s_rxtx, tx_block, BLOCK_SIZE);
		zassert_equal(ret, 0);

		TC_PRINT("%d->OK\n", i);
	}

	ret = i2s_trigger(dev_i2s_rxtx, I2S_DIR_BOTH, I2S_TRIGGER_START);
	zassert_equal(ret, 0, "RX/TX START trigger failed\n");

#ifdef CONFIG_I2S_TI_CC27XX
	/* CC27XX: Read at least 1 buffer before DRAIN to ensure synchronization.
	 * DRAIN stops RX after current block, not after all queued TX blocks.
	 */
	ret = i2s_read(dev_i2s_rxtx, &rx_block[0], &rx_size);
	zassert_equal(ret, 0);
	zassert_equal(rx_size, BLOCK_SIZE);
#endif

	/* All data written, drain TX queue and stop both streams. */
	ret = i2s_trigger(dev_i2s_rxtx, I2S_DIR_BOTH, I2S_TRIGGER_DRAIN);
	zassert_equal(ret, 0, "RX/TX DRAIN trigger failed");

#ifdef CONFIG_I2S_TI_CC27XX
	/* CC27XX: Read remaining 2 buffers after DRAIN */
	ret = i2s_read(dev_i2s_rxtx, &rx_block[1], &rx_size);
	zassert_equal(ret, 0);
	zassert_equal(rx_size, BLOCK_SIZE);

	ret = i2s_read(dev_i2s_rxtx, &rx_block[2], &rx_size);
	zassert_equal(ret, 0);
	zassert_equal(rx_size, BLOCK_SIZE);
#else
	/* Standard flow: Read all 3 buffers after DRAIN */
	ret = i2s_read(dev_i2s_rxtx, &rx_block[0], &rx_size);
	zassert_equal(ret, 0);
	zassert_equal(rx_size, BLOCK_SIZE);

	ret = i2s_read(dev_i2s_rxtx, &rx_block[1], &rx_size);
	zassert_equal(ret, 0);
	zassert_equal(rx_size, BLOCK_SIZE);

	ret = i2s_read(dev_i2s_rxtx, &rx_block[2], &rx_size);
	zassert_equal(ret, 0);
	zassert_equal(rx_size, BLOCK_SIZE);
#endif

	/* Verify received data */
	ret = verify_buf((uint16_t *)rx_block[0], 0);
	zassert_equal(ret, 0);
	k_mem_slab_free(&rx_0_mem_slab, rx_block[0]);
	TC_PRINT("%d<-OK\n", 1);

	ret = verify_buf((uint16_t *)rx_block[1], 1);
	zassert_equal(ret, 0);
	k_mem_slab_free(&rx_0_mem_slab, rx_block[1]);
	TC_PRINT("%d<-OK\n", 2);

	ret = verify_buf((uint16_t *)rx_block[2], 2);
	zassert_equal(ret, 0);
	k_mem_slab_free(&rx_0_mem_slab, rx_block[2]);
	TC_PRINT("%d<-OK\n", 3);
}

/** @brief Long I2S transfer using I2S_DIR_BOTH.
 *
 * - START trigger starts both the transmission and reception.
 * - Sending / receiving a long sequence of data returns success.
 * - DRAIN trigger empties the transmit queue and stops both streams.
 */
ZTEST(drivers_i2s_speed_both_rxtx, test_i2s_dir_both_transfer_long)
{
	if (!dir_both_supported) {
		TC_PRINT("I2S_DIR_BOTH value is not supported.\n");
		ztest_test_skip();
		return;
	}

	void *rx_block[NUM_BLOCKS];
	void *tx_block[NUM_BLOCKS];
	size_t rx_size;
	int tx_idx;
	int rx_idx = 0;
	int num_verified;
	int ret;

	/* Prepare TX data blocks */
	for (tx_idx = 0; tx_idx < NUM_BLOCKS; tx_idx++) {
		ret = k_mem_slab_alloc(&tx_0_mem_slab, &tx_block[tx_idx],
				       K_FOREVER);
		zassert_equal(ret, 0);
		fill_buf((uint16_t *)tx_block[tx_idx], tx_idx % 3);
	}

	tx_idx = 0;

	/* Prefill TX queue */
	ret = i2s_write(dev_i2s_rxtx, tx_block[tx_idx++], BLOCK_SIZE);
	zassert_equal(ret, 0);

	ret = i2s_write(dev_i2s_rxtx, tx_block[tx_idx++], BLOCK_SIZE);
	zassert_equal(ret, 0);

	ret = i2s_trigger(dev_i2s_rxtx, I2S_DIR_BOTH, I2S_TRIGGER_START);
	zassert_equal(ret, 0, "RX/TX START trigger failed\n");

	for (; tx_idx < NUM_BLOCKS; ) {
		ret = i2s_write(dev_i2s_rxtx, tx_block[tx_idx++], BLOCK_SIZE);
		zassert_equal(ret, 0);

		ret = i2s_read(dev_i2s_rxtx, &rx_block[rx_idx++], &rx_size);
		zassert_equal(ret, 0);
		zassert_equal(rx_size, BLOCK_SIZE);
	}

	/* All data written, drain TX queue and stop both streams. */
	ret = i2s_trigger(dev_i2s_rxtx, I2S_DIR_BOTH, I2S_TRIGGER_DRAIN);
	zassert_equal(ret, 0, "RX/TX DRAIN trigger failed");

	ret = i2s_read(dev_i2s_rxtx, &rx_block[rx_idx++], &rx_size);
	zassert_equal(ret, 0);
	zassert_equal(rx_size, BLOCK_SIZE);

	ret = i2s_read(dev_i2s_rxtx, &rx_block[rx_idx++], &rx_size);
	zassert_equal(ret, 0);
	zassert_equal(rx_size, BLOCK_SIZE);

	TC_PRINT("%d TX blocks sent\n", tx_idx);
	TC_PRINT("%d RX blocks received\n", rx_idx);

	/* Verify received data */
	num_verified = 0;
	for (rx_idx = 0; rx_idx < NUM_BLOCKS; rx_idx++) {
		ret = verify_buf((uint16_t *)rx_block[rx_idx], rx_idx % 3);
		if (ret != 0) {
			TC_PRINT("%d RX block invalid\n", rx_idx);
		} else {
			num_verified++;
		}
		k_mem_slab_free(&rx_0_mem_slab, rx_block[rx_idx]);
	}
	zassert_equal(num_verified, NUM_BLOCKS, "Invalid RX blocks received");
}
