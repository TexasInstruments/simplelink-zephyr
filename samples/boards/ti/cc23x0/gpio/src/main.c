/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

#define SLEEP_TIME_MS	1

/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define GPIO_NODE DT_NODELABEL(gpio0)

#if !DT_NODE_HAS_STATUS(GPIO_NODE, okay)
#error "Unsupported board: gpio0 devicetree alias is not defined"
#endif

int booster_pack_gpios[] = {	18, 3, 24,
				0, 9, 10, 12, 13,
				19, 11, 23, 25, 1,
				2, 5, 7, 8, 21,
				6, 14, 15};

const struct device *dev = DEVICE_DT_GET(GPIO_NODE);

#define CHECK_MODE(_flags, _x) #_x, _flags&_x?"YES":"NO"

void print_gpio(uint32_t pin, uint32_t flags)
{
	printk("PIN [%d]\n", pin);
	printk(" %20.20s\t[%3.3s]\n", CHECK_MODE(flags, GPIO_INPUT));
	printk(" %20.20s\t[%3.3s]\n", CHECK_MODE(flags, GPIO_OUTPUT));
	printk(" %20.20s\t[%3.3s]\n", CHECK_MODE(flags, GPIO_OUTPUT_INIT_HIGH));
	printk(" %20.20s\t[%3.3s]\n", CHECK_MODE(flags, GPIO_OUTPUT_INIT_LOW));
	printk(" %20.20s\t[%3.3s]\n", CHECK_MODE(flags, GPIO_PULL_UP));
	printk(" %20.20s\t[%3.3s]\n", CHECK_MODE(flags, GPIO_PULL_DOWN));
	printk(" %20.20s\t[%3.3s]\n", CHECK_MODE(flags, GPIO_INT_EDGE_RISING));
	printk(" %20.20s\t[%3.3s]\n", CHECK_MODE(flags, GPIO_INT_EDGE_FALLING));
	printk(" %20.20s\t[%3.3s]\n", CHECK_MODE(flags, GPIO_INT_DISABLE));
	printk(" %20.20s\t[%3.3s]\n", CHECK_MODE(flags, GPIO_OPEN_SOURCE));
}

int main(void)
{
	int ret;
	uint32_t flags = 0;

	printk("SETUP ALL INPUT\n");
	for (int i = 0 ; i < (sizeof(booster_pack_gpios) / sizeof(int)) ; i++) {
		ret = gpio_pin_configure(dev, booster_pack_gpios[i], GPIO_INPUT);
		if (ret != 0) {
			printk("Error %d: failed to configure %d\n", ret, booster_pack_gpios[i]);
			return 0;
		} else {
			printk("PIN %d: configured ret=%d\n", booster_pack_gpios[i], ret);
			ret = gpio_pin_get_config(dev, booster_pack_gpios[i], &flags);
			if (ret != 0) {
				printk("Error %d: failed to configure %d\n", ret,
					booster_pack_gpios[i]);
				return 0;
			} else {
				print_gpio(booster_pack_gpios[i], flags);
			}
		}
	}

	printk("SETUP ALL OUTPUT\n");
	for (int i = 0 ; i < (sizeof(booster_pack_gpios) / sizeof(int)) ; i++) {
		ret = gpio_pin_configure(dev, booster_pack_gpios[i], GPIO_OUTPUT);
		if (ret != 0) {
			printk("Error %d: failed to configure %d\n", ret, booster_pack_gpios[i]);
			return 0;
		} else {
			printk("PIN %d: configured ret=%d\n", booster_pack_gpios[i], ret);
			ret = gpio_pin_get_config(dev, booster_pack_gpios[i], &flags);
			if (ret != 0) {
				printk("Error %d: failed to configure %d\n", ret,
					booster_pack_gpios[i]);
				return 0;
			} else {
				print_gpio(booster_pack_gpios[i], flags);
			}
		}
	}

	printk("DONE\n");

	return 0;
}
