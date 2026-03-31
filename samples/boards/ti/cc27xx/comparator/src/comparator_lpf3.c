#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>

#include "comparator_lpf3.h"

/**
 * Bit 30 in IOC for DIO is set to enable hysteresis.
 * Bit 29 in IOC for DIO is set to configure it as an input.
 * Bits [14:13] in IOC for DIO is set to 2 to enable pull-up.
 */

#define LPCOMP_GPIO_CONFIG (1U << 30) | (1U << 29) | (2U << 13)

#define LPCOMP_GPIO_MUX IOC_MUX_ANALOG

/**
 * Comparator output takes 1 to 3 LFOSC clock cycles to
 * settle after enabling.
 *
 */

#define LPCOMP_SETTLING_TIME (110U)

#define LPCOMP_POS_INPUT_A1_GPIO 19
#define LPCOMP_POS_INPUT_A2_GPIO 20
#define LPCOMP_POS_INPUT_A3_GPIO 21
#define LPCOMP_POS_INPUT_OPEN    0xFF

#define LPCOMP_NEG_INPUT_A2_GPIO 20
#define LPCOMP_NEG_INPUT_A3_GPIO 21
#define LPCOMP_NEG_INPUT_OPEN    0xFF

#define LPCOMP_OUTPUT_0_GPIO 0
#define LPCOMP_OUTPUT_1_GPIO 15

#define LPCOMP_OUTPUT_GPIO_MUX 4U
#define LPCOMP_IRQ_LINE        (INT_CPUIRQ1 - 16U)

static comparator_lpf3_handle *comparator_isr_handle;

static void comparator_lpf3_isr(const void *arg)
{
	comparator_lpf3_handle *handle = comparator_isr_handle;

	LPCMPClearEvent();

	if (handle) {
		if (handle->data->callback) {
			handle->data->callback(handle, handle->data->callback_user_data);
		}
	}
}

int comparator_lpf3_init(comparator_lpf3_handle *handle)
{
	if (handle == NULL) {
		return -EINVAL;
	}

	if ((handle->config == NULL) || (handle->data == NULL)) {
		return -EINVAL;
	}

	if (handle->data->is_open == true) {
		return -EBUSY;
	}

	int irq_key;

	/* Validate configurations. */

	bool valid_configuration = true;

	/* Validate positive input to comparator. */

	if ((handle->config->positive_input != POS_INPUT_A1) &&
	    (handle->config->positive_input != POS_INPUT_A2) &&
	    (handle->config->positive_input != POS_INPUT_A3) &&
	    (handle->config->positive_input != POS_INPUT_VDDS)) {
		valid_configuration = false;
	}

	/* Validate negative input to comparator. */

	if ((handle->config->negative_input != NEG_INPUT_A2) &&
	    (handle->config->negative_input != NEG_INPUT_A3) &&
	    (handle->config->negative_input != NEG_INPUT_VDDD) &&
	    (handle->config->negative_input != NEG_INPUT_VDDS)) {
		valid_configuration = false;
	}

	/* Validate if external pins assigned to positive and negative pins of
	 * the comparator are the same.
	 */

	if (((handle->config->positive_input == POS_INPUT_A2) &&
	     (handle->config->negative_input == NEG_INPUT_A2)) ||
	    ((handle->config->positive_input == POS_INPUT_A3) &&
	     (handle->config->negative_input == NEG_INPUT_A3))) {
		valid_configuration = false;
	}

	/* Validate voltage divider path. */

	if ((handle->config->voltage_divider_path != VDIV_PATH_POS) &&
	    (handle->config->voltage_divider_path != VDIV_PATH_NEG)) {
		valid_configuration = false;
	}

	/* Validate voltage division factor. */

	if ((handle->config->voltage_division_factor != VDIV_FACTOR_1_1) &&
	    (handle->config->voltage_division_factor != VDIV_FACTOR_1_2) &&
	    (handle->config->voltage_division_factor != VDIV_FACTOR_1_3) &&
	    (handle->config->voltage_division_factor != VDIV_FACTOR_1_4) &&
	    (handle->config->voltage_division_factor != VDIV_FACTOR_3_4)) {
		valid_configuration = false;
	}

	/* Check the initial trigger level. */

	if ((handle->config->initial_trigger_level != TRIGGER_NONE) &&
	    (handle->config->initial_trigger_level != TRIGGER_FALLING) &&
	    (handle->config->initial_trigger_level != TRIGGER_RISING)) {
		valid_configuration = false;
	}

	/* If output is enabled, then check if valid output is selected. */

	if (handle->config->output_enable) {
		if ((handle->config->output != OUTPUT_0) && (handle->config->output != OUTPUT_1)) {
			valid_configuration = false;
		}
	}

	/* If configuration is invalid, do not proceed. */

	if (!valid_configuration) {
		handle->data->is_open = false;
		return -EINVAL;
	}

	/* Store initial trigger configuration. */

	handle->data->trigger = handle->config->initial_trigger_level;

	/* Configure interrupts. */

	irq_key = irq_lock();

	if (handle->config->initial_trigger_level != TRIGGER_NONE) {
		EVTSVTConfigureEvent(EVTSVT_SUB_CPUIRQ1, EVTSVT_PUB_AON_LPMCMP_IRQ);
	}

	IRQ_CONNECT(LPCOMP_IRQ_LINE, 0, comparator_lpf3_isr, NULL, 0);

	if (handle->data->trigger != TRIGGER_NONE) {
		irq_enable(LPCOMP_IRQ_LINE);
	}

	/* Set current comparator handle structure. */

	comparator_isr_handle = handle;

	irq_unlock(irq_key);

	/* Disable comparator before configuration. */

	LPCMPDisableEvent();

	/* Set trigger level for comparator. */

	if (handle->data->trigger != TRIGGER_NONE) {
		uint32_t interrupt_config;

		interrupt_config = (handle->data->trigger == TRIGGER_RISING) ? LPCMP_POLARITY_RISE
									     : LPCMP_POLARITY_FALL;

		LPCMPSetPolarity(interrupt_config);
		LPCMPEnableWakeup();
	}

	/* If external pin is used for comparator inputs, configure the external pin. */

	if (handle->config->positive_input != POS_INPUT_VDDS) {
		/*
		 *   In LPF3 devices, A1 is connected to DIO19, A2 is connected to DIO20
		 *   and A3 is connected to DIO21. Refer to TRM for more details.
		 */

		uint32_t gpio_index;

		switch (handle->config->positive_input) {
		case POS_INPUT_A1:
			gpio_index = LPCOMP_POS_INPUT_A1_GPIO;
			break;

		case POS_INPUT_A2:
			gpio_index = LPCOMP_POS_INPUT_A2_GPIO;
			break;

		case POS_INPUT_A3:
			gpio_index = LPCOMP_POS_INPUT_A3_GPIO;
			break;

		default:
			gpio_index = LPCOMP_POS_INPUT_OPEN;
			break;
		}

		IOCSetConfigAndMux(gpio_index, LPCOMP_GPIO_CONFIG, LPCOMP_GPIO_MUX);
	}

	/* Set input to positive terminal of the comparator. */

	LPCMPSelectPositiveInput((uint32_t)handle->config->positive_input);

	/* If external pin is used for comparator inputs, configure the external pin. */

	if ((handle->config->negative_input != NEG_INPUT_VDDS) &&
	    (handle->config->negative_input != NEG_INPUT_VDDD)) {
		/*
		 *   In LPF3 devices, A1 is connected to DIO19, A2 is connected to DIO20
		 *   and A3 is connected to DIO21. Refer to TRM for more details.
		 */

		uint32_t gpio_index;

		switch (handle->config->negative_input) {
		case NEG_INPUT_A2:
			gpio_index = LPCOMP_NEG_INPUT_A2_GPIO;
			break;

		case NEG_INPUT_A3:
			gpio_index = LPCOMP_NEG_INPUT_A3_GPIO;
			break;

		default:
			gpio_index = LPCOMP_NEG_INPUT_OPEN;
			break;
		}

		IOCSetConfigAndMux(gpio_index, LPCOMP_GPIO_CONFIG, LPCOMP_GPIO_MUX);
	}

	/* Set input to negative terminal for the comparator. */

	LPCMPSelectNegativeInput((uint32_t)handle->config->negative_input);

	/* If user has enabled comparator output, then configure relevant GPIOs. */

	if (handle->config->output_enable) {
		if (handle->config->output == OUTPUT_0) {
			IOCSetConfigAndMux(LPCOMP_OUTPUT_0_GPIO, 0, LPCOMP_OUTPUT_GPIO_MUX);
		} else {
			IOCSetConfigAndMux(LPCOMP_OUTPUT_1_GPIO, 0, LPCOMP_OUTPUT_GPIO_MUX);
		}

		/* Enable comparator output to GPIO pad. */

		HWREG(SYS0_BASE + SYS0_O_LPCMPCFG) |= SYS0_LPCMPCFG_COUTEN_M;
	} else {
		/* Disable comparator output to GPIO pad. */

		HWREG(SYS0_BASE + SYS0_O_LPCMPCFG) &= ~SYS0_LPCMPCFG_COUTEN_M;
	}

	/* Set voltage division ratio and which terminal the voltage division should be
	 * applied to.
	 */

	LPCMPSetDividerRatio((uint32_t)handle->config->voltage_division_factor);
	LPCMPSetDividerPath((uint32_t)handle->config->voltage_divider_path);

	handle->data->is_open = true;

	/* Enable the comparator and wait for 110us for the comparator to
	 *  get activated.
	 */

	LPCMPEnable();
	HapiWaitUs(LPCOMP_SETTLING_TIME);
	LPCMPClearEvent();

	/* If the comparator trigger is set to TRIGGER_RISING or TRIGGER_FALLING,
	 * enable interrupts only if CONFIG_LPF3_ENABLE_COMPARATOR_INTERRUPT is
	 * set to 'y'.
	 */

#if CONFIG_LPF3_ENABLE_COMPARATOR_INTERRUPT

	if (handle->data->trigger != TRIGGER_NONE) {
		LPCMPEnableEvent();
	}

#endif /* CONFIG_LPF3_ENABLE_COMPARATOR_INTERRUPT */

	return 0;
}

int comparator_lpf3_get_output(comparator_lpf3_handle *handle)
{
	int irq_key;
	int level;

	irq_key = irq_lock();

	if (handle == NULL) {
		irq_unlock(irq_key);
		return -EINVAL;
	}

	if ((handle->config == NULL) || (handle->data == NULL)) {
		irq_unlock(irq_key);
		return -EINVAL;
	}

	if (handle->data->is_open == false) {
		irq_unlock(irq_key);
		return -EBUSY;
	}

	level = (int)LPCMPIsOutputHigh();

	irq_unlock(irq_key);

	return level;
}

int comparator_lpf3_set_trigger(comparator_lpf3_handle *handle, comparator_lpf3_trigger trigger)
{
	if (handle == NULL) {
		return -EINVAL;
	}

	if ((handle->config == NULL) || (handle->data == NULL)) {
		return -EINVAL;
	}

	if ((trigger != TRIGGER_NONE) && (trigger != TRIGGER_RISING) &&
	    (trigger != TRIGGER_FALLING)) {
		return -EINVAL;
	}

	if (!handle->data->is_open) {
		return -EBUSY;
	}

	LPCMPDisableEvent();
	LPCMPDisable();

	if (trigger == TRIGGER_NONE) {
		irq_disable(LPCOMP_IRQ_LINE);
		EVTSVTConfigureEvent(EVTSVT_SUB_CPUIRQ1, 0);
		LPCMPDisableWakeup();
	} else {
		LPCMPSetPolarity((trigger == TRIGGER_FALLING) ? LPCMP_POLARITY_FALL
							      : LPCMP_POLARITY_RISE);
		LPCMPEnableWakeup();

		if (handle->data->trigger == TRIGGER_NONE) {
			irq_enable(LPCOMP_IRQ_LINE);
			EVTSVTConfigureEvent(EVTSVT_SUB_CPUIRQ1, EVTSVT_PUB_AON_LPMCMP_IRQ);
		}
	}

	handle->data->trigger = trigger;

	LPCMPEnable();
	HapiWaitUs(LPCOMP_SETTLING_TIME);
	LPCMPClearEvent();
	HapiWaitUs(LPCOMP_SETTLING_TIME);

#if CONFIG_LPF3_ENABLE_COMPARATOR_INTERRUPT
	if (trigger != TRIGGER_NONE) {
		LPCMPEnableEvent();
	}
#endif /* CONFIG_LPF3_ENABLE_COMPARATOR_INTERRUPT */

	return 0;
}

int comparator_lpf3_set_trigger_callback(comparator_lpf3_handle *handle,
					 comparator_lpf3_callback callback, void *user_data)
{
	if (handle == NULL) {
		return -EINVAL;
	}

	if ((handle->config == NULL) || (handle->data == NULL)) {
		return -EINVAL;
	}

	if (handle->data->is_open == false) {
		return -EBUSY;
	}

	if (callback == NULL) {
		return -EINVAL;
	}

	handle->data->callback = callback;
	handle->data->callback_user_data = user_data;

	return 0;
}

int comparator_lpf3_trigger_is_pending(comparator_lpf3_handle *handle)
{
	if (handle == NULL) {
		return -EINVAL;
	}

	if ((handle->config == NULL) || (handle->data == NULL)) {
		return -EINVAL;
	}

	if (handle->data->is_open == false) {
		return -EBUSY;
	}

	if (HWREG(SYS0_BASE + SYS0_O_LPCMPCFG) & SYS0_LPCMPCFG_EVTIFG) {
		LPCMPClearEvent();
		return 1;
	}

	return 0;
}

int comparator_lpf3_deinit(comparator_lpf3_handle *handle)
{
	int irq_key;

	irq_key = irq_lock();

	if (handle == NULL) {
		irq_unlock(irq_key);
		return -EINVAL;
	}

	if ((handle->config == NULL) || (handle->data == NULL)) {
		irq_unlock(irq_key);
		return -EINVAL;
	}

	if (!handle->data->is_open) {
		irq_unlock(irq_key);
		return -EBUSY;
	}

	LPCMPDisableEvent();
	LPCMPDisable();

	handle->data->is_open = false;
	handle->data->callback_user_data = NULL;
	handle->data->callback = NULL;

	comparator_isr_handle = NULL;

	LPCMPClearEvent();

#if CONFIG_LPF3_ENABLE_COMPARATOR_INTERRUPT

	irq_disable(LPCOMP_IRQ_LINE);
	EVTSVTConfigureEvent(EVTSVT_SUB_CPUIRQ1, 0);

#endif /* CONFIG_LPF3_ENABLE_COMPARATOR_INTERRUPT */

	HWREG(SYS0_BASE + SYS0_O_LPCMPCFG) = 0U;

	irq_unlock(irq_key);

	return 0;
}
