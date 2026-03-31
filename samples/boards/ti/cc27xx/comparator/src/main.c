/*
 *		Demo application for comparator driver for CC27xx devices.
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <driverlib/lpcmp.h>
#include <driverlib/evtsvt.h>
#include <driverlib/ioc.h>
#include <driverlib/gpio.h>
#include "comparator_lpf3.h"

#define COMPARATOR_COMMON_TEST_CONFIG_COUT ARRAY_SIZE(comparator_common_test_configurations)

#define LPCOMP_POSITIVE_TERMINAL_NAME(x)                                                           \
	(x == POS_INPUT_A1)   ? "POS_INPUT_A1 (DIO19)"                                             \
	: (x == POS_INPUT_A2) ? "POS_INPUT_A2 (DIO20)"                                             \
	: (x == POS_INPUT_A3) ? "POS_INPUT_A3 (DIO21)"                                             \
			      : "INVALID"

#define LPCOMP_NEGATIVE_TERMINAL_NAME(x)                                                           \
	(x == NEG_INPUT_A2)   ? "NEG_INPUT_A2 (DIO20)"                                             \
	: (x == NEG_INPUT_A3) ? "NEG_INPUT_A3 (DIO21)"                                             \
			      : "INVALID"

#define LPCOMP_OUTPUT_TERMINAL_NAME(x)                                                             \
	(x == OUTPUT_NONE) ? "OUTPUT_NONE"                                                         \
	: (x == OUTPUT_0)  ? "OUTPUT_0 (DIO0)"                                                     \
	: (x == OUTPUT_1)  ? "OUTPUT_1 (DIO15)"                                                    \
			   : "INVALID"

#define LPCOMP_VALIDATE_RESULT(positive, negative, output)                                         \
	(positive > negative) ? ((output == 1) ? 1 : 0) : ((output == 0) ? 1 : 0)

#define LPCOMP_VALIDATE_RESULT_STR(positive, negative, output)                                     \
	(positive > negative) ? ((output == 1) ? "PASSED" : "FAILED")                              \
			      : ((output == 0) ? "PASSED" : "FAILED")

#define LPCOMP_CONFIG(_positive_input, _negative_input, _divider_path, _division_factor,           \
		      _trigger_level, _output_enable, _output)                                     \
	{.positive_input = _positive_input,                                                        \
	 .negative_input = _negative_input,                                                        \
	 .voltage_divider_path = _divider_path,                                                    \
	 .voltage_division_factor = _division_factor,                                              \
	 .initial_trigger_level = _trigger_level,                                                  \
	 .output_enable = _output_enable,                                                          \
	 .output = _output}

/* Comparator test I/Os. */

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

const struct gpio_dt_spec lpcomp_test_inputs[] = {
	GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, lpcomp_input_0_gpios),
	GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, lpcomp_input_1_gpios),
	GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, lpcomp_input_2_gpios),
};

const struct gpio_dt_spec lpcomp_test_outputs[] = {
	GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, lpcomp_output_0_gpios),
	GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, lpcomp_output_1_gpios),
};

#define LPCOMP_TEST_ITERATION_INTERVAL_MS 100U

/* Test configurations. */

const comparator_lpf3_config comparator_common_test_configurations[] = {

	LPCOMP_CONFIG(POS_INPUT_A1, NEG_INPUT_A2, VDIV_PATH_NEG, VDIV_FACTOR_1_1, TRIGGER_NONE,
		      false, OUTPUT_NONE),
	LPCOMP_CONFIG(POS_INPUT_A1, NEG_INPUT_A3, VDIV_PATH_NEG, VDIV_FACTOR_1_1, TRIGGER_NONE,
		      false, OUTPUT_NONE),
	LPCOMP_CONFIG(POS_INPUT_A2, NEG_INPUT_A3, VDIV_PATH_NEG, VDIV_FACTOR_1_1, TRIGGER_NONE,
		      false, OUTPUT_NONE),
	LPCOMP_CONFIG(POS_INPUT_A3, NEG_INPUT_A2, VDIV_PATH_NEG, VDIV_FACTOR_1_1, TRIGGER_NONE,
		      false, OUTPUT_NONE),
	LPCOMP_CONFIG(POS_INPUT_A1, NEG_INPUT_A2, VDIV_PATH_NEG, VDIV_FACTOR_1_1, TRIGGER_NONE,
		      true, OUTPUT_0),
	LPCOMP_CONFIG(POS_INPUT_A1, NEG_INPUT_A3, VDIV_PATH_NEG, VDIV_FACTOR_1_1, TRIGGER_NONE,
		      true, OUTPUT_0),
	LPCOMP_CONFIG(POS_INPUT_A2, NEG_INPUT_A3, VDIV_PATH_NEG, VDIV_FACTOR_1_1, TRIGGER_NONE,
		      true, OUTPUT_0),
	LPCOMP_CONFIG(POS_INPUT_A3, NEG_INPUT_A2, VDIV_PATH_NEG, VDIV_FACTOR_1_1, TRIGGER_NONE,
		      true, OUTPUT_0),
	LPCOMP_CONFIG(POS_INPUT_A1, NEG_INPUT_A2, VDIV_PATH_NEG, VDIV_FACTOR_1_1, TRIGGER_NONE,
		      true, OUTPUT_1),
	LPCOMP_CONFIG(POS_INPUT_A1, NEG_INPUT_A3, VDIV_PATH_NEG, VDIV_FACTOR_1_1, TRIGGER_NONE,
		      true, OUTPUT_1),
	LPCOMP_CONFIG(POS_INPUT_A2, NEG_INPUT_A3, VDIV_PATH_NEG, VDIV_FACTOR_1_1, TRIGGER_NONE,
		      true, OUTPUT_1),
	LPCOMP_CONFIG(POS_INPUT_A3, NEG_INPUT_A2, VDIV_PATH_NEG, VDIV_FACTOR_1_1, TRIGGER_NONE,
		      true, OUTPUT_1),
};

/* Comparator object structure. */

comparator_lpf3_data comparator_data;

/* Comparator handle structure. */

comparator_lpf3_handle comparator_handle;

/* Comparator event counter. */

volatile int comparator_event_count;

/* Pass test case count. */

int test_pass_count;

/* Failed test count. */

int test_fail_count;

/* Callback function for comparator trigger event. */

static void comparator_callback(comparator_lpf3_handle *handle, void *args)
{
	comparator_event_count++;
	printf("Comparator: event_trigger_with_interrupts_test: Comparator callback triggered for "
	       "event.\n");
}

/* TEST 1: Input combination tests. */

static void __attribute__((used)) comparator_input_combination_test(void)
{
	int index = 0;
	int comparator_input_combination = 0;
	int ret = 0;
	const struct gpio_dt_spec *lpcomp_positive_input_control;
	const struct gpio_dt_spec *lpcomp_negative_input_control;
	const struct gpio_dt_spec *lpcomp_output_probe;

	printf("Comparator: input_combination_test: Starting test.\n");
	printf("Comparator: input_combination_test: Number of test iterations: %d\n",
	       COMPARATOR_COMMON_TEST_CONFIG_COUT);

	comparator_handle.data = &comparator_data;

	for (index = 0; index < COMPARATOR_COMMON_TEST_CONFIG_COUT; index++) {
		memset(&comparator_data, 0, sizeof(comparator_lpf3_data));

		comparator_handle.config = &comparator_common_test_configurations[index];

		/* Select control GPIOs and probe GPIOs for the comparator inputs and output. */

		switch (comparator_handle.config->positive_input) {
		case POS_INPUT_A1:
			lpcomp_positive_input_control = &lpcomp_test_inputs[0];
			break;

		case POS_INPUT_A2:
			lpcomp_positive_input_control = &lpcomp_test_inputs[1];
			break;

		case POS_INPUT_A3:
			lpcomp_positive_input_control = &lpcomp_test_inputs[2];
			break;

		default:
			lpcomp_positive_input_control = NULL;
		}

		switch (comparator_handle.config->negative_input) {
		case NEG_INPUT_A2:
			lpcomp_negative_input_control = &lpcomp_test_inputs[1];
			break;

		case NEG_INPUT_A3:
			lpcomp_negative_input_control = &lpcomp_test_inputs[2];
			break;

		default:
			lpcomp_negative_input_control = NULL;
		}

		switch (comparator_handle.config->output) {
		case OUTPUT_0:
			lpcomp_output_probe = &lpcomp_test_outputs[0];
			break;

		case OUTPUT_1:
			lpcomp_output_probe = &lpcomp_test_outputs[1];
			break;

		default:
			lpcomp_output_probe = NULL;
			break;
		}

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 0);

		/* Initialize comparator. */

		ret = comparator_lpf3_init(&comparator_handle);

		if (!ret) {
			printf("Comparator: input_combination_test: Comparator initialized "
			       "successfully.\n");
		} else {
			printf("Comparator: input_combination_test: Comparator failed to "
			       "initialize "
			       "(reason: %d)\n",
			       ret);
		}

		printf("Comparator: input_combination_test: Comparator positive input: %s\n",
		       LPCOMP_POSITIVE_TERMINAL_NAME(comparator_handle.config->positive_input));

		printf("Comparator: input_combination_test: Comparator negative input: %s\n",
		       LPCOMP_NEGATIVE_TERMINAL_NAME(comparator_handle.config->negative_input));

		printf("Comparator: input_combination_test: Comparator output: %s\n",
		       LPCOMP_OUTPUT_TERMINAL_NAME(comparator_handle.config->output));

		/* Iterate through all possible input combinations and
		 * validate comparator output.
		 */

		uint32_t positive_input_value;
		uint32_t negative_input_value;

		for (comparator_input_combination = 0; comparator_input_combination < 4;
		     comparator_input_combination++) {

			positive_input_value = (comparator_input_combination & 1);
			negative_input_value = ((comparator_input_combination >> 1) & 1);

			printf("Comparator: input_combination_test: Comparator positive input "
			       "value: "
			       "%d\n",
			       positive_input_value);

			printf("Comparator: input_combination_test: Comparator negative input "
			       "value: "
			       "%d\n",
			       negative_input_value);

			/* Set comparator positive & negative terminal inputs. */

			gpio_pin_set_dt(lpcomp_positive_input_control, positive_input_value);
			gpio_pin_set_dt(lpcomp_negative_input_control, negative_input_value);

			/* Get the comparator output. */

			k_msleep(10);

			ret = comparator_lpf3_get_output(&comparator_handle);

			if ((ret == 0) || (ret == 1)) {
				printf("Comparator: input_combination_test: Comparator output "
				       "value: "
				       "%d\n",
				       ret);

				/* Validate outputs. */

				if (positive_input_value != negative_input_value) {
					printf("Comparator: input_combination_test: POS_INPUT= "
					       "%d, NEG_INPUT = %d, OUTPUT = %d : [%s]\n",
					       positive_input_value, negative_input_value, ret,
					       LPCOMP_VALIDATE_RESULT_STR(positive_input_value,
									  negative_input_value,
									  ret));

					LPCOMP_VALIDATE_RESULT(positive_input_value,
							       negative_input_value, ret)
					? (test_pass_count++) : (test_fail_count++);
				}

			} else {
				printf("Comparator: input_combination_test: Failed to get "
				       "comparator "
				       "output (reason: %d)\n",
				       ret);
			}

			/* If comparator output is mapped to a physical pin, verify pin output. */

			if (comparator_handle.config->output != OUTPUT_NONE) {
				ret = gpio_pin_get_dt(lpcomp_output_probe);

				if ((ret == 0) || (ret == 1)) {
					printf("Comparator: input_combination_test: Comparator "
					       "output "
					       "value at physical pin: %d\n",
					       ret);

					/* Validate outputs. */

					if (positive_input_value != negative_input_value) {
						printf("Comparator: input_combination_test: "
						       "POS_INPUT= "
						       "%d, NEG_INPUT = %d, OUTPUT(GPIO) = %d : "
						       "[%s]\n",
						       positive_input_value, negative_input_value,
						       ret,
						       LPCOMP_VALIDATE_RESULT_STR(
							       positive_input_value,
							       negative_input_value, ret));

						LPCOMP_VALIDATE_RESULT(positive_input_value,
								       negative_input_value, ret)
						? (test_pass_count++) : (test_fail_count++);
					}
				} else {
					printf("Comparator: input_combination_test: Failed to get "
					       "comparator output at physical pin (reason: %d)\n",
					       ret);
				}
			}

			k_msleep(LPCOMP_TEST_ITERATION_INTERVAL_MS);
		}

		/* Deinitialize comparator. */

		ret = comparator_lpf3_deinit(&comparator_handle);

		if (!ret) {
			printf("Comparator: input_combination_test: Comparator deinitialized "
			       "successfully.\n");
		} else {
			printf("Comparator: input_combination_test: Comparator failed to "
			       "deinitialize "
			       "(reason: %d)\n",
			       ret);
		}

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 0);

		k_msleep(1000);
	}
}

/* TEST 2: Comparator event trigger test (no interrupts) */

static void __attribute__((used)) comparator_event_trigger_without_interrupts_test(void)
{
	int index = 0;
	int ret = 0;
	const struct gpio_dt_spec *lpcomp_positive_input_control;
	const struct gpio_dt_spec *lpcomp_negative_input_control;
	const struct gpio_dt_spec *lpcomp_output_probe;

	printf("Comparator: event_trigger_without_interrupts_test: Starting test.\n");
	printf("Comparator: event_trigger_without_interrupts_test: Number of test iterations: %d\n",
	       COMPARATOR_COMMON_TEST_CONFIG_COUT);

	comparator_handle.data = &comparator_data;

	for (index = 0; index < COMPARATOR_COMMON_TEST_CONFIG_COUT; index++) {
		memset(&comparator_data, 0, sizeof(comparator_lpf3_data));

		comparator_handle.config = &comparator_common_test_configurations[index];

		if (!ret) {
			printf("Comparator: event_trigger_without_interrupts_test: Comparator "
			       "initialized successfully.\n");
		} else {
			printf("Comparator: event_trigger_without_interrupts_test: Comparator "
			       "failed to initialize (reason: %d)\n",
			       ret);
		}

		/* Select control GPIOs and probe GPIOs for the comparator inputs and output. */

		switch (comparator_handle.config->positive_input) {
		case POS_INPUT_A1:
			lpcomp_positive_input_control = &lpcomp_test_inputs[0];
			break;

		case POS_INPUT_A2:
			lpcomp_positive_input_control = &lpcomp_test_inputs[1];
			break;

		case POS_INPUT_A3:
			lpcomp_positive_input_control = &lpcomp_test_inputs[2];
			break;

		default:
			lpcomp_positive_input_control = NULL;
		}

		switch (comparator_handle.config->negative_input) {
		case NEG_INPUT_A2:
			lpcomp_negative_input_control = &lpcomp_test_inputs[1];
			break;

		case NEG_INPUT_A3:
			lpcomp_negative_input_control = &lpcomp_test_inputs[2];
			break;

		default:
			lpcomp_negative_input_control = NULL;
		}

		switch (comparator_handle.config->output) {
		case OUTPUT_0:
			lpcomp_output_probe = &lpcomp_test_outputs[0];
			break;

		case OUTPUT_1:
			lpcomp_output_probe = &lpcomp_test_outputs[1];
			break;

		default:
			lpcomp_output_probe = NULL;
			break;
		}

		printf("Comparator: event_trigger_without_interrupts_test: Comparator positive "
		       "input: %s\n",
		       LPCOMP_POSITIVE_TERMINAL_NAME(comparator_handle.config->positive_input));

		printf("Comparator: event_trigger_without_interrupts_test: Comparator negative "
		       "input: %s\n",
		       LPCOMP_NEGATIVE_TERMINAL_NAME(comparator_handle.config->negative_input));

		printf("Comparator: event_trigger_without_interrupts_test: Comparator output: %s\n",
		       LPCOMP_OUTPUT_TERMINAL_NAME(comparator_handle.config->output));

		printf("Comparator: event_trigger_without_interrupts_test: Configuring trigger to "
		       "NONE.\n");

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 0);

		k_msleep(10);

		/* Initialize comparator. */

		ret = comparator_lpf3_init(&comparator_handle);

		k_msleep(10);

		/* Set comparator trigger to none. */

		comparator_lpf3_set_trigger(&comparator_handle, TRIGGER_NONE);

		k_msleep(10);

		(void)comparator_lpf3_trigger_is_pending(&comparator_handle);

		k_msleep(10);

		/* Produce a rising edge event. */

		gpio_pin_set_dt(lpcomp_positive_input_control, 1);
		gpio_pin_set_dt(lpcomp_negative_input_control, 0);

		k_msleep(10);

		ret = comparator_lpf3_trigger_is_pending(&comparator_handle);

		if (!ret) {
			printf("Comparator: event_trigger_without_interrupts_test: Event not "
			       "generated when trigger is none and rising edge event occurred: "
			       "[PASSED]\n");

			test_pass_count++;
		} else {
			printf("Comparator: event_trigger_without_interrupts_test: Event not "
			       "generated when trigger is none and rising edge event occurred: "
			       "[FAILED]\n");

			test_fail_count++;
		}

		/* Produce a falling edge event. */

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 1);

		k_msleep(10);

		ret = comparator_lpf3_trigger_is_pending(&comparator_handle);

		if (!ret) {
			printf("Comparator: event_trigger_without_interrupts_test: Event not "
			       "generated when trigger is none and falling edge event occurred: "
			       "[PASSED]\n");

			test_pass_count++;
		} else {
			printf("Comparator: event_trigger_without_interrupts_test: Event not "
			       "generated when trigger is none and falling edge event occurred: "
			       "[FAILED]\n");

			test_fail_count++;
		}

		/* Set comparator trigger to rising. */

		printf("Comparator: event_trigger_without_interrupts_test: Configuring trigger to "
		       "RISING.\n");

		comparator_lpf3_set_trigger(&comparator_handle, TRIGGER_RISING);

		k_msleep(10);

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 1);

		k_msleep(10);

		(void)comparator_lpf3_trigger_is_pending(&comparator_handle);

		k_msleep(10);

		/* Produce a rising edge event. */

		gpio_pin_set_dt(lpcomp_positive_input_control, 1);
		gpio_pin_set_dt(lpcomp_negative_input_control, 0);

		k_msleep(10);

		ret = comparator_lpf3_trigger_is_pending(&comparator_handle);

		if (ret) {
			printf("Comparator: event_trigger_without_interrupts_test: Event generated "
			       "when trigger is none and rising edge event occurred: [PASSED]\n");

			test_pass_count++;
		} else {
			printf("Comparator: event_trigger_without_interrupts_test: Event generated "
			       "when trigger is none and rising edge event occurred: [FAILED]\n");

			test_fail_count++;
		}

		/* Produce a falling edge event. */

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 1);

		k_msleep(10);

		ret = comparator_lpf3_trigger_is_pending(&comparator_handle);

		if (!ret) {
			printf("Comparator: event_trigger_without_interrupts_test: Event not "
			       "generated when trigger is none and falling edge event occurred: "
			       "[PASSED]\n");

			test_pass_count++;
		} else {
			printf("Comparator: event_trigger_without_interrupts_test: Event not "
			       "generated when trigger is none and falling edge event occurred: "
			       "[FAILED]\n");

			test_fail_count++;
		}

		/* Set comparator trigger to falling. */

		printf("Comparator: event_trigger_without_interrupts_test: Configuring trigger to "
		       "FALLING.\n");

		comparator_lpf3_set_trigger(&comparator_handle, TRIGGER_FALLING);

		k_msleep(10);

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 1);

		k_msleep(10);

		(void)comparator_lpf3_trigger_is_pending(&comparator_handle);

		k_msleep(10);

		/* Produce a rising edge event. */

		gpio_pin_set_dt(lpcomp_positive_input_control, 1);
		gpio_pin_set_dt(lpcomp_negative_input_control, 0);

		k_msleep(10);

		ret = comparator_lpf3_trigger_is_pending(&comparator_handle);

		if (!ret) {
			printf("Comparator: event_trigger_without_interrupts_test: Event not "
			       "generated when trigger is none and rising edge event occurred: "
			       "[PASSED]\n");

			test_pass_count++;
		} else {
			printf("Comparator: event_trigger_without_interrupts_test: Event not "
			       "generated when trigger is none and rising edge event occurred: "
			       "[FAILED]\n");

			test_fail_count++;
		}

		/* Produce a falling edge event. */

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 1);

		k_msleep(10);

		ret = comparator_lpf3_trigger_is_pending(&comparator_handle);

		if (ret) {
			printf("Comparator: event_trigger_without_interrupts_test: Event generated "
			       "when trigger is none and falling edge event occurred: [PASSED]\n");

			test_pass_count++;
		} else {
			printf("Comparator: event_trigger_without_interrupts_test: Event generated "
			       "when trigger is none and falling edge event occurred: [FAILED]\n");

			test_fail_count++;
		}

		/* Deinitialize comparator. */

		ret = comparator_lpf3_deinit(&comparator_handle);

		if (!ret) {
			printf("Comparator: input_combination_test: Comparator deinitialized "
			       "successfully.\n");
		} else {
			printf("Comparator: input_combination_test: Comparator failed to "
			       "deinitialize "
			       "(reason: %d)\n",
			       ret);
		}

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 0);

		k_msleep(1000);
	}
}

/* TEST 3: Comparator event trigger test (with interrupts) */

static void __attribute__((used)) comparator_event_trigger_with_interrupts_test(void)
{
	int index = 0;
	int ret = 0;
	const struct gpio_dt_spec *lpcomp_positive_input_control;
	const struct gpio_dt_spec *lpcomp_negative_input_control;
	const struct gpio_dt_spec *lpcomp_output_probe;

	printf("Comparator: event_trigger_with_interrupts_test: Starting test.\n");
	printf("Comparator: event_trigger_with_interrupts_test: Number of test iterations: %d\n",
	       COMPARATOR_COMMON_TEST_CONFIG_COUT);

	comparator_handle.data = &comparator_data;

	for (index = 0; index < COMPARATOR_COMMON_TEST_CONFIG_COUT; index++) {
		memset(&comparator_data, 0, sizeof(comparator_lpf3_data));

		comparator_handle.config = &comparator_common_test_configurations[index];

		/* Select control GPIOs and probe GPIOs for the comparator inputs and output. */

		switch (comparator_handle.config->positive_input) {
		case POS_INPUT_A1:
			lpcomp_positive_input_control = &lpcomp_test_inputs[0];
			break;

		case POS_INPUT_A2:
			lpcomp_positive_input_control = &lpcomp_test_inputs[1];
			break;

		case POS_INPUT_A3:
			lpcomp_positive_input_control = &lpcomp_test_inputs[2];
			break;

		default:
			lpcomp_positive_input_control = NULL;
		}

		switch (comparator_handle.config->negative_input) {
		case NEG_INPUT_A2:
			lpcomp_negative_input_control = &lpcomp_test_inputs[1];
			break;

		case NEG_INPUT_A3:
			lpcomp_negative_input_control = &lpcomp_test_inputs[2];
			break;

		default:
			lpcomp_negative_input_control = NULL;
		}

		switch (comparator_handle.config->output) {
		case OUTPUT_0:
			lpcomp_output_probe = &lpcomp_test_outputs[0];
			break;

		case OUTPUT_1:
			lpcomp_output_probe = &lpcomp_test_outputs[1];
			break;

		default:
			lpcomp_output_probe = NULL;
			break;
		}

		printf("Comparator: event_trigger_with_interrupts_test: Comparator positive input: "
		       "%s\n",
		       LPCOMP_POSITIVE_TERMINAL_NAME(comparator_handle.config->positive_input));

		printf("Comparator: event_trigger_with_interrupts_test: Comparator negative input: "
		       "%s\n",
		       LPCOMP_NEGATIVE_TERMINAL_NAME(comparator_handle.config->negative_input));

		printf("Comparator: event_trigger_with_interrupts_test: Comparator output: %s\n",
		       LPCOMP_OUTPUT_TERMINAL_NAME(comparator_handle.config->output));

		printf("Comparator: event_trigger_with_interrupts_test: Configuring trigger to "
		       "NONE.\n");

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 0);

		k_msleep(10);

		/* Initialize comparator. */

		ret = comparator_lpf3_init(&comparator_handle);

		comparator_lpf3_set_trigger_callback(&comparator_handle, comparator_callback, NULL);

		if (!ret) {
			printf("Comparator: event_trigger_with_interrupts_test: Comparator "
			       "initialized successfully.\n");
		} else {
			printf("Comparator: event_trigger_with_interrupts_test: Comparator failed "
			       "to initialize (reason: %d)\n",
			       ret);
		}

		/* Set comparator trigger to none. */

		comparator_lpf3_set_trigger(&comparator_handle, TRIGGER_NONE);

		k_msleep(10);

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 1);

		(void)comparator_lpf3_trigger_is_pending(&comparator_handle);

		k_msleep(10);

		/* Produce a rising edge event. */

		gpio_pin_set_dt(lpcomp_positive_input_control, 1);
		gpio_pin_set_dt(lpcomp_negative_input_control, 0);

		k_msleep(10);

		if (!comparator_event_count) {
			printf("Comparator: event_trigger_with_interrupts_test: Event not "
			       "generated when trigger is none and rising edge event occurred: "
			       "[PASSED]\n");

			test_pass_count++;
		} else {
			printf("Comparator: event_trigger_with_interrupts_test: Event not "
			       "generated when trigger is none and rising edge event occurred: "
			       "[FAILED]\n");

			test_fail_count++;
		}

		comparator_event_count = 0;

		/* Produce a falling edge event. */

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 1);

		k_msleep(10);

		if (!comparator_event_count) {
			printf("Comparator: event_trigger_with_interrupts_test: Event not "
			       "generated when trigger is none and falling edge event occurred: "
			       "[PASSED]\n");

			test_pass_count++;
		} else {
			printf("Comparator: event_trigger_with_interrupts_test: Event not "
			       "generated when trigger is none and falling edge event occurred: "
			       "[FAILED]\n");

			test_fail_count++;
		}

		comparator_event_count = 0;

		/* Set comparator trigger to rising. */

		printf("Comparator: event_trigger_without_interrupts_test: Configuring trigger to "
		       "RISING.\n");

		comparator_lpf3_set_trigger(&comparator_handle, TRIGGER_RISING);

		k_msleep(10);

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 1);

		(void)comparator_lpf3_trigger_is_pending(&comparator_handle);

		k_msleep(10);

		/* Produce a rising edge event. */

		gpio_pin_set_dt(lpcomp_positive_input_control, 1);
		gpio_pin_set_dt(lpcomp_negative_input_control, 0);

		k_msleep(10);

		if (comparator_event_count) {
			printf("Comparator: event_trigger_with_interrupts_test: Event generated "
			       "when trigger is none and rising edge event occurred: [PASSED]\n");

			test_pass_count++;
		} else {
			printf("Comparator: event_trigger_with_interrupts_test: Event generated "
			       "when trigger is none and rising edge event occurred: [FAILED]\n");

			test_fail_count++;
		}

		comparator_event_count = 0;

		/* Produce a falling edge event. */

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 1);

		k_msleep(10);

		if (!comparator_event_count) {
			printf("Comparator: event_trigger_with_interrupts_test: Event not "
			       "generated when trigger is none and falling edge event occurred: "
			       "[PASSED]\n");

			test_pass_count++;
		} else {
			printf("Comparator: event_trigger_with_interrupts_test: Event not "
			       "generated when trigger is none and falling edge event occurred: "
			       "[FAILED]\n");

			test_fail_count++;
		}

		comparator_event_count = 0;

		/* Set comparator trigger to falling. */

		printf("Comparator: event_trigger_without_interrupts_test: Configuring trigger to "
		       "FALLING.\n");

		comparator_lpf3_set_trigger(&comparator_handle, TRIGGER_FALLING);

		k_msleep(10);

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 1);

		(void)comparator_lpf3_trigger_is_pending(&comparator_handle);

		k_msleep(10);

		/* Produce a rising edge event. */

		gpio_pin_set_dt(lpcomp_positive_input_control, 1);
		gpio_pin_set_dt(lpcomp_negative_input_control, 0);

		k_msleep(10);

		if (!comparator_event_count) {
			printf("Comparator: event_trigger_with_interrupts_test: Event not "
			       "generated when trigger is none and rising edge event occurred: "
			       "[PASSED]\n");

			test_pass_count++;
		} else {
			printf("Comparator: event_trigger_with_interrupts_test: Event not "
			       "generated when trigger is none and rising edge event occurred: "
			       "[FAILED]\n");

			test_fail_count++;
		}

		comparator_event_count = 0;

		/* Produce a falling edge event. */

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 1);

		k_msleep(10);

		if (comparator_event_count) {
			printf("Comparator: event_trigger_with_interrupts_test: Event generated "
			       "when trigger is none and falling edge event occurred: [PASSED]\n");

			test_pass_count++;
		} else {
			printf("Comparator: event_trigger_with_interrupts_test: Event generated "
			       "when trigger is none and falling edge event occurred: [FAILED]\n");

			test_fail_count++;
		}

		comparator_event_count = 0;

		if (!comparator_event_count) {
			printf("Comparator: event_trigger_with_interrupts_test: Comparator "
			       "deinitialized successfully.\n");
		} else {
			printf("Comparator: event_trigger_with_interrupts_test: Comparator failed "
			       "to deinitialize (reason: %d)\n",
			       ret);
		}

		/* Deinitialize comparator. */

		ret = comparator_lpf3_deinit(&comparator_handle);

		comparator_event_count = 0;

		gpio_pin_set_dt(lpcomp_positive_input_control, 0);
		gpio_pin_set_dt(lpcomp_negative_input_control, 1);

		k_msleep(1000);
	}
}

int main(void)
{
	int index = 0;

	/* Configure LPCOMP test inputs. */

	for (index = 0; index < ARRAY_SIZE(lpcomp_test_inputs); index++) {
		if (!gpio_is_ready_dt(&lpcomp_test_inputs[index])) {
			return -EIO;
		}

		if (gpio_pin_configure_dt(&lpcomp_test_inputs[index],
					  GPIO_OUTPUT | GPIO_OUTPUT_LOW)) {
			return -EIO;
		}
	}

	/* Configure LPCOMP test outputs. */

	for (index = 0; index < ARRAY_SIZE(lpcomp_test_outputs); index++) {
		if (!gpio_is_ready_dt(&lpcomp_test_outputs[index])) {
			return -EIO;
		}

		if (gpio_pin_configure_dt(&lpcomp_test_outputs[index], GPIO_INPUT)) {
			return -EIO;
		}
	}

#if CONFIG_LPCOMP_INPUT_COMBINATION_TEST_ENABLE
	comparator_input_combination_test();
#endif /* CONFIG_LPCOMP_INPUT_COMBINATION_TEST_ENABLE */

#if CONFIG_LPCOMP_EVENT_TEST_ENABLE
#if CONFIG_LPF3_ENABLE_COMPARATOR_INTERRUPT
	comparator_event_trigger_with_interrupts_test();
#else
	comparator_event_trigger_without_interrupts_test();
#endif /* CONFIG_LPF3_ENABLE_COMPARATOR_INTERRUPT */
#endif /* CONFIG_LPCOMP_EVENT_TEST_ENABLE */

	printf("\n\n======== TEST SUMMARY ========\n\n");
	printf("Total number of tests: %d\n", test_pass_count + test_fail_count);
	printf("Number of passed tests: %d\n", test_pass_count);
	printf("Number of failed tests: %d\n", test_fail_count);

	return 0;
}
