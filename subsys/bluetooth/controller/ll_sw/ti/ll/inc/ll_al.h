/******************************************************************************

 @file  ll_al.h

 @brief This file contains the data structures and APIs for handling
        Bluetooth Low Energy Accept List structures using the CC26xx
        RF Core Firmware Specification.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2009 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*********************************************************************
 *
 * WARNING!!!
 *
 * THE API'S FOUND IN THIS FILE ARE FOR INTERNAL STACK USE ONLY!
 * FUNCTIONS SHOULD NOT BE CALLED DIRECTLY FROM APPLICATIONS, AND ANY
 * CALLS TO THESE FUNCTIONS FROM OUTSIDE OF THE STACK MAY RESULT IN
 * UNEXPECTED BEHAVIOR.
 *
 */

#ifndef AL_H
#define AL_H

/*******************************************************************************
 * INCLUDES
 */

#include "bcomdef.h"
#include "ll.h"
#include "ll_config.h"

/*******************************************************************************
 * MACROS
 */

// Advertising Accept List
// Note: Assumes alEntryFlags = accept list entry's flags.

#define CLR_AL_ENTRY( alEntryFlags )                                           \
  (alEntryFlags) = 0

#define SET_AL_ENTRY_FREE( alEntryFlags )                                      \
  (alEntryFlags) &= ~BV(0)

#define SET_AL_ENTRY_BUSY( alEntryFlags )                                      \
  (alEntryFlags) |= BV(0)

#define IS_AL_ENTRY_FREE( alEntryFlags )                                       \
  (((alEntryFlags) & BV(0)) == 0)

#define IS_AL_ENTRY_BUSY( alEntryFlags )                                       \
  (((alEntryFlags) & BV(0)) == 1)

#define SET_AL_ENTRY_PUBLIC( alEntryFlags )                                    \
  (alEntryFlags) &= ~BV(1)

#define SET_AL_ENTRY_RANDOM( alEntryFlags )                                    \
  (alEntryFlags) |= BV(1)

#define GET_AL_ENTRY_ADDR_TYPE( alEntryFlags )                                 \
  (((alEntryFlags) & BV(1)) >> 1)

#define CLR_AL_ENTRY_IGNORE( alEntryFlags )                                    \
  (alEntryFlags) &= ~BV(2)

#define SET_AL_ENTRY_IGNORE( alEntryFlags )                                    \
  (alEntryFlags) |= BV(2)

#define SET_AL_ENTRY_PRIV_IGNORE( alEntryFlags )                               \
  (alEntryFlags) |= BV(3)

#define CLR_AL_ENTRY_PRIV_IGNORE( alEntryFlags )                               \
  (alEntryFlags) &= ~BV(3)

#define GET_AL_TABLE_POINTER( pAlEntry )                                       \
    ((alTable_t *)((uint8 *)(pAlEntry) - sizeof(alTable_t) + sizeof(uint32_t)))

/*******************************************************************************
 * CONSTANTS
 */

// API
#define BLE_IGNORE_AL_ENTRY            0
#define BLE_USE_AL_ENTRY               1

// Miscellaneous
#define BLE_BDADDR_SIZE                6
#define BLE_MAX_NUM_AL_ENTRIES         (alSize)  // at 8 bytes per AL entry
#define BLE_NO_AL_MATCH_FOUND          0xFF

#ifdef CC23X0
#define BLE_MAX_NUM_AL_SCAN_ENTRIES    15
#else
#define BLE_MAX_NUM_AL_SCAN_ENTRIES    BLE_MAX_NUM_AL_ENTRIES
#endif

#define BLE_NUM_AL_ENTRIES_ZERO        0   // Error return value for number of accept list entries

#ifdef USE_DFL
#define BLE_NUM_AL_ENTRIES             (BLE_MAX_NUM_AL_ENTRIES)
#else
#define BLE_NUM_AL_ENTRIES             ((BLE_MAX_NUM_AL_ENTRIES) + (2 * (BLE_RESOLVING_LIST_SIZE)) + 1)
#endif

/*******************************************************************************
 * TYPEDEFS
 */

// BLE Filter List Flags
// | 15..4 |        3       |        2          |      1       |      0       |
// |  N/A  | Privacy Ignore | Duplicate Ignored | Address Type | Entry In Use |
//
typedef uint16_t alFlgs_t;

// Accept List Entry
// Note: see RCL filter list entry struct (RCL_FL_Entry).
PACKED_TYPEDEF_STRUCT /* Creates a warning in other compilers as it is passed to RCL API which expects a non-packed structure */
{
  alFlgs_t alFlags;                    // W:  accept list flags (RW for bit 2)
  uint8    devAddr[BLE_BDADDR_SIZE];   // W:  BLE address
} alEntry_t;

PACKED_TYPEDEF_STRUCT
{
  // LL structure part
  uint8     numAlEntries;
  uint8     numBusyAlEntries;
  uint16    reserve;
  alEntry_t *pAlEntries;
  // RCL structure part - according to the RCL filter list struct (RCL_FilterList)
  uint32    numEntries;
  // all 16 entries located here while pAlEntries will point to them
  // while the RCL filterList will point to the numEntries (start of the RCL_FilterList)
} alTable_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

extern alTable_t *alTable;
extern alTable_t *alTableScan;

/*******************************************************************************
 * GLOBAL ROUTINES
 */

extern void       AL_Init( alTable_t * );

extern void       AL_Scan_Init( alTable_t * );

extern llStatus_t AL_Clear( alTable_t * );

extern void       AL_ClearEntry( alEntry_t * );

extern uint8      AL_GetSize( alTable_t * );

extern uint8      AL_GetNumFreeEntries(  alTable_t * );

extern uint8      AL_FindEntry( alTable_t *, uint8 *, uint8 );

extern llStatus_t AL_AddEntry( alTable_t *, uint8 *, uint8, uint8 );

extern llStatus_t AL_RemoveEntry( alTable_t *, uint8 *, uint8 );

extern llStatus_t AL_SetAlIgnore( alTable_t *, uint8 *, uint8 );

extern llStatus_t AL_ClearIgnoreList( alTable_t * );

extern alEntry_t *AL_Alloc( uint8 );

extern void       AL_Free( alEntry_t * );

extern alEntry_t *AL_Copy( alEntry_t *, alEntry_t * );

extern alTable_t *AL_GetALPtr( void );

/*******************************************************************************
 */

#endif /* AL_H */
