/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "at/AtProcess.h"
#include "at/AtTerm.h"
#include "radio/radio.h"

#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
#define LED0_NODE DT_ALIAS(led0)
const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#define HAS_LED0 1
#else
#define HAS_LED0 0
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(led1), okay)
#define LED1_NODE DT_ALIAS(led1)
const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
#define HAS_LED1 1
#else
#define HAS_LED1 0
#endif

int main(void)
{
#if HAS_LED0
	gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
#endif
#if HAS_LED1
	gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
#endif

	AtTerm_init();
	AtTerm_clearTerm();
	AtTerm_sendString("RF Diagnostics Example\r\n");

	Radio_Init();

	while (1) {
		AtProcess_processingLoop();
	}

	return 0;
}
