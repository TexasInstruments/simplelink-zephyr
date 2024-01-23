/******************************************************************************

 @file  ll_ecc.h

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

#ifndef LL_ECC_H
#define LL_ECC_H

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

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */
extern uint8_t localPrivKeyMaterial[LL_SC_RAND_NUM_LEN]; // random private key
extern const uint8_t localDebugPrivateKeyMaterial[LL_SC_RAND_NUM_LEN];

/*******************************************************************************
 * Functions
 */
/*******************************************************************************
 * @fn          ll_eccInit API
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
extern void ll_eccInit( void );

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
extern int8_t ll_ReadLocalP256PublicKey( uint8 *publicKey );

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
 * @param       dhKey - pointer to generated dhKey.
 *
 * @return      None.
 */
extern int8_t ll_GenerateDHKey( uint8 *publicKey, uint8_t *dhKey);

#ifdef __cplusplus
}


#endif

#endif /* LL_ECC_H */
