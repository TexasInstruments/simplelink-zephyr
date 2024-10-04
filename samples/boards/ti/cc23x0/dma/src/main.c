/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/dma.h>

#define LOG_LEVEL CONFIG_DMA_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#define DMA_NODE DT_NODELABEL(dma)
const struct device *dev = DEVICE_DT_GET(DMA_NODE);

#define DMA_MEM2MEM_CH 6

#define NB_DATA  1024
#define NB_PRINT 8

#define SLEEP_TIME_MS 20

static void dma_cb(const struct device *dev, void *user_data, uint32_t channel, int status);

uint32_t src_data[NB_DATA];
uint32_t dst_data[NB_DATA];

static struct dma_block_config block_cfg = {
	.source_address = (uint32_t)src_data,
	.dest_address = (uint32_t)dst_data,
	.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
	.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
	.block_size = NB_DATA * sizeof(*src_data),
};

static struct dma_config dma_cfg = {
	.dma_slot = 0,
	.channel_direction = MEMORY_TO_MEMORY,
	.block_count = 1,
	.head_block = &block_cfg,
	.source_data_size = sizeof(*src_data),
	.dest_data_size = sizeof(*dst_data),
	.source_burst_length = 8 * sizeof(*src_data),
	.dma_callback = dma_cb,
	.user_data = &block_cfg,
};

static void dma_cb(const struct device *dev, void *user_data, uint32_t channel, int status)
{
	struct dma_block_config *block_cfg = user_data;
	int i;

	LOG_INF("On ch %d, %08x to %08x (%u bytes)",
		channel,
		block_cfg->source_address,
		block_cfg->dest_address,
		block_cfg->block_size);

	printk("src_data = ");
	for (i = 0 ; i < NB_PRINT ; i++)
		printk("%d ", src_data[i]);
	printk("[...] ");
	printk("%d\n", src_data[NB_DATA - 1]);

	printk("dst_data = ");
	for (i = 0 ; i < NB_PRINT ; i++)
		printk("%d ", dst_data[i]);
	printk("[...] ");
	printk("%d\n", dst_data[NB_DATA - 1]);
}

int main(void)
{
	struct dma_status stat = {0};
	int ret;
	int i;

	LOG_INF("-------------------- DMA TEST 1 --------------------");

	for (i = 0 ; i < NB_DATA ; i++)
		src_data[i] = i;

	ret = dma_config(dev, DMA_MEM2MEM_CH, &dma_cfg);
	if (ret)
		return ret;

	dma_get_status(dev, DMA_MEM2MEM_CH, &stat);
	LOG_INF("dir = %d, busy = %d, pending = %d", stat.dir, stat.busy, stat.pending_length);

	dma_start(dev, DMA_MEM2MEM_CH);

	do {
		dma_get_status(dev, DMA_MEM2MEM_CH, &stat);
		LOG_INF("busy = %d, pending = %d", stat.busy, stat.pending_length);
	} while (stat.busy);

	ret = memcmp(src_data, dst_data, block_cfg.block_size);
	if (!ret)
		LOG_INF("TEST OK");
	else
		LOG_ERR("TEST NOK ! (%d)", ret);

	k_msleep(SLEEP_TIME_MS);

	LOG_INF("-------------------- DMA TEST 2 --------------------");

	for (i = 0 ; i < NB_DATA ; i++)
		src_data[i] = i + 1;

	ret = dma_reload(dev, DMA_MEM2MEM_CH, (uint32_t)src_data, (uint32_t)dst_data, block_cfg.block_size);
	if (ret)
		goto error;

	dma_get_status(dev, DMA_MEM2MEM_CH, &stat);
	LOG_INF("dir = %d, busy = %d, pending = %d", stat.dir, stat.busy, stat.pending_length);

	dma_start(dev, DMA_MEM2MEM_CH);

	do {
		dma_get_status(dev, DMA_MEM2MEM_CH, &stat);
		LOG_INF("busy = %d, pending = %d", stat.busy, stat.pending_length);
	} while (stat.busy);

	ret = memcmp(src_data, dst_data, block_cfg.block_size);
	if (!ret)
		LOG_INF("TEST OK");
	else
		LOG_ERR("TEST NOK ! (%d)", ret);

error:
	dma_stop(dev, DMA_MEM2MEM_CH);

	return ret;
}
