/******************************************************************************

 @file  timac_board.h

 @brief interface definition for board dependent service for TIMAC stack.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2013 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef TIMACBOARD_H
#define TIMACBOARD_H

#include <stdbool.h>
#include <stdint.h>
#include <icall.h>

/** board service function to write to GPIO */
#define TIMACBOARD_FUNC_GPIO_WRITE     0

/** GPIO index for debug LED 1 */
#define TIMACBOARD_GPIO_OUTPUT_LED1    0

/** GPIO index for debug LED 2 */
#define TIMACBOARD_GPIO_OUTPUT_LED2    1

/** GPIO index for debug LED 3 */
#define TIMACBOARD_GPIO_OUTPUT_LED3    2

/** GPIO index for debug LED 2 */
#define TIMACBOARD_GPIO_OUTPUT_LED4    3

/** @internal Total number of GPIOs used */
#define TIMACBOARD_GPIO_OUTPUT_COUNT   4

/** GPIO configuration data structure */
typedef struct _timacboard_gpiocfg_t
{
  /** GPIO port.
   * Note that not all driverlib requires GPIO port,
   * but for compatibility, this field is always kept.
   */
  uint_least32_t port;

  /** GPIO pin bit field */
  uint_least32_t pin;
} TIMACBoard_GPIOCFG;

/** Board configuration data structure */
typedef struct _timacboard_cfg_t
{
  /** GPIO configuration */
  TIMACBoard_GPIOCFG gpio_outputs[TIMACBOARD_GPIO_OUTPUT_COUNT];
} TIMACBoard_CFG;

typedef struct _timacboard_set_led_args_t
{
  /** common arguments */
  ICall_FuncArgsHdr hdr;
  /** GPIO port/pin definition index mapped to LED */
  uint_least16_t index;
  /** whether to turn on or off the LED (TRUE or FALSE) */
  int_least16_t set;
} TIMACBoard_SetLEDArgs;

/** Customer defined board configuration */
extern const TIMACBoard_CFG TIMACBoard_config;

/**
 * Initializes board service module.
 * Note that @ref TIMACBoard_config must be set before calling
 * this function and all GPIO port direction must have been
 * configured.
 * This function does not configure GPIO port since
 * the same port may be used for multiple purposes and
 * hence it is not possible to configure GPIO port from a single
 * service point of view.
 */
extern void TIMACBoard_init(void);

/**
 * A function to be used by BLE stack to turn on/off debug LED.
 *
 * @param index       GPIO port/pin definition index mapped to debug LED.
 * @param set         TRUE to turn on LED.
 *                    FALSE to turn off LED.
 */
static inline ICall_Errno
TIMACBoard_setLED(size_t index, bool set)
{
  TIMACBoard_SetLEDArgs args;
  args.hdr.service = ICALL_SERVICE_CLASS_TIMAC_BOARD;
  args.hdr.func = TIMACBOARD_FUNC_GPIO_WRITE;
  args.index = (uint_least16_t) index;
  args.set = (int_least16_t) set;
  return ICall_dispatcher(&args.hdr);
}

#endif /* TIMACBOARD_H */
