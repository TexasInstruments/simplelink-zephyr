#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <driverlib/lpcmp.h>
#include <driverlib/evtsvt.h>
#include <driverlib/ioc.h>
#include <driverlib/gpio.h>
#include <driverlib/hapi.h>
#include <inc/hw_ints.h>

/* Comparator positive input channels. */

typedef enum {
	POS_INPUT_A1 = LPCMP_POS_INPUT_A1,
	POS_INPUT_A2 = LPCMP_POS_INPUT_A2,
	POS_INPUT_A3 = LPCMP_POS_INPUT_A3,
	POS_INPUT_VDDS = LPCMP_POS_INPUT_VDDS
} comparator_lpf3_positive_input;

/* Comparator negative input channels. */

typedef enum {
	NEG_INPUT_VDDD = LPCMP_NEG_INPUT_VDDD,
	NEG_INPUT_VDDS = LPCMP_NEG_INPUT_VDDS,
	NEG_INPUT_A2 = LPCMP_NEG_INPUT_A2,
	NEG_INPUT_A3 = LPCMP_NEG_INPUT_A3
} comparator_lpf3_negative_input;

/* Comparator output channels. */

typedef enum {
	OUTPUT_NONE = 0,
	OUTPUT_0,
	OUTPUT_1,
} comparator_lpf3_output;

/* Voltage divider path. */

typedef enum {
	VDIV_PATH_NEG = LPCMP_DIVISION_PATH_N,
	VDIV_PATH_POS = LPCMP_DIVISION_PATH_P
} comparator_lpf3_voltage_divider_path;

/* Voltage division factors. */

typedef enum {
	VDIV_FACTOR_1_1 = LPCMP_DIVISION_FACTOR_1_1,
	VDIV_FACTOR_3_4 = LPCMP_DIVISION_FACTOR_3_4,
	VDIV_FACTOR_1_3 = LPCMP_DIVISION_FACTOR_1_3,
	VDIV_FACTOR_1_2 = LPCMP_DIVISION_FACTOR_1_2,
	VDIV_FACTOR_1_4 = LPCMP_DIVISION_FACTOR_1_4
} comparator_lpf3_voltage_division_factor;

/* Comparator event generation edge selection. */

typedef enum {
	TRIGGER_NONE = 0,
	TRIGGER_RISING = 1,
	TRIGGER_FALLING = 2
} comparator_lpf3_trigger;

/* Forward declaration of configuration, data and handle structures. */

typedef struct comparator_lpf3_config_struct comparator_lpf3_config;
typedef struct comparator_lpf3_data_struct comparator_lpf3_data;
typedef struct comparator_lpf3_handle_struct comparator_lpf3_handle;

/* Comparator callback function */

typedef void (*comparator_lpf3_callback)(comparator_lpf3_handle *handle, void *user_data);

/*
 *   Configuration structure for the comparator driver. Maintains immutable
 *   settings relevant to the comparator module.
 */

typedef struct comparator_lpf3_config_struct {

	/* Signal to the positive terminal of the comparator. */
	comparator_lpf3_positive_input positive_input;

	/* Signal to the negative terminal of the comparator. */
	comparator_lpf3_negative_input negative_input;

	/* Output path to which the comparator output signal has to be routed to. */
	comparator_lpf3_output output;

	/* Enable comparator output to be mapped to a GPIO pin. */
	bool output_enable;

	/* Select whether voltage division must apply to positive or negative input. */
	comparator_lpf3_voltage_divider_path voltage_divider_path;

	/* Select the factor by which the voltage divider must attenuate the signal. */
	comparator_lpf3_voltage_division_factor voltage_division_factor;

	/* Initial trigger selection for the comparator. */
	comparator_lpf3_trigger initial_trigger_level;
} comparator_lpf3_config;

/* Structure for tracking the run-time configurations and state of the
 *  comparator driver.
 */

typedef struct comparator_lpf3_data_struct {

	/* Flag to check if the comparator module has been initialized or not. */
	bool is_open;

	/* Comparator trigger level. */
	comparator_lpf3_trigger trigger;

	/* Comparator event callback function. */
	comparator_lpf3_callback callback;

	/* Pointer to callback function argument. */
	void *callback_user_data;
} comparator_lpf3_data;

/* Comparator handle. */

typedef struct comparator_lpf3_handle_struct {
	const comparator_lpf3_config *config;
	comparator_lpf3_data *data;
} comparator_lpf3_handle;

/**
 * @brief               Initializes the comparator.
 * @param[in] handle    Pointer to a comparator handle structure.
 * @return              0 is initialization is successful. Error code if initialization
 *                      fails.
 * @details             Before calling this function, structure pointed to by \p handle
 *                      must be initialized with valid data and configuration pointers.
 *                      The configuration structure must have valid configuration settings
 *                      or else the initialization will fail.
 */

extern int comparator_lpf3_init(comparator_lpf3_handle *handle);

/**
 * @brief               Gets output of the comparator.
 * @param[in] handle    Pointer to a comparator handle structure.
 * @return              0 if comparator output is low, 1 if comparator output is high
 *                      or error code in case of error.
 * @details             Before calling this function make sure to call \ref comparator_lpf3_init.
 *                      Returns -EINVAL if \p handle is NULL.
 *                      Returns -EBUSY if \ref comparator_lpf3_init function is not called before
 *                      calling this function.
 */

extern int comparator_lpf3_get_output(comparator_lpf3_handle *handle);

/**
 * @brief               Sets comparator trigger level.
 * @param[in]   handle  Pointer to comparator handle structure.
 * @param[in]   trigger Comparator trigger edge. Can be either rising or falling.
 * @return              0 if configuration succeeds, or returns an error code if fails.
 *                      Returns -EBUSY if called before initializing the driver.
 *                      Returns -EINVAL if \p trigger is assigned any other value other
 *                      than TRIGGERN_NONE, TRIGGER_RISING, TRIGGER_FALLING.
 */

extern int comparator_lpf3_set_trigger(comparator_lpf3_handle *handle,
				       comparator_lpf3_trigger trigger);

/**
 * @brief               Sets callback function be called when a comparator event occurs.
 * @param[in] handle    Pointer to comparator handle structure.
 * @param[in] callback  Function to be called when comparator event occurs.
 * @param[in] user_data Pointer to callback function user argument.
 * @return              0 if configuration is successful. Returns error code in case of invalid
 * arguments.
 * @details             This function sets the callback function and the pointer to callback
 * function argument. When calling this function the driver must be initialized by calling \ref
 * comparator_lpf3_init Otherwise, the driver throws -EBUSY error indicating that the API is not
 * ready to be called. Also \p callback must not be NULL, otherwise, the function will return
 * -EINVAL as it does not accept NULL function pointers.
 */

extern int comparator_lpf3_set_trigger_callback(comparator_lpf3_handle *handle,
						comparator_lpf3_callback callback, void *user_data);

/**
 * @brief               Function to check if an interrupt is pending and clear it.
 * @param[in] handle    Pointer to comparator handle structure.
 * @return              1 if there was a pending event, 0 if no comparator event has occurred.
 * @details             This function checks for comparator events and returns 1 if a comparator
 * event has occurred, else returns 0. The function will returns -EINVAL if \p handle is NULL.
 */

extern int comparator_lpf3_trigger_is_pending(comparator_lpf3_handle *handle);

/**
 * @brief               Deinitializes the comparator.
 * @param[in] handle    Pointer to a comparator handle structure.
 * @return              0 is deinitialization is successful. Error code if initialization
 *                      fails.
 * @details             Disables the comparator module and its interrupts. Sets the callback
 * function and the pointer to the callback function argument to NULL. If using the same handle to
 * re-initialization, make sure to call \ref comparator_lpf3_set_trigger_callback after calling
 * comparator_lpf3_init.
 */

extern int comparator_lpf3_deinit(comparator_lpf3_handle *handle);
