/******************************************************************************
 @file:       icall_user_config.h

 @brief:    to do

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2016 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef ICALL_USER_CONFIG_H
#define ICALL_USER_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef ICALL_JT
/*******************************************************************************
 * INCLUDES
 */

#include <stdlib.h>

#if !defined(CC23X0) && !defined(CC33xx)
#include "nvintf.h"
#endif // !defined(CC23X0) && !defined(CC33xx)

#include "hal_assert.h"
#include <ti/drivers/cryptoutils/ecc/ECCParams.h>

/*******************************************************************************
 * TYPEDEFS
 */

PACKED_TYPEDEF_CONST_STRUCT
{
  int8   Pout;
#if defined(CC13X2P)
  uint32 txPwrVal;
#else // !CC13X2P
  uint16 txPwrVal;
#endif // CC13X2P
} txPwrVal_t;

PACKED_TYPEDEF_CONST_STRUCT
{
  txPwrVal_t *txPwrValsPtr;
  uint8       numTxPwrVals;
  int8        defaultTxPwrVal;
} txPwrTbl_t;

/* The define EM_CC1354P10_1_LP is needed since it is High PA device for
   other stacks (not for BLE) and thus needed to be defined */
#if defined(CC13X2P) || defined(EM_CC1354P10_1_LP)
PACKED_TYPEDEF_CONST_STRUCT
{
  int8 txPwrBackoff1MPhy;
  int8 txPwrBackoff2MPhy;
  int8 txPwrBackoffCoded;
} txPwrBackoffVal_t;

PACKED_TYPEDEF_CONST_STRUCT
{
  txPwrBackoffVal_t *txPwrBackoffValsPtr;
} txPwrBackoffTbl_t;

#endif // CC13X2P || EM_CC1354P10_1_LP

typedef const uint32 antennaIOEntry_t;

typedef struct
{
  uint32           antMask;
  uint8            antPropTblSize;
  antennaIOEntry_t *antPropTbl;
} cteAntProp_t;

typedef struct
{
  void  *coexConfig;
  void  *coexConfigBle;
} coexUseCaseConfig_t;

typedef const uint32 rfDrvTblPtr_t;

typedef const uint32 eccDrvTblPtr_t;

typedef const uint32 icallServiceTblPtr_t;

typedef const uint32 cryptoDrvTblPtr_t;

typedef const uint32 rtosApiTblPtr_t;

typedef const uint32 trngDrvTblPtr_t;

typedef const uint32 extflashDrvTblPtr_t;

typedef const uint32 tirtosSwiCmdTblPtr_t;

typedef struct
{
#ifdef CC23X0
  uint8 k;
#else
  rfDrvTblPtr_t        *rfDrvTbl;
  eccDrvTblPtr_t       *eccDrvTbl;
  cryptoDrvTblPtr_t    *cryptoDrvTbl;
  trngDrvTblPtr_t      *trngDrvTbl;
  rtosApiTblPtr_t      *rtosApiTbl;
  extflashDrvTblPtr_t  *extflashDrvTbl;
  void                 *nvintfStructPtr;
#endif
} drvTblPtr_t ;

typedef struct
{
  uint8_t         rfFeModeBias;
#ifndef CC23X0
  regOverride_t  *rfRegTbl;
  regOverride_t  *rfRegTbl1M;
  regOverride_t  *rfRegTbl2M;
  regOverride_t  *rfRegTblCoded;
  txPwrTbl_t     *txPwrTbl;
  /* The define EM_CC1354P10_1_LP is needed since it is High PA device for
     other stacks (not for BLE) and thus needed to be defined */
#if defined(CC13X2P) || (EM_CC1354P10_1_LP)
  txPwrBackoffTbl_t     *txPwrBackoffTbl;           // Tx power backoff table
  regOverride_t         *rfRegOverrideTxStdTblptr;  // Default PA
  regOverride_t         *rfRegOverrideTx20TblPtr;   // High power PA
#endif //CC13X2P
  regOverride_t         *rfRegOverrideCtePtr;       // CTE overrides
  cteAntProp_t          *cteAntennaPropPtr;         // CTE antenna properties
  uint8_t               privOverrideOffset;
  coexUseCaseConfig_t   *coexUseCaseConfigPtr;      // CoEx priority and RX request configuration
#endif
} boardConfig_t;

typedef struct
{
  uint32_t              timerTickPeriod;
  uint32_t              timerMaxMillisecond;
  assertCback_t         *assertCback;
#ifndef CC23X0
  icallServiceTblPtr_t  *icallServiceTbl;
#endif
} applicationService_t ;

typedef struct
{
  const void               *stackConfig;
#ifndef CC23X0
  const drvTblPtr_t        *drvTblPtr;
#endif
  const boardConfig_t      *boardConfig;
  applicationService_t     *appServiceInfo;
} icall_userCfg_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */
extern applicationService_t   bleAppServiceInfoTable;
// nvintf function pointer struct, populated at runtime
#if !defined(CC23X0) && !defined(CC33xx)
extern NVINTF_nvFuncts_t nvintfFncStruct;
#endif // !defined(CC23X0) && !defined(CC33xx)

/*********************************************************************
 * FUNCTIONS
 */
extern assertCback_t appAssertCback; // only App's ble_user_config.c
extern assertCback_t halAssertCback; // only Stack's ble_user_config.c

#endif /* ICALL_JT */

#ifdef __cplusplus
}
#endif

#endif /* ICALL_USER_CONFIG_H */
