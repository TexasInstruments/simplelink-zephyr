/******************************************************************************

 @file  ll_enc.c

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

#ifndef CONFIG_SOC_CC2340R5
#include <ti/drivers/AESCCM.h>
#include <ti/drivers/AESECB.h>
#include <ti/drivers/AESCTRDRBG.h>
#ifdef CC23X0
#include <ti/drivers/aesccm/AESCCMLPF3.h>
#include <ti/drivers/aesecb/AESECBLPF3.h>
#include <ti/drivers/cryptoutils/sharedresources/CryptoResourceLPF3.h>
#else
#include <ti/drivers/aesccm/AESCCMCC26XX.h>
#include <ti/drivers/aesecb/AESECBCC26XX.h>
#include <ti/drivers/cryptoutils/sharedresources/CryptoResourceCC26XX.h>
#ifndef CC33xx
#include "trng_api.h"
#include "crypto_api.h"
#endif // !CC33xx
#endif // CC23X0
#include <ti/drivers/cryptoutils/cryptokey/CryptoKeyPlaintext.h>
#endif
#include <ti/drivers/utils/Random.h>
#include "bcomdef.h"
#include "hal_mcu.h"
#include "ll_common.h"
#include "ll_config.h"
#include "ll_enc.h"
#include "rom_jt.h"

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

//
#define ENC_KEY_LEN_MAX                32
#define ENC_KEY_LEN                    16
#define AES_BLOCK_LEN                  16  // 16 bytes or 128 bits
#define AAD_LEN                        1
#define FIELD_LEN                      2

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */
#ifndef CONFIG_SOC_CC2340R5
AESCTRDRBG_Handle drbgHandle;
#endif
/*******************************************************************************
 * GLOBAL VARIABLES
 */

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
// cache of FIPS compliant true random numbers for IV and SKD formation
// ALT: Remove caching as CC26xx is fast enough.
uint8 cachedTRNGdata[ LL_ENC_TRUE_RAND_BUF_SIZE ] ALIGNED;

#ifdef DEBUG_ENC
const uint8 testSKD[] = { 0x79, 0x68, 0x57, 0x46, 0x35, 0x24, 0x13, 0x02,   // peripheral
                          0x13, 0x02, 0xF1, 0xE0, 0xDF, 0xCE, 0xBD, 0xAC }; // central
const uint8 testIV[]  = { 0xBE, 0xBA, 0xAF, 0xDE,    // peripheral
                          0x24, 0xAB, 0xDC, 0xBA };  // central
#endif  // DEBUG_ENC

#endif // ADV_CONN_CFG | INIT_CFG

#ifndef CONFIG_SOC_CC2340R5
// data needed for TI crypto driver for AESCCM
AESCCM_Handle    encHandleCCM;
AESCCM_Params    encParamsCCM;
AESCCM_Operation operationCCM;

// data needed for TI crypto driver for AESECB
AESECB_Handle    encHandleECB;
AESECB_Params    encParamsECB;
AESECB_Operation operationECB;
// Reserve space for CryptoKey and key material
CryptoKey        cryptoKey;
#endif
uint8_t          enckey[ENC_KEY_LEN_MAX] ALIGNED = {0};

/*******************************************************************************
 * PROTOTYPES
 */
/*******************************************************************************
 * LOCAL FUNCTIONS
 */
void LL_AESCCM_Init(void);
void LL_AESECB_Init(void);

/*******************************************************************************
 * API
 */

/*******************************************************************************
 * @fn          LL_ENC_Init API
 *
 * @brief       For the internal crypto (BLE) driver:
 *              This function is used to reset the AES engine and reset its
 *              DMAC. It also sets up the Central run time parameters, clears
 *              interrutps, and sets the interrupt to Level.
 *
 *              Note: This routine leaves CM3 Crypto interrupts disabled.
 *
 *              For the external driver:
 *              This function initializes the crypto parameters, and opens
 *              the driver.
 *
 *              Note: This function allocates the key without specifying one.
 *                    The key is loaded as needed without releases the key
 *                    store.
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
void LL_ENC_Init( void )
{
#ifndef CONFIG_SOC_CC2340R5
  /* Initialize both AESCCM and AESECB objects as BLE stack is using both */
  LL_AESCCM_Init();
  LL_AESECB_Init();

  // Initialize cryptoKey data structure
  CryptoKeyPlaintext_initKey(&cryptoKey, enckey, ENC_KEY_LEN);
  cryptoKey.u.plaintext.keyMaterial = enckey;
#endif
  return;
}

/*******************************************************************************
 * @fn          LL_AESCCM_Init API
 *
 * @brief       For the internal crypto (BLE) driver:
 *              This function is used to reset the AES engine and reset its
 *              DMAC. It also sets up the Central run time parameters, clears
 *              interrutps, and sets the interrupt to Level.
 *
 *              Note: This routine leaves CM3 Crypto interrupts disabled.
 *
 *              For the external driver:
 *              This function initializes the crypto parameters, and opens
 *              the driver.
 *
 *              Note: This function allocates the key without specifying one.
 *                    The key is loaded as needed without releases the key
 *                    store.
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
void LL_AESCCM_Init( void )
{
#ifndef CONFIG_SOC_CC2340R5

  /* Initialize AESCCM object */
  AESCCM_init();

  AESCCM_Params_init(&encParamsCCM);

  if(*llConfigTable.cryptoMode == CRYPTO_DRV_MODE_POLLING)
  {
    encParamsCCM.returnBehavior = AESCCM_RETURN_BEHAVIOR_POLLING;
  }
  else if(*llConfigTable.cryptoMode == CRYPTO_DRV_MODE_BLOCKING)
  {
    encParamsCCM.returnBehavior = AESCCM_RETURN_BEHAVIOR_BLOCKING;
  }
  else
  {
    encParamsCCM.returnBehavior = AESCCM_RETURN_BEHAVIOR_CALLBACK;
    //encParamsCCM.callbackFxn = ccmCallback;
  }

  encHandleCCM = AESCCM_open(0, &encParamsCCM);
  HAL_ASSERT( encHandleCCM != NULL );

  AESCCM_Operation_init(&operationCCM);
#endif
  return;
}


/*******************************************************************************
 * @fn          LL_AESECB_Init API
 *
 * @brief       For the internal crypto (BLE) driver:
 *              This function is used to reset the AES engine and reset its
 *              DMAC. It also sets up the Central run time parameters, clears
 *              interrutps, and sets the interrupt to Level.
 *
 *              Note: This routine leaves CM3 Crypto interrupts disabled.
 *
 *              For the external driver:
 *              This function initializes the crypto parameters, and opens
 *              the driver.
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
void LL_AESECB_Init( void )
{
#ifndef CONFIG_SOC_CC2340R5

  /* Initialize both AESCCM objects as BLE stack is using both */
  AESECB_init();

  AESECB_Params_init( &encParamsECB );
  if(*llConfigTable.cryptoMode == CRYPTO_DRV_MODE_POLLING)
  {
    encParamsECB.returnBehavior = AESECB_RETURN_BEHAVIOR_POLLING;
  }
  else if(*llConfigTable.cryptoMode == CRYPTO_DRV_MODE_BLOCKING)
  {
    encParamsECB.returnBehavior = AESECB_RETURN_BEHAVIOR_BLOCKING;
  }
  else
  {
    encParamsECB.returnBehavior = AESECB_RETURN_BEHAVIOR_CALLBACK;
    // Note: Note: If a callback is used, then it should be hooked using encParamsECB.callbackFxn
  }
  encHandleECB = AESECB_open(0, &encParamsECB);
  HAL_ASSERT( encHandleECB != NULL );

  AESECB_Operation_init(&operationECB);
#endif
  return;
}


/*******************************************************************************
 * @fn          LL_ENC_ReverseBytes API
 *
 * @brief       This function is used to reverse the order of the bytes in
 *              an array in place.
 *
 *              Note: The max length is 128 bytes; the min length is > 0.
 *
 * input parameters
 *
 * @param       buf - Pointer to buffer containing bytes to be reversed.
 * @param       len - Number of bytes in buffer.
 *
 * output parameters
 *
 * @param       buf - Pointer to buffer containing reversed bytes.
 *
 * @return      None.
 */
void LL_ENC_ReverseBytes( uint8 *buf,
                          uint8 len )
{
  uint8 temp;
  uint8 index = (uint8)(len - 1);
  uint8 i;

  // adjust length as only half the operations are needed
  len >>= 1;

  // reverse the order of the bytes
  for (i=0; i<len; i++)
  {
    temp           = buf[i];
    buf[i]         = buf[index - i];
    buf[index - i] = temp;
  }

  return;
}


/*******************************************************************************
 * @fn          LL_ENC_GeneratePseudoRandNum API
 *
 * @brief       This function is used to generate a pseudo random byte.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      A pseudo random byte.
 */
uint8 LL_ENC_GeneratePseudoRandNum( void )
{
  // grab on random byte
  return( Random_getNumber() & 0xFF );
}


/*******************************************************************************
 * @fn          LL_ENC_GenerateTrueRandNum API
 *
 * @brief       This function is used to generate a sequence of true random
 *              bytes. The result is FIPS compliant. This routine also re-seeds
 *              the PRNG, ensuring an improper seed isn't used.
 *
 *              Note: This algorithm was not checked for FIPS compliance over
 *                    voltage change.
 *
 * input parameters
 *
 * @param       len - The number of true random bytes desired (1..255).
 *
 * output parameters
 *
 * @param       buf - A pointer to a buffer for storing true random bytes.
 *
 * @return      LL_STATUS_SUCCESS
 */
uint8 LL_ENC_GenerateTrueRandNum( uint8 *buf,
                                  uint8  len )
{
  // Use DRBG driver instead of TRNG driver (slow init time) - LL_ENC_GenerateTRNGRandNum(buf, len);
  return(LL_ENC_GenerateDRBGRandNum(buf, len));
}

/*******************************************************************************
 * @fn          LL_ENC_GenerateTRNGRandNum API
 *
 * @brief       This function is used to generate a sequence of random
 *              bytes by TRNG driver
 *
 * input parameters
 *
 * @param       len - The number of random bytes desired (1..255).
 *
 * output parameters
 *
 * @param       buf - A pointer to a buffer for storing true random bytes.
 *
 * @return      LL_STATUS_SUCCESS
 */
uint8 LL_ENC_GenerateTRNGRandNum( uint8 *buf,
                                  uint8  len )
{
  if (len == 1)
  {
    *buf = (uint8)rand();
  }
  else if (len == 2)
  {
    *buf = (uint16)rand();
  }
  else
  {
    *buf = (uint32)rand();
  }


  return LL_STATUS_SUCCESS;
#ifndef CONFIG_SOC_CC2340R5
  CryptoKey entropy;
  uint8 status;

  // Initialize the CryptoKey
  CryptoKeyPlaintext_initBlankKey(&entropy, buf, len);

#ifdef CC23X0
  status = RNG_generateKey(trngHandle, &entropy);

  if (status != RNG_STATUS_SUCCESS)
  {
     HAL_ASSERT( HAL_ASSERT_CAUSE_INTERNAL_ERROR );
  }
#else
  status = TRNG_generateEntropy(trngHandle, &entropy);

  if (status != TRNG_STATUS_SUCCESS)
  {
     HAL_ASSERT( HAL_ASSERT_CAUSE_INTERNAL_ERROR );
  }
#endif // CC23X0

#endif
}

/*******************************************************************************
 * @fn          LL_ENC_GenerateDRBGRandNum API
 *
 * @brief       This function is used to generate a sequence of random
 *              bytes by DRBG driver - Deterministic Rrandom Bit Generator
 *              This function uses seed generated by the TRNG and can be
 *              reseed when DRBG returns 'RESEED_REQUIRED'
 *
 * input parameters
 *
 * @param       len - The number of random bytes desired (1..255).
 *
 * output parameters
 *
 * @param       buf - A pointer to a buffer for storing true random bytes.
 *
 * @return      LL_STATUS_SUCCESS
 */
uint8 LL_ENC_GenerateDRBGRandNum( uint8 *buf,
                                  uint8  len )
{
    if (len == 1)
    {
      *buf = (uint8)rand();
    }
    else if (len == 2)
    {
      *buf = (uint16)rand();
    }
    else
    {
      *buf = (uint32)rand();
    }


    return LL_STATUS_SUCCESS;

#ifndef CONFIG_SOC_CC2340R5

  CryptoKey entropyKey;
  int_fast16_t result;
  halIntState_t  cs;

  // DRBG must be initialized
  if (drbgHandle == NULL || buf == NULL)
  {
    return (LL_STATUS_ERROR_HW_FAILURE);
  }

  // Start generating random numbers
  // Since we don't know if buf is Word aligned, we will use a temp workzone - enckey
  if (len > sizeof(enckey))
  {
    return (LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED);
  }

  // Initialize entropyKey
  CryptoKeyPlaintext_initBlankKey(&entropyKey, enckey, len);

  HAL_ENTER_CRITICAL_SECTION(cs);
  result = AESCTRDRBG_generateKey(drbgHandle, &entropyKey);
  HAL_EXIT_CRITICAL_SECTION(cs);

  // Check return value and reseed if needed. This should happen only after many invocations
  // of AESCTRDRBG_getBytes().
  if (result == AESCTRDRBG_STATUS_RESEED_REQUIRED)
  {
      int_fast16_t reseedResult;
      uint8_t reseedBuffer[AESCTRDRBG_SEED_LENGTH_AES_128];

      // Generate new seed number by TRNG
      reseedResult = LL_ENC_GenerateTRNGRandNum(reseedBuffer, AESCTRDRBG_SEED_LENGTH_AES_128);

#ifdef CC23X0
      if (reseedResult != RNG_STATUS_SUCCESS)
#else
      if (reseedResult != TRNG_STATUS_SUCCESS)
#endif //CC23X0
      {
        return (LL_STATUS_ERROR_HW_FAILURE);
      }

      HAL_ENTER_CRITICAL_SECTION(cs);
      reseedResult = AESCTRDRBG_reseed(drbgHandle, reseedBuffer, NULL, 0);
      HAL_EXIT_CRITICAL_SECTION(cs);

      if (reseedResult != AESCTRDRBG_STATUS_SUCCESS)
      {
        return (LL_STATUS_ERROR_HW_FAILURE);
      }

      HAL_ENTER_CRITICAL_SECTION(cs);
      // Call again after re-seed
      result = AESCTRDRBG_generateKey(drbgHandle, &entropyKey);
      HAL_EXIT_CRITICAL_SECTION(cs);
  }

  if (result != AESCTRDRBG_STATUS_SUCCESS)
  {
    return (LL_STATUS_ERROR_HW_FAILURE);
  }

  // If we got here then we are good
  // Copy the random bytes from enckey to buf provided by user
  MAP_osal_memcpy(buf, enckey, len);

  return LL_STATUS_SUCCESS;
#endif // CONFIG_SOC_CC2340R5
}

/*******************************************************************************
 * @fn          LL_ENC_GenerateDRBGSeedNum API
 *
 * @brief       This function is used to initiate the DRBG driver
 *              DRBG - Deterministic Rrandom Bit Generator
 *              TRNG must be opened before calling this function
 *              This function uses seed generated by the TRNG and can be
 *              reseed when DRBG returns 'RESEED_REQUIRED'
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void LL_ENC_GenerateDRBGSeedNum()
{
#ifndef CONFIG_SOC_CC2340R5
  AESCTRDRBG_Params drbgParams;
  uint8_t seedBuffer[AESCTRDRBG_SEED_LENGTH_AES_128];

  // TRNG must be initialized
  if (trngHandle == NULL)
  {
    // Handle error
    LL_ASSERT(FALSE);
  }

  // Generate initial seed buffer by the TRNG for the DRBG
  LL_ENC_GenerateTRNGRandNum(seedBuffer, AESCTRDRBG_SEED_LENGTH_AES_128);
#ifdef DeviceFamily_CC27XX
  seedBuffer[0] ^= ownPublicAddr[0];
  seedBuffer[1] ^= ownPublicAddr[1];
  seedBuffer[2] ^= ownPublicAddr[2];
  seedBuffer[3] ^= ownPublicAddr[3];
  seedBuffer[4] ^= ownPublicAddr[4];
  seedBuffer[5] ^= ownPublicAddr[5];
#endif
  // Open DRBG with the above seed buffer
  AESCTRDRBG_init();

  // Instantiate the AESCTRDRBG instance
  AESCTRDRBG_Params_init(&drbgParams);
  drbgParams.keyLength = AESCTRDRBG_AES_KEY_LENGTH_128;
  drbgParams.reseedInterval = 10000;
  drbgParams.seed = seedBuffer;

  drbgHandle = AESCTRDRBG_open(0, &drbgParams);
  if (drbgHandle == NULL)
  {
    // Handle error
    LL_ASSERT(FALSE);
  }
#endif
}

#if defined(CTRL_CONFIG) && ((CTRL_CONFIG & ADV_CONN_CFG) || (CTRL_CONFIG & INIT_CFG))
/*******************************************************************************
 * @fn          LL_ENC_GenDeviceSKD API
 *
 * @brief       This function is used to generate a device's half of the SKD.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       SKD - A pointer to a buffer in which to place the device's SKD.
 *
 * @return      None.
 */
void LL_ENC_GenDeviceSKD( uint8 *SKD )
{
  uint8 i;

  // used cached FIPS compliant TRNG data
  // Note: SKD (central or peripheral) ues first LL_ENC_SKD_LEN/2 of the bytes.
  // Note: Calling routine will schedule a cache update as a postRF operation.
  for (i=0; i<LL_ENC_SKD_LINK_LEN; i++)
  {
#ifdef DEBUG_ENC
    if ( llState == LL_STATE_CONN_PERIPHERAL )
    {
      SKD[i] = testSKD[i];
    }
    else // llState == LL_STATE_CONN_CENTRAL
    {
      SKD[i] = testSKD[i+LL_ENC_SKD_LINK_LEN];
    }
#else // !DEBUG_ENC
    // ALT: Remove caching as CC26xx is fast enough.
    SKD[i] = cachedTRNGdata[i];  // (void)MAP_LL_ENC_GenerateTrueRandNum
#endif // DEBUG_ENC
  }

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && ((CTRL_CONFIG & ADV_CONN_CFG) || (CTRL_CONFIG & INIT_CFG))
/*******************************************************************************
 * @fn          LL_ENC_GenDeviceIV API
 *
 * @brief       This function is used to generate a device's half of the IV.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       IV - A pointer to a buffer in which to place the device's IV.
 *
 * @return      None.
 */
void LL_ENC_GenDeviceIV( uint8 *IV )
{
  uint8 i;

  // used cached FIPS compliant TRNG data
  // Note: SKD (central or peripheral) takes first LL_ENC_SKD_LEN/2 of the bytes.
  // Note: Calling routine will schedule a cache update as a postRF operation.
  for (i=0; i<LL_ENC_IV_LINK_LEN; i++)
  {
#ifdef DEBUG_ENC
    if ( llState == LL_STATE_CONN_PERIPHERAL )
    {
      IV[i] = testIV[i];
    }
    else // llState == LL_STATE_CONN_CENTRAL
    {
      IV[i] = testIV[i+LL_ENC_IV_LINK_LEN];
    }
#else // !DEBUG_ENC
    // ALT: Remove caching as CC26xx is fast enough.
    IV[i] = cachedTRNGdata[i+(LL_ENC_SKD_LEN / 2)];
#endif // DEBUG_ENC
  }

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && ((CTRL_CONFIG & ADV_CONN_CFG) || (CTRL_CONFIG & INIT_CFG))
/*******************************************************************************
 * @fn          LL_ENC_GenerateNonce API
 *
 * @brief       This function is used to form the Nonce.
 *
 *              Note: This routine assumes that the nonce already contains the
 *                    IV in bytes 5..13.
 * input parameters
 *
 * @param       pktCnt    - The packet count.
 * @param       direction - LL_ENC_TX_DIRECTION or LL_ENC_RX_DIRECTION.
 * @param       nonce     - Pointer to Nonce to be updated.
 *
 * output parameters
 *
 * @param       nonce - Pointer to Nonce that has been updated.
 *
 * @return      None.
 */
void LL_ENC_GenerateNonce( uint32 pktCnt,
                           uint8  direction,
                           uint8  *nonce )
{
  // add packet count to the nonce
  // Note: Packet count is supposed to be 39 bits; assume 32 for now.
  nonce[0] = ((uint8 *)&pktCnt)[0]; //(pktCnt >>  0) & 0xFF;
  nonce[1] = ((uint8 *)&pktCnt)[1]; //(pktCnt >>  8) & 0xFF;
  nonce[2] = ((uint8 *)&pktCnt)[2]; //(pktCnt >> 16) & 0xFF;
  nonce[3] = ((uint8 *)&pktCnt)[3]; //(pktCnt >> 24) & 0xFF;

  // add direction bit to MSO
  // Note: Last 7 bits of 39-bit packet count are assumed to be zero as only
  //       a 32 bit counter is currently supported.
  nonce[4] = (direction << 7);

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG


/*******************************************************************************
 * @fn          LL_ENC_LoadKey API
 *
 * @brief       For internal driver:
 *              This function is used to load the cryptography key into the
 *              AES engine.
 *
 *              For the external driver:
 *              This function calls the driver load key function to update
 *              the key at the key store.
 *
 *              Note: Once the key store is allocated, it is never freed.
 *
 * input parameters
 *
 * @param       key - Pointer to 16 byte cryptography key.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_ENC_LoadKey( uint8 *key )
{
#ifndef CONFIG_SOC_CC2340R5
  halIntState_t cs;

  HAL_ENTER_CRITICAL_SECTION(cs);

  // The cryptoKey is initialized already, just update the key material
  MAP_osal_memcpy(cryptoKey.u.plaintext.keyMaterial, key, ENC_KEY_LEN);

  HAL_EXIT_CRITICAL_SECTION(cs);
#endif
  return;
}


/*******************************************************************************
 * @fn          LL_ENC_AES128_Encrypt API
 *
 * @brief       For internal driver:
 *              This function takes a key, plaintext, and generates ciphertext
 *              by AES128 encryption. This function is used to generate the
 *              BLE Session Key (SK) from the Long Term Key (LTK) and Session
 *              Key Diversifier (SKD).
 *
 *              Note: The array indexes are MSO..LSO for LTK, SKD, and SK.
 *
 *              For the external driver:
 *              This function calls the driver transact to perform AES ECB
 *              encryption based on the provided key and plaintext.
 *
 * input parameters
 *
 * @param       key       - The 128 bit key to be used for encryption.
 * @param       plaintext - The 128 bit plain text before encryption.
 *
 * output parameters
 *
 * @param       ciphertext - The 128 bit cipher text after encryption.
 *
 * @return      None.
 */
void LL_ENC_AES128_Encrypt( uint8 *key,
                            uint8 *plaintext,
                            uint8 *ciphertext )
{
  halIntState_t cs;

  HAL_ENTER_CRITICAL_SECTION(cs);

#ifndef CONFIG_SOC_CC2340R5
  MAP_LL_ENC_LoadKey(key);

  operationECB.key               = &cryptoKey;
  operationECB.input             = plaintext;
  operationECB.output            = ciphertext;
  operationECB.inputLength       = AES_BLOCK_LEN;

  if(AESECB_oneStepEncrypt(encHandleECB, &operationECB) != AESECB_STATUS_SUCCESS)
  {
    LL_ASSERT(FALSE);
  }
#endif
  HAL_EXIT_CRITICAL_SECTION(cs);

  return;
}



/*******************************************************************************
 * @fn          LL_ENC_AES128_Decrypt API
 *
 * @brief       For internal driver:
 *              This function takes a key, ciphertext, and generates plaintext
 *              by AES128 decryption.
 *
 *              Note: The array indexes are MSO..LSO for LTK, SKD, and SK.
 *
 *              For the external driver:
 *              This function calls the driver transact to perform AES ECB
 *              decryption based on the provided key and ciphertext.
 *
 * input parameters
 *
 * @param       key        - The 128 bit key to be used for decryption.
 * @param       ciphertext - The 128 bit cipher text to be decrypted.
 *
 * output parameters
 *
 * @param       plaintext  - The 128 bit plain text after decryption.
 *
 * @return      None.
 */
void LL_ENC_AES128_Decrypt(uint8 *key,
                           uint8 *ciphertext,
                           uint8 *plaintext)
{
#ifndef CONFIG_SOC_CC2340R5

  halIntState_t cs;

  HAL_ENTER_CRITICAL_SECTION(cs);

  MAP_LL_ENC_LoadKey(key);

  operationECB.key               = &cryptoKey;
  operationECB.input             = ciphertext;
  operationECB.output            = plaintext;
  operationECB.inputLength       = AES_BLOCK_LEN;

  // check whether to use polling or blocking
  // Note: Blocking is only supported when there's an LL RTOS task!
  if(AESECB_oneStepDecrypt(encHandleECB, &operationECB) != AESECB_STATUS_SUCCESS)
  {
    LL_ASSERT(FALSE);
  }

  HAL_EXIT_CRITICAL_SECTION(cs);
#endif
  return;
}

#if defined(CTRL_CONFIG) && ((CTRL_CONFIG & ADV_CONN_CFG) || (CTRL_CONFIG & INIT_CFG))
/*******************************************************************************
 * @fn          LL_ENC_EncryptMsg API
 *
 * @brief       For internal driver:
 *              This function is used to encrypt a packet buffer and its
 *              corresponding authentication MIC value. The encrypted
 *              MIC is stored directly in the buffer after the encrypted data.
 *
 *              For the external driver:
 *              This function calls the driver transact to perform AES CCM
 *              encryption of packet data.
 *
 *              Note: Assumes the Session Key (SK) has already been loaded
 *                    into the AES engine (i.e. this is only needed once).
 *
 * input parameters
 *
 * @param       nonce  - Pointer to 13 byte nonce.
 * @param       pktHdr - Packet header LLID.
 * @param       pktLen - Length of buffer to be encrypted.
 * @param       pBuf   - Pointer to buffer to be encrypted.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
int16 LL_ENC_EncryptMsg( uint8 *nonce,
                         uint8  pktHdr,
                         uint8  pktLen,
                         uint8 *pBuf )
{
#ifndef CONFIG_SOC_CC2340R5

  // ensure the packet header's NESN, SN, and MD bits are masked out
  pktHdr &= LL_DATA_PDU_HDR_LLID_MASK | BV(LL_DATA_PDU_HDR_CP_BIT);

  operationCCM.key               = &cryptoKey;
  operationCCM.aad               = &pktHdr;
  operationCCM.aadLength         = AAD_LEN;
  operationCCM.input             = pBuf;
  operationCCM.output            = pBuf;
  operationCCM.inputLength       = pktLen;
  operationCCM.nonce             = nonce;
  operationCCM.nonceLength       = LL_ENC_NONCE_LEN;
  operationCCM.mac               = pBuf+pktLen;
  operationCCM.macLength         = LL_PKT_MIC_LEN;

  // Guard against SWI interrupts specifically from RX callback. EncryptMsg and
  // DecryptMsg(called from a SWI) share a binary semaphore transSem. If you are
  // unlucky this can result in CryptoCC26XX_transactPolling() being interrupted
  // by Decrypt after it has pended on transSem but before it has posted causing
  // a deadlock as the SWI will never yield to the task running EncryptMsg.
  return (AESCCM_oneStepEncrypt(encHandleCCM, &operationCCM));
#endif
  return -1;
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && ((CTRL_CONFIG & ADV_CONN_CFG) || (CTRL_CONFIG & INIT_CFG))
/*******************************************************************************
 * @fn          LL_ENC_DecryptMsg API
 *
 * @brief       For internal driver:
 *              This function is used to decrypt a packet buffer and and to
 *              generate the corresponding authentication MIC value. The MIC is
 *              returned as an output parameter so it can be used to compare
 *              against the MIC stored after the decrypted packet buffer to
 *              authenticate the packet.
 *
 *              For the external driver:
 *              This function calls the driver transact to perform AES CCM
 *              decryption of packet data.
 *
 *              Note: Packet length should NOT include MIC, but the MIC should
 *                    follow the data.
 *
 *              Note: Assumes the Session Key (SK) has already been loaded
 *                    into the AES engine (i.e. this is only needed once).
 *
 * input parameters
 *
 * @param       nonce  - Pointer to 13 byte nonce.
 * @param       pktHdr - Packet header LLID.
 * @param       pktLen - Length of buffer to be decrypted.
 * @param       pBuf   - Pointer to buffer to be decrypted.
 * @param       mic    - Encrypted MIC generated based on buffer data.
 *
 * output parameters
 *
 * @param       mic    - Decrypted MIC.
 *
 * @return      None.
 */
int16 LL_ENC_DecryptMsg( uint8 *nonce,
                         uint8  pktHdr,
                         uint8  pktLen,
                         uint8 *pBuf,
                         uint8 *mic )
{

  int16 decryptionResult = 0;
#ifndef CONFIG_SOC_CC2340R5

  // ensure the packet header's NESN, SN, and MD bits are masked out
  pktHdr &= LL_DATA_PDU_HDR_LLID_MASK | BV(LL_DATA_PDU_HDR_CP_BIT);

  operationCCM.key               = &cryptoKey;
  operationCCM.aad               = &pktHdr;
  operationCCM.aadLength         = AAD_LEN;           // 1 byte
  operationCCM.input             = pBuf;
  operationCCM.inputLength       = pktLen;
  operationCCM.output            = pBuf;
  operationCCM.nonce             = nonce;
  operationCCM.nonceLength       = LL_ENC_NONCE_LEN;  // 15 -2 bytes
  operationCCM.mac               = mic;
  operationCCM.macLength         = LL_PKT_MIC_LEN;    // 4 byte

  // check whether to use polling or blocking
  // Note: Blocking is only suppprted when there's a LL RTOS task!
  decryptionResult = AESCCM_oneStepDecrypt(encHandleCCM, &operationCCM);

  if (decryptionResult != AESCCM_STATUS_SUCCESS)
  {
    // Fill up the decrypted message with 0's
    MAP_osal_memset(pBuf, 0, pktLen);
  }
#endif
  return decryptionResult;
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          LL_ENC_Encrypt API
 *
 * @brief       This function is used to encrypt a packet. It loads the key,
 *              generates the nonce, generates the MIC, and encrypts the data
 *              and MIC.
 *
 *              Note: This routine assumes that the buffer is 32 bytes and has
 *                    already been zero padded.
 *
 * input parameters
 *
 * @param       connPtr - A pointer to this connection's information.
 * @param       pktHdr  - Packet header, LLID only.
 * @param       pktLen  - Length of buffer to be encrypted.
 * @param       pBuf    - Pointer to buffer to be encrypted.
 *
 * output parameters
 *
 * @param       pBuf   - Pointer to encrypted buffer.
 *
 * @return      None.
 */
int16 LL_ENC_Encrypt( llConnState_t *connPtr,
                      uint8          pktHdr,
                      uint8          pktLen,
                      uint8         *pBuf )
{
  int16 status;
  halIntState_t cs;

  HAL_ENTER_CRITICAL_SECTION(cs);

  if ( connPtr->llTask->taskID == LL_TASK_ID_CENTRAL )
  {
    // generate the nonce based on packet count, IV, and direction
    MAP_LL_ENC_GenerateNonce( connPtr->encInfo.txPktCount,
                              LL_ENC_TX_DIRECTION_CENTRAL,
                              connPtr->encInfo.nonce );
  }
  else if ( connPtr->llTask->taskID == LL_TASK_ID_PERIPHERAL )
  {
    // generate the nonce based on packet count, IV, and direction
    MAP_LL_ENC_GenerateNonce( connPtr->encInfo.txPktCount,
                              LL_ENC_TX_DIRECTION_PERIPHERAL,
                              connPtr->encInfo.nonce );
  }
  else // unexpected taskID for a connection!
  {
    LL_ASSERT( FALSE );
  }

  // Load Key
  // Note: Normally this would only need to be done once when the SK is derived
  //       from the LTK and SKD. However, when in sleep, the AES block loses
  //       this key. Also, when multiple connections are supported, the key
  //       will be different.
  MAP_LL_ENC_LoadKey( connPtr->encInfo.SK );

  // encrypt packet in the TX FIFO
  // Note: encrypted MIC is placed directly in buffer after payload.
  status = MAP_LL_ENC_EncryptMsg( connPtr->encInfo.nonce,
                                  pktHdr,
                                  pktLen,
                                  pBuf );

  // up the count for the next TX'ed data packet
  // Note: This is supposed to be 39 bit counter, but for now, we don't
  //       envision receiving 550 billion packets during a connection!
  connPtr->encInfo.txPktCount++;

  HAL_EXIT_CRITICAL_SECTION(cs);

  return status;
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          LL_ENC_Decrypt API
 *
 * @brief       This function is used to decrypt a packet. It loads the key,
 *              generates the nonce, decrypts the data and MIC, generates a
 *              MIC based on the decrypted data, and authenticates the packet
 *              by comparing the two MIC values.
 *
 *              Note: This routine assumes that the buffer is 32 bytes,
 *                    regardless of the number of bytes of data.
 *
 * input parameters
 *
 * @param       connPtr - A pointer to this connection's information.
 * @param       pktHdr  - Packet header, LLID only.
 * @param       pktLen  - Length of buffer to be decrypted.
 * @param       pBuf    - Pointer to buffer to be decrypted.
 *
 * output parameters
 *
 * @param       pBuf   - Pointer to decrypted buffer.
 *
 * @return      status.
 */
int16 LL_ENC_Decrypt( llConnState_t *connPtr,
                      uint8          pktHdr,
                      uint8          pktLen,
                      uint8         *pBuf )
{
  int16 status;
  halIntState_t cs;

  LL_ASSERT( pktLen > 0  );

  HAL_ENTER_CRITICAL_SECTION(cs);

  if ( connPtr->llTask->taskID == LL_TASK_ID_CENTRAL )
  {
    // generate the nonce based on packet count, IV, and direction
    MAP_LL_ENC_GenerateNonce( connPtr->encInfo.rxPktCount,
                              LL_ENC_RX_DIRECTION_CENTRAL,
                              connPtr->encInfo.nonce );
  }
  else if ( connPtr->llTask->taskID == LL_TASK_ID_PERIPHERAL )
  {
    // generate the nonce based on packet count, IV, and direction
    MAP_LL_ENC_GenerateNonce( connPtr->encInfo.rxPktCount,
                              LL_ENC_RX_DIRECTION_PERIPHERAL,
                              connPtr->encInfo.nonce );
  }
  else // unexpected taskID for a connection!
  {
    LL_ASSERT( FALSE );
  }

  // Load Key
  // Note: Normally this would only need to be done once when the SK is derived
  //      from the LTK and SKD. However, when in sleep, the AES block loses
  //      this key. Also, when multiple connections are supported, the key
  //      will be different.
  MAP_LL_ENC_LoadKey( connPtr->encInfo.SK );

  // encrypt packet in the TX FIFO
  // Note: Encrypted MIC is placed directly in buffer after payload.
  status = MAP_LL_ENC_DecryptMsg( connPtr->encInfo.nonce,
                                  pktHdr,
                                  pktLen,
                                  pBuf,
                                 (pBuf + pktLen) );

  // up the count for the next RX'ed data packet
  // Note: This is supposed to be 39 bit counter, but for now, we don't
  //       envision receiving 550 billion packets during a connection!
  connPtr->encInfo.rxPktCount++;

  HAL_EXIT_CRITICAL_SECTION(cs);

  return( status );
}
#endif // ADV_CONN_CFG | INIT_CFG

/*******************************************************************************
 */
