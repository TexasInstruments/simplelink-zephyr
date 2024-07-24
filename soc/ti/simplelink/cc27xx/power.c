/*
 * Copyright (c) 2025 Texas Instruments Incorporated
 * Copyright (c) 2024 Baylibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <string.h>

#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>

#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC27XX.h>

#include <driverlib/pmctl.h>
#include <driverlib/ckmd.h>

static int power_initialize(void)
{
	unsigned int ret;

	ret = irq_lock();

	/* Set non-default cap array trims */
	CKMDSetInitialCapTrim(33, 33);
	CKMDSetTargetCapTrim(33, 33);

	Power_init();

	if (DT_HAS_COMPAT_STATUS_OKAY(ti_cc27xx_lf_xosc)) {
		PowerLPF3_selectLFXT();
	}

	PMCTLSetVoltageRegulator(PMCTL_VOLTAGE_REGULATOR_DCDC);

	irq_unlock(ret);

	return 0;
}

SYS_INIT(power_initialize, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
