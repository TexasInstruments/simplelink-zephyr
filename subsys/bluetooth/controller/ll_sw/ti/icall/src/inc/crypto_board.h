/******************************************************************************

 @file  crypto_board.h

 @brief interface definition for board dependent service for Crypto.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2013 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef CRYPTOBOARD_H
#define CRYPTOBOARD_H

#include <stdbool.h>
#include <stdint.h>
#include <icall.h>

/** board service function to use Crypto module */
#define CRYPTOBOARD_AES_LOAD_KEY       0

#define CRYPTOBOARD_AES_RELEASE_KEY    1

#define CRYPTOBOARD_AES_ECB            2

#define CRYPTOBOARD_AES_CCM            3

#define CRYPTOBOARD_AES_CCM_INV        4

/** enum for Key locations in the KEY RAM */
typedef enum AESKeyLocation {
    AESKEY_0 = 0,
    AESKEY_1 = 1,
    AESKEY_2 = 2,
    AESKEY_3 = 3,
    AESKEY_4 = 4,
    AESKEY_5 = 5,
    AESKEY_6 = 6,
    AESKEY_7 = 7
} AESKeyLocation;


/** AES ECB Structure */
typedef struct _cryptoboard_aes_args_t
{
  /** common arguments */
  ICall_FuncArgsHdr hdr;
  /**  Address of key **/
  uint_least8_t *keySrc;
  /** key RAM location of key **/
  AESKeyLocation keyLocation;
  /** pointer to input message **/
  uint_least8_t *msgIn;
  /** pointer to output message **/
  uint_least8_t *msgOut;
  /** set to true for encrypt and false for decrypt **/
  bool encrypt;
} CryptoBoard_AesArgs;


/** AES CCM Structure */
typedef struct AESCCM_Transact {
  /** Is the the length of the authentication field - */
  uint_least32_t authLength;
  /** A pointer to 13-byte or 12-byte Nonce. */
  int_least8_t   *nonce;
  /** A poiner to the octet string input message */
  int_least8_t   *plainText;
  /** The length of the message */
  uint_least32_t  plainTextLength;
  /** The Additional Authentication Data or AAD */
  uint_least8_t  *header;
  /** The length of the header in octets */
  uint_least32_t  headerLength;
  /** The size of the length field (2 or 3) */
  uint_least32_t  fieldLength;
  /** A keystore entry number */
  uint_least8_t   keyLocation;
} AESCCM_Transact;


typedef struct _cryptoboard_aesccm_args_t
{
   /** common arguments */
  ICall_FuncArgsHdr hdr;
  /** CCM Structure */
  AESCCM_Transact AesCcmTransact;
  /** MIC */
  uint_least8_t *msgOut;
  /** encrypt decrypt flag . If set to
   *  false authentication only operation */
  bool encrypt_decrypt;
}CryptoBoard_AesCcmArgs;


/**
 * Initializes Crypto service module.
 */
extern void Crypto_init(void);


/**
 *  Write the key into the Key Ram.
 *
 *  @param keySrc is a pointer to an AES Key.
 *  @param keyLocation is the location of the key in Key RAM.
 *
 *  The keyLocation parameter is an enumerated type which specifies
 *  the Key Ram location in which the key is stored.
 *  This parameter can have any of the following values:
 *
 *  AESKEY_0
 *  AESKEY_1
 *  AESKEY_2
 *  AESKEY_3
 *  AESKEY_4
 *  AESKEY_5
 *  AESKEY_6
 *  AESKEY_7
 *
 *  returns AES_SUCCESS if successful and AES_KEYSTORE_READ_ERROR if failing
 */
static inline ICall_Errno
CryptoBoard_AesLoadKey(uint_least8_t *keySrc, AESKeyLocation keyLocation)
{
  CryptoBoard_AesArgs args;
  args.hdr.service = ICALL_SERVICE_CLASS_CRYPTO;
  args.hdr.func = CRYPTOBOARD_AES_LOAD_KEY ;
  args.keySrc = keySrc;
  args.keyLocation = keyLocation;
  return ICall_dispatcher(&args.hdr);
}

/**
 *  Release a key from the Key Ram.
 *
 *  @param keyLocation is the location of the key in Key RAM.
 *
 *  The keyLocation parameter is an enumerated type which specifies
 *  the Key Ram location in which the key is stored.
 *  This parameter can have any of the following values:
 *
 *  AESKEY_0
 *  AESKEY_1
 *  AESKEY_2
 *  AESKEY_3
 *  AESKEY_4
 *  AESKEY_5
 *  AESKEY_6
 *  AESKEY_7
 *
 *  returns AES_SUCCESS if successful and AES_KEYSTORE_READ_ERROR if failing
 */
static inline ICall_Errno
CryptoBoard_AesReleaseKey(AESKeyLocation keyLocation)
{
  CryptoBoard_AesArgs args;
  args.hdr.service = ICALL_SERVICE_CLASS_CRYPTO;
  args.hdr.func = CRYPTOBOARD_AES_RELEASE_KEY ;
  args.keyLocation = keyLocation;
  return ICall_dispatcher(&args.hdr);
}


/**
 *
 * Start an AES-ECB operation (encryption or decryption).
 *
 * @param msgIn is a pointer to the input data.
 * @param msgOut is a pointer to the output data.
 * @param keyLocation is the location of the key in Key RAM.
 * @param encrypt is set 'true' to encrypt or set 'false' to decrypt.
 *
 *  The keyLocation parameter is an enumerated type which specifies
 *  the Key Ram location in which the key is stored.
 *  This parameter can have any of the following values:
 *
 *  AESKEY_0
 *  AESKEY_1
 *  AESKEY_2
 *  AESKEY_3
 *  AESKEY_4
 *  AESKEY_5
 *  AESKEY_6
 *  AESKEY_7
 *
 *  returns AES_SUCCESS if successful else returns error code
 */
static inline ICall_Errno
CryptoBoard_AesEcb(uint_least8_t *msgIn, uint_least8_t *msgOut,
                   AESKeyLocation keyLocation, bool encrypt)
{
  CryptoBoard_AesArgs args;
  args.hdr.service = ICALL_SERVICE_CLASS_CRYPTO;
  args.hdr.func = CRYPTOBOARD_AES_ECB ;
  args.msgIn = msgIn;
  args.msgOut = msgOut;
  args.keyLocation = keyLocation;
  args.encrypt = encrypt;
  return ICall_dispatcher(&args.hdr);
}


/**
 *
 * CCM Encrypt operation
 *
 * @param encrypt determines whether to run encryption or not.
 * @param Mval is the the length of the authentication field -
 * 0, 2, 4, 6, 8, 10, 12, 14 or 16 octets.
 * @param Nonce is a pointer to 13-byte or 12-byte Nonce.
 * @param M is a poiner to the octet string input message,
 * @param len_m is the length of the message,
 * @param A is the length of the header (Additional Authentication
 * Data or AAD).
 * @param len_a is the length of the header in octets.
 * @param MAC is the pointer to MIC
 * @param ccmLval is the size of the length field (2 or 3).
 * @param keylocation is the location in Key RAM where the key is stored.
 * @param bIntEnable enables interrupts.
 *
 *  The keyLocation parameter is an enumerated type which specifies
 *  the Key Ram location in which the key is stored.
 *  This parameter can have any of the following values:
 *
 *  AESKEY_0
 *  AESKEY_1
 *  AESKEY_2
 *  AESKEY_3
 *  AESKEY_4
 *  AESKEY_5
 *  AESKEY_6
 *  AESKEY_7
 *
 * returns AES_SUCCESS if successful - otherwise
 * AES_KEYSTORE_READ_ERROR or AES_DMA_BUS_ERROR
 */

static inline ICall_Errno
CryptoBoard_CCMEncrypt(uint_least8_t encrypt, uint_least8_t Mval,
                       uint_least8_t *Nonce, uint_least8_t *M,
                       uint_least16_t len_m, uint_least8_t *A,
                       uint_least16_t len_a, uint_least8_t *MAC,
                       uint_least8_t ccmLVal, uint_least8_t keyLocation)

{
  CryptoBoard_AesCcmArgs args;
  args.hdr.service = ICALL_SERVICE_CLASS_CRYPTO;
  args.hdr.func = CRYPTOBOARD_AES_CCM;
  args.AesCcmTransact.authLength = Mval;
  args.AesCcmTransact.nonce = (int_least8_t *)Nonce;
  args.AesCcmTransact.plainText = (int_least8_t *)M;
  args.AesCcmTransact.plainTextLength = len_m;
  args.AesCcmTransact.header = A;
  args.AesCcmTransact.headerLength = len_a;
  args.AesCcmTransact.fieldLength =  ccmLVal;
  args.AesCcmTransact.keyLocation = (AESKeyLocation)keyLocation;
  args.msgOut = MAC;
  args.encrypt_decrypt = encrypt;
  return ICall_dispatcher(&args.hdr);
}


/**
 *
 * CCM Decrypt operation
 *
 * @param decrypt determines whether to run encryption or not.
 * @param Mval is the the length of the authentication field -
 * 0, 2, 4, 6, 8, 10, 12, 14 or 16 octets.
 * @param Nonce is a pointer to 13-byte or 12-byte Nonce.
 * @param M is a poiner to the octet string input message,
 * @param len_m is the length of the message,
 * @param A is the length of the header (Additional Authentication
 * Data or AAD).
 * @param len_a is the length of the header in octets.
 * @param MAC is the pointer to MIC
 * @param ccmLval is the size of the length field (2 or 3).
 * @param keylocation is the location in Key RAM where the key is stored.
 * @param bIntEnable enables interrupts.
 *
 *  The keyLocation parameter is an enumerated type which specifies
 *  the Key Ram location in which the key is stored.
 *  This parameter can have any of the following values:
 *
 *  AESKEY_0
 *  AESKEY_1
 *  AESKEY_2
 *  AESKEY_3
 *  AESKEY_4
 *  AESKEY_5
 *  AESKEY_6
 *  AESKEY_7
 *
 * returns AES_SUCCESS if successful - otherwise
 * AES_KEYSTORE_READ_ERROR or AES_DMA_BUS_ERROR
 */
static inline ICall_Errno
CryptoBoard_CCMDecrypt(uint_least8_t decrypt, uint_least8_t Mval,
                       uint_least8_t *Nonce, uint_least8_t *M,
                       uint_least16_t len_m, uint_least8_t *A,
                       uint_least16_t len_a, uint_least8_t *MAC,
                       uint_least8_t ccmLVal, uint_least8_t keyLocation)

{
  CryptoBoard_AesCcmArgs args;
  args.hdr.service = ICALL_SERVICE_CLASS_CRYPTO;
  args.hdr.func = CRYPTOBOARD_AES_CCM_INV;
  args.AesCcmTransact.authLength = Mval;
  args.AesCcmTransact.nonce = (int_least8_t *)Nonce;
  args.AesCcmTransact.plainText = (int_least8_t *)M;
  args.AesCcmTransact.plainTextLength =  len_m;
  args.AesCcmTransact.header = A;
  args.AesCcmTransact.headerLength = len_a;
  args.AesCcmTransact.fieldLength = ccmLVal;
  args.AesCcmTransact.keyLocation = (AESKeyLocation) keyLocation;
  args.msgOut = MAC;
  args.encrypt_decrypt = decrypt;
  return ICall_dispatcher(&args.hdr);
}

#endif /* CRYPTOBOARD_H */
