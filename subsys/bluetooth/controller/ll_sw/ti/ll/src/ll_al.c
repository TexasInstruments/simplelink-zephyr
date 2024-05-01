/******************************************************************************

 @file  ll_al.c

 @brief This file contains the data structures and APIs for handling
        Bluetooth Low Energy Accept List structures based on the CC26xx
        RF Core Firmware Specification.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2009 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*******************************************************************************
 * INCLUDES
 */

#include "ll_al.h"
#include "bcomdef.h"
#include "ll.h"
#include "ll_common.h"

//
#include "rom_jt.h"

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

// Accept List Structures
alTable_t *alTable;
alTable_t *alTableScan;

/*
** Application Programming Interface
*/

/*******************************************************************************
 * @fn          AL_Init
 *
 * @brief       This routine is used to initialize the size of the accept list
 *              table, then clear it. It is intended for accept list tables that
 *              are created statically. This is necessary because each accept
 *              list entry has a numEntries field, but only the first entry
 *              uses this field to indicate how large the accept list table is.
 *
 *              Note: It is imperative that the data structure for the accept
 *                    list table has been allocated based on the size indicated
 *                    by pAlTable->numAlEntries.
 *
 * input parameters
 *
 * @param       pAlTable - Pointer to accept list table.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void AL_Init( alTable_t *pAlTable )
{
  // Check Valid input
  if ( pAlTable == NULL )
  {
    return;
  }

  // Sanity Check
  // These errors are considered fatal programming errors.
  LL_ASSERT( pAlTable != NULL );

  // set the AL table size
  pAlTable->numAlEntries = BLE_MAX_NUM_AL_ENTRIES;

  // set the number of accept list entries in the first entry (used by radio)
#ifdef USE_RCL
  pAlTable->numEntries = pAlTable->numAlEntries;
#else
  pAlTable->pAlEntries[0].numEntries = pAlTable->numAlEntries;
#endif
  // clear the accept list table
  (void)MAP_AL_Clear( pAlTable );

  return;
}

/*******************************************************************************
 * @fn          AL_Clear
 *
 * @brief       This routine is used clear all the accept list entries in the
 *              accept list table.
 *
 *              Note: The size of the accept list table, given by the first
 *                    accept list entry, remains unaffected.
 *
 * input parameters
 *
 * @param       pAlTable - Pointer to accept list table.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      llStatus_t
 */
llStatus_t AL_Clear( alTable_t *pAlTable )
{
  // Check Valid input
  if (( pAlTable == NULL ) || ( pAlTable->numAlEntries == BLE_NUM_AL_ENTRIES_ZERO ))
  {
    return LL_STATUS_ERROR_INVALID_PARAMS;
  }

  // Sanity Check
  // These errors are considered fatal programming errors.
  LL_ASSERT( pAlTable != NULL );
  LL_ASSERT( pAlTable->numAlEntries != 0 );

  // clear all entries
  for (uint8 i=0; i<pAlTable->numAlEntries; i++)
  {
    // clear a given entry
    MAP_AL_ClearEntry( &pAlTable->pAlEntries[i] );
  }

  // init the number of accept list entries in use
  pAlTable->numBusyAlEntries = 0;

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * @fn          AL_ClearEntry
 *
 * @brief       This routine is used clear accept list table entry.
 *
 * input parameters
 *
 * @param       pAlEntry - Pointer to a accept list entry.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void AL_ClearEntry( alEntry_t *pAlEntry )
{
  // Sanity Check
  LL_ASSERT( pAlEntry != NULL );

  // Check Valid input
  if ( pAlEntry == NULL )
  {
    return;
  }

  // clear accept list entry configuration field
  // Note: This disables entry, sets address type to public, clears ignore bit
  //       for scan, and clears the privacy ignore bit.
  CLR_AL_ENTRY( pAlEntry->alFlags );

  // zero BLE device address
  for (uint8 i=0; i<BLE_BDADDR_SIZE; i++)
  {
    pAlEntry->devAddr[i] = 0;
  }

  return;
}


/*******************************************************************************
 * @fn          AL_GetSize
 *
 * @brief       This routine returns the number of accept list entries in the
 *              accept list table.
 *
 * input parameters
 *
 * @param       pAlTable - Pointer to a accept list table.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Number of accept list entries in accept list table.
 */
uint8 AL_GetSize( alTable_t *pAlTable )
{
  // Sanity Check
  LL_ASSERT( pAlTable != NULL );

  // Check Valid input
  if ( pAlTable == NULL )
  {
    return BLE_NUM_AL_ENTRIES_ZERO;
  }

  return ( pAlTable->numAlEntries );
}


/*******************************************************************************
 * @fn          AL_GetNumFreeEntries
 *
 * @brief       This routine returns the number of available accept list entries
 *              in the accept list table.
 *
 * input parameters
 *
 * @param       pAlTable - Pointer to a accept list table.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Number of available accept list entries in accept list table.
 */
uint8 AL_GetNumFreeEntries( alTable_t *pAlTable )
{
  // Check Valid input
  if (( pAlTable == NULL ) || ( pAlTable->numAlEntries == BLE_NUM_AL_ENTRIES_ZERO))
  {
    return BLE_NUM_AL_ENTRIES_ZERO;
  }

  // Sanity Check
  LL_ASSERT( pAlTable != NULL );
  LL_ASSERT( pAlTable->numAlEntries != 0 );

  //return( pAcceptlist[0].numEntries - numBusyAlEntries );
  return( pAlTable->numAlEntries - pAlTable->numBusyAlEntries );
}


/*******************************************************************************
 * @fn          AL_FindEntry
 *
 * @brief       This routine searches the accept list table for an address and
 *              address type match. If found, the index of the table entry is
 *              returned. If not found, a fail value is returned.
 *
 * input parameters
 *
 * @param       pAlTable    - Pointer to accept list table.
 * @param       devAddr     - Pointer to device address.
 * @param       devAddrType - LL_ADDR_TYPE_PUBLIC, LL_ADDR_TYPE_RANDOM.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      A valid accept list table index, or BLE_MAX_NUM_AL_ENTRIES.
 */
uint8 AL_FindEntry( alTable_t *pAlTable,
                    uint8     *devAddr,
                    uint8      devAddrType )
{
  uint8 i = 0;
  uint8 j = 0;

  // Check Valid input
  if (( pAlTable == NULL ) || ( pAlTable->numAlEntries == BLE_NUM_AL_ENTRIES_ZERO ))
  {
    return (BLE_MAX_NUM_AL_ENTRIES);
  }

  // Sanity Check
  LL_ASSERT( pAlTable != NULL );
  LL_ASSERT( pAlTable->numAlEntries != 0 );

  // search for an address and address type match
  // Note: Onlyl first entry contains the number of entries in the accept list.
  for (i=0; i<pAlTable->numAlEntries; i++)
  {
    // check that the entry is in use
    if ( IS_AL_ENTRY_BUSY(pAlTable->pAlEntries[i].alFlags) )
    {
      // it is, so compare the address
      for (j=0; j<BLE_BDADDR_SIZE; j++)
      {
        if ( pAlTable->pAlEntries[i].devAddr[j] != *(devAddr+j) )
        {
          // we have a miss match, so on to the next entry
          break;
        }
      }

      // check if we have a match
      if ( j == BLE_BDADDR_SIZE )
      {
        // yes, so now check if the address type matches
        if ( GET_AL_ENTRY_ADDR_TYPE(pAlTable->pAlEntries[i].alFlags) == devAddrType )
        {
          // it does, so we found our entry; return the index
          return( i );
        } // address type match?
      } // address match?
    } // entry valid?
  } // for each entry

  // indicate failure by returning the size of the accept list table, since
  // valid indexes are from 0..BLE_MAX_NUM_AL_ENTRIES-1
  return( pAlTable->numAlEntries );
}


/*******************************************************************************
 * @fn          AL_AddEntry
 *
 * @brief       Adds a accept list entry. The address is added at the next
 *              available index. If the table is full, it returns a failure.
 *
 * input parameters
 *
 * @param       pAlTable    - Pointer to accept list table.
 * @param       devAddr     - Pointer to accept list entry device address.
 * @param       devAddrType - Accept list entry device address type.
 * @param       setIgnore   - Set Ignore bit in accept list entry.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      llStatus_t
 */
llStatus_t AL_AddEntry( alTable_t *pAlTable,
                        uint8     *devAddr,
                        uint8      devAddrType,
                        uint8      setIgnore )
{
  uint8 i = 0;
  uint8 j = 0;

  // Sanity Check
  LL_ASSERT( pAlTable != NULL );
  LL_ASSERT( devAddr != NULL );

  // Check Valid input
  if (( pAlTable == NULL ) || ( devAddr == NULL ))
  {
    return LL_STATUS_ERROR_INVALID_PARAMS;
  }

  // check if there is room for the addition
  if ( pAlTable->numBusyAlEntries == pAlTable->numAlEntries )
  {
    // accept list table is full
    return( LL_STATUS_ERROR_AL_TABLE_FULL );
  }

  // check if this address and address type already exists in the accept list
  if ( MAP_AL_FindEntry( pAlTable, devAddr, devAddrType ) != pAlTable->numAlEntries )
  {
    // yes, so return without doing anything
    return( LL_STATUS_SUCCESS );
  }

  // otherwise, find the next available entry
  for (i=0; i<pAlTable->numAlEntries; i++)
  {
    // is this entry free?
    if ( IS_AL_ENTRY_FREE(pAlTable->pAlEntries[i].alFlags) )
    {
      // yes, so set the entry as busy
      SET_AL_ENTRY_BUSY(pAlTable->pAlEntries[i].alFlags);

      // copy the supplied address to the AL entry
      for (j=0; j<BLE_BDADDR_SIZE; j++)
      {
        pAlTable->pAlEntries[i].devAddr[j] = *devAddr++;
      }

      // specify whether this is public or random address
      if ( devAddrType == LL_DEV_ADDR_TYPE_PUBLIC )
      {
        // set the address type to public
        SET_AL_ENTRY_PUBLIC(pAlTable->pAlEntries[i].alFlags);
      }
      else // LL_DEV_ADDR_TYPE_RANDOM
      {
        // set the address type to random
        SET_AL_ENTRY_RANDOM(pAlTable->pAlEntries[i].alFlags);
      }

      // bump our count of busy accept list entries
      pAlTable->numBusyAlEntries++;

      // check if the entry should be ignored
      if ( setIgnore == BLE_IGNORE_AL_ENTRY )
      {
        // yes, so set AL entry to be ignored
        SET_AL_ENTRY_IGNORE(pAlTable->pAlEntries[i].alFlags);
      }
      else // BLE_USE_AL_ENTRY
      {
        // so clear the corresponding denylist entry index
        CLR_AL_ENTRY_IGNORE(pAlTable->pAlEntries[i].alFlags);
      }

      return( LL_STATUS_SUCCESS );
    }
  }

  // Sanity Check
  // Fatal Error - The AL table should have had at least one free entry.
  LL_ASSERT( FALSE );

  return( LL_STATUS_ERROR_AL_TABLE_FAULT );
}


/*******************************************************************************
 * @fn          AL_RemoveEntry
 *
 * @brief       Removes a accept list entry based on its address and address
 *              type. If located, then the entry is set invalid, and the address
 *              is cleared. If the table is empty, or the address and address
 *              type is not located, it returns a failure.
 *
 * input parameters
 *
 * @param       pAlTable    - Pointer to accept list table.
 * @param       devAddr     - Pointer to accept list entry device address.
 * @param       devAddrType - Accept list entry device address type.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      llStatus_t
 */
llStatus_t AL_RemoveEntry( alTable_t *pAlTable,
                           uint8     *devAddr,
                           uint8      devAddrType )

{
  uint8 alEntryIndex;

  // Check Valid input
  if (( pAlTable == NULL ) || ( devAddr == NULL ))
  {
    return LL_STATUS_ERROR_INVALID_PARAMS;
  }

  // Sanity Check
  LL_ASSERT( pAlTable != NULL );
  LL_ASSERT( devAddr != NULL );

  // check if there's at least one entry in the table
  if ( pAlTable->numBusyAlEntries == 0 )
  {
    // the accept list table is empty
    return( LL_STATUS_ERROR_AL_TABLE_EMPTY );
  }

  // search for the AL entry basedon device address and address type
  if ( (alEntryIndex = MAP_AL_FindEntry( pAlTable,
                                         devAddr,
                                         devAddrType )) != pAlTable->numAlEntries )
  {
    // yes, so free the entry
    SET_AL_ENTRY_FREE( pAlTable->pAlEntries[alEntryIndex].alFlags );

    // clear ignore bit
    CLR_AL_ENTRY_IGNORE( pAlTable->pAlEntries[alEntryIndex].alFlags );

    // clear the address
    for (uint8 i=0; i<BLE_BDADDR_SIZE; i++)
    {
      pAlTable->pAlEntries[alEntryIndex].devAddr[i] = 0;
    }

    // clear the address type to public
    SET_AL_ENTRY_PUBLIC( pAlTable->pAlEntries[alEntryIndex].alFlags );

    // decrease our count of busy accept list entries
    pAlTable->numBusyAlEntries--;

    return( LL_STATUS_SUCCESS );
  }
  else // the entry was not found in the accept list table
  {
    return LL_STATUS_ERROR_AL_ENTRY_NOT_FOUND;
  }
}


/*******************************************************************************
 * @fn          AL_SetAlIgnore
 *
 * @brief       Search the accept list table for an entry, and if found, set the
 *              corresponding ignore bit.
 *
 * input parameters
 *
 * @param       pAlTable    - Pointer to accept list table.
 * @param       devAddr     - Pointer to accept list entry device address.
 * @param       devAddrType - Accept list entry device address type.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS or LL_STATUS_ERROR_AL_ENTRY_NOT_FOUND.
 */

llStatus_t AL_SetAlIgnore( alTable_t *pAlTable,
                           uint8     *devAddr,
                           uint8      devAddrType )
{
  uint8 alEntryIndex;

  // Check Valid input
  if (( pAlTable == NULL ) || ( devAddr == NULL ))
  {
    return LL_STATUS_ERROR_INVALID_PARAMS;
  }

  // Sanity Check
  // These errors are considered fatal programming errors.
  LL_ASSERT( pAlTable != NULL );
  LL_ASSERT( pAlTable->numAlEntries != 0 );

  // search for the AL entry based on device address and address type
  if ( (alEntryIndex = MAP_AL_FindEntry( pAlTable,
                                         devAddr,
                                         devAddrType )) != pAlTable->numAlEntries )
  {
    // found, so set AL entry to be ignored
    SET_AL_ENTRY_IGNORE( pAlTable->pAlEntries[alEntryIndex].alFlags );

    return( LL_STATUS_SUCCESS );
  }
  else // the entry was not found in the accept list table
  {
    return( LL_STATUS_ERROR_AL_ENTRY_NOT_FOUND );
  }
}


/*******************************************************************************
 * @fn          AL_ClearIgnoreList
 *
 * @brief       This routine is used to clear the ignore bits in all the accept
 *              list entries in the accept list table, whether they are free
 *              or busy.
 *
 * input parameters
 *
 * @param       pAlTable - Pointer to accept list table.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
llStatus_t AL_ClearIgnoreList( alTable_t *pAlTable )
{
  // Check Valid input
  if ( pAlTable == NULL )
  {
    return LL_STATUS_ERROR_INVALID_PARAMS;
  }

  // Sanity Check
  // These errors are considered fatal programming errors.
  LL_ASSERT( pAlTable != NULL );
  LL_ASSERT( pAlTable->numAlEntries != 0 );

  // clear all entries
#ifdef USE_RCL
  for (uint8 i=0; i<pAlTable->numEntries; i++)
#else
  for (uint8 i=0; i<pAlTable->pAlEntries[0].numEntries; i++)
#endif
  {
    // clear ignore bit in accept list entry
    CLR_AL_ENTRY_IGNORE( pAlTable->pAlEntries[i].alFlags );
  }

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * @fn          AL_Scan_Init
 *
 * @brief       This routine is used to initialize the size of the accept list
 *              table, then clear it. It is intended for accept list tables that
 *              are created statically. This is necessary because each accept
 *              list entry has a numEntries field, but only the first entry
 *              uses this field to indicate how large the accept list table is.
 *
 *              Note: It is imperative that the data structure for the accept
 *                    list table has been allocated based on the size indicated
 *                    by pAlTable->numAlEntries.
 *
 * input parameters
 *
 * @param       pAlTable - Pointer to scan list table.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void AL_Scan_Init ( alTable_t *pAlTable )
{
  // Check Valid input
  if ( pAlTable == NULL )
  {
    return;
  }

  // Sanity Check
  // These errors are considered fatal programming errors.
  LL_ASSERT( pAlTable != NULL );

  // set the AL table size
  pAlTable->numAlEntries = BLE_MAX_NUM_AL_SCAN_ENTRIES;

  // set the number of accept list entries in the first entry (used by radio)
#ifdef USE_RCL
  pAlTable->numEntries = pAlTable->numAlEntries;
#else
  pAlTable->pAlEntries[0].numEntries = pAlTable->numAlEntries;
#endif
  // clear the accept list table
  (void)MAP_AL_Clear( pAlTable );

  return;
}

/*******************************************************************************
 * @fn          AL_GetAcceptListPtr
 *
 * @brief       This routine is used to get the pointer of the accept list.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      pAlTable - Pointer to accept list table..
 */
alTable_t *AL_GetALPtr( void )
{
    return alTable;
}
/*******************************************************************************
 */
