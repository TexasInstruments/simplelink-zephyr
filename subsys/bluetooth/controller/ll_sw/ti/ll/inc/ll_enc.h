/******************************************************************************

 @file  ll_enc.h

 @brief This file contains the Link Layer (LL) types, constants,
        API's etc. for the Bluetooth Low Energy (BLE) Controller
        CCM encryption and decryption.

        This API is based on ULP BT LE D09R23.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2009 $
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

#ifndef LL_ENC_H
#define LL_ENC_H

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * INCLUDES
 */

#include "bcomdef.h"
#include "ll_common.h"

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

#define LL_ENC_TX_DIRECTION_CENTRAL       1
#define LL_ENC_TX_DIRECTION_PERIPHERAL    0
#define LL_ENC_RX_DIRECTION_CENTRAL       0
#define LL_ENC_RX_DIRECTION_PERIPHERAL    1

#define LL_ENC_DATA_BANK_MASK        0xFF7F

#define LL_ENC_TRUE_RAND_BUF_SIZE    ((LL_ENC_IV_LEN/2) + (LL_ENC_SKD_LEN/2))

// Generate Session Key using LTK for key and SKD for plaintext.
#define LL_ENC_GenerateSK            MAP_LL_ENC_AES128_Encrypt

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

// ALT: Remove caching as CC26xx is fast enough.
extern uint8 cachedTRNGdata[];

/*******************************************************************************
 * Functions
 */

extern void  LL_ENC_Init( void );
extern void  LL_ENC_LoadKey( uint8 *key );
extern int16 LL_ENC_EncryptMsg( uint8 *nonce, uint8 pktHdr, uint8 pktLen, uint8 *pBuf );
extern int16 LL_ENC_DecryptMsg( uint8 *nonce, uint8 pktHdr, uint8 pktLen, uint8 *pBuf, uint8 *mic );

// Random Number Generation
extern uint8 LL_ENC_GeneratePseudoRandNum( void );
extern uint8 LL_ENC_GenerateTrueRandNum( uint8 *buf, uint8 len );
extern uint8 LL_ENC_GenerateDRBGRandNum( uint8 *buf, uint8 len );
extern void  LL_ENC_GenerateDRBGSeedNum();
extern uint8 LL_ENC_GenerateTRNGRandNum( uint8 *buf, uint8 len );

// CCM Encryption
extern void  LL_ENC_AES128_Encrypt( uint8 *key, uint8 *plaintext,  uint8 *ciphertext );
extern void  LL_ENC_AES128_Decrypt( uint8 *key, uint8 *ciphertext, uint8 *plaintext );
extern void  LL_ENC_ReverseBytes( uint8 *buf, uint8 len );
extern void  LL_ENC_GenDeviceSKD( uint8 *SKD );
extern void  LL_ENC_GenDeviceIV( uint8 *IV );
extern void  LL_ENC_GenerateNonce( uint32 pktCnt, uint8 direction, uint8 *nonce );
extern int16 LL_ENC_Encrypt( llConnState_t *connPtr, uint8 pktHdr, uint8 pktLen, uint8 *pBuf );
extern int16 LL_ENC_Decrypt( llConnState_t *connPtr, uint8 pktHdr, uint8 pktLen, uint8 *pBuf );

#ifdef __cplusplus
}
#endif

#endif /* LL_ENC_H */
