/******************************************************************************

 @file  ll_ecc.c

 @brief This file contains the BLE encryption API for the LL.

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

//#ifndef CONFIG_SOC_CC2340R5
#include <ti/drivers/ECDH.h>
#ifdef CC23X0
#include <ti/drivers/ecdh/ECDHLPF3SW.h>
#include <ti/drivers/cryptoutils/sharedresources/CryptoResourceLPF3.h>
#else
#if !defined(DeviceFamily_CC26X1)
#include <ti/drivers/ecdh/ECDHCC26X2.h>
#else
#include <ti/drivers/ecdh/ECDHCC26X1.h>
#endif
#include <ti/drivers/cryptoutils/sharedresources/CryptoResourceCC26XX.h>
#include "rtos_api.h"
#ifndef CC33xx
#include "crypto_api.h"
#include "ecc_api.h"
#endif // !CC33xx
#endif // !CC23X0

#include <ti/drivers/cryptoutils/cryptokey/CryptoKeyPlaintext.h>
#include <ti/drivers/cryptoutils/ecc/ECCParams.h>
//#endif
#include "bcomdef.h"
#include "hal_mcu.h"
#include "ll_common.h"
#include "ll_config.h"

#include "ll_enc.h"
#include "ll_ecc.h"
#include "rom_jt.h"
#include "hci.h"
#include "ll_user_config.h"

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

// store the private key for debug according to spec
const uint8_t localDebugPrivateKeyMaterial[LL_SC_RAND_NUM_LEN] =
  {0xBD, 0x1A, 0x3C, 0xCD, 0xA6, 0xB8, 0x99, 0x58, 0x99, 0xB7, 0x40, 0xEB,
   0x7B, 0x60, 0xFF, 0x4A, 0x50, 0x3F, 0x10, 0xD2, 0xE3, 0xB3, 0xC9, 0x74,
   0x38, 0x5F, 0xC5, 0xA3, 0xD4, 0xF6, 0x49, 0x3F};

 // store the public key for debug according to spec
const uint8_t localDebugPublicKeyMaterial[LL_SC_P256_KEY_LEN] =
  {0xE6, 0x9D, 0x35, 0x0E, 0x48, 0x01, 0x03, 0xCC, 0xDB, 0xFD, 0xF4, 0xAC,
   0x11, 0x91, 0xF4, 0xEF, 0xB9, 0xA5, 0xF9, 0xE9, 0xA7, 0x83, 0x2C, 0x5E,
   0x2C, 0xBE, 0x97, 0xF2, 0xD2, 0x03, 0xB0, 0x20,
   0x8B, 0xD2, 0x89, 0x15, 0xD0, 0x8E, 0x1C, 0x74, 0x24, 0x30, 0xED, 0x8F,
   0xC2, 0x45, 0x63, 0x76, 0x5C, 0x15, 0x52, 0x5A, 0xBF, 0x9A, 0x32, 0x63,
   0x6D, 0xEB, 0x2A, 0x65, 0x49, 0x9C, 0x80, 0xDC};

uint8_t localPrivKeyMaterial[LL_SC_RAND_NUM_LEN]; // random private key

//#ifndef CONFIG_SOC_CC2340R5
CryptoKey localPrivateKey;
CryptoKey localPublicKey;
CryptoKey remotePublicKey;
CryptoKey sharedSecret;
CryptoKey symmetricKey;

ECDH_Handle ecdhHandle;
//#endif
int_fast16_t operationResult;

//#ifndef CONFIG_SOC_CC2340R5
// ECDH parameter
ECDH_Params eccParams;

// public key parameter
ECDH_OperationGeneratePublicKey operationGeneratePublicKey;
ECDH_OperationComputeSharedSecret operationComputeSharedSecret;
//#endif
uint8_t localPubKeyMaterial[ LL_SC_P256_KEY_LEN ]; // Place holder for public key

/*******************************************************************************
 * PROTOTYPES
 */
/*******************************************************************************
 * LOCAL FUNCTIONS
 */

/*******************************************************************************
 * API
 */

/*******************************************************************************
 * @fn          ll_ecc_Init API
 *
 * @brief       This API is used to initiate the generation of a Diffie-
 *              Hellman key in the Controller for use over the LE transport.
 *              This command takes the remote P-256 public key as input. The
 *              Diffie-Hellman key generation uses the private key generated
 *              by LE_Read_Local_P256_Public_Key command.
 *
 *              Note: This function is only called once!
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
void ll_eccInit( void )
{
//#ifndef CONFIG_SOC_CC2340R5

  // Initialize ECC Driver
  ECDH_init();

  // Initialize ECC Driver parameters
  ECDH_Params_init(&eccParams);
  if(*llConfigTable.ecdhMode == ECDH_RETURN_BEHAVIOR_POLLING)
  {
    eccParams.returnBehavior = ECDH_RETURN_BEHAVIOR_POLLING;
  }
  else if(*llConfigTable.ecdhMode == ECDH_RETURN_BEHAVIOR_BLOCKING)
  {
    eccParams.returnBehavior = ECDH_RETURN_BEHAVIOR_BLOCKING;
  }
  else
  {
        /* this else clause is required, even if the
           programmer expects this will never be reached
           Fix Misra-C Required: MISRA.IF.NO_ELSE */
  }

  // Since we are using default ECDH_Params, we just pass in NULL for that parameter.
  ecdhHandle = ECDH_open(0, &eccParams);

  HAL_ASSERT( ecdhHandle != NULL );
//#endif
  return;
}

/*******************************************************************************
 * @fn          ll_ReadLocalP256PublicKey API
 *
 * @brief       This function is used to read the local P-256 public key from
 *              the Controller. The Controller shall generate a new P-256
 *              public/private key pair.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       publicKey - pointer to buffer to store the generated public key.
 *
 * @return      None.
 *
 */
int8_t ll_ReadLocalP256PublicKey( uint8 *publicKey )
{

  // generate random number
  // Note: Assumed here that this is a standard FIPS compliant PRAND value.
#ifndef DEBUG_SC
  MAP_LL_ENC_GenerateTrueRandNum( localPrivKeyMaterial, LL_SC_RAND_NUM_LEN );
#endif // DEBUG_SC

//#ifndef CONFIG_SOC_CC2340R5
  // Initialize myPrivateKey and myPublicKey
  CryptoKeyPlaintext_initKey(&localPrivateKey, (uint8_t *)localPrivKeyMaterial, LL_SC_RAND_NUM_LEN);
  CryptoKeyPlaintext_initBlankKey(&localPublicKey, (uint8_t *)publicKey, LL_SC_P256_KEY_LEN_OCTET_STRING_FORMAT);
  ECDH_OperationGeneratePublicKey_init(&operationGeneratePublicKey);
  operationGeneratePublicKey.curve            = llConfigTable.userCfgPtr->eccCurveParams;
  operationGeneratePublicKey.myPrivateKey     = &localPrivateKey;
  operationGeneratePublicKey.myPublicKey      = &localPublicKey;

  // Generate the keying material for myPublicKey and store it in myPublicKeyingMaterial
  operationResult = ECDH_generatePublicKey(ecdhHandle, &operationGeneratePublicKey);

  if (operationResult != ECDH_STATUS_SUCCESS)
  {
    // Handle error
    LL_ASSERT( FALSE );
  }

  // ECDH_generatePublicKey() is assuming localPrivKeyMaterial is in big-endian format.
  // The stack is using little-endian, therefore, reversing localPrivKeyMaterial to little-endian.
  MAP_LL_ENC_ReverseBytes(localPrivKeyMaterial, LL_SC_RAND_NUM_LEN);
//#endif
  // check the ECC software status; reuse status for LL status
  return operationResult;
}

/*******************************************************************************
 * @fn          ll_GenerateDHKey API
 *
 * @brief       This API is used to initiate the generation of a Diffie-
 *              Hellman key in the Controller for use over the LE transport.
 *              This command takes the remote P-256 public key as input. The
 *              Diffie-Hellman key generation uses the private key generated
 *              by LE_Read_Local_P256_Public_Key command.
 *
 *              WARNING: THIS ROUTINE WILL TIE UP THE LL FOR ABOUT 160ms!
 *
 * input parameters
 *
 * @param       publicKey: The remote P-256 public key (X-Y format).
 *
 * output parameters
 *
 * @param       dhKey - pointer to dhKey.
 *
 * @return      None.
 *
 */
int8_t ll_GenerateDHKey( uint8 *publicKey, uint8_t *dhKey )
{
//#ifndef CONFIG_SOC_CC2340R5

  //uint8 dhKey[ (2*LL_SC_DHKEY_LEN) ]; // shared secret

  // Initialize their public CryptoKey and the shared secret CryptoKey
#ifndef DEBUG_SC
  CryptoKeyPlaintext_initKey(&localPrivateKey, (uint8_t *)localPrivKeyMaterial, LL_SC_RAND_NUM_LEN);
  CryptoKeyPlaintext_initKey(&remotePublicKey, publicKey, LL_SC_P256_KEY_LEN_OCTET_STRING_FORMAT);
#else
  CryptoKeyPlaintext_initKey(&remotePublicKey, (uint8_t *)localDebugPublicKeyMaterial, LL_SC_P256_KEY_LEN);
#endif

  CryptoKeyPlaintext_initBlankKey(&sharedSecret, dhKey, ((2*LL_SC_DHKEY_LEN) + 1));

  // The ECC_NISTP256 struct is provided in ti/drivers/types/EccParams.h and the corresponding device-specific implementation
  ECDH_OperationComputeSharedSecret_init(&operationComputeSharedSecret);
  operationComputeSharedSecret.curve              = llConfigTable.userCfgPtr->eccCurveParams;
  operationComputeSharedSecret.myPrivateKey       = &localPrivateKey;
  operationComputeSharedSecret.theirPublicKey     = &remotePublicKey;
  operationComputeSharedSecret.sharedSecret       = &sharedSecret;

  // ECDH_computeSharedSecret() expects to get localPrivKeyMaterial in big-endian format.
  // The stack is using little-endian, therefore, reversing localPrivKeyMaterial to big-endian.
  MAP_LL_ENC_ReverseBytes(localPrivKeyMaterial, LL_SC_RAND_NUM_LEN);

  // Compute the shared secret/DH Key and copy it to sharedSecretKeyingMaterial
  operationResult = ECDH_computeSharedSecret(ecdhHandle, &operationComputeSharedSecret);

  // Reversing localPrivKeyMaterial back to little-endian.
  MAP_LL_ENC_ReverseBytes(localPrivKeyMaterial, LL_SC_RAND_NUM_LEN);
//#endif
  // check the ECC software status; reuse status for LL status
  return operationResult;
}

/*******************************************************************************
 */
