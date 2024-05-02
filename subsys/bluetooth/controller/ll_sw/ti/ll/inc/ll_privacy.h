/******************************************************************************

 @file  ll_privacy.h

 @brief This file contains the data structures and APIs for handling
        Bluetooth Low Energy Privacy 1.2

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2015 $
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

#ifndef LL_PRIVACY_H
#define LL_PRIVACY_H

/*******************************************************************************
 * INCLUDES
 */

#include "bcomdef.h"
#include "ll.h"
#include "ll_common.h"
#include "ll_al.h"
#include "ll_dfl.h"

/*******************************************************************************
 * MACROS
 */

#define MASK_ID_ADDRTYPE( addrType ) ((addrType) & LL_DEV_ADDR_TYPE_MASK)

#define ADDRTYPE_TO_OWNADDR( addrType )                                        \
        ( (MASK_ID_ADDRTYPE((addrType)) == LL_DEV_ADDR_TYPE_PUBLIC) ?          \
          (ownPublicAddr)                                           :          \
          (ownRandomAddr) )

// Privacy policy flags - set, clear, and test.
// Clear all flags
#define CLEAR_ALL_TESTS( privPolicyFlags )                                     \
  ((privPolicyFlags) = 0)


// Address resolution enable flag - set, clear and test
#define CLEAR_ADDRESS_RESOLUTION_TEST( privPolicyFlags )                       \
  ((privPolicyFlags) &= ~BV(0))
#define SET_ADDRESS_RESOLUTION_TEST( privPolicyFlags )                         \
  ((privPolicyFlags) |= BV(0))
#define IS_ADDRESS_RESOLUTION_TEST_REQUIRED( privPolicyFlags )                 \
  (((privPolicyFlags) & BV(0)) != 0)

// RPA resolvable flag - set, clear and test
#define CLEAR_RESOLVABLE_RPA_TEST( privPolicyFlags )                          \
  ((privPolicyFlags) &= ~BV(1))
#define SET_RESOLVABLE_RPA_TEST( privPolicyFlags )                             \
  ((privPolicyFlags) |= BV(1))
#define IS_RESOLVABLE_RPA_TEST_TEST_REQUIRED( privPolicyFlags )                \
  (((privPolicyFlags) & BV(1)) != 0)

// Address is in accept list flag - set, clear and test
#define CLEAR_ADDRESS_IN_ACCEPT_LIST_TEST( privPolicyFlags )                   \
  (privPolicyFlags) &= ~BV(2)
#define SET_ADDRESS_IN_ACCEPT_LIST_TEST( privPolicyFlags )                     \
  (privPolicyFlags) |= BV(2)
#define IS_ADDRESS_IN_ACCEPT_LIST_TEST_REQUIRED( privPolicyFlags )             \
  (((privPolicyFlags) & BV(2)) != 0)

// Device privacy mode or valid IRK flag - set, clear and test
#define CLEAR_DPM_OR_INVALID_IRK_TEST( privPolicyFlags )                       \
  ((privPolicyFlags) &= ~BV(3))
#define SET_DPM_OR_INVALID_IRK_TEST( privPolicyFlags )                         \
  ((privPolicyFlags) |= BV(3))
#define IS_DPM_OR_INVALID_IRK_TEST_REQUIRED( privPolicyFlags )                 \
  (((privPolicyFlags) & BV(3)) != 0)

/*******************************************************************************
 * CONSTANTS
 */

#define BLE_RESOLVING_LIST_SIZE      (rlSize) // Resolving List Size For Peer + Local IRK/RP/IdAddr
#ifdef USE_DFL
#define EXT_ACCEPT_LIST_SIZE         0
#else
#define EXT_ACCEPT_LIST_SIZE         (2 * BLE_RESOLVING_LIST_SIZE)
#endif
#define LOCAL_RL_INDEX               0
#define EMPTY_RESOLVE_LIST_ENTRY     0xFF
#define INVALID_RESOLVE_LIST_INDEX   0xFFU
#define INVALID_EXT_ACCEPT_LIST_INDEX 0
#define MIN_RPA_TIMEOUT              0x0001 // 1s
#define MAX_RPA_TIMEOUT              0xA1B8 // 41400s or 11.5 hours
#define DEFAULT_RPA_TIMEOUT          0x0384 // 900s or 15 mins

// Masks for MSB of Device Address
#define RANDOM_ADDR_MASK             0xC0
//
#define NON_RESOLVABLE_ADDR_MASK     0x00
#define RESOLVABLE_ADDR_MASK         0x40
#define STATIC_RANDOM_ADDR_MASK      0xC0

// Accept List Size
#define AL_SIZE_STANDARD             0
#define AL_SIZE_EXTENDED             1

// Accept List Ignore For Privacy
#define PRIV_IGNORE_AL_ENTRY         0
#define PRIV_USE_AL_ENTRY            1

#ifdef QUAL_TEST
// List size limited to 3 entries because of test case LL/CON/INI/BV-23-C
#define LOCAL_IRK_LIST_SIZE          3
#endif

// Optional status for privacy policy tests
#define POLICY_NO_FAILED_TEST              0U
#define POLICY_INVALID_PARAMETERS          0xFF

/*******************************************************************************
 * TYPEDEFS
 */


// Optional required tests Flags for privacy policys
// | 8..5  |         4          |             3             |             2             |        1        |             0             |
// |  N/A  | DPM or Invalid IRK | Address is in Accept List | Address in Resolving List | Resolveable RPA | Enable Address Resolution |
//
typedef uint8_t privTestflags_t;

// RPA TargetA Entry Configuration
// |   7..2   |    1     |    0    |
// | Reserved | addrType | bEnable |
typedef uint8 rpaTargetAConf_t;

typedef struct
{
  uint8 idAddrType;                  // Identity Address Type (Public or Random Static)
  uint8 idAddr[ B_ADDR_LEN ];        // Identity Address
  uint8 RPA[ B_ADDR_LEN ];           // Resolvable Private Address
  uint8 IRK[ KEYLEN ];               // Identity Resolving Key
  uint8 privMode;                    // Privacy Mode (NPM - 0 , DPM - 1)
} rlEntry_t;

PACKED_TYPEDEF_STRUCT
{
  uint8 offset;                      //Index which indicates the end of the standard AL and the beginning of the ExtAL
  rpaTargetAConf_t conf;             //TargetA RPA configuration
  uint8 address[ LSB_2_BYTES ];      //Least significant 16 bits of the address contained in the entry
  uint8 addressHi[ MSB_4_BYTES ];    //Most significant 32 bits of the address contained in the entry
} rpaTargetAList_t;

PACKED_TYPEDEF_STRUCT
{
  uint8 *pRpaInitA;                  //Pointer to alternative address to send as InitA in a CONNECT_IND or AUX_CONNECT_REQ
  rpaTargetAList_t *pRpaTargetAList; //Pointer to a list of alternative addresses to send as TargetA in an AUX_CONNECT_RSP
} rpaCfg_t;

#ifdef QUAL_TEST
typedef struct
{
  uint8 peerAddrType;           // Peer Identity Address Type (Public or Random Static)
  uint8 peerAddr[ B_ADDR_LEN ]; // Peer Identity Address
  uint8 IRK[ KEYLEN ];          // Local Identity Resolving Key
} localIrkList_t;
#endif

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

extern privInfo_t privInfo;
extern rlEntry_t  *resolvingList;
extern rpaCfg_t   *pRpaCfg;
/*******************************************************************************
 * GLOBAL ROUTINES
 */

extern void             LL_PRIV_Init( void );

extern uint32           LL_PRIV_Ah( uint8 *irk, uint8 *prand );

extern void             LL_PRIV_GenerateRPA( uint8 *irk, uint8 *rpa );

extern void             LL_PRIV_GenerateNRPA( uint8 *publicAddr, uint8 *nrpa );

extern void             LL_PRIV_GenerateRSA( uint8 *rsa );

extern uint8            LL_PRIV_ResolveRPA( uint8 *rpa, uint8 *irk );

extern uint8            LL_PRIV_IsRPA( uint8 rxAddrType, uint8 *rxAddr );

extern uint8            LL_PRIV_IsNRPA( uint8 rxAddrType, uint8 *rxAddr );

extern uint8            LL_PRIV_IsIDA( uint8 rxAddrType, uint8 *rxAddr );

extern uint8            LL_PRIV_IsResolvable( uint8 *rpa, rlEntry_t *resolvingList );

extern uint8            LL_PRIV_IsZeroIRK( uint8 *irk );

extern uint8            LL_PRIV_FindPeerInRL( rlEntry_t *resolvingList, uint8 idType, uint8 *idAddr );

extern void             LL_PRIV_UpdateRL( rlEntry_t *resolvingList );

extern uint8            LL_PRIV_NumberPeerRLEntries( rlEntry_t *resolvingList );

extern void             LL_PRIV_CheckRLPeerId( rlEntry_t *resolvingList, alTable_t *pAlTable );

extern void             LL_PRIV_CheckRLPeerIdEntry( rlEntry_t *resolvingList, alTable_t *pAlTable );

extern rlEntry_t        *LL_PRIV_GetResolvingList( void );

extern privTestflags_t  LL_PRIV_PrivacyPolicyTests( uint8 *peerAddr, uint8 peeraddrType, uint8 rlIndex, privTestflags_t requiredTests );

extern llStatus_t       LL_PRIV_RemoveInvalidPeerId( rlEntry_t *pResolvingListEntry, dynamicFL_t *pDynamicFL, rankDynamicFL_t *prankFLTable );

// Extended (i.e. Private) Accept List

extern void             LL_PRIV_SetupPrivacy( alTable_t *pAlTable );

extern void             LL_PRIV_TeardownPrivacy( alTable_t *pAlTable );

extern void             LL_PRIV_ClearExtAL( alTable_t *pAlTable );

extern void             LL_PRIV_ClearAllPrivIgn( alTable_t *pAlTable );

extern uint8            LL_PRIV_AddExtALEntry( alTable_t *pAlTable, uint8 *devAddr, uint8 devAddrType, uint8 setIgnore );

extern void             LL_PRIV_UpdateExtALEntry( alTable_t *pAlTable, uint8 *oldRPA, uint8 *newRPA );

extern uint8            LL_PRIV_FindExtALEntry( alTable_t *pAlTable, uint8 *devAddr, uint8 devAddrType );

extern void             LL_PRIV_SetALSize( alTable_t *pAlTable, uint8 alSizeType );

extern uint8            LL_PRIV_FindEmptyExtALEntry(alTable_t *pAlTable);

#ifdef QUAL_TEST
extern void             LL_PRIV_UpdateLocalIrkList( uint8 *peerAddr, uint8 peerAddrType, uint8 *localIrk );

extern uint8            *LL_PRIV_GetLocalIrk( uint8 *peerAddr, uint8 peerAddrType);

#endif
/*******************************************************************************
 */

#endif /* LL_PRIVACY_H */
