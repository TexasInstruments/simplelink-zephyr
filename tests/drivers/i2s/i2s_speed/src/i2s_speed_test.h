/*
 * Copyright (c) 2017 comsuisse AG
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _I2S_SPEED_TEST_H
#define _I2S_SPEED_TEST_H

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/drivers/i2s.h>

#define I2S_DEV_NODE_RX DT_ALIAS(i2s_node0)
#ifdef CONFIG_I2S_TEST_SEPARATE_DEVICES
#define I2S_DEV_NODE_TX DT_ALIAS(i2s_node1)
#else
#define I2S_DEV_NODE_TX DT_ALIAS(i2s_node0)
#endif

#define NUM_BLOCKS 20
#define SAMPLE_NO 64
#define BLOCK_SIZE (2 * sizeof(int16_t) * SAMPLE_NO)

#define TIMEOUT          2000
#define FRAME_CLK_FREQ   44000

/* Global device pointers */
extern const struct device *dev_i2s_rx;
extern const struct device *dev_i2s_tx;
extern const struct device *dev_i2s_rxtx;
extern bool dir_both_supported;

/* Memory slabs */
extern struct k_mem_slab rx_0_mem_slab;
extern struct k_mem_slab tx_0_mem_slab;

/* Test data */
extern int16_t data_l[SAMPLE_NO];
extern int16_t data_r[SAMPLE_NO];

/* Common functions */
void fill_buf(int16_t *tx_block, int att);
int verify_buf(int16_t *rx_block, int att);
int configure_stream(const struct device *dev_i2s, enum i2s_dir dir);

#endif /* _I2S_SPEED_TEST_H */
