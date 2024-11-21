/******************************************************************************

 @file  ll_dfl.h

 @brief This file contains the APIs for handling with the dynamic filter list
        data structures. This filter list using by the radio core, and
        managed by the controller. The dynamic filter list working with LRU
        (Last Recent Use) mechanism which maintained by the API's in this
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

#ifndef LL_DFL_H
#define LL_DFL_H
/*******************************************************************************
 * INCLUDES
 */
#include "ll_dfl_internal.h"

/*******************************************************************************
 * CONSTANTS
 */

// Invaliid rank for dynamic filter list.
#define DFL_INVALID_RANK                        0xFFU
// Invaliid index for dynamic filter list entry.
#define DFL_INVALID_INDEX                       0xFFU
// Invaliid number of dynamic filter list entries.
#define BLE_INVALID_NUM_FL_ENTRIES              0xFFU

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * EXTERNS
 */

/*******************************************************************************
 * TYPEDEFS
 */

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
 * @fn          LL_DFL_GetDynamicFLSize
 *
 * @brief       This subroutine used to get the number of entries of the filter
 *              list.
 *
 * @design      BLE_LOKI-355
 * input parameters
 *
 * @param       pDynamicFL - pointer to dynamic filter list
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The number of entries of the filter list or
 *              BLE_INVALID_NUM_FL_ENTRIES for input invalid.
 */
uint8 LL_DFL_GetDynamicFLSize( RCL_FilterList* const pDynamicFL );

/*******************************************************************************
 * @fn          LL_DFL_SetDynamicFLSize
 *
 * @brief       This subroutine used to set the number of entries of the filter
 *              list.
 *
 * @design      BLE_LOKI-355
 * input parameters
 *
 * @param       pDynamicFL - pointer to dynamic filter list
 * @param       size - The number of entries of the filter list
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_DFL_SetDynamicFLSize( RCL_FilterList* pDynamicFL, uint8 size );

/*******************************************************************************
 * @fn          LL_DFL_Init
 *
 * @brief       This subroutine used to prepare the the rank table of the
 *              dynamic filter list and the dynamic filter list.
 *              The ranks in the rank table will set to DFL_SIZE.
 *              The entries in the dynamic filter list will set the addresses
 *              to 0x0, and their flags will be cleared.
 *
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
 * @return      LL_STATUS_SUCCESS               - for success.
 *              LL_STATUS_ERROR_INVALID_PARAMS  - for invalid input.
 */
llStatus_t  LL_DFL_Init( RCL_FilterList    *pDynamicFL,
                         rankDynamicFL_t   *pRankFLTable );

/*******************************************************************************
 * @fn          LL_DFL_AddEntry
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
 * @param       devAddr      - pointer to the device address.
 * @param       devAddrType  - address type of the device.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The index of the new/replaced entry in the dynamic filter list.
 *              Valid index shall be in the range (0 - (DFL_SIZE-1)).
 *              DFL_INVALID_INDEX index indicates that the input parameters
 *              invalid.
 */
uint8 LL_DFL_AddEntry( RCL_FilterList   *pDynamicFL,
                       rankDynamicFL_t  *pRankFLTable,
                       uint8            *devAddr,
                       uint8            devAddrType );

/*******************************************************************************
 * @fn          LL_DFL_RemoveEntry
 *
 * @brief       This subroutine used to remove an entry in the dynamicFL using
 *              the index of the entry.
 *
 *
 * @design      BLE_LOKI-355
 * input parameters
 *
 * @param       pDynamicFL   - pointer to dynamic filter list.
 * @param       pRankFLTable - pointer to the rank table of the dynamic filter
 *                             list.
 * @param       indexEntry   - index entry to be removed in the dynamic filter
 *                             list. Valid index shall be in the
 *                             range (0 - (DFL_SIZE-1)).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS              - for success.
 *              LL_STATUS_ERROR_INVALID_PARAMS - for invalid input.
 */
llStatus_t LL_DFL_RemoveEntry( RCL_FilterList* const  pDynamicFL,
                               rankDynamicFL_t        *pRankFLTable,
                               uint8                  indexEntry );

/*******************************************************************************
 * @fn          LL_DFL_UpdateEntry
 *
 * @brief       This subroutine used to find and replace a RPA address in the
 *              dynamicFL with an updated RPA. If the old RPA not found in the
 *              dynamicFL, new entry will be added.
 *              The updated Entry rank will set to 0 in order to maintaining
 *              the LRU mechanism.
 *              The updates/new entry index will be return. Invalid index will
 *              be returned when the input parameters are invalid.
 *              The flag bits of the entry remains the same.
 *
 *
 * @design      BLE_LOKI-355
 * input parameters
 *
 * @param       pDynamicFL   - pointer to dynamic filter list
 * @param       pRankFLTable - pointer to the rank table of the dynamic filter
 *                             list.
 * @param       oldRPA       - The old RPA address of the device.
 * @param       newRPA       - The new RPA address of the device.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The index of the new/updated entry in the dynamic filter list.
 *              Valid index shall be in the range (0 - (DFL_SIZE-1)).
 *              DFL_INVALID_INDEX index indicates that the input parameters
 *              invalid.
 */
uint8 LL_DFL_UpdateEntry( RCL_FilterList    *pDynamicFL,
                          rankDynamicFL_t   *pRankFLTable,
                          uint8             *oldRPA,
                          uint8             *newRPA );

/*******************************************************************************
 * @fn          LL_DFL_FindEntry
 *
 * @brief       This subroutine used to find an entry in the dynamic filter
 *              list by device address and address type.
 *              If found, the index of the entry will return. If not found, a
 *              fail value will return.
 *
 *
 * @design      BLE_LOKI-355
 * input parameters
 *
 * @param       pDynamicFL   - pointer to dynamic filter list
 * @param       devAddr      - pointer to the device address.
 * @param       devAddrType  - address type of the device.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The index of the found entry in the dynamic filter list.
 *              Valid index shall be in the range (0 - (DFL_SIZE-1)).
 *              DFL_INVALID_INDEX index indicates that the input parameters
 *              invalid.
 */
uint8 LL_DFL_FindEntry( RCL_FilterList* const pDynamicFL,
                        uint8*                devAddr,
                        uint8                 devAddrType);

/*******************************************************************************
 * @fn          LL_DFL_GetDynamicFilterlist
 *
 * @brief       This subroutine used to get pointer of the dynamic filter list.
 *
 *
 * @design      BLE_LOKI-355
 * input parameters
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      pointer to dynamic filter list.
 */
RCL_FilterList *LL_DFL_GetDynamicFilterlist( void );

/*******************************************************************************
 * @fn          LL_DFL_GetRankTable
 *
 * @brief       This subroutine used to get pointer to the rank table of the
 *              dynamic filter list.
 *
 *
 * @design      BLE_LOKI-355
 * input parameters
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      pointer to the rank table of the dynamic filter list.
 */
rankDynamicFL_t *LL_DFL_GetRankTable( void );

#endif /* LL_DFL_H */
