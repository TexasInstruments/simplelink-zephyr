/* board.c - Generic HW interaction hooks */

/*
 * Copyright (c) 2021 Nordic Semiconductor
 * Copyright (c) 2025 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/mesh.h>
#include <zephyr/drivers/gpio.h>
#include "board.h"

/* Locate led0 as alias or label by that name */
#if DT_NODE_EXISTS(DT_ALIAS(led0))
#define LED0 DT_ALIAS(led0)
#elif DT_NODE_EXISTS(DT_NODELABEL(led0))
#define LED0 DT_NODELABEL(led0)
#else
#define LED0 DT_INVALID_NODE
#endif

#if DT_NODE_EXISTS(LED0)
#define LED0_DEV DT_PHANDLE(LED0, gpios)
#define LED0_PIN DT_PHA(LED0, gpios, pin)
#define LED0_FLAGS DT_PHA(LED0, gpios, flags)

static const struct device *const led_dev = DEVICE_DT_GET(LED0_DEV);
#endif /* LED0 */

static int led_init(void)
{
#if DT_NODE_EXISTS(LED0)
	int err;

	if (!device_is_ready(led_dev)) {
		return -ENODEV;
	}

	err = gpio_pin_configure(led_dev, LED0_PIN,
				 LED0_FLAGS | GPIO_OUTPUT_INACTIVE);
	if (err) {
		return err;
	}
#else
	printk("WARNING: LEDs not supported on this board.\n");
#endif

	return 0;
}

int board_init(void)
{
	int err;

	err = led_init();
	if (err) {
		return err;
	}

	return 0;
}

void board_led_set(bool val)
{
#if DT_NODE_EXISTS(LED0)
	gpio_pin_set(led_dev, LED0_PIN, val);
#endif
}

void board_output_number(bt_mesh_output_action_t action, uint32_t number)
{
}

void board_prov_complete(void)
{
}
