/*
 * Copyright (c) 2026 Conclusive Engineering Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/pm/pm.h>
#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/init.h>
#include <cmsis_core.h>

#include <inc/hw_types.h>
#include <inc/hw_memmap.h>
#include <inc/hw_hostmcu_aon.h>
#include <inc/hw_rtc.h>
#include <inc/hw_systim.h>

/* Missing defines from hw_hostmcu_aon.h */
#define ELP_WAKEUP_THRESHOLD 0x3U
#define NVIC_IRQ_REG_NUM(num) ((num) >> 5U)
#define NVIC_IRQ_BIT_POS(num) BIT(((uint32_t)(num)) & 0x1FU)
#define HOSTMCU_AON_CFGWICSNS_DEBUGSS_HOST_CSYSPWRUPREQ_EN (1U << 14)
#define HOSTMCU_AON_CFGWUTP_DEBUGSS_HOST_CSYSPWRUPREQ_FAST (1U << 14)
#define HOSTMCU_AON_CFGWICSNS_RTC_EN (1U << 13)
#define HOSTMCU_AON_CFGWUTP_RTC_FAST (1U << 13)

#define ELP_TIMER_IS_ENABLED() \
	(((sys_read32(HOSTMCU_AON_BASE + HOSTMCU_AON_O_ELPTMREN) & \
	   HOSTMCU_AON_ELPTMREN_VAL_M)) != HOSTMCU_AON_ELPTMREN_VAL_DIS)

#define ELP_TIMER_STOP_AND_RELOAD() \
	do { \
		sys_write32(sys_read32(HOSTMCU_AON_BASE + HOSTMCU_AON_O_ELPTMREN) | \
				HOSTMCU_AON_ELPTMREN_ELPTMRRST, \
				HOSTMCU_AON_BASE + HOSTMCU_AON_O_ELPTMREN); \
		while (ELP_TIMER_IS_ENABLED()) { \
		} \
		sys_write32(sys_read32(HOSTMCU_AON_BASE + HOSTMCU_AON_O_ELPTMREN) & \
				~(HOSTMCU_AON_ELPTMREN_ELPTMRRST | \
				  HOSTMCU_AON_ELPTMREN_ELPTMRSET), \
				HOSTMCU_AON_BASE + HOSTMCU_AON_O_ELPTMREN); \
		sys_write32(sys_read32(HOSTMCU_AON_BASE + HOSTMCU_AON_O_ELPTMREN) | \
				HOSTMCU_AON_ELPTMREN_ELPTMRLD, \
				HOSTMCU_AON_BASE + HOSTMCU_AON_O_ELPTMREN); \
	} while (0)

#define ELP_TIMER_START() \
	sys_write32(sys_read32(HOSTMCU_AON_BASE + HOSTMCU_AON_O_ELPTMREN) | \
			HOSTMCU_AON_ELPTMREN_ELPTMRSET, \
			HOSTMCU_AON_BASE + HOSTMCU_AON_O_ELPTMREN)

static int cc35xx_pm_hw_init(void)
{
	/* Use RTC channel 0 in compare mode for the system timer. */
	sys_write32(RTC_IMSET_EV0_SET, RTC_BASE + RTC_O_IMSET);

	/* Halt RTC/SYSTIM when CPU is halted by the debugger. */
	sys_write32(RTC_EMU_HALT_STOP, RTC_BASE + RTC_O_EMU);
	sys_write32(SYSTIM_EMU_HALT_STOP, SYSTIM_BASE + SYSTIM_O_EMU);

	/* RTC wake source is required for standby wakeup. */
	sys_write32(sys_read32(HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGWICSNS) |
			HOSTMCU_AON_CFGWICSNS_RTC_EN,
			HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGWICSNS);
	sys_write32(sys_read32(HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGWUTP) |
			HOSTMCU_AON_CFGWUTP_RTC_FAST,
			HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGWUTP);

	/* Clear DEBUGSS wake source separately; ROM may leave it enabled. */
	sys_write32(sys_read32(HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGWICSNS) &
			~HOSTMCU_AON_CFGWICSNS_DEBUGSS_HOST_CSYSPWRUPREQ_EN,
			HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGWICSNS);
	sys_write32(sys_read32(HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGWUTP) &
			~HOSTMCU_AON_CFGWUTP_DEBUGSS_HOST_CSYSPWRUPREQ_FAST,
			HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGWUTP);

	irq_enable(INT_RTC_EVENT_IRQn);
	irq_disable(INT_DEBUGSS_HOST_CSYSPWRUPREQ_IRQn);
	NVIC_ClearPendingIRQ(INT_DEBUGSS_HOST_CSYSPWRUPREQ_IRQn);

	return 0;
}

static int ti_cc35xx_power_init(void)
{
	unsigned int key = irq_lock();

	cc35xx_pm_hw_init();

	irq_unlock(key);

	return 0;
}

SYS_INIT(ti_cc35xx_power_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

__ramfunc static void cc35xx_pm_enter_standby_ram(void)
{
	uint32_t irq_status0;
	uint32_t irq_status1;
	uint32_t elptmren;
	bool elp_running;
	bool elp_sw_mode;

	SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

	irq_status0 = NVIC->ISER[0];
	irq_status1 = NVIC->ISER[1];
	elptmren = sys_read32(HOSTMCU_AON_BASE + HOSTMCU_AON_O_ELPTMREN);

	elp_running = (elptmren & HOSTMCU_AON_ELPTMREN_VAL_M) ==
		HOSTMCU_AON_ELPTMREN_VAL_EN;
	elp_sw_mode = (elptmren & HOSTMCU_AON_ELPTMREN_TMRSWCTL_M) ==
		HOSTMCU_AON_ELPTMREN_TMRSWCTL_SW;

	NVIC->ICER[1] = irq_status1;
	NVIC->ICER[0] = irq_status0;
	__DSB();
	__ISB();

	__COMPILER_BARRIER();
	NVIC->ISER[NVIC_IRQ_REG_NUM(INT_HOST_ELP_TMR_WAKEUP_REQ_IRQn)] =
		NVIC_IRQ_BIT_POS(INT_HOST_ELP_TMR_WAKEUP_REQ_IRQn);
	__COMPILER_BARRIER();
	sys_write32(sys_read32(HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGWICSNS) &
			~HOSTMCU_AON_CFGWICSNS_VAL_TMRREQ_EN,
			HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGWICSNS);
	NVIC->ICPR[NVIC_IRQ_REG_NUM(INT_HOST_ELP_TMR_WAKEUP_REQ_IRQn)] =
		NVIC_IRQ_BIT_POS(INT_HOST_ELP_TMR_WAKEUP_REQ_IRQn);

	if (!elp_sw_mode) {
		sys_write32(sys_read32(HOSTMCU_AON_BASE + HOSTMCU_AON_O_ELPTMREN) |
				HOSTMCU_AON_ELPTMREN_TMRSWCTL_SW,
				HOSTMCU_AON_BASE + HOSTMCU_AON_O_ELPTMREN);
	}

	ELP_TIMER_STOP_AND_RELOAD();

	sys_write32(ELP_WAKEUP_THRESHOLD | HOSTMCU_AON_CFGTMRWU_EN_EN,
			HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGTMRWU);

	ELP_TIMER_START();

	__DSB();
	__WFI();
	__ISB();

	sys_write32(sys_read32(HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGTMRWU) &
			~HOSTMCU_AON_CFGTMRWU_EN_EN,
			HOSTMCU_AON_BASE + HOSTMCU_AON_O_CFGTMRWU);

	NVIC->ICER[NVIC_IRQ_REG_NUM(INT_HOST_ELP_TMR_WAKEUP_REQ_IRQn)] =
		NVIC_IRQ_BIT_POS(INT_HOST_ELP_TMR_WAKEUP_REQ_IRQn);
	__DSB();
	__ISB();

	sys_write32(HOSTMCU_AON_TMRWUREQ_CLR_M,
			HOSTMCU_AON_BASE + HOSTMCU_AON_O_TMRWUREQ);
	sys_write32(sys_read32(HOSTMCU_AON_BASE + HOSTMCU_AON_O_ELPTMREN) &
			~HOSTMCU_AON_ELPTMREN_ELPTMRSET,
			HOSTMCU_AON_BASE + HOSTMCU_AON_O_ELPTMREN);

	if (!elp_sw_mode || (elp_sw_mode && !elp_running)) {
		ELP_TIMER_STOP_AND_RELOAD();
	}

	NVIC->ICPR[NVIC_IRQ_REG_NUM(INT_HOST_ELP_TMR_WAKEUP_REQ_IRQn)] =
		NVIC_IRQ_BIT_POS(INT_HOST_ELP_TMR_WAKEUP_REQ_IRQn);

	__COMPILER_BARRIER();
	NVIC->ISER[1] = irq_status1;
	NVIC->ISER[0] = irq_status0;
	__COMPILER_BARRIER();

	SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
}

void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(substate_id);

	/*
	 * idle() enters pm_system_suspend() with IRQs locked via arch_irq_lock(),
	 * which maps to BASEPRI on Cortex-M. Switch to PRIMASK-based masking
	 * while entering low power so the RTC wake IRQ can be taken normally
	 * after wakeup.
	 */
	__disable_irq();
	__set_BASEPRI(0);

	switch (state) {
	case PM_STATE_RUNTIME_IDLE:
		__WFI();
		break;
	case PM_STATE_STANDBY:
		/* Don't enter deep sleep if no real timeout */
		if (_kernel.idle <= 0) {
			break;
		}

		sys_write32(RTC_ICLR_EV0_CLR, RTC_BASE + RTC_O_ICLR);
		NVIC_ClearPendingIRQ(INT_RTC_EVENT_IRQn);

		cc35xx_pm_enter_standby_ram();
		break;
	default:
		break;
	}
}

void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(state);
	ARG_UNUSED(substate_id);

	__enable_irq();
}
