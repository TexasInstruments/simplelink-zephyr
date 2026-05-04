/*
 * Copyright (c) 2026 Texas Instruments.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_lpf3_rtc_timer

#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/timeutil.h>

#include <inc/hw_rtc.h>
#include <inc/hw_memmap.h>
#include <inc/hw_systim.h>
#include <inc/hw_types.h>
#include <inc/hw_evtsvt.h>

#include <time.h>
#include <string.h>

#include "rtc_utils.h"

#define RTC_TI_LPF3_TOP_19_BITS_MASK 0xFFFFE000
#define RTC_TI_LPF3_TOP_16_BITS_MASK 0xFFFF0000

/* RTC comparator event register (8us), CCH08U has a resolution of 8us.
 * This is a helper macro to convert seconds to 8 microsecond units.
 */

#define RTC_TI_LPF3_SECONDS_TO_8US(x) ((x)*125000)

struct rtc_ti_lpf3_data {
	struct k_spinlock lock;
	uint64_t time_offset;
#if CONFIG_RTC_UPDATE
	rtc_update_callback update_callback;
	void *update_callback_data;
#endif /* CONFIG_RTC_UPDATE */
};

struct rtc_ti_lpf3_config {
	void (*irq_config)(void);
};

static int rtc_ti_lpf3_init(const struct device *dev)
{
	struct rtc_ti_lpf3_data *data = dev->data;
	const struct rtc_ti_lpf3_config *config = dev->config;

	data->time_offset = 0;

	/* Configure RTC to halt when CPU stopped during debug */
	sys_write32(RTC_EMU_HALT_STOP, RTC_BASE + RTC_O_EMU);

	config->irq_config();

	return 0;
}

static uint64_t rtc_ti_lpf3_get_raw_time(struct rtc_ti_lpf3_data *data)
{
	k_spinlock_key_t key;
	uint64_t usec;
	uint32_t time524m;

#if CONFIG_SOC_SERIES_CC27XX
	uint32_t time1u;
#endif /* CONFIG_SOC_SERIES_CC27XX */

#if CONFIG_SOC_SERIES_CC23X0
	uint32_t time8u;
#endif /* CONFIG_SOC_SERIES_CC23X0 */

	key = k_spin_lock(&data->lock);

	while (true) {
#if CONFIG_SOC_SERIES_CC27XX
		time1u = sys_read32(RTC_BASE + RTC_O_TIME1U);
#elif CONFIG_SOC_SERIES_CC23X0
		time8u = sys_read32(RTC_BASE + RTC_O_TIME8U);
#else
#error RTC driver not implemented for this SoC
#endif

		time524m = sys_read32(RTC_BASE + RTC_O_TIME524M);

#if CONFIG_SOC_SERIES_CC23X0
		/**
		 * Check if TIME8U wrapped around.
		 * In such case read time again to get correct
		 * concatenated value.
		 */
		if ((time8u >> 31) == 1) {
			if ((sys_read32(RTC_BASE + RTC_O_TIME8U) >> 31) == 0) {
				continue;
			}
		}

#elif CONFIG_SOC_SERIES_CC27XX
		/**
		 * Check if TIME1U wrapped around.
		 * In such case read time again to get correct
		 * concatenated value.
		 */
		if ((time1u >> 31) == 1) {
			if ((sys_read32(RTC_BASE + RTC_O_TIME1U) >> 31) == 0) {
				continue;
			}
		}
#else
#error RTC driver not implemented for this SoC
#endif

		break;
	}

#if CONFIG_SOC_SERIES_CC27XX
	usec = (uint64_t)(time524m & RTC_TI_LPF3_TOP_19_BITS_MASK) << 19;
	usec |= time1u;
#endif

#if CONFIG_SOC_SERIES_CC23X0
	usec = (uint64_t)(time524m & RTC_TI_LPF3_TOP_16_BITS_MASK) << 19;
	usec |= (((uint64_t)time8u) << 3);
#endif

	usec = usec + data->time_offset;

	k_spin_unlock(&data->lock, key);

	return usec;
}

static void rtc_ti_lpf3_set_dtime(struct rtc_ti_lpf3_data *data, int64_t delta_us)
{
	k_spinlock_key_t key;

	key = k_spin_lock(&data->lock);

	data->time_offset = (uint64_t)((int64_t)data->time_offset + (int64_t)delta_us);

	k_spin_unlock(&data->lock, key);
}

static int rtc_ti_lpf3_set_time(const struct device *dev, const struct rtc_time *timeptr)
{
	struct rtc_ti_lpf3_data *data = dev->data;
	int64_t sec_unix_target;
	int64_t usec_hw_target;
	uint64_t usec_hw_time;
	int64_t delta_us;

	if (!timeptr || (timeptr->tm_year < 0)) {
		return -EINVAL;
	}

	sec_unix_target = timeutil_timegm64((const struct tm *)timeptr);

	if (sec_unix_target > INT64_MAX / USEC_PER_SEC) {
		return -ERANGE;
	}

	usec_hw_target = sec_unix_target * USEC_PER_SEC + (timeptr->tm_nsec / 1000);

	usec_hw_time = rtc_ti_lpf3_get_raw_time(data);

	delta_us = usec_hw_target - usec_hw_time;

	rtc_ti_lpf3_set_dtime(data, delta_us);

	return 0;
}

static int rtc_ti_lpf3_get_time(const struct device *dev, struct rtc_time *timeptr)
{
	struct rtc_ti_lpf3_data *data = dev->data;
	uint64_t sec_hw_time;
	uint64_t usec_hw_time;
	uint64_t sec_unix_time;

	if (timeptr == NULL) {
		return -EINVAL;
	}

	usec_hw_time = rtc_ti_lpf3_get_raw_time(data);
	sec_hw_time = usec_hw_time / USEC_PER_SEC;
	sec_unix_time = sec_hw_time;

	gmtime_r(&sec_unix_time, (struct tm *)timeptr);

	timeptr->tm_nsec = (((int)(usec_hw_time % USEC_PER_SEC)) * 1000);

	/* Unsupported */
	timeptr->tm_isdst = -1;

	return 0;
}

#if CONFIG_RTC_UPDATE

static int rtc_ti_lpf3_update_set_callback(const struct device *dev, rtc_update_callback callback,
					   void *user_data)
{
	k_spinlock_key_t key;
	uint32_t time8u;
	uint32_t ch0cc8u;

	struct rtc_ti_lpf3_data *data = dev->data;

	key = k_spin_lock(&data->lock);

	if ((callback == NULL) && (user_data == NULL)) {

		sys_write32(RTC_ICLR_EV0_CLR, RTC_BASE + RTC_O_ICLR);
		sys_write32(RTC_IMCLR_EV0_CLR, RTC_BASE + RTC_O_IMCLR);
		sys_write32(RTC_ARMCLR_CH0_CLR, RTC_BASE + RTC_O_ARMCLR);

		data->update_callback = NULL;
		data->update_callback_data = NULL;

	} else if (callback != NULL) {

		sys_write32(RTC_ICLR_EV0_CLR, RTC_BASE + RTC_O_ICLR);
		sys_write32(RTC_IMCLR_EV0_CLR, RTC_BASE + RTC_O_IMCLR);
		sys_write32(RTC_ARMCLR_CH0_CLR, RTC_BASE + RTC_O_ARMCLR);

		data->update_callback = callback;
		data->update_callback_data = user_data;

		time8u = sys_read32(RTC_BASE + RTC_O_TIME8U);

		ch0cc8u = time8u + (uint32_t)(125000U - (time8u % 125000U)) - (uint32_t)((data->time_offset % 1000000ULL)/8ULL);

		sys_write32(ch0cc8u, RTC_BASE + RTC_O_CH0CC8U);

		sys_write32(RTC_IMSET_EV0_SET, RTC_BASE + RTC_O_IMSET);
	}

	k_spin_unlock(&data->lock, key);

	return 0;
}

#endif /* CONFIG_RTC_UPDATE */

static void rtc_ti_lpf3_isr(const struct device *dev)
{
	uint32_t time8u;
#if CONFIG_RTC_UPDATE
	struct rtc_ti_lpf3_data *data = dev->data;
#endif /* CONFIG_RTC_UPDATE */

	if (sys_read32(RTC_BASE + RTC_O_RIS) & 1) {
		sys_write32(RTC_ICLR_EV0_CLR, RTC_BASE + RTC_O_ICLR);
	} else {
		return;
	}

	time8u = sys_read32(RTC_BASE + RTC_O_TIME8U);

#if CONFIG_RTC_UPDATE
	if (data->update_callback) {
		uint32_t compare;

		compare = sys_read32(RTC_BASE + RTC_O_TIME8U);
		compare += RTC_TI_LPF3_SECONDS_TO_8US(1);
		sys_write32(compare, RTC_BASE + RTC_O_CH0CC8U);

		data->update_callback(dev, data->update_callback_data);
	}
#endif /* CONFIG_RTC_UPDATE */
}

static const struct rtc_driver_api rtc_ti_lpf3_driver_api = {
	.set_time = rtc_ti_lpf3_set_time,
	.get_time = rtc_ti_lpf3_get_time,
#if CONFIG_RTC_UPDATE
	.update_set_callback = rtc_ti_lpf3_update_set_callback,
#endif /* CONFIG_RTC_UPDATE */
};

#define RTC_TI_LPF3_DEVICE(id)                                                                     \
	static struct rtc_ti_lpf3_data rtc_ti_lpf3_data_##id;                                      \
                                                                                                   \
	static void rtc_ti_lpf3_irq_config(void)                                                   \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority), rtc_ti_lpf3_isr,            \
			    DEVICE_DT_INST_GET(0), 0);                                             \
                                                                                                   \
		sys_write32(EVTSVT_CPUIRQ0SEL_PUBID_AON_RTC_COMB,                                  \
			    EVTSVT_BASE + (EVTSVT_O_CPUIRQ0SEL + (4 * DT_INST_IRQN(0))));          \
                                                                                                   \
		irq_enable(DT_INST_IRQN(0));                                                       \
	}                                                                                          \
                                                                                                   \
	static const struct rtc_ti_lpf3_config rtc_ti_lpf3_config_##id = {                         \
		.irq_config = rtc_ti_lpf3_irq_config,                                              \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(id, rtc_ti_lpf3_init, NULL, &rtc_ti_lpf3_data_##id,                  \
			      &rtc_ti_lpf3_config_##id, POST_KERNEL, CONFIG_RTC_INIT_PRIORITY,     \
			      &rtc_ti_lpf3_driver_api);

DT_INST_FOREACH_STATUS_OKAY(RTC_TI_LPF3_DEVICE);
