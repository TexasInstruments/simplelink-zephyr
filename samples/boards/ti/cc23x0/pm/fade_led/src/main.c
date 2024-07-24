/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file Sample app to demonstrate PWM-based LED fade
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>

static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0));

#define NUM_STEPS	50U
#define SLEEP_MSEC	25U

int main(void)
{
	printk("PWM-based LED fade\n");

	if (!pwm_is_ready_dt(&pwm_led0)) {
		printk("Error: PWM device %s is not ready\n",
		       pwm_led0.dev->name);
		return 0;
	}

	while (1) {
		pwm_set_pulse_dt(&pwm_led0, pwm_led0.period);
		k_msleep(1000);
		pwm_set_pulse_dt(&pwm_led0, 0);
		k_msleep(1000);
	}
	return 0;
}
