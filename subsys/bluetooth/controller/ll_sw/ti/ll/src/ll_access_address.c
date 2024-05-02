/******************************************************************************

 @file  ll_access_address.c

 @brief This file contains the BLE Link Layer (LL) routines used by the
        Initiator for calculating the Access Address, as specified in the
        BLE Controller Specification D09R08.

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

#include "bcomdef.h"

#ifndef CONFIG_SOC_CC2340R5
#include <ti/drivers/cryptoutils/cryptokey/CryptoKeyPlaintext.h>
#endif
#include "ll_common.h"
#ifndef CC23X0
#include "trng_api.h"
#endif
#include "ll_enc.h"
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

/*******************************************************************************
 * Functions
 */

/*******************************************************************************
 * @fn          llValidAccessAddr
 *
 * @brief       This function is called to check if an access address is a
 *              valid access address.
 *
 * input parameters
 *
 * @param       accessAddr - Connection synchronization word.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Indication of valid access address:
 *              TRUE: Access address is valid.
 *              FALSE: Access address is invalid.
 */
uint8 llValidAccessAddr( uint32 accessAddr )
{
  // check if this access address has already been chosen before
  if ( !MAP_llEqAlreadyValidAddr( accessAddr ) )
  {
    // test if the access address meets all the requirements
    if ( !MAP_llGtSixConsecZerosOrOnes(accessAddr)   &&  // test if there are greater than six consecutive zeros
         !MAP_llEqSynchWord(accessAddr)              &&  // test if equal to advertising packet sync word
         !MAP_llOneBitSynchWordDiffer(accessAddr)    &&  // test if it differs only in one bit from the advertising packet sync word
         !MAP_llEqualBytes(accessAddr)               &&  // test if all four bytes are equal
         !MAP_llGtTwentyFourTransitions(accessAddr)  &&  // test that there aren't more than 24 transitions
         !MAP_llLtThreeOnesInLsb(accessAddr)         &&  // test if there's at least three ones in lsb
         !MAP_llGtElevenTransitionsInLsh(accessAddr) &&  // test there's no more than 11 transitions in lsh
         !MAP_llLtTwoChangesInLastSixBits(accessAddr) )  // test there's a minimum of two changes in the last six bit portion
    {
      return( TRUE );
    }
  }

  return( FALSE );
}


/*******************************************************************************
 * @fn          llGenerateValidAccessAddr
 *
 * @brief       This function is called to randomly generate a valid access
 *              address.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      A valid 32-bit access address.
 */
uint32 llGenerateValidAccessAddr( void )
{
  uint32    accessAddr = 0x5065549D;
//  do
//  {
//    // generate a true random 32 bit number
//    MAP_LL_ENC_GenerateTrueRandNum((uint8 *)(&accessAddr), 4);
//    // verify if it is valid
//  } while(  !MAP_llValidAccessAddr( accessAddr ) );

  return( accessAddr );
}


/*******************************************************************************
 * @fn          llGtSixConsecZerosOrOnes
 *
 * @brief       This function is called to check if there are more than six
 *              consecutive zeros or ones in the access address.
 *
 * input parameters
 *
 * @param       accessAddr - Connection synchronization word.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Indicates if access address meets criteria:
 *              TRUE: Access address does not meet criteria.
 *              FALSE: Access address does meet criteria.
 */
uint8 llGtSixConsecZerosOrOnes( uint32 accessAddr )
{
  uint8 prevBit, nextBit, count, i;

  // init count
  count = 1;

  // init first bit
  prevBit = (uint8)(accessAddr & 1);

  // for each subsequent bit
  for (i=1; i<32; i++)
  {
    // get the next bit
    nextBit = (uint8)( (accessAddr >> i) & 1 );

    // check if it is the same as previous bit
    if (nextBit == prevBit)
    {
      // same, so increment count and check if more than six
      if (++count > 6)
      {
        // yep, more than six ones or zeros in a row
        return( TRUE );
      }
    }
    else // different from prev
    {
      // so replace prev with this bit, reset count, and keep checking
      prevBit = nextBit;
      count = 1;
    }
  }

  return( FALSE );
}


/*******************************************************************************
 * @fn          llEqSynchWord
 *
 * @brief       This function is called to check if the access address is equal
 *              to the pre-defined Advertiser synch word.
 *
 * input parameters
 *
 * @param       accessAddr - Connection synchronization word.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Indicates if access address meets criteria:
 *              TRUE: Access address does not meet criteria.
 *              FALSE: Access address does meet criteria.
 */
uint8 llEqSynchWord( uint32 accessAddr )
{
  return ( (uint8)(accessAddr == (uint32)ADV_SYNCH_WORD) );
}


/*******************************************************************************
 * @fn          llOneBitSynchWordDiffer
 *
 * @brief       This function is called to check if the access address differs
 *              from the pre-defined Advertiser synch word by only one bit.
 *
 * input parameters
 *
 * @param       accessAddr - Connection synchronization word.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Indicates if access address meets criteria:
 *              TRUE: Access address does not meet criteria.
 *              FALSE: Access address does meet criteria.
 */
uint8 llOneBitSynchWordDiffer( uint32 accessAddr )
{
  uint32 numOnes;

  // exclusive or
  numOnes = (accessAddr ^ (uint32)ADV_SYNCH_WORD);

  // check if a power of two (i.e. only one bit is set)
  // Note: If accessAddr is ADV_SYNCH_WORD, the this routine will return TRUE
  //       even though there are no bits that differ. But note that the routine
  //       llEqSynchWord will catch this case, so we don't really need to
  //       check for it here. However, to be thorough, the explict check for
  //       no bits differing is made here.
  return( (numOnes != 0) && ((numOnes & (numOnes-1)) == 0) );
}


/*******************************************************************************
 * @fn          llEqualBytes
 *
 * @brief       This function is called to check if the access address's four
 *              bytes are equal.
 *
 * input parameters
 *
 * @param       accessAddr - Connection synchronization word.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Indicates if access address meets criteria:
 *              TRUE: Access address does not meet criteria.
 *              FALSE: Access address does meet criteria.
 */
uint8 llEqualBytes( uint32 accessAddr)
{
  uint8 b0, b1, b2, b3;

  b0 = ((uint8 *)&accessAddr)[0];  //((accessAddr >>  0) & 0xFF);
  b1 = ((uint8 *)&accessAddr)[1];  //((accessAddr >>  8) & 0xFF);
  b2 = ((uint8 *)&accessAddr)[2];  //((accessAddr >> 16) & 0xFF);
  b3 = ((uint8 *)&accessAddr)[3];  //((accessAddr >> 24) & 0xFF);

  return( (uint8)((b0 == b1) && (b0 == b2) && (b0 == b3)) );
}


/*******************************************************************************
 * @fn          llGtTwentyFourTransitions
 *
 * @brief       This function is called to check if the access address has more
 *              than 24 bit transitions.
 *
 * input parameters
 *
 * @param       accessAddr - Connection synchronization word.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Indicates if access address meets criteria:
 *              TRUE: Access address does not meet criteria.
 *              FALSE: Access address does meet criteria.
 */
uint8 llGtTwentyFourTransitions( uint32 accessAddr )
{
  uint8 prevBit, nextBit, count, i;

  // init count
  count = 0;

  // init first bit
  prevBit = (uint8)(accessAddr & 1);

  // for each subsequent bit
  for (i=1; i<32; i++)
  {
    // check if next bit is same as previous bit
    nextBit = (uint8)((accessAddr >> i) & 1);

    // check if we have a transition or not
    if (nextBit != prevBit)
    {
      // transition, so increment count and check if more than 24
      if (++count > 24)
      {
        // yep, more than 24 transitions
        return( TRUE );
      }

      // replace prev with this bit
      prevBit = nextBit;
    }
  }

  return( FALSE );
}


/*******************************************************************************
 * @fn          llLtTwoChangesInLastSixBits
 *
 * @brief       This function is called to check if the access address has a
 *              minimum of two changes in the last six bits.
 *
 * input parameters
 *
 * @param       accessAddr - Connection synchronization word.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Indicates if access address meets criteria:
 *              TRUE: Access address does not meet criteria.
 *              FALSE: Access address does meet criteria.
 */
uint8 llLtTwoChangesInLastSixBits( uint32 accessAddr )
{
  uint8 prevBit, nextBit, count, i;

  // init count
  count = 0;

  // init first bit, which is bit 27 as we want only the last six bits
  prevBit = (uint8)((accessAddr >> 26) & 1);

  // for each subsequent bit
  for (i=27; i<32; i++)
  {
    // check if next bit is same as previous bit
    nextBit = (uint8)((accessAddr >> i) & 1);

    // check if we have a transition or not
    if (nextBit != prevBit)
    {
      // transition, so increment count and check if equal to 2
      if (++count == 2)
      {
        // yep, at least 2 transitions in last six bits
        return( FALSE );
      }

      // replace prev with this bit
      prevBit = nextBit;
    }
  }

  // less than 2 transitions in last six bits
  return( TRUE );
}


/*******************************************************************************
 * @fn          llEqAlreadyValidAddr
 *
 * @brief       This function is called to check if the access address is the
 *              the same as any already existing valid access address.
 *
 *              Note: It is assumed that a LL connection data structure has not
 *                    yet been allocated as this is a potential connection. The
 *                    the check of already existing identical access addresses
 *                    is restricted to only already existing connections.
 *
 * input parameters
 *
 * @param       accessAddr - Connection synchronization word.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Indicates if access address meets criteria:
 *              TRUE: Access address does not meet criteria.
 *              FALSE: Access address does meet criteria.
 */
uint8 llEqAlreadyValidAddr( uint32 accessAddr )
{
  uint8 i;

  // check any already existing connections don't have the same access address
  for (i=0; i<llConns.numLLConns; i++)
  {
    if ( accessAddr == llConns.llConnection[ i ].accessAddr )
    {
      return( TRUE );
    }
  }

  return( FALSE );
}

/*******************************************************************************
 * @fn          llLtThreeOnesInLsb
 *
 * @brief       This function is called to check if the access address has at
 *              least three ones in the least significant byte.
 *
 * input parameters
 *
 * @param       accessAddr - Connection synchronization word.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Indicates if access address meets criteria:
 *              TRUE:  Access address does not meet criteria.
 *              FALSE: Access address meets criteria.
 */
uint8 llLtThreeOnesInLsb( uint32 accessAddr )
{
  uint8 count, i;

  // check if feature is enabled
  if ( !(deviceFeatureSet.featureSet[1] & LL_FEATURE_CODED_PHY) )
  {
    return( FALSE );
  }

  // init count
  count = 0;

  // for each subsequent bit
  for (i=0; i<8; i++)
  {
    // if there's a one bit
    if ( accessAddr & (1 << i) )
    {
      // check if we're done
      if ( ++count == 3 )
      {
        // yep, at least three ones in lsb
        return( FALSE );
      }
    }
  }

  return( TRUE );
}


/*******************************************************************************
 * @fn          llGtElevenTransitionsInLsh
 *
 * @brief       This function is called to check if the access address has more
 *              than 11 bit transitions in the least significant 16 bits.
 *
 * input parameters
 *
 * @param       accessAddr - Connection synchronization word.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Indicates if access address meets criteria:
 *              TRUE: Access address does not meet criteria.
 *              FALSE: Access address does meet criteria.
 */
uint8 llGtElevenTransitionsInLsh( uint32 accessAddr )
{
  // check if feature is enabled
  if ( deviceFeatureSet.featureSet[1] & LL_FEATURE_CODED_PHY )
  {
    uint8 prevBit, nextBit, count, i;

    // init count
    count = 0;

    // init first bit
    prevBit = (uint8)(accessAddr & 1);

    // for each subsequent bit
    for (i=1; i<16; i++)
    {
      // check if next bit is same as previous bit
      nextBit = (uint8)((accessAddr >> i) & 1);

      // check if we have a transition or not
      if (nextBit != prevBit)
      {
        // transition, so increment count and check if more than 11
        if (++count > 11)
        {
          // yep, more than 11 transitions
          return( TRUE );
        }

        // replace prev with this bit
        prevBit = nextBit;
      }
    }
  }

  return( FALSE );
}

/*******************************************************************************
 */
