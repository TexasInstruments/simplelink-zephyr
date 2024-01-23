/******************************************************************************

 @file  rom.h

 @brief This file contains redirection to ROM initialization and jump tables
        for each specific ROM image

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2016 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifndef ROM_H
#define ROM_H

/*******************************************************************************
 * INCLUDES
 */

// If this is not a ROM_BUILD then all MAP_ prefixed functions can be
// directly mapped and linked to the given function's address instead
// of a jump table entry. These mappings are not dependent on the ROM
// image and can be found in the following header:
#include "map_direct.h"

/*******************************************************************************
 * EXTERNS
 */

// ROM's C Runtime initialization
extern void ROM_Init( void );
extern void FPB_Init( void );
extern void CommonROM_Init( void );

#endif // ROM_H
