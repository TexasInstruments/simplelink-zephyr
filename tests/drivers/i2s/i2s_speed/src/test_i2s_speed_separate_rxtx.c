/*
 * Copyright (c) 2017 comsuisse AG
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "i2s_speed_test.h"

#if !IS_ENABLED(CONFIG_I2S_TEST_USE_I2S_DIR_BOTH)

/** @brief Short I2S transfer.
 *
 * - TX stream START trigger starts transmission.
 * - RX stream START trigger starts reception.
 * - sending / receiving a short sequence of data returns success.
 * - TX stream DRAIN trigger empties the transmit queue.
 * - RX stream STOP trigger stops reception.
 */
ZTEST(drivers_i2s_speed, test_i2s_transfer_short)
{
	if (IS_ENABLED(CONFIG_I2S_TEST_USE_I2S_DIR_BOTH)) {
		TC_PRINT("RX/TX transfer requires use of I2S_DIR_BOTH.\n");
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

		ret = i2s_write(dev_i2s_tx, tx_block, BLOCK_SIZE);
		zassert_equal(ret, 0);

		TC_PRINT("%d->OK\n", i);
	}

	/* Start reception */
	ret = i2s_trigger(dev_i2s_rx, I2S_DIR_RX, I2S_TRIGGER_START);
	zassert_equal(ret, 0, "RX START trigger failed");

	/* Start transmission */
	ret = i2s_trigger(dev_i2s_tx, I2S_DIR_TX, I2S_TRIGGER_START);
	zassert_equal(ret, 0, "TX START trigger failed");

	/* All data written, drain TX queue and stop the transmission */
	ret = i2s_trigger(dev_i2s_tx, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
	zassert_equal(ret, 0, "TX DRAIN trigger failed");

	ret = i2s_read(dev_i2s_rx, &rx_block[0], &rx_size);
	zassert_equal(ret, 0);
	zassert_equal(rx_size, BLOCK_SIZE);

	ret = i2s_read(dev_i2s_rx, &rx_block[1], &rx_size);
	zassert_equal(ret, 0);
	zassert_equal(rx_size, BLOCK_SIZE);

	/* All but one data block read, stop reception */
	ret = i2s_trigger(dev_i2s_rx, I2S_DIR_RX, I2S_TRIGGER_STOP);
	zassert_equal(ret, 0, "RX STOP trigger failed");

	ret = i2s_read(dev_i2s_rx, &rx_block[2], &rx_size);
	zassert_equal(ret, 0);
	zassert_equal(rx_size, BLOCK_SIZE);

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

/** @brief Long I2S transfer.
 *
 * - TX stream START trigger starts transmission.
 * - RX stream START trigger starts reception.
 * - sending / receiving a long sequence of data returns success.
 * - TX stream DRAIN trigger empties the transmit queue.
 * - RX stream STOP trigger stops reception.
 */
ZTEST(drivers_i2s_speed, test_i2s_transfer_long)
{
	if (IS_ENABLED(CONFIG_I2S_TEST_USE_I2S_DIR_BOTH)) {
		TC_PRINT("RX/TX transfer requires use of I2S_DIR_BOTH.\n");
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
	ret = i2s_write(dev_i2s_tx, tx_block[tx_idx++], BLOCK_SIZE);
	zassert_equal(ret, 0);

	ret = i2s_write(dev_i2s_tx, tx_block[tx_idx++], BLOCK_SIZE);
	zassert_equal(ret, 0);

	/* Start reception */
	ret = i2s_trigger(dev_i2s_rx, I2S_DIR_RX, I2S_TRIGGER_START);
	zassert_equal(ret, 0, "RX START trigger failed");

	/* Start transmission */
	ret = i2s_trigger(dev_i2s_tx, I2S_DIR_TX, I2S_TRIGGER_START);
	zassert_equal(ret, 0, "TX START trigger failed");

	for (; tx_idx < NUM_BLOCKS; ) {
		ret = i2s_write(dev_i2s_tx, tx_block[tx_idx++], BLOCK_SIZE);
		zassert_equal(ret, 0);

		ret = i2s_read(dev_i2s_rx, &rx_block[rx_idx++], &rx_size);
		zassert_equal(ret, 0);
		zassert_equal(rx_size, BLOCK_SIZE);
	}

	/* All data written, flush TX queue and stop the transmission */
	ret = i2s_trigger(dev_i2s_tx, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
	zassert_equal(ret, 0, "TX DRAIN trigger failed");

	ret = i2s_read(dev_i2s_rx, &rx_block[rx_idx++], &rx_size);
	zassert_equal(ret, 0);
	zassert_equal(rx_size, BLOCK_SIZE);

	/* All but one data block read, stop reception */
	ret = i2s_trigger(dev_i2s_rx, I2S_DIR_RX, I2S_TRIGGER_STOP);
	zassert_equal(ret, 0, "RX STOP trigger failed");

	ret = i2s_read(dev_i2s_rx, &rx_block[rx_idx++], &rx_size);
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
#endif /* !IS_ENABLED(CONFIG_I2S_TEST_USE_I2S_DIR_BOTH) */
