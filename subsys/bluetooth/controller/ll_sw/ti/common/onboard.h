/******************************************************************************

 @file  onboard.h

 @brief Defines constants and prototypes for Evaluation boards
        This file targets the Texas Instruments CC26xx Device Family.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2006 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifndef ONBOARD_H
#define ONBOARD_H

#include <ti/drivers/Power.h>
#include "hal_mcu.h"

/*********************************************************************
 */

/* system restart and boot loader used from MTEL.c */
#define SystemReset()        Power_reset();
// #define SystemResetSoft()    Onboard_soft_reset();

/*
 * Board specific random number generator
 */
extern uint16 Onboard_rand( void );

/*********************************************************************
 */

#endif
