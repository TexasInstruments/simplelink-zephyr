/******************************************************************************

 @file  crypto_board.c

 @brief TI-RTOS and *Ware implementation of Crypto board service

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2013 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#include <xdc/std.h>
#include "crypto_board.h"

#include <inc/hw_types.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/crypto/CryptoCC26XX.h>
#include "icall.h"

/** @internal service dispatcher entity ID */
static ICall_EntityID Crypto_entity;
CryptoCC26XX_Handle CryptoCC26XXHandle;

/* service function.
 * See type description of ICall_ServiceFunc for
 * descriptions of arguments. */
static ICall_Errno Crypto_func(ICall_FuncArgsHdr *args)
{
  if(args->func == CRYPTOBOARD_AES_LOAD_KEY)
  {
    CryptoBoard_AesArgs *cryptoArgs = (CryptoBoard_AesArgs *) args;
    return CryptoCC26XX_allocateKey( CryptoCC26XXHandle,
                                     (CryptoCC26XX_KeyLocation)cryptoArgs->keyLocation,
                                     (const uint32_t *)cryptoArgs->keySrc );
  }
  else if(args->func == CRYPTOBOARD_AES_RELEASE_KEY)
  {
    CryptoBoard_AesArgs *cryptoArgs = (CryptoBoard_AesArgs *) args;
    int keyIndex = (int)cryptoArgs->keyLocation;
    return CryptoCC26XX_releaseKey( CryptoCC26XXHandle, &keyIndex );
  }
  else if(args->func == CRYPTOBOARD_AES_ECB)
  {
    CryptoBoard_AesArgs *cryptoArgs = (CryptoBoard_AesArgs *) args;
    CryptoCC26XX_AESECB_Transaction transaction;

    transaction.opType = CRYPTOCC26XX_OP_AES_ECB;
    transaction.keyIndex = cryptoArgs->keyLocation;
    transaction.msgIn = (uint32_t *)cryptoArgs->msgIn;
    transaction.msgOut = (uint32_t *)cryptoArgs->msgOut;
    return CryptoCC26XX_transact( CryptoCC26XXHandle,
                                  (CryptoCC26XX_Transaction *)&transaction );
  }
  else if(args->func == CRYPTOBOARD_AES_CCM)
  {
    CryptoBoard_AesCcmArgs *cryptoArgs = (CryptoBoard_AesCcmArgs *) args;
    CryptoCC26XX_AESCCM_Transaction transaction;

    transaction.opType = CRYPTOCC26XX_OP_AES_CCM;
    transaction.keyIndex = cryptoArgs->AesCcmTransact.keyLocation;
    transaction.authLength = cryptoArgs->AesCcmTransact.authLength;
    transaction.nonce = (char *)cryptoArgs->AesCcmTransact.nonce;
    transaction.msgIn = (char *)cryptoArgs->AesCcmTransact.plainText;
    transaction.header = (char *)cryptoArgs->AesCcmTransact.header;
    transaction.fieldLength = cryptoArgs->AesCcmTransact.fieldLength;
    transaction.msgInLength = cryptoArgs->AesCcmTransact.plainTextLength;
    transaction.headerLength = cryptoArgs->AesCcmTransact.headerLength;
    transaction.msgOut = (uint32_t *)cryptoArgs->msgOut;
    return CryptoCC26XX_transactPolling( CryptoCC26XXHandle,
                                  (CryptoCC26XX_Transaction *)&transaction );
  }
  else if(args->func == CRYPTOBOARD_AES_CCM_INV)
  {
    CryptoBoard_AesCcmArgs *cryptoArgs = (CryptoBoard_AesCcmArgs *) args;
    CryptoCC26XX_AESCCM_Transaction transaction;

    transaction.opType = CRYPTOCC26XX_OP_AES_CCMINV;
    transaction.keyIndex = cryptoArgs->AesCcmTransact.keyLocation;
    transaction.authLength = cryptoArgs->AesCcmTransact.authLength;
    transaction.nonce = (char *)cryptoArgs->AesCcmTransact.nonce;
    transaction.msgIn = (char *)cryptoArgs->AesCcmTransact.plainText;
    transaction.header = (char *)cryptoArgs->AesCcmTransact.header;
    transaction.fieldLength = cryptoArgs->AesCcmTransact.fieldLength;
    transaction.msgInLength = cryptoArgs->AesCcmTransact.plainTextLength;
    transaction.headerLength = cryptoArgs->AesCcmTransact.headerLength;
    transaction.msgOut = (uint32_t *)cryptoArgs->msgOut;
    return CryptoCC26XX_transactPolling( CryptoCC26XXHandle,
                                  (CryptoCC26XX_Transaction *)&transaction );
  }
  else
  {
    /* Unknown function ID */
    return ICALL_ERRNO_INVALID_FUNCTION;
  }
}


/* See header file to find the function description. */
void Crypto_init(void)
{
  ICall_SyncHandle sem;

  /* Enroll the service to the dispatcher */
  if (ICall_enrollService(ICALL_SERVICE_CLASS_CRYPTO,
                          Crypto_func, &Crypto_entity, &sem) !=
                          ICALL_ERRNO_SUCCESS)
  {
    ICall_abort();
  }
}
