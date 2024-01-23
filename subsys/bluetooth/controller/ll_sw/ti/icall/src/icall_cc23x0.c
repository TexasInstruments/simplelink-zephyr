/******************************************************************************

 @file  icall_cc2650.c

 @brief CC2650 specific ICall function implementation

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2013 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#include "icall_platform.h"
#include <icall_cc23x0_defs.h>

#include <stdint.h>

#include <ti/drivers/Power.h>
#ifdef DeviceFamily_CC27XX
#include <ti/drivers/power/PowerCC27XX.h>
#else
#include <ti/drivers/power/PowerCC23X0.h>
#endif
#ifdef __IAR_SYSTEMS_ICC__
#include <intrinsics.h>
#endif

/**
 * @internal Flag offset where dependencies starts.
 * Note that the value may change if ICallCC26xxDefs.h changes.
 * When the flags change, not only this module but the client code
 * also changes resulting into requirement to update both images.
 */
#define ICALLCC2650_PWR_CFG_D_OFFSET                   5

/**
 * @internal Invalid configuration flag mask.
 * Note that the value may change if ICallCC26xxDefs.h changes.
 * When the flags change, not only this module but the client code
 * also changes resulting into requirement to update both images.
 */
#define ICALLCC2650_PWR_CFG_NMASK                      0xff000000l

/** @internal a function called from here but not exposed in PowerCC26XX.h */
//extern bool PowerCC26XX_isStableXOSC_HF(void);

/** @internal power activity counter */
static uint_least8_t ICallPlatform_pwrActivityCount = 0;

/** @internal power constraint dependency bitmap */
typedef uint_least32_t ICallPlatform_pwrBitmap_t;

/** @internal power activity counter action configuration */
static ICallPlatform_pwrBitmap_t ICallPlatform_pwrCfgACAction =
  ( ICALL_PWR_C_SD_DISALLOW |
    ICALL_PWR_C_SB_DISALLOW );

/** @internal power notify handler */
static int_least32_t
ICallPlatform_pwrNotify(unsigned int eventType, uintptr_t eventArg,
                        uintptr_t clientArg)
{
  ICall_PwrNotifyData *data = (ICall_PwrNotifyData *) clientArg;
  ICall_PwrTransition transition;
  switch (eventType)
  {
  case PowerLPF3_AWAKE_STANDBY:
    transition = ICALL_PWR_AWAKE_FROM_STANDBY;
    break;
  case PowerLPF3_ENTERING_STANDBY:
    transition = ICALL_PWR_ENTER_STANDBY;
    break;
  case PowerLPF3_ENTERING_SHUTDOWN:
    transition = ICALL_PWR_ENTER_SHUTDOWN;
    break;
  default:
    ICALL_HOOK_ABORT_FUNC();
    break;
  }
  data->_private(transition, data);
  return Power_NOTIFYDONE;
}

/**
 * @internal
 * Sets constraints and dependencies.
 *
 * @param bitmap  bitmap of constraint and dependency flags.
 */
static void ICallPlatform_pwrRequireImpl(uint_fast32_t bitmap)
{
  for (;;)
  {
#ifdef __IAR_SYSTEMS_ICC__
    char pos = __CLZ(bitmap);
#elif __GNUC__
    int pos = __builtin_clz(bitmap);
#else
    int pos = __clz(bitmap);
#endif
    if (pos == 32)
      break;
    pos = 31 - pos;
    bitmap ^= 1ul << pos;
    if (pos < ICALLCC2650_PWR_CFG_D_OFFSET)
    {
      Power_setConstraint((uint_least32_t) (pos));
    }
    else
    {
      Power_setDependency(pos - ICALLCC2650_PWR_CFG_D_OFFSET);
    }
  }
}

/**
 * @internal
 * Releases constraints and dependencies.
 *
 * @param bitmap  bitmap of constraint and dependency flags.
 */
static void ICallPlatform_pwrDispenseImpl(uint_fast32_t bitmap)
{
  for (;;)
  {
#ifdef __IAR_SYSTEMS_ICC__
    char pos = __CLZ(bitmap);
#elif __GNUC__
    int pos = __builtin_clz(bitmap);
#else
    int pos = __clz(bitmap);
#endif
    if (pos == 32)
      break;
    pos = 31 - pos;
    bitmap ^= 1ul << pos;
    if (pos < ICALLCC2650_PWR_CFG_D_OFFSET)
    {
      Power_releaseConstraint((uint_least32_t) (pos));
    }
    else
    {
      Power_releaseDependency(pos - ICALLCC2650_PWR_CFG_D_OFFSET);
    }
  }
}

/* See ICallPlatform.h for description */
ICall_Errno
ICallPlatform_pwrUpdActivityCounter(ICall_PwrUpdActivityCounterArgs *args)
{
  args->pwrRequired = TRUE;
  if (args->incFlag)
  {
    /* Increment counter */
    ICall_CSState state = ICall_enterCSImpl();
    if (ICallPlatform_pwrActivityCount == 255)
    {
      /* Counter value overflow */
      ICall_leaveCSImpl(state);
      return ICALL_ERRNO_OVERFLOW;
    }
    ICallPlatform_pwrActivityCount++;
    if (ICallPlatform_pwrActivityCount == 1)
    {
      /* Needs to stay in power active state.
       * Set constraints and dependencies as configured. */
      ICallPlatform_pwrRequireImpl(ICallPlatform_pwrCfgACAction);
    }
    ICall_leaveCSImpl(state);
  }
  else
  {
    /* Decrement counter */
    ICall_CSState state = ICall_enterCSImpl();
    if (ICallPlatform_pwrActivityCount == 0)
    {
      /* Counter value underflow */
      ICall_leaveCSImpl(state);
      return ICALL_ERRNO_UNDERFLOW;
    }
    ICallPlatform_pwrActivityCount--;
    if (ICallPlatform_pwrActivityCount == 0)
    {
      /* No activity.
       * Lift the constraints and dependencies as configured. */
      ICall_leaveCSImpl(state);
      ICallPlatform_pwrDispenseImpl(ICallPlatform_pwrCfgACAction);
      args->pwrRequired = FALSE;
    }
    else
    {
      ICall_leaveCSImpl(state);
    }
  }
  return ICALL_ERRNO_SUCCESS;
}

/* See ICallPlatform.h for description */
ICall_Errno
ICallPlatform_pwrConfigACAction(ICall_PwrBitmapArgs *args)
{
  ICall_CSState state;

  if (args->bitmap & ICALLCC2650_PWR_CFG_NMASK)
  {
    return ICALL_ERRNO_INVALID_PARAMETER;
  }

  state = ICall_enterCSImpl();

  if (ICallPlatform_pwrActivityCount > 0)
  {
    /* Release previous constraints */
    ICallPlatform_pwrDispenseImpl(ICallPlatform_pwrCfgACAction);
    /* Set new constraints */
    ICallPlatform_pwrRequireImpl(args->bitmap);
  }
  ICallPlatform_pwrCfgACAction = args->bitmap;

  ICall_leaveCSImpl(state);
  return ICALL_ERRNO_SUCCESS;
}

/* See ICallPlatform.h for description */
ICall_Errno
ICallPlatform_pwrRequire(ICall_PwrBitmapArgs *args)
{
  if (args->bitmap & ICALLCC2650_PWR_CFG_NMASK)
  {
    return ICALL_ERRNO_INVALID_PARAMETER;
  }

  ICallPlatform_pwrRequireImpl(args->bitmap);

  return ICALL_ERRNO_SUCCESS;
}

/* See ICallPlatform.h for description */
ICall_Errno
ICallPlatform_pwrDispense(ICall_PwrBitmapArgs *args)
{
  if (args->bitmap & ICALLCC2650_PWR_CFG_NMASK)
  {
    return ICALL_ERRNO_INVALID_PARAMETER;
  }

  ICallPlatform_pwrDispenseImpl(args->bitmap);

  return ICALL_ERRNO_SUCCESS;
}

/* See ICallPlatform.h for description */
ICall_Errno
ICallPlatform_pwrRegisterNotify(ICall_PwrRegisterNotifyArgs *args)
{
  Power_NotifyObj *obj[1];
  size_t i;

  static const uint_least32_t events[1] =
  {
     ((uint_least32_t) PowerLPF3_ENTERING_STANDBY |
     (uint_least32_t) PowerLPF3_ENTERING_SHUTDOWN |
     (uint_least32_t) PowerLPF3_AWAKE_STANDBY)
  };

  args->obj->_private = args->fn;
  for (i = 0; i < sizeof(obj)/sizeof(obj[0]); i++)
  {
    obj[i] =  (Power_NotifyObj *) ICall_mallocImpl(sizeof(Power_NotifyObj));
    if (!obj[i])
    {
      size_t j;
      for (j = 0; j < i; j++)
      {
        ICall_freeImpl(obj[j]);
      }
      return ICALL_ERRNO_NO_RESOURCE;
    }
  }
  for (i = 0; i < sizeof(obj)/sizeof(obj[0]); i++)
  {
    Power_registerNotify(obj[i], events[i],
                       (Power_NotifyFxn)ICallPlatform_pwrNotify,
                         (uintptr_t) args->obj);
  }
  return ICALL_ERRNO_SUCCESS;
}

/* See ICallPlatform.h for description */
ICall_Errno
ICallPlatform_pwrIsStableXOSCHF(ICall_GetBoolArgs* args)
{
  return ICALL_ERRNO_SUCCESS;
}

/* See ICallPlatform.h for description */
ICall_Errno
ICallPlatform_pwrSwitchXOSCHF(ICall_FuncArgsHdr* args)
{
  return ICALL_ERRNO_SUCCESS;
}

/* See ICallPlatform.h for description */
ICall_Errno
ICallPlatform_pwrGetTransitionState(ICall_PwrGetTransitionStateArgs *args)
{
  args->state = Power_getTransitionState();
  return ICALL_ERRNO_SUCCESS;
}

/* See ICallPlatform.h for description */
ICall_Errno
ICallPlatform_pwrGetXOSCStartupTime(ICall_PwrGetXOSCStartupTimeArgs * args)
{
  return ICALL_ERRNO_SUCCESS;
}
