/******************************************************************************

 @file  ll_privacy.c

 @brief This file contains the Link Layer (LL) API for the Bluetooth
        Low Energy (BLE) Controller.

        This API is based on the Bluetooth Core Specification,
        V4.2, Vol. 6.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2016 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#include "bcomdef.h"

/*******************************************************************************
 * INCLUDES
 */

#include "onboard.h"
#include "hal_mcu.h"
#ifndef CC23X0
#include "mb.h"
#endif
#include "../../ll/inc/ble.h"
#include "../../ll/inc/ll.h"
#include "../../ll/inc/ll_common.h"
#include "../../ll/inc/ll_privacy.h"
#include "../../ll/inc/ll_enc.h"
#include "../../ll/inc/ll_al.h"
#include "hal_gpio_wrapper.h"
//
#include "rom_jt.h"

// SW Tracer
#ifdef DEBUG_SW_TRACE
#define DBG_ENABLE
#include "dbgid_sys_mst.h"
#endif // DEBUG_SW_TRACE

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

privInfo_t privInfo;
rlEntry_t  *resolvingList;
#ifndef CC23X0
rpaCfg_t   *pRpaCfg;      //Pointer to RPA configuration structure
#endif

#ifdef QUAL_TEST
localIrkList_t  localIrkList[LOCAL_IRK_LIST_SIZE];
#endif

/*******************************************************************************
 * API
 */

/*******************************************************************************
 * @fn          LL_PRIV_Init API
 *
 * @brief       This function is used to disable Privacy 1.2 in the Controller,
 *              and clear the Resolving List.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_PRIV_Init( void )
{
  // disable Address Resolution
  privInfo.addrResolution = FALSE;

  // stop timer
  MAP_osal_stop_timerEx( LL_TaskID, LL_EVT_ADDRESS_RESOLUTION_TIMEOUT );

  // clear the event (in case set)
  MAP_osal_clear_event( LL_TaskID, LL_EVT_ADDRESS_RESOLUTION_TIMEOUT );

  // restore standard accept list and disable Rx Ignore interrupt
  // no need to to restore and disable Rx Ignore interrupt for the scan list
  MAP_LL_PRIV_TeardownPrivacy( alTable );

  // clear the resolving list (Peer entries only)
  MAP_LL_ClearResolvingList();

  // clear Local entry as well
  resolvingList[LOCAL_RL_INDEX].idAddrType = EMPTY_RESOLVE_LIST_ENTRY;
  MAP_osal_memset( resolvingList[LOCAL_RL_INDEX].RPA, 0, B_ADDR_LEN );
  MAP_osal_memset( resolvingList[LOCAL_RL_INDEX].IRK, 0, KEYLEN );

  // set default Address Resolution Timeout value
  privInfo.rpaTimeout = DEFAULT_RPA_TIMEOUT * 1000; // in ms

#ifdef QUAL_TEST
  for (uint8 i=0; i < LOCAL_IRK_LIST_SIZE; i++)
  {
    localIrkList[i].peerAddrType = EMPTY_RESOLVE_LIST_ENTRY;
  }
#endif

  return;
}

/*******************************************************************************
 * @fn          LL_PRIV_Ah API
 *
 * @brief       This function is used to generate a localHash via AES128 using
 *              PRAND as plaintext and IRK as key.
 *
 * input parameters
 *
 * @param       irk   - The 128 bit Identity Resolving Key (MSB..LSB).
 * @param       prand - A 24 bit pseudo random number (LSB..MSB).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The 24 bit localHash.
 */
uint32 LL_PRIV_Ah( uint8 *irk, uint8 *prand )
{
  uint8  r[16] = {0};
  uint8  localHash[16];

  // pad PRAND with 13 bytes of zero
  // Note: While the byte order of prand is (LSB..MSB), the byte order of r
  //       for AES128 is MSB..LSB!
  r[13] = prand[2];
  r[14] = prand[1];
  r[15] = prand[0];

  // generate the localHash
  MAP_LL_ENC_AES128_Encrypt( irk, r, localHash );

  // return only the modulo 24 of the localHash
  // Note: The output of localHash is MSB..LSB.
  return( (localHash[13] << 16) | (localHash[14] << 8) | localHash[15] );
}


/*******************************************************************************
 * @fn          LL_PRIV_GenerateRPA API
 *
 * @brief       This function is used to generate an Resolvable Private Address
 *              (RPA) based on an IRK.
 *
 * input parameters
 *
 * @param       irk - The 128 bit Identity Resolving Key.
 * @param       rpa - A 48 bit Resolvable Private Address.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_PRIV_GenerateRPA( uint8 *irk, uint8 *rpa )
{
  uint32 prand;
  uint32 hash;

  // generate the 24 bit pseudo-random value
  // Note: Must not be all 1's or all 0's.
#ifdef DEBUG_PRIVACY
  prand = 0x00112233;
#else //  !DEBUG_PRIVACY
  do
  {
    (void)MAP_LL_Rand( (uint8 *)&prand, 3 );
  } while( ((prand & 0x00FFFFFF) == 0) || ((prand & 0x00FFFFFF) == 0x00FFFFFF) );
#endif // DEBUG_PRIVACY

  // make an RPA (msb = b01)
  prand = (prand & 0x003FFFFF) | (RESOLVABLE_ADDR_MASK << 16);

  // create the hash
  hash = MAP_LL_PRIV_Ah( irk, (uint8 *)&prand );

  // return the RPA
  //*rpa = ((prand << 24) | (hash & 0x00FFFFFF));
  rpa[0] = (hash >>   0) & 0xFF;
  rpa[1] = (hash >>   8) & 0xFF;
  rpa[2] = (hash >>  16) & 0xFF;
  rpa[3] = (prand >>  0) & 0xFF;
  rpa[4] = (prand >>  8) & 0xFF;
  rpa[5] = (prand >> 16) & 0xFF;

  return;
}


/*******************************************************************************
 * @fn          LL_PRIV_GenerateNRPA API
 *
 * @brief       This function is used to generate a non-resolvable private
 *              address (NRPA).
 *
 * input parameters
 *
 * @param       publicAddr - Device public address.
 *
 * output parameters
 *
 * @param       nrpa - Pointer to non-resolvable private address.
 *
 * @return      None.
 */
void LL_PRIV_GenerateNRPA( uint8 *publicAddr, uint8 *nrpa )
{
  // find a NRPA that isn't equal to the public address
  do
  {
    // find a random 48 bit number that doesn't have all zeros or all ones
    do
    {
      (void)MAP_LL_Rand( nrpa, B_ADDR_LEN );

    } while( ( ((*((uint32 *)nrpa) & 0xFFFFFFFF) == 0) &&
              ((*((uint16 *)(nrpa + 4)) & 0x0000FFFF) == 0) ) ||
             ( ((*((uint32 *)nrpa) & 0xFFFFFFFF) == 0xFFFFFFFF) &&
              ((*((uint16 *)(nrpa + 4)) & 0x0000FFFF) == 0x0000FFFF) ) );

    // make it non-resolvable
    nrpa[5] &= NON_RESOLVABLE_ADDR_MASK;

  } while( MAP_osal_memcmp( nrpa, publicAddr, B_ADDR_LEN ) );

  return;
}


/*******************************************************************************
 * @fn          LL_PRIV_GenerateRSA API
 *
 * @brief       This function is used to generate a random static address (RSA).
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       rsa - Pointer to random static address.
 *
 * @return      None.
 */
void LL_PRIV_GenerateRSA( uint8 *rsa )
{
  // find a random 48 bit number that doesn't have all zeros or all ones
  do
  {
    (void)MAP_LL_Rand( rsa, B_ADDR_LEN );

  } while( ( ((*((uint32 *)rsa) & 0xFFFFFFFF) == 0) &&
             ((*((uint16 *)(rsa + 4)) & 0x0000FFFF) == 0) ) ||
           ( ((*((uint32 *)rsa) & 0xFFFFFFFF) == 0xFFFFFFFF) &&
             ((*((uint16 *)(rsa + 4)) & 0x0000FFFF) == 0x0000FFFF) ) );

  // make it static
  rsa[B_ADDR_LEN-1] |= STATIC_RANDOM_ADDR_MASK;

  return;
}


/*******************************************************************************
 * @fn          LL_PRIV_ResolveRPA API
 *
 * @brief       This function is used to resolve an RPA based on an IRK.
 *
 * input parameters
 *
 * @param       rpa - A 48 bit Resolvable Private Address.
 * @param       irk - The 128 bit Identity Resolving Key.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE:  RPA resolved
 *              FALSE: RPA not resolved.
 */
uint8 LL_PRIV_ResolveRPA( uint8 *rpa, uint8 *irk )
{
#ifndef CC23X0
  uint32 prand = (uint32)(*((uint32 *)&rpa[3])) & 0x00FFFFFF;
  uint32 hash  = (uint32)(*((uint32 *)&rpa[0])) & 0x00FFFFFF;
#else
  uint32 prand;
  uint32 hash;
  osal_memcpy((uint8*)&prand, &rpa[3], 3);
  osal_memcpy((uint8*)&hash, &rpa[0], 3);

  prand &= 0x00FFFFFF;
  hash &= 0x00FFFFFF;
#endif
  return ( MAP_LL_PRIV_Ah(irk, (uint8 *)&prand) == hash );
}


/*******************************************************************************
 * @fn          LL_PRIV_IsRPA API
 *
 * @brief       This function is used to determine whether a received address
 *              from the peer is a Resolvable Private Address based on the
 *              top two bits of the address, and the received address type.
 *
 * input parameters
 *
 * @param       rxAddrType - LL_DEV_ADDR_TYPE_PUBLIC, LL_DEV_ADDR_TYPE_RANDOM
 * @param       rxAddr     - Device address.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE:  Address is an RPA.
 *              FALSE: Address is not an RPA.
 */
uint8 LL_PRIV_IsRPA( uint8 rxAddrType, uint8 *rxAddr )
{
  // check if the address is an RPA
  return( (rxAddrType == LL_DEV_ADDR_TYPE_RANDOM) &&
          ((rxAddr[B_ADDR_LEN-1] & RANDOM_ADDR_MASK) == RESOLVABLE_ADDR_MASK) );
}


/*******************************************************************************
 * @fn          LL_PRIV_IsNRPA API
 *
 * @brief       This function is used to determine whether a received address
 *              from the peer is a Resolvable Private Address based on the
 *              top two bits of the address, and the received address type.
 *
 * input parameters
 *
 * @param       rxAddrType - LL_DEV_ADDR_TYPE_PUBLIC, LL_DEV_ADDR_TYPE_RANDOM
 * @param       rxAddr     - Device address.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE:  Address is an RPA.
 *              FALSE: Address is not an RPA.
 */
uint8 LL_PRIV_IsNRPA( uint8 rxAddrType, uint8 *rxAddr )
{
  // check if the address is an RPA
  return( (rxAddrType == LL_DEV_ADDR_TYPE_RANDOM) &&
          ((rxAddr[B_ADDR_LEN-1] & RANDOM_ADDR_MASK) == NON_RESOLVABLE_ADDR_MASK) );
}


/*******************************************************************************
 * @fn          LL_PRIV_IsIDA API
 *
 * @brief       This function is used to determine whether a received address
 *              from the peer is a Identity Address: either a public address
 *              based on address type, or a Rancom Static address based on the
 *              type and the top two bits of the address.
 *
 * input parameters
 *
 * @param       rxAddrType - LL_DEV_ADDR_TYPE_PUBLIC, LL_DEV_ADDR_TYPE_RANDOM
 * @param       rxAddr     - Device address.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE:  Address is an ID Address.
 *              FALSE: Address is not an ID Address.
 */
uint8 LL_PRIV_IsIDA( uint8 rxAddrType, uint8 *rxAddr )
{
  // check if the address is an RPA
  return( (MASK_ID_ADDRTYPE(rxAddrType) == LL_DEV_ADDR_TYPE_PUBLIC) ||
          ((MASK_ID_ADDRTYPE(rxAddrType) == LL_DEV_ADDR_TYPE_RANDOM) &&
           ((rxAddr[B_ADDR_LEN-1] & RANDOM_ADDR_MASK) == STATIC_RANDOM_ADDR_MASK)) );
}

/*******************************************************************************
 * @fn          LL_PRIV_IsResolvable API
 *
 * @brief       This function is used to resolve an RPA against the Resolving
 *              List (RL).
 *
 *              Note: An extra RL entry is added for the local RPA, which is
 *                    mapped to index zero.
 *
 * input parameters
 *
 * @param       rpa           - A 48 bit Resolvable Private Address.
 * @param       resolvingList - The Resolving List (RL).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      0..BLE_RESOLVING_LIST_SIZE: Index of resolved RPA in RL.
 *              INVALID_RESOLVE_LIST_INDEX: The RPA could not be resolved.
 */
uint8 LL_PRIV_IsResolvable( uint8 *rpa, rlEntry_t *resolvingList )
{
  uint8 i;

  // no existing RPA's match, so see if any are resolvable
  for (i=0; i<=BLE_RESOLVING_LIST_SIZE; i++)
  {
    // check if the Resolving List entry is valid
    if ( resolvingList[i].idAddrType != EMPTY_RESOLVE_LIST_ENTRY )
    {
      // check if the RPA is in the Resolving List
      if ( MAP_LL_PRIV_ResolveRPA( rpa, resolvingList[i].IRK ) ) return(i);
    }
  }

  // failed to resolve
  return( INVALID_RESOLVE_LIST_INDEX );
}


/*******************************************************************************
 * @fn          LL_PRIV_IsZeroIRK API
 *
 * @brief       This function is used to determine if an IRK is zero.
 *
 * input parameters
 *
 * @param       irk - The 128 bit Identity Resolving Key.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE:  IRK is zero.
 *              FALSE: IRK is not zero.
 */
uint8 LL_PRIV_IsZeroIRK( uint8 *irk )
{
  uint8 i;

  // check every byte for zero
  for (i=0; i<KEYLEN; i++)
  {
    // check if at least one byte isn't zero, then IRK isn't zero
    if ( irk[i] != 0x00 )
    {
      return( FALSE );
    }
  }

  return( TRUE );
}


/*******************************************************************************
 * @fn          LL_PRIV_FindPeerInRL API
 *
 * @brief       This function is used to find the Resolving List (RL) index of
 *              a peer address. If the peer address and address type is RPA and
 *              LL_DEV_ADDR_TYPE_RANDOM, then the RL is searched for an RPA
 *              match. Otherwise, the RL is searched for Identity Address match.
 *
 * input parameters
 *
 * @param       resolvingList - The Resolving List (RL).
 * @param       peerAddrType  - LL_DEV_ADDR_TYPE_PUBLIC,
 *                              LL_DEV_ADDR_TYPE_RANDOM,
 * @param       peerAddr      - A 48 bit Address.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      1..BLE_RESOLVING_LIST_SIZE: RL index of peer.
 *              INVALID_RESOLVE_LIST_INDEX: Unable to locate the peer in the RL.
 */
uint8 LL_PRIV_FindPeerInRL( rlEntry_t *resolvingList,
                            uint8      peerAddrType,
                            uint8     *peerAddr )
{
  uint8 i;

  // check if the peer's address is an RPA
  if ( MAP_LL_PRIV_IsRPA( peerAddrType, peerAddr ) )
  {
    for (i=1; i<=BLE_RESOLVING_LIST_SIZE; i++)
    {
      // check if the Resolving List entry is valid
      if ( resolvingList[i].idAddrType != EMPTY_RESOLVE_LIST_ENTRY )
      {
        // check if there's an ID match
        if ( MAP_osal_memcmp(peerAddr, resolvingList[i].RPA, B_ADDR_LEN) )
        {
          // there is, so return the index
          return( i );
        }
      }
    }
  }
  else // peer is not an RPA
  {
    for (i=1; i<=BLE_RESOLVING_LIST_SIZE; i++)
    {
      // check if the Resolving List entry is valid
      if ( resolvingList[i].idAddrType != EMPTY_RESOLVE_LIST_ENTRY )
      {
        // check if there's an ID match
        if ( (peerAddrType == resolvingList[i].idAddrType) &&
             MAP_osal_memcmp(peerAddr, resolvingList[i].idAddr, B_ADDR_LEN) )
        {
          // there is, so return the index
          return( i );
        }
      }
    }
  }

  // no match found
  return( INVALID_RESOLVE_LIST_INDEX );
}


/*******************************************************************************
 * @fn          LL_PRIV_UpdateRL API
 *
 * @brief       This function is used to re-generate the Resolvable Private
 *              Address (RPA) for all valid Resolving List (RL) entries with a
 *              valid Identity Resolving Key (IRK). It is used when a connection
 *              fails due to a MIC error to protect against a successfully
 *              resolved RPA against an incorrect IRK.
 *
 *              Note: An extra RL entry is added for the local RPA, which is
 *                    mapped to index zero.
 *
 * input parameters
 *
 * @param       resolvingList - The Resolving List (RL).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_PRIV_UpdateRL( rlEntry_t *resolvingList )
{
  // update each entries RPA
  for (uint8 i=0; i<=BLE_RESOLVING_LIST_SIZE; i++)
  {
    // check if the Resolving List entry is valid
    if ( resolvingList[i].idAddrType != EMPTY_RESOLVE_LIST_ENTRY )
    {
      // check if the Local IRK is valid (i.e. not equal to zero)
      if ( !MAP_LL_PRIV_IsZeroIRK( resolvingList[i].IRK ) )
      {
        // IRK is valid, so update the RPA
        MAP_LL_PRIV_GenerateRPA( resolvingList[i].IRK,
                                 resolvingList[i].RPA );
      }
    }
  }

  return;
}


/*******************************************************************************
 * @fn          LL_PRIV_NumberPeerRLEntries API
 *
 * @brief       This function is used to count the number of peer entries in
 *              the Resolving List.
 *
 * input parameters
 *
 * @param       resolvingList - The Resolving List (RL).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      0..BLE_RESOLVING_LIST_SIZE
 */
uint8 LL_PRIV_NumberPeerRLEntries( rlEntry_t *resolvingList )
{
  uint8 i;

  // check peer RL entries for a valid IRK
  for (i=1; i<=BLE_RESOLVING_LIST_SIZE; i++)
  {
    // check if the Resolving List entry is valid
    if ( resolvingList[i].idAddrType != EMPTY_RESOLVE_LIST_ENTRY )
    {
      return( TRUE );
    }
  }

  return( FALSE );
}


/*******************************************************************************
 * @fn          LL_PRIV_CheckRLPeerId API
 *
 * @brief       This function is used to check every Peer ID in the Resolving
 *              List that has a valid IRK and uses Network Privacy Mode, and
 *              if in the AL, mark it "ignore", and if not in the AL, add
 *              it to the extended accept list and mark it "ignore".
 *
 * input parameters
 *
 * @param       resolvingList - The Resolving List (RL).
 * @param       pAlTable      - Pointer to accept list table.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_PRIV_CheckRLPeerId( rlEntry_t *resolvingList,
                            alTable_t *pAlTable )
{
  uint8 rlIndex;

  // check for peer ID in RL with a valid IRK using Network Privacy Mode
  for (rlIndex=1; rlIndex<=BLE_RESOLVING_LIST_SIZE; rlIndex++)
  {
    // check if the Resolving List entry is valid
    if ( resolvingList[rlIndex].idAddrType != EMPTY_RESOLVE_LIST_ENTRY )
    {
      MAP_LL_PRIV_CheckRLPeerIdEntry( &resolvingList[rlIndex],
                                      pAlTable );
    }
  }

  return;
}

/*******************************************************************************
 * @fn          LL_PRIV_CheckRLPeerIdEntry API
 *
 * @brief       This function is used to check a Peer ID in the Resolving
 *              List that has a valid IRK and uses Network Privacy Mode, and
 *              if in the AL, mark it "ignore", and if not in the AL, add
 *              it to the extended accept list and mark it "ignore".
 *
 * input parameters
 *
 * @param       resolvingList - Pointer to a valid Resolving List (RL) entry.
 * @param       pAlTable      - Pointer to accept list table.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_PRIV_CheckRLPeerIdEntry( rlEntry_t *resolvingList,
                                 alTable_t *pAlTable )
{
  // check if the IRK is valid and using Network Privacy Mode
  if ( !MAP_LL_PRIV_IsZeroIRK( resolvingList->IRK ) &&
       (resolvingList->privMode == LL_NETWORK_PRIVACY_MODE) )
  {
    // invalid Peer ID; check if already in the AL
    uint8 alIndex = MAP_AL_FindEntry( pAlTable,
                                      resolvingList->idAddr,
                                      resolvingList->idAddrType );

    if ( alIndex != BLE_MAX_NUM_AL_ENTRIES )
    {
      // found Peer ID in the AL, so mark it "ignored"
      SET_AL_ENTRY_PRIV_IGNORE( pAlTable->pAlEntries[alIndex].alFlags );

    }
    else // not in AL
    {
      // so add the Peer ID to the extended accept list
      alIndex = MAP_LL_PRIV_AddExtALEntry( pAlTable,
                                           resolvingList->idAddr,
                                           resolvingList->idAddrType,
                                           PRIV_IGNORE_AL_ENTRY );

      // make sure the accept list index is valid
      if ( alIndex == INVALID_EXT_ACCEPT_LIST_INDEX )
      {
        // Sanity Check:
        // There must be an Extended AL entry for every RL Peer ID!
        LL_ASSERT( FALSE );

        // report failure to Host
        MAP_llHardwareError( HW_FAIL_EXTENDED_AL_FAULT );
      }
    }
#ifdef CC23X0
    // add RPA to ExtAL
     MAP_LL_PRIV_AddExtALEntry( pAlTable,
                                resolvingList->RPA,
                                LL_DEV_ADDR_TYPE_RANDOM,
                                PRIV_USE_AL_ENTRY );
#endif
  }
}

////////////////////////////////////////////////////////////////////////////////
// Extended (i.e. Private) Accept List Functions
////////////////////////////////////////////////////////////////////////////////

/*******************************************************************************
 * @fn          LL_PRIV_SetupPrivacy API
 *
 * @brief       This routine is used to setup for privacy by preparing the
 *              extended accept list, and enabling the Rx Ignore interrupt.
 *              This routine also determines whether any Peer ID addresses
 *              in the resolving list are valid, based on Privacy Mode.
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
void LL_PRIV_SetupPrivacy( alTable_t *pAlTable )
{
  // clear all private AL entries
  // Note: This restores the AL size to is original size.
  MAP_LL_PRIV_ClearExtAL( pAlTable );

  // set AL size to Extended
  MAP_LL_PRIV_SetALSize( pAlTable, AL_SIZE_EXTENDED );

  // make sure all privacy ignore bits are first cleared
  MAP_LL_PRIV_ClearAllPrivIgn( pAlTable );

  // mark invalid Peer ID address in AL
  // Note: When a Peer ID in the RL has a valid IRK, but is set to Network
  //       Privacy Mode, then that Peer ID should be ignored.
  MAP_LL_PRIV_CheckRLPeerId( resolvingList, pAlTable );

  return;
}


/*******************************************************************************
 * @fn          LL_PRIV_TeardownPrivacy API
 *
 * @brief       This routine is used to take down privacy by restoring the
 *              standard accept list, and disabling the Rx Ignore interrupt.
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
void LL_PRIV_TeardownPrivacy( alTable_t *pAlTable )
{
  // make sure all privacy ignore bits are first cleared
  MAP_LL_PRIV_ClearAllPrivIgn( pAlTable );

  // clear all private AL entries
  MAP_LL_PRIV_ClearExtAL( pAlTable );

  // set AL size to Standard
  // Note: LL_PRIV_ClearExtAL restores the AL size, but better to be clear here.
  MAP_LL_PRIV_SetALSize( pAlTable, AL_SIZE_STANDARD );

  return;
}


/*******************************************************************************
 * @fn          LL_PRIV_ClearExtAL
 *
 * @brief       This routine is used clear all the accept list (AL) entries in
 *              the extended (i.e. private) AL table, and restore the original
 *              size of the AL.
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
void LL_PRIV_ClearExtAL( alTable_t *pAlTable )
{
  // check parameter
  if ( pAlTable != NULL )
  {
    uint8 i;

    // clear all entries
    for (i=BLE_MAX_NUM_AL_ENTRIES; i<=(BLE_MAX_NUM_AL_ENTRIES + EXT_ACCEPT_LIST_SIZE); i++)
    {
      // clear a given entry
      MAP_AL_ClearEntry( &pAlTable->pAlEntries[i] );
    }

    // restore size of AL
    MAP_LL_PRIV_SetALSize( pAlTable, AL_SIZE_STANDARD );
  }

  return;
}


/*******************************************************************************
 * @fn          LL_PRIV_ClearAllPrivIgn
 *
 * @brief       This routine is used clear all Privacy Ignore bit for every
 *              entry in the accept list (AL), that is, both the standard AL
 *              entries and the extended (i.e. private) AL entries.
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
void LL_PRIV_ClearAllPrivIgn( alTable_t *pAlTable )
{
    // check parameter
  if ( pAlTable != NULL )
  {
#ifdef CC23X0
    for (uint8 i=0; i<pAlTable->numEntries; i++)
#else
    for (uint8 i=0; i<pAlTable->pAlEntries[0].numEntries; i++)
#endif
    {
      // clear a given entry
      CLR_AL_ENTRY_PRIV_IGNORE( pAlTable->pAlEntries[i].alFlags );
    }
  }

  return;
}


/*******************************************************************************
 * @fn          MAP_LL_PRIV_AddExtALEntry
 *
 * @brief       This routine is used to add a address to the Extended Accept
 *              List. The AL entry privacy ignore bit can optionally be
 *              set when a new entry is created.
 *
 * input parameters
 *
 * @param       pAlTable      - Pointer to accept list table.
 * @param       devAddr       - Pointer to accept list entry device address.
 * @param       devAddrType   - Accept list entry device address type.
 * @param       setPrivIgnore - PRIV_IGNORE_AL_ENTRY | CLR_AL_ENTRY_PRIV_IGNORE
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      A valid accept list table index, or zero.
 */
uint8 LL_PRIV_AddExtALEntry( alTable_t *pAlTable,
                             uint8     *devAddr,
                             uint8      devAddrType,
                             uint8      setPrivIgnore )
{
  // find a free entry
  for (uint8 i=BLE_MAX_NUM_AL_ENTRIES; i<=(BLE_MAX_NUM_AL_ENTRIES + EXT_ACCEPT_LIST_SIZE); i++)
  {
    // check if entry if free
    if ( IS_AL_ENTRY_FREE( pAlTable->pAlEntries[i].alFlags ) )
    {
      // set the extended AL entry as being in use
      SET_AL_ENTRY_BUSY( pAlTable->pAlEntries[i].alFlags );

      // copy the device address to the AL entry
      MAP_osal_memcpy( pAlTable->pAlEntries[i].devAddr, devAddr, B_ADDR_LEN );

      // specify whether this is public or random address
      if ( devAddrType == LL_DEV_ADDR_TYPE_PUBLIC )
      {
        // set the address type to public
        SET_AL_ENTRY_PUBLIC( pAlTable->pAlEntries[i].alFlags );
      }
      else // LL_DEV_ADDR_TYPE_RANDOM
      {
        // set the address type to random
        SET_AL_ENTRY_RANDOM( pAlTable->pAlEntries[i].alFlags );
      }

      // check if the entry should be ignored
      if ( setPrivIgnore == PRIV_IGNORE_AL_ENTRY )
      {
        // yes, so set AL entry to be ignored
        SET_AL_ENTRY_PRIV_IGNORE( pAlTable->pAlEntries[i].alFlags );
      }
      else // PRIV_USE_AL_ENTRY
      {
        // so clear the corresponding denylist entry index
        CLR_AL_ENTRY_PRIV_IGNORE( pAlTable->pAlEntries[i].alFlags );
      }

      return(i);
    }
  }

  return( INVALID_EXT_ACCEPT_LIST_INDEX );
}


/*******************************************************************************
 * @fn          LL_PRIV_UpdateExtALEntry
 *
 * @brief       This routine is used to find and replace a private AL entry
 *              with an updated RPA. The AL entry ignore bit can optionally be
 *              set when a new entry is created. A replaced address will leave
 *              the ignore bit unaffected.
 *
 *              Note: This routine assumes the address type remains the same!
 *
 * input parameters
 *
 * @param       pAlTable - Pointer to AL table.
 * @param       oldRPA   - Pointer to old RPA.
 * @param       newRPA   - Pointer to new RPA.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_PRIV_UpdateExtALEntry( alTable_t *pAlTable,
                               uint8     *oldRPA,
                               uint8     *newRPA )
{
  // check parameter
  if ( (pAlTable != NULL) && (oldRPA != NULL) && (newRPA != NULL) )
  {
    uint8 index;

    // check if the oldRPA actually is an RPA
    if ( MAP_LL_PRIV_IsRPA( LL_DEV_ADDR_TYPE_RANDOM, oldRPA ) )
    {
      index = MAP_LL_PRIV_FindExtALEntry( pAlTable, oldRPA, LL_DEV_ADDR_TYPE_RANDOM );

      // see if RPA is already in the Extended AL
      if ( index != INVALID_EXT_ACCEPT_LIST_INDEX )
      {
        // replace the oldRPA in the Extended AL entry
        // Note: The AL Ignore bit is not affected.
        MAP_osal_memcpy( pAlTable->pAlEntries[index].devAddr, newRPA, B_ADDR_LEN );

        return;
      }
    }

    // either oldRPA wasn't an actual RPA, or it wasn't found in AL, so add newRPA
    for (index=BLE_MAX_NUM_AL_ENTRIES; index<=(BLE_MAX_NUM_AL_ENTRIES + EXT_ACCEPT_LIST_SIZE); index++)
    {
      // check if entry if free
      if ( IS_AL_ENTRY_FREE( pAlTable->pAlEntries[index].alFlags ) )
      {
        // set the extended AL entry as being in use
        SET_AL_ENTRY_BUSY( pAlTable->pAlEntries[index].alFlags );

        // copy the RPA to the AL entry
        MAP_osal_memcpy( pAlTable->pAlEntries[index].devAddr, newRPA, B_ADDR_LEN );

        // set the address type to random
        SET_AL_ENTRY_RANDOM( pAlTable->pAlEntries[index].alFlags );

        return;
      }
    }
  }

  return;
}


/*******************************************************************************
 * @fn          LL_PRIV_FindExtALEntry
 *
 * @brief       This routine is used to find an entry in the extended
 *              (i.e. private) accept list (AL).
 *
 * input parameters
 *
 * @param       pAlTable    - Pointer to AL table.
 * @param       devAddr     - Pointer to accept list entry device address.
 * @param       devAddrType - Accept list entry device address type.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      A valid accept list table index, or zero.
 */
uint8 LL_PRIV_FindExtALEntry( alTable_t *pAlTable,
                              uint8     *devAddr,
                              uint8      devAddrType )
{
  uint8 i;

  for (i=BLE_MAX_NUM_AL_ENTRIES; i<=(BLE_MAX_NUM_AL_ENTRIES + EXT_ACCEPT_LIST_SIZE); i++)
  {
    // check that the entry is in use
    if ( IS_AL_ENTRY_BUSY(pAlTable->pAlEntries[i].alFlags) )
    {
      // check for a match
      if ( MAP_osal_memcmp(devAddr, pAlTable->pAlEntries[i].devAddr, B_ADDR_LEN) &&
           GET_AL_ENTRY_ADDR_TYPE(pAlTable->pAlEntries[i].alFlags) == devAddrType )
      {
        return( i );
      }
    }
  }

  return( INVALID_EXT_ACCEPT_LIST_INDEX );
}

/*******************************************************************************
 * @fn          LL_PRIV_FindEmptyExtALEntry
 *
 * @brief       This routine is used to find an entry in the extended
 *              (i.e. private) accept list (AL).
 *
 * input parameters
 *
 * @param       pAlTable    - Pointer to AL table.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      A valid ext accept list table index, or zero.
 */
uint8 LL_PRIV_FindEmptyExtALEntry( alTable_t *pAlTable )
{
  uint8 i;

  for (i=BLE_MAX_NUM_AL_ENTRIES; i<=(BLE_MAX_NUM_AL_ENTRIES + EXT_ACCEPT_LIST_SIZE); i++)
  {
    // check that the entry is in use
    if ( IS_AL_ENTRY_BUSY(pAlTable->pAlEntries[i].alFlags) == 0)
    {
      return( i );
    }
  }

  return( INVALID_EXT_ACCEPT_LIST_INDEX );
}

/*******************************************************************************
 * @fn          LL_PRIV_SetALSize
 *
 * @brief       This routine is used to set the size of the Accept List (AL) to
 *              standard, or private.
 *
 * input parameters
 *
 * @param       pAlTable    - Pointer to accept list table.
 * @param       alSizeType  - AL_SIZE_STANDARD, AL_SIZE_EXTENDED
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_PRIV_SetALSize( alTable_t *pAlTable,
                        uint8      alSizeType )
{
  // check parameter
  if ( pAlTable != NULL )
  {
    if ( alSizeType == AL_SIZE_STANDARD )
    {
#ifdef CC23X0
      pAlTable->numEntries = BLE_MAX_NUM_AL_ENTRIES;
#else
      pAlTable->pAlEntries[0].numEntries = BLE_MAX_NUM_AL_ENTRIES;
#endif
    }
    else if ( alSizeType == AL_SIZE_EXTENDED )
    {
      // set the size of the AL to be the size of the AL + two times the size
      // of the RL + 1
      // Note: To handle Network Privacy Mode, there needs to be space in the
      //       extended AL for every Peer ID in the RL. This is required to
      //       work in conjunction with the privIgnMode feature of the CM0.
#ifdef CC23X0
      pAlTable->numEntries = BLE_MAX_NUM_AL_ENTRIES +
                             EXT_ACCEPT_LIST_SIZE    + 1;
#else
      pAlTable->pAlEntries[0].numEntries = BLE_MAX_NUM_AL_ENTRIES +
                                           EXT_ACCEPT_LIST_SIZE    + 1;
#endif
    }
  }

  return;
}

#ifdef QUAL_TEST
/*******************************************************************************
 * @fn          LL_PRIV_UpdateLocalIrkList API
 *
 * @brief       This function is used to add a Local IRK for each peer address
 *
 * input parameters
 *
 * @param       peerAddr       - Peer device address.
 * @param       peerAddrType   - Peer device address type.
 * @param       localIrk       - Local IRK
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_PRIV_UpdateLocalIrkList( uint8     *peerAddr,
                                 uint8      peerAddrType,
                                 uint8     *localIrk )
{
  uint8 i;

  for (i=0; i < LOCAL_IRK_LIST_SIZE; i++)
  {
    // check if the entry is free
    if ( localIrkList[i].peerAddrType == EMPTY_RESOLVE_LIST_ENTRY )
    {
      localIrkList[i].peerAddrType = peerAddrType;
      MAP_osal_memcpy( localIrkList[i].peerAddr, peerAddr, B_ADDR_LEN );

      // save the Local IRK
      MAP_osal_memcpy( localIrkList[i].IRK, localIrk, KEYLEN );
      return;
    }
  }

  return;
}

/*******************************************************************************
 * @fn          LL_PRIV_GetLocalIrk API
 *
 * @brief       This function is used to get the Local IRK for requested peer address
 *
 * input parameters
 *
 * @param       peerAddr       - Peer device address.
 * @param       peerAddrType   - Peer device address type.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Local IRK or NULL in case the peer address was not found.
 */
uint8 * LL_PRIV_GetLocalIrk( uint8     *peerAddr,
                             uint8      peerAddrType)
{
  uint8 i;

  for (i=0; i < LOCAL_IRK_LIST_SIZE; i++)
  {
    // find the peer address
    if (( localIrkList[i].peerAddrType == peerAddrType ) &&
        (MAP_osal_memcmp( localIrkList[i].peerAddr, peerAddr, B_ADDR_LEN )))
    {
      // peer address was found - return the proper local IRK
      return localIrkList[i].IRK;
    }
  }
  // peer address was not found
  return NULL;
}
#endif
/*******************************************************************************
 */
