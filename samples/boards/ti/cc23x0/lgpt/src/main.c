/*
 * Copyright (c) 2023 Baylibre SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/sys/printk.h>

#define DELAY 349000
#define ALARM_CHANNEL_ID 0

struct counter_alarm_cfg alarm_cfg[3];

#define LGPT0 DT_NODELABEL(lgpt0)
#define LGPT1 DT_NODELABEL(lgpt1)
#define LGPT2 DT_NODELABEL(lgpt2)
#define LGPT3 DT_NODELABEL(lgpt3)

static uint32_t cnt = 0;

static void test_counter_interrupt_fn(const struct device *counter_dev,
				      uint8_t chan_id, uint32_t ticks,
				      void *user_data)
{
	/* Limit to 1000 interupts */
	if (cnt > 1000) {
		counter_stop(counter_dev);
		return;
	}

	struct counter_alarm_cfg *config = user_data;
	int err;

	printk("!!! Alarm !!!\n");

	/* Set a new alarm with a double length duration */
	config->ticks = config->ticks * 2U;

	if (config->ticks > 0xffff) {
		config->ticks = 100;
	}

	printk("[%d] chan(%d) Set alarm in %u us (%u ticks)\n", cnt++, chan_id,
	       (uint32_t)(counter_ticks_to_us(counter_dev, config->ticks)),
	       config->ticks);

	err = counter_set_channel_alarm(counter_dev, chan_id, user_data);
	if (err != 0) {
		printk("Alarm could not be set\n");
	}
}

int main(void)
{
	int err;

	uint32_t delay = 100000;

	const struct device *const lgpt_dev[4] = {DEVICE_DT_GET(LGPT0), DEVICE_DT_GET(LGPT1), DEVICE_DT_GET(LGPT2), DEVICE_DT_GET(LGPT3)};

	printk("\n --- Counter sample ---\n");

	if (!device_is_ready(lgpt_dev[0])) {
		printk("LGPT0 not ready.\n");
		return 0;
	}

	if (!device_is_ready(lgpt_dev[1])) {
		printk("LGPT1 not ready.\n");
		return 0;
	}

	if (!device_is_ready(lgpt_dev[2])) {
		printk("LGPT2 not ready.\n");
		return 0;
	}

	if (!device_is_ready(lgpt_dev[3])) {
		printk("LGPT3 not ready.\n");
		return 0;
	}

	printk("\n --- All LGPT timers are ready ---\n");

	// Loop all LGPTs
	for (int l = 0 ; l < 4 ; l++) {
		for (int ch = 0 ; ch < 3 ; ch++) {
			alarm_cfg[ch].flags = 0;
			alarm_cfg[ch].ticks = counter_us_to_ticks(lgpt_dev[l], delay);
			alarm_cfg[ch].callback = test_counter_interrupt_fn;
			alarm_cfg[ch].user_data = &alarm_cfg[ch];

			err = counter_set_channel_alarm(lgpt_dev[l], ch, &alarm_cfg[ch]);

			printk("[LGPT%d]--[%d] Set alarm in %u us (%u ticks)\n", l, ch,
			       (uint32_t)(counter_ticks_to_us(lgpt_dev[l], alarm_cfg[ch].ticks)),
			       alarm_cfg[ch].ticks);

			if (-EINVAL == err) {
				printk("[ERR] Alarm settings invalid\n");
			} else if (-ENOTSUP == err) {
				printk("[ERR] Alarm setting request not supported\n");
			} else if (err != 0) {
				printk("[ERR] Error\n");
			}

			delay += 10000;
		}
	}

	counter_start(lgpt_dev[3]);
	counter_start(lgpt_dev[0]);
	counter_start(lgpt_dev[2]);
	counter_start(lgpt_dev[1]);

	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
