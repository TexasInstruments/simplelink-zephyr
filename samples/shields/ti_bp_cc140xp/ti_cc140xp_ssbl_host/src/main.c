/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "ssbl_host.h"

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

/* Diagnostic only - not part of the SSBL protocol */
static const struct gpio_dt_spec ssbl_int_gpio =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), int_gpios);

/*
 * Warning - This is only an example key. This should not be used for production devices.
 *
 * This key should only be used for early testing. Its matching private key is
 * publicly known, so it provides no real security guarantee.
 *
 * Open source tools such as OpenSSL can be used to generate your own private/public key
 * pair. The private key of which should be securely maintained with very limited access
 * protocols. The public key from the pair can be swapped in for this example key for
 * continued testing.
 */
static const ssblCustKey_t customerKey = {
	.keyId = { 0xF0, 0x0D, 0xCA, 0xFE, 0xBE, 0xEF, 0xFE, 0xED },
	.key = {{
		/* Uncompressed Prefix */
		0x04,

		/* X Component */
		0xce, 0x7d, 0x73, 0x79, 0xad, 0xb5, 0x64, 0x3a, 0x58, 0x25, 0xe9, 0xd5, 0xd4, 0x81, 0x21, 0x0d,
		0x63, 0x73, 0x87, 0x34, 0xd8, 0x98, 0x35, 0xf1, 0x05, 0x34, 0x6c, 0xf6, 0x30, 0xd6, 0x91, 0x2f,

		/* Y Component */
		0x6c, 0x92, 0xe0, 0x81, 0x4d, 0x05, 0xf1, 0x3a, 0x8d, 0x3e, 0x28, 0x22, 0xcf, 0x9e, 0xe9, 0x29,
		0xfb, 0x3c, 0x0f, 0x38, 0xd7, 0x50, 0x7e, 0x6f, 0x4f, 0x77, 0xa3, 0xe3, 0x15, 0xbb, 0x51, 0x27
	}},
};

/*
 * CC27xx internal flash is memory-mapped, so the partition's DT address can
 * be used directly as a raw pointer.
 */
static uint8_t * const trxFwUpdateBuffer = (uint8_t *)DT_REG_ADDR(DT_NODELABEL(ssbl_fw_partition));

int main(void)
{
#if HAS_LED0
	gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
#endif
#if HAS_LED1
	gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
#endif

	/* Turn on user LED */
#if HAS_LED1
	gpio_pin_set_dt(&led1, 1);
#endif

	/* Diagnostic input with pull-up, read at boot */
	gpio_pin_configure_dt(&ssbl_int_gpio, GPIO_INPUT | GPIO_PULL_UP);
	printk("DIO28 (INT, unused by SSBL) at boot: %d\n", gpio_pin_get_dt(&ssbl_int_gpio));

	printk("Starting the Secure Serial Bootloader(SSBL) Host example\n");
	printk("This example assumes 'trx_fw_update_final.bin' has already been "
	       "programmed at the ssbl-fw-update flash partition, offset 0x%x.\n",
	       (unsigned int)(uintptr_t)trxFwUpdateBuffer);

	/*
	 * This is one example of how your application should setup the ssblDownload_t struct for the
	 * SSBL Host. Your usage may be slightly different. Please configure the ssblDownload_t struct
	 * to fit your use case.
	 *
	 * If optional customer key signature verification is desired then the customer public key must
	 * be programmed to the CC140X device before a FW update can be successful. By running this
	 * example with
	 *     * ( ssblHostDownloadStruct.programKey == true ) and
	 *     * ( ssblHostDownloadStruct.custKey == &customerKey )
	 * the SSBL Host will handle programming the provided customerKey to the CC140X.
	 *
	 * **Warning** - Programming the customer public key is a one time operation. It cannot be
	 * undone and a new/different public key cannot be programmed after the first key is programmed.
	 * Be VERY sure you want to program the customer key before you continue.
	 */
	const ssblDownload_t ssblHostDownloadStruct = {
		.programKey = false, /* See warning above */
		.custKey = NULL, /* &customerKey - see warning above */
		.trxFwUpdateBuffer = trxFwUpdateBuffer, /* Must be word aligned */
		.triggerSsbl = true,
	};
	ARG_UNUSED(customerKey);

	BLDR_STATUS_T status = performSsblDownloadSequence(&ssblHostDownloadStruct);

	if (status != BLDR_CMD_RET_SUCCESS) {
		printk("Download Sequence Failed. Status: 0x%x\n", status);
		while (1) {
			k_sleep(K_FOREVER);
		}
	}

	printk("Download sequence completed successfully :) !!\n");

#if HAS_LED1
	gpio_pin_set_dt(&led1, 0);
#endif
#if HAS_LED0
	gpio_pin_set_dt(&led0, 1);
#endif

	return 0;
}
