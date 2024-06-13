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
#include "ll_dfl.h"
#include "rom_jt.h"
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
dynamicFL_t       dynamicFL;   // Dynamic filter list
rankDynamicFL_t   rankFLTable; // Rank table of the dynamic filter list

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
* Public function defined in ll_dfl.h.
*/
uint8 LL_DFL_GetDynamicFLSize( dynamicFL_t *dynamicFL )
{
    uint8 RetVal = 0;

    // Sanity Check
    LL_ASSERT(dynamicFL != ((void *)0)); // (stands for NULL)

    (void)MAP_osal_memcpy(&RetVal, &dynamicFL->numEntries, 1);
    return RetVal;
}

/*******************************************************************************
* Public function defined in ll_dfl.h.
*/
void LL_DFL_SetDynamicFLSize( dynamicFL_t *dynamicFL, uint8 size )
{
    if( dynamicFL != NULL )
    {
        dynamicFL->numEntries = size;
    }
}

/*******************************************************************************
* Public function defined in ll_dfl.h.
*/
dynamicFLEntry_t* LL_DFL_GetDynamicFLEntries( dynamicFL_t *dynamicFL )
{
    // Sanity Check
    LL_ASSERT( dynamicFL != ((void *)0)); // (stands for NULL)

    return dynamicFL->entries;
}

/*******************************************************************************
* Public function defined in rfBleDpl.h.
*/
void* rfBleDpl_GetRadioFLPtr( dynamicFL_t *dynamicFL )
{
    // Sanity Check
    LL_ASSERT( dynamicFL != ((void *)0)); // (stands for NULL)

    return (void *)(dynamicFL);
}

/*******************************************************************************
* API function defined in ll_dfl.h.
*/
llStatus_t LL_DFL_Init( dynamicFL_t       *pDynamicFL,
                        rankDynamicFL_t   *pRankFLTable )
{
    llStatus_t status = LL_STATUS_SUCCESS;  // Init status to success.
    dynamicFLEntry_t *pEntries = NULL;      // Init the pointer of the filter
                                            // list entries to null.

    // Verify that input parameters are valid
    if( ( pDynamicFL == NULL ) || ( pRankFLTable == NULL ) )
    {
        status = LL_STATUS_ERROR_INVALID_PARAMS;
    }
    else
    {
        // Get pointer to the filter list entries
        pEntries = LL_DFL_GetDynamicFLEntries(pDynamicFL);

        // Iterate over the dynamic filter list and rank table and
        // initialize each entry
        for ( int8 i=0; i<DFL_SIZE ; i++ )
        {
            // Set device address to 0
            (void)MAP_osal_memset( pEntries[i].devAddr, 0, B_ADDR_LEN );

            // Clear all flags ( clear all flags will set the entry as being free)
            CLR_DFL_ENTRY( pEntries[i].dflFlags );

            // Set rank of the entry to the invalid rank - size of the filter list
            pRankFLTable->entries[i] = DFL_SIZE;
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
uint8 LL_DFL_AddEntry( dynamicFL_t      *pDynamicFL,
                      rankDynamicFL_t   *pRankFLTable,
                      uint8             *devAddr,
                      uint8             devAddrType )
{
    uint8 operation = DFL_RANK_ADD_NEW_ENTRY; // The operation is adding a new
                                              // entry or replacing an existing
                                              // Entry.
    uint8 newEntryIndex = DFL_SIZE;           // The index of the new entry in
                                              // the dynamicFL. Init to DFL_SIZE.
    dynamicFLEntry_t *pEntries = NULL;        // Init the pointer to the filter
                                              // list entries to null.

    // Verify that input parameters are valid
    if( ( pDynamicFL == NULL ) || ( pRankFLTable == NULL ) )
    {
        newEntryIndex = DFL_SIZE; // To approve with Maxim - maybe can leave it empty?
    }
    else
    {
        // Get pointer to the filter list entries
        pEntries = LL_DFL_GetDynamicFLEntries( pDynamicFL );

        // Get available index to insert/replace the new entry
        newEntryIndex = llDFLGetAvailableEntry( pDynamicFL, pRankFLTable );

        // Check if the newEntryIndex is free
        if ( (IS_DFL_ENTRY_FREE(pEntries[newEntryIndex].dflFlags)) == FALSE)
        {
            // The newEntryIndex in used, than update the operation to
            // DFL_RANK_UPDATE_ENTRY
            operation = DFL_RANK_UPDATE_ENTRY;
        }

        // Set entry as being in use
        SET_DFL_ENTRY_BUSY(pEntries[newEntryIndex].dflFlags);

        // Set address type
        if (devAddrType == LL_DEV_ADDR_TYPE_PUBLIC)
        {
            SET_DFL_ENTRY_PUBLIC(pEntries[newEntryIndex].dflFlags);
        }
        else
        {
            SET_DFL_ENTRY_RANDOM(pEntries[newEntryIndex].dflFlags);
        }

        // Clear privacy ignore bit
        CLR_DFL_ENTRY_PRIV_IGNORE(pEntries[newEntryIndex].dflFlags);

        // Copy device address
        (void)MAP_osal_memcpy( pEntries[newEntryIndex].devAddr, devAddr, B_ADDR_LEN );

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
llStatus_t LL_DFL_RemoveEntry( dynamicFL_t      *pDynamicFL,
                               rankDynamicFL_t  *pRankFLTable,
                               uint8            indexEntry )
{
    llStatus_t status = LL_STATUS_SUCCESS; // Init status to success.
    dynamicFLEntry_t *pEntries = NULL;     // Init the pointer to the filter
                                           // list entries to null.

    // Get pointer to the filter list entries
    pEntries = LL_DFL_GetDynamicFLEntries( pDynamicFL );

    // Verify that input parameters are valid
    if( (indexEntry >= (uint8)DFL_SIZE) || (pDynamicFL == NULL) || (pRankFLTable == NULL))
    {
        status = LL_STATUS_ERROR_INVALID_PARAMS;
    }
    else
    {
        // Set device address to 0x0
        (void)MAP_osal_memset( pEntries[indexEntry].devAddr, 0, B_ADDR_LEN );

        // Clear all the Flags
        CLR_DFL_ENTRY( pEntries[indexEntry].dflFlags );

        // Mark Entry as Free
        SET_DFL_ENTRY_FREE( pEntries[indexEntry].dflFlags );

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
uint8 LL_DFL_UpdateEntry( dynamicFL_t       *pDynamicFL,
                          rankDynamicFL_t   *pRankFLTable,
                          uint8             *oldRPA,
                          uint8             *newRPA )
{
    uint8 indexEntry = DFL_SIZE;       // The index of the found/new entry in
                                       // the dynamicFL. Init to DFL_SIZE.
    dynamicFLEntry_t *pEntries = NULL; // Init the pointer to the filter
                                       // list entries to null.

    // Verify that input parameters are valid
    if( ( pDynamicFL == NULL ) || ( pRankFLTable == NULL ) ||
        ( oldRPA == NULL ) || ( newRPA == NULL ))
    {
        indexEntry = DFL_SIZE; // To approve with Maxim - maybe can leave it empty?
    }

    else
    {
        // Get pointer to the filter list entries
        pEntries = LL_DFL_GetDynamicFLEntries( pDynamicFL );

        // Find the index entry of the old RPA in the dynamicFL
        indexEntry = LL_DFL_FindEntry( pDynamicFL, oldRPA, LL_DEV_ADDR_TYPE_RANDOM);

        // Check if the index entry is valid
        if ( indexEntry < (uint8)DFL_SIZE )
        {
            // replace the oldRPA in the DynamicFL entry
            (void)MAP_osal_memcpy( pEntries[indexEntry].devAddr, newRPA, B_ADDR_LEN );

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
            indexEntry = LL_DFL_AddEntry( pDynamicFL, pRankFLTable, newRPA, LL_DEV_ADDR_TYPE_RANDOM );
        }
    }

    // Return the index of the replace/new/updated/Not valid entry
    return indexEntry;
}


/*******************************************************************************
* Internal function defined in ll_dfl.h.
*/
uint8 LL_DFL_FindEntry( dynamicFL_t    *pDynamicFL,
                        uint8          *devAddr,
                        uint8          devAddrType)
{
    uint8 entryIndex = DFL_SIZE;
    dynamicFLEntry_t *pEntries = NULL; // Init the pointer to the filter
                                       // list entries to null.

    // Verify that input parameters are valid
    if( ( pDynamicFL == NULL ) || ( devAddr == NULL ) )
    {
        entryIndex = DFL_SIZE; // To approve with Maxim - maybe can leave it empty?
    }

    else
    {
        // Get pointer to the filter list entries
        pEntries = LL_DFL_GetDynamicFLEntries( pDynamicFL );

        // Iterate over the dynamic filter list check if there is a match between
        // device and type address in DynamicFL to the input.
        for ( uint8 i=0; i<(uint8)DFL_SIZE ; i++ )
        {
            // Check if the entry is busy
            if( IS_DFL_ENTRY_FREE(pEntries[i].dflFlags) == FALSE )
            {
                // Check if there is a match between the input device address to the
                // dynamicFL device address.
                if (MAP_osal_memcmp(devAddr, pEntries[i].devAddr, B_ADDR_LEN)
                    == UTRUE)
                {
                    // Check if there is a match between address type.
                    if (GET_DFL_ENTRY_ADDR_TYPE(pEntries[i].dflFlags) ==
                        devAddrType)
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
dynamicFL_t *LL_DFL_GetDynamicFilterlist( void )
{
    return (dynamicFL_t*)&dynamicFL;
}

/*******************************************************************************
* API function defined in ll_dfl.h.
*/
rankDynamicFL_t *LL_DFL_GetRankTable( void )
{
    return (rankDynamicFL_t*)&rankFLTable;
}

/*******************************************************************************
* Internal function defined in ll_dfl_internal.h.
*/
uint8 llDFLGetAvailableEntry( dynamicFL_t        *dynamicFL,
                              rankDynamicFL_t    *pRankFLTable )
{
    // Init availableIdx, maxRankIdx to invalid index.
    uint8 availableIdx = DFL_SIZE;     // The index of the available entry in
                                       // the dynamicFL.
    uint8 maxRankIdx = DFL_SIZE;       // The index of the highest rank entry.
    uint8 maxRankValue = 0;            // The rank values of the highest rank
                                       // entry.
    dynamicFLEntry_t *pEntries = NULL; // Init the pointer of the filter
                                       // list entries to null.

    // Verify that input parameters are valid
    if( ( dynamicFL == NULL ) || ( pRankFLTable == NULL ) )
    {
        availableIdx = DFL_SIZE; // To approve with Maxim - maybe can leave it empty?
    }

    else
    {
        // Get pointer to the filter list entries
        pEntries = LL_DFL_GetDynamicFLEntries(dynamicFL);

        // Iterate over the dynamic filter list and rank table and
        // update the maximal rank value
        for ( int8 i=0; i<DFL_SIZE ; i++ )
        {
            // Check if the entry is free
            if( IS_DFL_ENTRY_FREE( pEntries->dflFlags ) == TRUE )
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
        if (availableIdx == (uint8)DFL_SIZE)
        {
            // Update available index to the entry with the highest rank value.
            availableIdx = maxRankIdx;
        }
    }

    // return the available entry idx or DFL_SIZE if there is invalid param.
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

        // Set delta to -1 when removing entry, and restore the rank of the removed
        // index entry to DFL_SIZE.
        if (operation == DFL_RANK_REMOVE_ENTRY)
        {
            delta = -1;
            pRankFLTable->entries[indexEntry] = DFL_SIZE;
        }
        else // The operation is update/add new entry, then set delta to +1, and set
            // the rank of the new/update index entry to 0.
        {
            delta = 1;
            pRankFLTable->entries[indexEntry] = 0;
        }

        // Iterate over the rank table and update the rank values
        for ( int8 i=0; i<DFL_SIZE ; i++ )
        {
            // Verify that the rank is valid and the index is not the input index
            if ( (pRankFLTable->entries[i] != (uint8)DFL_SIZE) && ((uint8)i != indexEntry) )
            {
                // Check if the entry has to be update according to the operation
                // and the rank of the entry.
                if( ( (operation == DFL_RANK_UPDATE_ENTRY) &&
                    (pRankFLTable->entries[i] < prevRank) ) ||
                    ( (operation == DFL_RANK_REMOVE_ENTRY) &&
                    (pRankFLTable->entries[i] > prevRank) ) ||
                    ( (operation == DFL_RANK_ADD_NEW_ENTRY) &&
                    (pRankFLTable->entries[i] < (uint8)DFL_SIZE) ))
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
