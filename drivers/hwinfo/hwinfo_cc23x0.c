/*
 * Copyright (c) 2025 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/byteorder.h>

#include <ti/drivers/Power.h>

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_fcfg.h)
#include DeviceFamily_constructPath(inc/hw_sys0.h)
#include DeviceFamily_constructPath(inc/hw_fcfg.h)
#include DeviceFamily_constructPath(inc/hw_memmap.h)

/* Number of bytes in device ID */
#define CC23X0_DEVID_SIZE 16
#define EUI64_SIZE 8

int z_impl_hwinfo_get_reset_cause(uint32_t *cause)
{
	uint32_t reset_src;

	reset_src = PowerLPF3_getResetReason();

	switch (reset_src) {
	case PMCTL_RESET_POR:
		*cause = RESET_POR;
		break;
	case PMCTL_RESET_PIN:
		*cause = RESET_PIN;
		break;
	case PMCTL_RESET_VDDS:
	case PMCTL_RESET_VDDR:
		*cause = RESET_BROWNOUT;
		break;
	case PMCTL_RESET_LFXT:
		*cause = RESET_CLOCK;
		break;
	case PMCTL_RESET_SYSTEM:
		*cause = RESET_SOFTWARE;
		break;
	case PMCTL_RESET_TSD:
		*cause = RESET_TEMPERATURE;
		break;
	case PMCTL_RESET_WATCHDOG:
		*cause = RESET_WATCHDOG;
		break;
	case PMCTL_RESET_LOCKUP:
		*cause = RESET_CPU_LOCKUP;
		break;
	case PMCTL_RESET_SWD:
		*cause = RESET_DEBUG;
		break;
	}

	return 0;
}

int z_impl_hwinfo_get_supported_reset_cause(uint32_t *supported)
{
	*supported = (RESET_POR
			  | RESET_PIN
		      | RESET_BROWNOUT
		      | RESET_CLOCK
		      | RESET_SOFTWARE
		      | RESET_TEMPERATURE
			  | RESET_WATCHDOG
			  | RESET_CPU_LOCKUP
			  | RESET_DEBUG);

	return 0;
}

int z_impl_hwinfo_clear_reset_cause(void)
{
	return -ENOSYS;
}

ssize_t z_impl_hwinfo_get_device_id(uint8_t *buffer, size_t length)
{
	if (length > CC23X0_DEVID_SIZE) {
		length = CC23X0_DEVID_SIZE;
	}

	memcpy(buffer, &(fcfg->deviceInfo.dieId), length);

	return length;
}

/* The 64 bit mac address in fcfg is the EUI64 */
int z_impl_hwinfo_get_device_eui64(uint8_t *buffer)
{

	memcpy(buffer, &(fcfg->deviceInfo.macAddr), EUI64_SIZE);

	return 0;
}
