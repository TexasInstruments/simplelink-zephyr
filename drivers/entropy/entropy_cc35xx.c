/*
 * Copyright (c) 2025 Conclusive Engineering Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_cc35xx_hsm

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/entropy.h>
#include <zephyr/irq.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/ring_buffer.h>

#include <inc/hw_memmap.h>
#include <inc/hw_soc_aon.h>
#include <inc/hw_hsm.h>
#include <inc/hw_hsm_non_sec.h>
#include <inc/hw_hsm_sec.h>

#include <third_party/hsmddk/include/Integration/HSMSAL/HSMSAL.h>
#include <third_party/hsmddk/include/Kit/EIP130/DomainHelper/incl/eip130_domain_ecc_curves.h>
#include <third_party/hsmddk/include/Kit/EIP130/TokenHelper/incl/eip130_token_asset.h>
#include <third_party/hsmddk/include/Kit/EIP130/TokenHelper/incl/eip130_token_common.h>
#include <third_party/hsmddk/include/Kit/EIP130/TokenHelper/incl/eip130_token_crypto.h>
#include <third_party/hsmddk/include/Kit/EIP130/TokenHelper/incl/eip130_token_hash.h>
#include <third_party/hsmddk/include/Kit/EIP130/TokenHelper/incl/eip130_token_mac.h>
#include <third_party/hsmddk/include/Kit/EIP130/TokenHelper/incl/eip130_token_pk.h>
#include <third_party/hsmddk/include/Kit/EIP130/TokenHelper/incl/eip130_token_publicdata.h>
#include <third_party/hsmddk/include/Kit/EIP130/TokenHelper/incl/eip130_token_random.h>
#include <third_party/hsmddk/include/Integration/Adapter_VEX/incl/c_adapter_vex.h>
#include <third_party/hsmddk/include/Kit/EIP201/incl/eip201.h>
#include <third_party/hsmddk/include/Kit/DriverFramework/Device_API/incl/device_mgmt.h>
#include <third_party/hsmddk/include/Integration/Adapter_Generic/incl/adapter_interrupts.h>

#include <string.h>

#define SYSTEMINFO_TOKEN_WORD0      0x0F030000
#define CRYPTO_OFFICER_ID           0x4F5A3647
#define OUTPUT_TOKEN_ERROR          0x80000000
#define HSM_TRNG_RAW_KEY_ENC        0x5244
#define HSM_O_UNLOCK_CPUID0_CPUID1  0xFFFFFCFC
#define HSM_O_CPUID0_MB1_MB2_UNLOCK 0xFFFFFF77

#define HSMCRYPTO_BASE              HSM_BASE
#define HSMCRYPTO_O_MBSTA           HSM_O_MAILBOX_STAT
#define HSMCRYPTO_MBSTA_MB1IN_M     HSM_MAILBOX_STAT_INFULL1
#define HSMCRYPTO_MBSTA_MB1IN_FULL  HSM_MAILBOX_STAT_INFULL1
#define HSMCRYPTO_O_MB1IN           HSM_O_EIP130_072_MAILBOX1_IN
#define HSMCRYPTO_O_MBCTL           HSM_O_MBXCTL
#define HSMCRYPTO_MBCTL_MB1IN_FULL  HSM_MBXCTL_INFULL1
#define HSMCRYPTO_MBCTL_MB1LNK_LNK  HSM_MBXCTL_LINK1
#define HSMCRYPTO_O_MBLNKID         HSM_O_MAILBOX_LINKID
#define HSMCRYPTO_O_MBLCKOUT        HSM_O_MAILBOX_LOCKOUT
#define HSMCRYPTO_MBSTA_MB1OUT_M    HSM_MAILBOX_STAT_OUTFULL1
#define HSMCRYPTO_MBSTA_MB1OUT_FULL HSM_MAILBOX_STAT_OUTFULL1
#define HSMCRYPTO_O_MB1OUT          HSM_O_EIP130_072_MAILBOX1_IN
#define HSMCRYPTO_MBCTL_MB1OUT_EMTY HSM_MBXCTL_OUTEMP1

#define HSMLPF3_STATUS_SUCCESS ((int_fast16_t)0)
#define HSMLPF3_STATUS_ERROR ((int_fast16_t)-1)
#define HSM_RAW_RNG_BLOCK_SIZE (256U)
#define HSM_BUFFER_SIZE       HSM_RAW_RNG_BLOCK_SIZE
#define HSM_RAW_RNG_BLOCK_NUM (HSM_BUFFER_SIZE / HSM_RAW_RNG_BLOCK_SIZE)

/* Adjusted experimentally */
#define HSM_OPERATION_RETRIES        10
#define HSM_WAIT_FOR_RESULT_RETRIES  10
#define HSM_OPERATION_SEM_TIMEOUT_MS 10

struct entropy_cc35xx_hsm {
	struct k_sem operation;
	uint32_t retries;
	uint8_t buffer[HSM_BUFFER_SIZE];
};

struct entropy_cc35xx_data {
	struct k_sem pool_lock;
	struct k_work trng_work;
	struct entropy_cc35xx_hsm hsm;
	struct ring_buf pool;
	uint8_t buffer[CONFIG_ENTROPY_CC35XX_POOL_SIZE];
};

static inline int entropy_cc35xx_read_hsm_result(void)
{
	Eip130Token_Result_t result_token;
	uint8_t mailbox_number;

	mailbox_number = HSMSAL_GetMailBoxNumber();
	return (HSMSAL_ScanAndReadMailbox(&result_token, mailbox_number) == HSMSAL_SUCCESS) ? 0
											    : -EIO;
}

static inline int entropy_cc35xx_wait_for_hsm(void)
{
	for (int i = 0; i < HSM_WAIT_FOR_RESULT_RETRIES; ++i) {
		if (!entropy_cc35xx_read_hsm_result()) {
			return 0;
		}
	}

	return -ETIMEDOUT;
}

static void entropy_cc35xx_hsm_init_aic(void)
{
	Device_Handle_t gl_aic = Device_Find("EIP130_AIC");

	if (gl_aic != NULL) {
		/*
		 * Configure all sources for edge detect. Future improvement
		 * is to only enable the interrupts we need. We could also
		 * use EIP201_Config_Change(), but there is no adapter function()
		 */
		EIP201_SourceSettings_t settings = {
			.Source = 0xFF,
			.Config = EIP201_CONFIG_RISING_EDGE,
			.fEnable = false /* enable source only when active */
		};
		EIP201_Initialize(gl_aic, &settings, 1);
	}
}

static inline void entropy_cc35xx_kick_trng_config(struct entropy_cc35xx_data *data)
{
	uintptr_t key;
	Eip130Token_Command_t command_token;

	data->hsm.retries = 0;

	memset(&command_token, 0, sizeof(command_token));

	Eip130Token_Command_RandomNumber_Generate(&command_token, HSM_RAW_RNG_BLOCK_NUM,
						  (uintptr_t)data->hsm.buffer);
	Eip130Token_Command_RandomNumber_SetRawKey(&command_token, HSM_TRNG_RAW_KEY_ENC);

	key = irq_lock();
	HSMSAL_SubmitPhysicalToken(&command_token);
	irq_unlock(key);
}

static void entropy_cc35xx_kick_trng(struct entropy_cc35xx_data *data)
{
	if (ring_buf_space_get(&data->pool) == 0) {
		/* Ring buffer is full. Refill not needed so far */
		return;
	}

	if (data->hsm.retries++ >= HSM_OPERATION_RETRIES) {
		/* HSM recovering */
		NVIC_ClearPendingIRQ(DT_INST_IRQN(0));
		irq_disable(DT_INST_IRQN(0));
		entropy_cc35xx_wait_for_hsm();
		k_sem_give(&data->hsm.operation);
	} else if (k_sem_take(&data->hsm.operation, K_MSEC(HSM_OPERATION_SEM_TIMEOUT_MS)) != 0) {
		return;
	}

	entropy_cc35xx_kick_trng_config(data);

	irq_enable(DT_INST_IRQN(0));
}

static void entropy_cc35xx_trng_work_handler(struct k_work *work)
{
	struct entropy_cc35xx_data *data =
		CONTAINER_OF(work, struct entropy_cc35xx_data, trng_work);
	entropy_cc35xx_kick_trng(data);
}

static int entropy_cc35xx_get_entropy(const struct device *dev, uint8_t *buf, uint16_t len)
{
	struct entropy_cc35xx_data *data = dev->data;
	uint32_t cnt;

	if (len == 0 || len > sizeof(data->buffer)) {
		return -EINVAL;
	}

	entropy_cc35xx_kick_trng(data);

	while (len) {
		k_sem_take(&data->pool_lock, K_FOREVER);
		cnt = ring_buf_get(&data->pool, buf, len);
		k_sem_give(&data->pool_lock);

		entropy_cc35xx_kick_trng(data);

		buf += cnt;
		len -= cnt;
	}

	return 0;
}

static void entropy_cc35xx_isr(const struct device *dev)
{
	struct entropy_cc35xx_data *data = dev->data;

	NVIC_ClearPendingIRQ(DT_INST_IRQN(0));
	irq_disable(DT_INST_IRQN(0));

	entropy_cc35xx_read_hsm_result();

	ring_buf_put(&data->pool, data->hsm.buffer, sizeof(data->hsm.buffer));
	k_sem_give(&data->hsm.operation);

	k_work_submit(&data->trng_work);
}

static int entropy_cc35xx_init(const struct device *dev)
{
	struct entropy_cc35xx_data *data = dev->data;

	uint32_t result = HSMLPF3_STATUS_ERROR;
	uint32_t token[] = {SYSTEMINFO_TOKEN_WORD0, CRYPTO_OFFICER_ID};
	const uint32_t hsm_clk_enable = HSM_NON_SEC_CLK_MEM_CTRL_MEM_CLK_GO_EN |
					HSM_NON_SEC_CLK_MEM_CTRL_MEM_SLV_CLK_GO_EN |
					HSM_NON_SEC_CLK_MEM_CTRL_MEM_CTR_CLK_GO_EN |
					HSM_NON_SEC_CLK_MEM_CTRL_MEM_CLK_GO_M3_EN |
					HSM_NON_SEC_CLK_MEM_CTRL_MEM_SLV_CLK_GO_M3_EN |
					HSM_NON_SEC_CLK_MEM_CTRL_MEM_CTR_CLK_GO_M3_EN;
	const uint32_t hsm_clk_mask = HSM_NON_SEC_CLK_MEM_CTRL_MEM_CLK_GO_M |
				      HSM_NON_SEC_CLK_MEM_CTRL_MEM_SLV_CLK_GO_M |
				      HSM_NON_SEC_CLK_MEM_CTRL_MEM_CTR_CLK_GO_M |
				      HSM_NON_SEC_CLK_MEM_CTRL_MEM_CLK_GO_M3_M |
				      HSM_NON_SEC_CLK_MEM_CTRL_MEM_SLV_CLK_GO_M3_M |
				      HSM_NON_SEC_CLK_MEM_CTRL_MEM_CTR_CLK_GO_M3_M;

	uint32_t idx, hsm_status, hsm_reg_value;
	uintptr_t key, hsm_reg_addr;

	k_work_init(&data->trng_work, entropy_cc35xx_trng_work_handler);

	/* Initialize ring buffer */
	ring_buf_init(&data->pool, sizeof(data->buffer), data->buffer);

	/* Initialize HSM clock and mailbox, then boot it */
	hsm_reg_addr = SOC_AON_BASE + SOC_AON_O_HSMCFG;
	hsm_reg_value = sys_read32(hsm_reg_addr) | SOC_AON_HSMCFG_FIREWALL;
	sys_write32(hsm_reg_value, hsm_reg_addr);

	key = irq_lock();

	/* Disable HSM Clock */
	hsm_reg_addr = HSM_NON_SEC_BASE + HSM_NON_SEC_O_CLK_MEM_CTRL;
	hsm_reg_value = sys_read32(hsm_reg_addr) & ~(hsm_clk_mask);
	sys_write32(hsm_reg_value, hsm_reg_addr);

	hsm_reg_addr = HSM_SEC_BASE + HSM_SEC_O_CLKCTL;
	hsm_reg_value =
		sys_read32(hsm_reg_addr) & ~(HSM_SEC_CLKCTL_CLKGO_EN | HSM_SEC_CLKCTL_HIFCLKGO_EN |
					     HSM_SEC_CLKCTL_CNTCLKGO_EN);
	sys_write32(hsm_reg_value, hsm_reg_addr);

	/* Initialize HSM Clock */
	hsm_reg_addr = HSM_NON_SEC_BASE + HSM_NON_SEC_O_CLK_MEM_CTRL;
	sys_write32(hsm_reg_value, hsm_clk_enable);

	hsm_reg_addr = HSM_SEC_BASE + HSM_SEC_O_CLKCTL;
	hsm_reg_value =
		sys_read32(hsm_reg_addr) |
		(HSM_SEC_CLKCTL_CLKGO_EN | HSM_SEC_CLKCTL_HIFCLKGO_EN | HSM_SEC_CLKCTL_CNTCLKGO_EN);
	sys_write32(hsm_reg_value, hsm_reg_addr);

	/* Unlock CPUID0 and CPUID1 */
	hsm_reg_addr = HSMCRYPTO_BASE + HSMCRYPTO_O_MBLCKOUT;
	hsm_reg_value = HSM_O_UNLOCK_CPUID0_CPUID1;
	sys_write32(hsm_reg_value, hsm_reg_addr);

	/* Link mailbox */
	hsm_reg_addr = HSMCRYPTO_BASE + HSMCRYPTO_O_MBSTA;
	hsm_reg_value = sys_read32(hsm_reg_addr) | HSMCRYPTO_MBCTL_MB1LNK_LNK;
	hsm_reg_addr = HSMCRYPTO_BASE + HSMCRYPTO_O_MBCTL;
	sys_write32(hsm_reg_value, hsm_reg_addr);

	/* Allow non-secure/secure access (Set bits 7 and 3 to 1 if we need secure access) */
	hsm_reg_addr = HSMCRYPTO_BASE + HSMCRYPTO_O_MBLNKID;
	hsm_reg_value = (0 << HSM_MAILBOX_LINKID_LINKID1_S) | (0 << HSM_MAILBOX_LINKID_LINKID2_S) |
			(0 << HSM_MAILBOX_LINKID_PROTACC1_S) | (0 << HSM_MAILBOX_LINKID_PORTACC2_S);
	sys_write32(hsm_reg_value, hsm_reg_addr);

	/* Make sure CPU_ID=0 host can access mailbox 1 & 2 (no lockout) */
	hsm_reg_addr = HSMCRYPTO_BASE + HSMCRYPTO_O_MBLCKOUT;
	hsm_reg_value = sys_read32(hsm_reg_addr) & HSM_O_CPUID0_MB1_MB2_UNLOCK;
	sys_write32(hsm_reg_value, hsm_reg_addr);

	entropy_cc35xx_hsm_init_aic();

	hsm_reg_addr = HSMCRYPTO_BASE + HSM_O_MODULE_STATUS;
	hsm_status = sys_read32(hsm_reg_addr);
	if ((hsm_status & HSM_MODULE_STATUS_FATALERR) == 0) {
		/*
		 * At PRE_KERNEL_1 we don't use k_sleep() and other scheduling
		 * functions, so we use a busy wait loop here to wait for the
		 * input token to be processed inside the HSM.
		 * Based on experiments, it usually takes 0 cycles of the while loop
		 * to process the input token.
		 */
		hsm_reg_addr = HSMCRYPTO_BASE + HSMCRYPTO_O_MBSTA;
		while ((sys_read32(hsm_reg_addr) & HSMCRYPTO_MBSTA_MB1IN_M) ==
		       HSMCRYPTO_MBSTA_MB1IN_FULL)
			;

		/* Mailbox is empty so we can write the system info token to mbx1_in */
		for (idx = 0; idx < ARRAY_SIZE(token); idx++) {
			hsm_reg_addr = HSMCRYPTO_BASE + HSMCRYPTO_O_MB1IN + idx * 4;
			sys_write32(token[idx], hsm_reg_addr);
		}

		/* Mark mbx1 in as full */
		hsm_reg_addr = HSMCRYPTO_BASE + HSMCRYPTO_O_MBCTL;
		hsm_reg_value = HSMCRYPTO_MBCTL_MB1IN_FULL;
		sys_write32(hsm_reg_value, hsm_reg_addr);

		/*
		 * At PRE_KERNEL_1 we don't use k_sleep() and other scheduling
		 * functions, so we use a busy wait loop here to wait for the
		 * output token to be available in mbx1_out.
		 * Based on experiments, it usually takes 12 cycles of the while loop
		 * to get response from HSM.
		 */
		hsm_reg_addr = HSMCRYPTO_BASE + HSMCRYPTO_O_MBSTA;
		while ((sys_read32(hsm_reg_addr) & HSMCRYPTO_MBSTA_MB1OUT_M) !=
		       HSMCRYPTO_MBSTA_MB1OUT_FULL)
			;

		/* Check for output token error */
		hsm_reg_addr = HSMCRYPTO_BASE + HSMCRYPTO_O_MB1OUT;
		hsm_reg_value = sys_read32(hsm_reg_addr);
		if ((hsm_reg_value & OUTPUT_TOKEN_ERROR) == 0) {
			result = HSMLPF3_STATUS_SUCCESS;
		}

		hsm_reg_addr = HSMCRYPTO_BASE + HSMCRYPTO_O_MBCTL;
		hsm_reg_value = HSMCRYPTO_MBCTL_MB1OUT_EMTY;
		sys_write32(hsm_reg_value, hsm_reg_addr);
	}

	irq_unlock(key);

	if (result != HSMLPF3_STATUS_SUCCESS) {
		return -EIO;
	}

	if (HSMSAL_Init() != HSMSAL_SUCCESS) {
		return -EIO;
	}

	/*
	 * According to the CC35XX SDK:
	 * Reset of HSM, as well as HUK (Hardware Unique Key) provisioning on CC35XX, is performed
	 * at boot time by the TI Device boot loader. As a result, HUK provisioning is not needed.
	 */
	key = irq_lock();
	NVIC_ClearPendingIRQ(DT_INST_IRQN(0));
	irq_unlock(key);

	entropy_cc35xx_kick_trng_config(data);

	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority), entropy_cc35xx_isr,
		    DEVICE_DT_INST_GET(0), 0);

	irq_enable(DT_INST_IRQN(0));

	return 0;
}

static const struct entropy_driver_api entropy_cc35xx_driver_api = {
	.get_entropy = entropy_cc35xx_get_entropy,
};

static struct entropy_cc35xx_data entropy_cc35xx_data = {
	.pool_lock = Z_SEM_INITIALIZER(entropy_cc35xx_data.pool_lock, 1, 1),
	.hsm.operation = Z_SEM_INITIALIZER(entropy_cc35xx_data.hsm.operation, 1, 1),
};

DEVICE_DT_INST_DEFINE(0, entropy_cc35xx_init, PM_DEVICE_DT_INST_GET(0), &entropy_cc35xx_data, NULL,
		      PRE_KERNEL_1, CONFIG_ENTROPY_INIT_PRIORITY, &entropy_cc35xx_driver_api);
