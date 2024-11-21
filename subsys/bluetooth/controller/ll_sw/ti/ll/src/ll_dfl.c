/******************************************************************************

 @file  ll_dfl.c

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

/*******************************************************************************
 * INCLUDES
 */
#include <string.h>
#include "ll_dfl.h"
#include "map_direct.h"
#include "bcomdef.h"

/*******************************************************************************
 * CONSTANTS
 */

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
RCL_FilterList    gDynamicFL;   // Dynamic filter list
rankDynamicFL_t   gRankFLTable; // Rank table of the dynamic filter list

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
* Public function defined in ll_dfl.h.
*/
uint8 LL_DFL_GetDynamicFLSize( RCL_FilterList* const pDynamicFL )
{
    uint8 numEntries = BLE_INVALID_NUM_FL_ENTRIES; // Init number of Entries to
                                                   // invalid value.
    // Sanity Check
    if ( pDynamicFL != NULL )
    {
        numEntries = (uint8)pDynamicFL->numEntries;
    }

    return numEntries;
}

/*******************************************************************************
* Public function defined in ll_dfl.h.
*/
void LL_DFL_SetDynamicFLSize( RCL_FilterList* pDynamicFL, uint8 size )
{
    if( pDynamicFL != NULL )
    {
        pDynamicFL->numEntries = size;
    }
}

/*******************************************************************************
* API function defined in ll_dfl.h.
*/
llStatus_t LL_DFL_Init( RCL_FilterList    *pDynamicFL,
                        rankDynamicFL_t   *pRankFLTable )
{
    llStatus_t    status   = LL_STATUS_SUCCESS;  // Init status to success.
    RCL_FL_Entry *pEntries = NULL;               // Init the pointer of the
                                                 // filter list entries to null.

    // Verify that input parameters are valid
    if( ( pDynamicFL == NULL ) || ( pRankFLTable == NULL ) )
    {
        status = LL_STATUS_ERROR_INVALID_PARAMS;
    }
    else
    {
        // Get pointer to the filter list entries
        pEntries = pDynamicFL->entries;

        // Iterate over the dynamic filter list and rank table and
        // initialize each entry
        for ( uint8 i=0; i<DFL_SIZE ; i++ )
        {
            // Set device address to 0
            memset( pEntries[i].address, 0, B_ADDR_LEN );

            // Clear all flags ( clear all flags will set the entry as being free)
            pEntries[i].ctlWord = BLE_INITIAL_RCL_FL_Entry;

            // Set rank of the entry to the invalid rank
            pRankFLTable->entries[i] = DFL_INVALID_RANK;
        }

        // Set number of entries in structure.
        LL_DFL_SetDynamicFLSize( pDynamicFL, DFL_SIZE );
    }

    // Return the status of initializing the dynamic filter list and it's rank
    // table.
    return status;
}

/*******************************************************************************
* API function defined in ll_dfl.h.
*/
uint8 LL_DFL_AddEntry( RCL_FilterList     *pDynamicFL,
                       rankDynamicFL_t    *pRankFLTable,
                       uint8              *devAddr,
                       uint8              devAddrType )
{
    uint8 operation = DFL_RANK_ADD_NEW_ENTRY; // The operation is adding a new
                                              // entry or replacing an existing
                                              // Entry.
    uint8 newEntryIndex = DFL_INVALID_INDEX;  // The index of the new entry in
                                              // the dynamicFL. Init to
                                              // DFL_INVALID_INDEX.
    RCL_FL_Entry *pEntries = NULL;            // Init the pointer to the filter
                                              // list entries to null.

    // Verify that input parameters are valid
    if( ( pDynamicFL != NULL ) && ( pRankFLTable != NULL ) )
    {
        // Get pointer to the filter list entries
        pEntries = pDynamicFL->entries;

        // Get available index to insert/replace the new entry
        newEntryIndex = llDFLGetAvailableEntry( pDynamicFL, pRankFLTable );

        // Check if the newEntryIndex is busy
        if ( pEntries[newEntryIndex].ctl.enabled == UTRUE )
        {
            // The newEntryIndex in used, than update the operation to
            // DFL_RANK_UPDATE_ENTRY
            operation = DFL_RANK_UPDATE_ENTRY;
        }

        // Set entry as being in use
        pEntries[newEntryIndex].ctl.enabled = TRUE;

        // Set address type
        if (devAddrType == LL_DEV_ADDR_TYPE_PUBLIC)
        {
            pEntries[newEntryIndex].ctl.addType = LL_DEV_ADDR_TYPE_PUBLIC;
        }
        else
        {
            pEntries[newEntryIndex].ctl.addType = LL_DEV_ADDR_TYPE_RANDOM;
        }

        // Clear privacy ignore bit
        pEntries[newEntryIndex].ctl.privIgn = FALSE;

        // Copy device address
        memcpy( (void*)(pEntries[newEntryIndex].address), (const void*)devAddr, B_ADDR_LEN );

        // Update the rank table of dynamicFL as a result of adding/replacing an
        // entry.
        if (llDFLUpdateRanks( pRankFLTable, newEntryIndex, operation ) != USUCCESS)
        {
            newEntryIndex = DFL_SIZE;
        }
    }
    /* if the input params enetered correctly and the llDFLUpdateRanks was entered
    with valid params - return the index of the entry that added/replaced
    in the dynamicFilterList. Otherwise - return DFL_SIZE
    */
    return newEntryIndex;
}

/*******************************************************************************
* API function defined in ll_dfl.h.
*/
llStatus_t LL_DFL_RemoveEntry( RCL_FilterList* const   pDynamicFL,
                               rankDynamicFL_t         *pRankFLTable,
                               uint8                   indexEntry )
{
    llStatus_t    status   = LL_STATUS_SUCCESS; // Init status to success.
    RCL_FL_Entry *pEntries = NULL;              // Init the pointer to the
                                                // filter list entries to null.

    // Verify that input parameters are valid
    if( (indexEntry >= DFL_SIZE) || (pDynamicFL == NULL) ||
        (pRankFLTable == NULL) )
    {
        status = LL_STATUS_ERROR_INVALID_PARAMS;
    }
    else
    {
        // Get pointer to the filter list entries
        pEntries = pDynamicFL->entries;

        // Set device address to 0x0
        memset( pEntries[indexEntry].address, 0, B_ADDR_LEN );

        // Clear all the Flags
        pEntries[indexEntry].ctlWord = BLE_INITIAL_RCL_FL_Entry;

        // Mark Entry as Free
        pEntries[indexEntry].ctl.enabled = FALSE;

        // Update the rank table of the dynamicFL to maintain the LRU mechanism
        if(llDFLUpdateRanks( pRankFLTable, indexEntry, DFL_RANK_REMOVE_ENTRY ) != USUCCESS)
        {
            status = LL_STATUS_ERROR_INVALID_PARAMS;
        }
    }

    // Return the status of the removing operation.
    return status;
}

/*******************************************************************************
* API function defined in ll_dfl.h.
*/
uint8 LL_DFL_UpdateEntry( RCL_FilterList    *pDynamicFL,
                          rankDynamicFL_t   *pRankFLTable,
                          uint8             *oldRPA,
                          uint8             *newRPA )
{
    uint8 indexEntry = DFL_INVALID_INDEX; // The index of the found/new entry
                                          // in the dynamicFL. Init to
                                          // DFL_INVALID_INDEX.
    RCL_FL_Entry *pEntries = NULL;        // Init the pointer to the filter
                                          // list entries to null.

    // Verify that input parameters are valid
    if( ( pDynamicFL != NULL ) && ( pRankFLTable != NULL ) &&
        ( oldRPA != NULL ) && ( newRPA != NULL ))
    {
        // Get pointer to the filter list entries
        pEntries = pDynamicFL->entries;

        // Find the index entry of the old RPA in the dynamicFL
        indexEntry = LL_DFL_FindEntry( pDynamicFL, oldRPA, LL_DEV_ADDR_TYPE_RANDOM);

        // Check if the index entry is valid
        if ( indexEntry != DFL_INVALID_INDEX )
        {
            // replace the oldRPA in the DynamicFL entry
            memcpy( (void*)(pEntries[indexEntry].address), (const void*)newRPA, B_ADDR_LEN );

            // Update the rank table of dynamicFL as a result of replacing an
            // entry.
            if (llDFLUpdateRanks( pRankFLTable, indexEntry, DFL_RANK_UPDATE_ENTRY ) != USUCCESS)
            {
                indexEntry = DFL_SIZE;
            }
        }
        else
        {
            // Get index of free entry or oldest entry (highest rank entry)
            indexEntry = LL_DFL_AddEntry( pDynamicFL,
                                          pRankFLTable,
                                          newRPA,
                                          LL_DEV_ADDR_TYPE_RANDOM );
        }
    }

    // Return the index of the replace/new/updated/Not valid entry
    return indexEntry;
}


/*******************************************************************************
* API function defined in ll_dfl.h.
*/
uint8 LL_DFL_FindEntry( RCL_FilterList* const   pDynamicFL,
                        uint8*                  devAddr,
                        uint8                   devAddrType)
{
    uint8 entryIndex = DFL_INVALID_INDEX; // The index of the found/new entry
                                          // in the dynamicFL. Init to
                                          // DFL_INVALID_INDEX.
    RCL_FL_Entry *pEntries = NULL;        // Init the pointer to the filter
                                          // list entries to null.

    // Verify that input parameters are valid
    if( ( pDynamicFL != NULL ) && ( devAddr != NULL ) )
    {
        // Get pointer to the filter list entries
        pEntries = pDynamicFL->entries;

        // Iterate over the dynamic filter list check if there is a match between
        // device and type address in DynamicFL to the input.
        for ( uint8 i=0; i<(uint8)DFL_SIZE ; i++ )
        {
            // Check if the entry is busy
            if( pEntries[i].ctl.enabled == UTRUE )
            {
                // Check if there is a match between the input device address to the
                // dynamicFL device address.
                if ( memcmp( (void*)devAddr, (const void*)pEntries[i].address, B_ADDR_LEN)
                     == UTRUE )
                {
                    // Check if there is a match between address type.
                    if ( pEntries[i].ctl.addType == devAddrType )
                    {
                        // There is a match. The device found in the dynamicFL, so
                        // update the index of the entry in the dynamicFL and exit
                        // for loop.
                        entryIndex = i;
                        break;
                    } // Address type match checking
                } // Device address match checking
            } // Busy/free entry checking
        } // End of for loop
    }

    // Return the found index or invalid index if the entry is not found.
    return entryIndex;
}

/*******************************************************************************
* API function defined in ll_dfl.h.
*/
RCL_FilterList *LL_DFL_GetDynamicFilterlist( void )
{
    return (RCL_FilterList*)&gDynamicFL;
}

/*******************************************************************************
* API function defined in ll_dfl.h.
*/
rankDynamicFL_t *LL_DFL_GetRankTable( void )
{
    return (rankDynamicFL_t*)&gRankFLTable;
}

/*******************************************************************************
* Internal function defined in ll_dfl_internal.h.
*/
uint8 llDFLGetAvailableEntry( RCL_FilterList* const   pDynamicFL,
                              rankDynamicFL_t*        pRankFLTable )
{
    // Init availableIdx, maxRankIdx to invalid index.
    uint8 availableIdx = DFL_INVALID_INDEX; // The index of the available entry
                                            // in the dynamicFL.
    uint8 maxRankIdx = DFL_INVALID_RANK;    // The index of the highest rank
    uint8 maxRankValue = 0;                 // The rank values of the highest
                                            // rank entry.
    RCL_FL_Entry *pEntries = NULL;          // Init the pointer of the filter
                                            // list entries to null.

    // Verify that input parameters are valid
    if ( ( pDynamicFL != NULL ) && ( pRankFLTable != NULL ) )
    {
        // Get pointer to the filter list entries
        pEntries = pDynamicFL->entries;

        // Iterate over the dynamic filter list and rank table and
        // update the maximal rank value
        for ( uint8 i = 0; i < DFL_SIZE ; i++ )
        {
            // Check if the entry is free
            if( pEntries[i].ctl.enabled == UFALSE )
            {
                // Update available index to the free entry index and exit for
                // loop, since the available entry found.
                availableIdx = i;
                break;
            }
            // Update maxRankIdx and maxRankValue if current rank is higher
            // than maxRankValue
            if (maxRankValue < pRankFLTable->entries[i])
            {
                maxRankValue = pRankFLTable->entries[i];
                maxRankIdx = i;
            }
        }
        // Check if all the entries in dynamicFL in used.
        if ( availableIdx == DFL_INVALID_INDEX )
        {
            // Update available index to the entry with the highest rank value.
            availableIdx = maxRankIdx;
        }
    }

    // Return the available entry idx or DFL_SIZE if there is invalid param.
    return availableIdx;
}

/*******************************************************************************
* Internal function defined in ll_dfl_internal.h.
*/
llStatus_t llDFLUpdateRanks( rankDynamicFL_t   *pRankFLTable,
                             uint8             indexEntry,
                             uint8             operation )
{
    uint8 delta = 0;                        // delta of increment or decrement
                                            // of the entries in the rank table
                                            // the dynaicFL.
    uint8 prevRank = 0;                     // previous rank of the new or
                                            // existing entry.
    llStatus_t status = LL_STATUS_SUCCESS;  // Init status to success.

    // Verify that input parameters are valid
    if( pRankFLTable == NULL || indexEntry >= (uint8)DFL_SIZE )
    {
        status = LL_STATUS_ERROR_INVALID_PARAMS;
    }

    else
    {
        // Save the previous rank entry in order to update the relevant entries.
        prevRank = pRankFLTable->entries[indexEntry];

        // Set delta to -1 when removing an entry to maintain the LRU mechanism,
        // so that entries older than the indexEntry became newer,
        // and init the rank of the removed indexEntry to an invalid rank.
        if (operation == DFL_RANK_REMOVE_ENTRY)
        {
            delta = -1;
            pRankFLTable->entries[indexEntry] = DFL_INVALID_RANK;
        }
        else
        {
            // Set delta to +1 when updating/adding an entry to maintain the
            // LRU mechanism, so that entries newer than the indexEntry became older,
            // and init the rank of the removed indexEntry to 0 - the newest rank.
            delta = 1;
            pRankFLTable->entries[indexEntry] = 0;
        }

        // Iterate over the rank table and update the rank values
        for ( uint8 i=0; i<DFL_SIZE ; i++ )
        {
            // Verify that the rank is valid and the index is not the input index
            if ( ( pRankFLTable->entries[i] != DFL_INVALID_RANK ) && ( i != indexEntry ) )
            {
                // Check if the entry has to be update according to the operation
                // and the rank of the entry.
                if( ( (operation == DFL_RANK_UPDATE_ENTRY) &&
                      (pRankFLTable->entries[i] < prevRank) ) ||
                    ( (operation == DFL_RANK_REMOVE_ENTRY) &&
                      (pRankFLTable->entries[i] > prevRank) ) ||
                    ( operation == DFL_RANK_ADD_NEW_ENTRY ))
                {
                    // increment/decrement the rank
                    pRankFLTable->entries[i] += delta;
                } // Checking if the rank entry shall be update
            } // Checking valid rank and index
        }   // End of for loop
    }

    // Return the status of the rank updating operation.
    return status;
}
