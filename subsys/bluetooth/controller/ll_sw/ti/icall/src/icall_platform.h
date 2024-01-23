/******************************************************************************

 @file  icall_platform.h

 @brief Platform specific function interfaces required for ICall implementation

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2013 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef ICALLPLATFORM_H
#define ICALLPLATFORM_H

#include <stdint.h>

#include "icall.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Functions in platform independent ICall module that can be used
 * by platform dependent functions.
 */
extern ICall_CSState ICall_enterCSImpl(void);
extern void ICall_leaveCSImpl(ICall_CSState key);
extern void *ICall_mallocImpl(uint_fast16_t size);
extern void ICall_freeImpl(void *ptr);

#ifdef ICALL_HOOK_ABORT_FUNC
extern void ICALL_HOOK_ABORT_FUNC();
#else /* ICALL_HOOK_ABORT_FUNC */
/* Note that customer can use their own assert handler */
#include <stdlib.h>
/**
 * Abort function definition.
 * Note that at compile time, this macro can be overridden
 * to point to another function of void fn(void) type.
 */
#if defined (__IAR_SYSTEMS_ICC__)
#pragma diag_suppress=Pe111 // Suppressed warning "statement is unreachable"
#endif // __IAR_SYSTEMS_ICC__
#define ICALL_HOOK_ABORT_FUNC() abort()
#endif /* ICALL_HOOK_ABORT */

/**
 * @internal
 * Updates power activity counter
 *
 * @param args  arguments
 * @return return value of ICall_pwrUpdateActivityCounter()
 */
extern ICall_Errno
ICallPlatform_pwrUpdActivityCounter(ICall_PwrUpdActivityCounterArgs *args);

/**
 * @internal
 * Registers power state transition notify function
 *
 * @param args  arguments
 * @return return values of ICall_pwrRegisterNotify()
 */
extern ICall_Errno
ICallPlatform_pwrRegisterNotify(ICall_PwrRegisterNotifyArgs *args);

/**
 * @internal
 * Configures power activity counter action
 *
 * @param args arguments
 * @return return value of ICall_pwrConfigACAction()
 */
extern ICall_Errno
ICallPlatform_pwrConfigACAction(ICall_PwrBitmapArgs *args);

/**
 * @internal
 * Sets power constraints and dependencies
 *
 * @param args arguments
 * @return return value of ICall_pwrRequire()
 */
extern ICall_Errno
ICallPlatform_pwrRequire(ICall_PwrBitmapArgs *args);

/**
 * @internal
 * Releases power constraints and dependencies
 *
 * @param args arguments
 * @return return value of ICall_pwrDispense()
 */
extern ICall_Errno
ICallPlatform_pwrDispense(ICall_PwrBitmapArgs *args);

/**
 * @internal
 * Checks whether HF XOSC is stable.
 *
 * @return ICALL_ERRNO_SUCCESS
 */
extern ICall_Errno
ICallPlatform_pwrIsStableXOSCHF(ICall_GetBoolArgs* args);

/**
 * @internal
 * Switches clock source to HF XOSC.
 *
 * @return ICALL_ERRNO_SUCCESS
 */
extern ICall_Errno
ICallPlatform_pwrSwitchXOSCHF(ICall_FuncArgsHdr* args);

/**
 * @internal
 * Get the estimated crystal oscillator startup time
 *
 * @return ICALL_ERRNO_SUCCESS
 */
extern ICall_Errno
ICallPlatform_pwrGetXOSCStartupTime(ICall_PwrGetXOSCStartupTimeArgs * args);

/**
 * @internal
 * Retrieves power transition state.
 *
 * @return @ref ICALL_ERRNO_SUCCESS
 */
extern ICall_Errno
ICallPlatform_pwrGetTransitionState(ICall_PwrGetTransitionStateArgs *args);

#ifdef __cplusplus
}
#endif

#endif /* ICALLPLATFORM_H */
