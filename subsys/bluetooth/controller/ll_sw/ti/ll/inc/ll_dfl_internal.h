/******************************************************************************

 @file  ll_dfl_internal.h

 @brief This file contains the internal function for handling with the dynamic
        filter list data structures. This filter list using by the radio core,
        and managed by the controller. The dynamic filter list working with
        LRU (Last Recent Use) mechanism which maintained by the API's in this
        file.
        Each entry in the dynamic filter list has a rank so the higher the
        rank, the older the entry.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2023 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifndef LL_DFL_INTERNAL_H
#define LL_DFL_INTERNAL_H
/*******************************************************************************
 * INCLUDES
 */
#include "ble.h"

/*******************************************************************************
 * CONSTANTS
 */
// optional operation - add/update/remove entry from DynamicFL
#define DFL_RANK_ADD_NEW_ENTRY                  0U
#define DFL_RANK_UPDATE_ENTRY                   1U
#define DFL_RANK_REMOVE_ENTRY                   2U

// Number of entries in the radio filter list.
#define DFL_SIZE                                16

/*******************************************************************************
 * MACROS
 */
// Dynamic filter list flags
// Note: Assumes dflFlags = dynamic filter list entry's flags.
#define CLR_DFL_ENTRY( dflFlags )               \
  (dflFlags) = 0
#define IS_DFL_ENTRY_FREE( dflFlags )           \
  (((dflFlags) & BV(0)) == 0)
#define SET_DFL_ENTRY_BUSY( dflFlags )          \
  (dflFlags) |= BV(0)
#define SET_DFL_ENTRY_FREE( dflFlags )          \
  (dflFlags) &= ~BV(0)
#define GET_DFL_ENTRY_ADDR_TYPE( dflFlags )     \
  (((dflFlags) & BV(1)) >> 1)
#define SET_DFL_ENTRY_PUBLIC( dflFlags )        \
  (dflFlags) &= ~BV(1)
#define SET_DFL_ENTRY_RANDOM( dflFlags )        \
  (dflFlags) |= BV(1)
#define CLR_DFL_ENTRY_DUP_IGNORE( dflFlags )    \
  (dflFlags) &= ~BV(2)
#define SET_DFL_ENTRY_DUP_IGNORE( dflFlags )    \
  (dflFlags) |= BV(2)
#define SET_DFL_ENTRY_PRIV_IGNORE( dflFlags )   \
  (dflFlags) |= BV(3)
#define CLR_DFL_ENTRY_PRIV_IGNORE( dflFlags )   \
  (dflFlags) &= ~BV(3)

/*******************************************************************************
 * EXTERNS
 */

/*******************************************************************************
 * TYPEDEFS
 */
// RCL - BLE Filter List Flags
// | 15..4 |        3       |        2          |      1       |      0       |
// |  N/A  | Privacy Ignore | Duplicate Ignored | Address Type | Entry In Use |
//
typedef uint8 dflFlags_t; // To approve with Maxim

// Dynamic Filter List Entry
// Note: see RCL filter list entry struct (RCL_FL_Entry).
PACKED_TYPEDEF_STRUCT
{
  dflFlags_t        dflFlags;
  uint8             devAddr[BLE_BDADDR_SIZE];
} dynamicFLEntry_t;

// Dynamic Filter List
// Note: see RCL filter list struct (RCL_FilterList).
PACKED_TYPEDEF_STRUCT
{
  uint8             numEntries;
  dynamicFLEntry_t  entries[DFL_SIZE];
} dynamicFL_t;

// rank table of the dynamic filter list
typedef struct
{
  uint8           entries[DFL_SIZE];
} rankDynamicFL_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * LOCAL FUNCTION DECLARATIONS
 */

/*******************************************************************************
 * @fn          llDFLGetAvailableEntry
 *
 * @brief       This subroutine used to get index of available entry in the
 *              dynamic filter list.
 *              The subroutine will return index of an empty entry, and if all
 *              the entries in used, the subroutine will return the oldest
 *              entry (i.e the entry with the highest rank).
 *
 * @design      BLE_LOKI-355
 * input parameters
 *
 * @param       pDynamicFL   - pointer to dynamic filter list
 * @param       pRankFLTable - pointer to the rank table of the dynamic filter
 *                             list.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The index of the available entry in the dynamic filter list.
 *              The index shall be in the range (0 - (DFL_SIZE-1)).
 */
uint8 llDFLGetAvailableEntry( dynamicFL_t        *dynamicFL,
                              rankDynamicFL_t    *pRankFLTable );

/*******************************************************************************
 * @fn          llDFLUpdateRanks
 *
 * @brief       This subroutine used to maintain the LRU mechanism on the rank
 *              table of the dynamic filter list.
 *              The subroutine will be called when adding new entry, removing
 *              or updating an existing entry:
 *              - When adding a new device to the filter list, the rank of the
 *                new entry will be updated to 0, and the remaining busy entries
 *                will increment by 1.
 *              - When removing an existing entry in the filter list, the rank of
 *                the existing entry will be updated to DFL_SIZE, and the
 *                remaining busy entries with higher rank will decrement by 1.
 *                will increment by 1.
 *              - When updating an existing entry in the filter list, the rank of
 *                the existing entry will be updated to 0, and the remaining
 *                busy entries with lower rank will increment by 1.
 *
 *
 * @design      BLE_LOKI-355
 * input parameters
 *
 * @param       pRankFLTable - pointer to the rank table of the dynamic filter
 *                             list.
 * @param       indexEntry   - index entry of the dynamic filter list.
 *                             (shall be in the range 0-DFL_SIZE)
 * @param       operation    - The operation performed on the entry in the
 *                             dynamic filter list.
 *                             (shall be DFL_RANK_ADD_NEW_ENTRY,
 *                             DFL_RANK_UPDATE_ENTRY or DFL_RANK_REMOVE_ENTRY).
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS              - for success.
 *              LL_STATUS_ERROR_INVALID_PARAMS - for invalid input.
 */
llStatus_t llDFLUpdateRanks( rankDynamicFL_t   *pRankFLTable,
                             uint8             indexEntry,
                             uint8             operation );

#endif // LL_DFL_INTERNAL_H
