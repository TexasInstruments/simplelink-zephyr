/******************************************************************************

 @file  ll_cs_sec.c

 @brief Generates the LL CS Vectors.
        Triggers the CS DRBG to generate random bits for the different
        DRBG transactions.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2023 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*******************************************************************************
 * INCLUDES
 */
#include "bcomdef.h"
#include "ll_cs_common.h"
/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * EXTERNS
 */

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * @fn          llCsSecGenerateIV
 *
 * @brief       This function is used to generate a device's half of the CS_IV.
 * Random numbers generated using the requirements for random
 * number generation defined in specification[Vol 2] Part H, Section 2
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       csIV - pointer to Initialization vector
 *
 * output parameters
 *
 * @param       csIV.
 *
 * @return      Status
 */
uint8 llCsSecGenerateIV(uint8* csIV);

/*******************************************************************************
 * @fn          llCsSecGenerateIN
 *
 * @brief       This function is used to generate a device's half of the CS_IN.
 * Random numbers generated using the requirements for random
 * number generation defined in specification[Vol 2] Part H, Section 2
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       csIN - pointer to Instantiation vector
 *
 * output parameters
 *
 * @param       csIN.
 *
 * @return      Status
 */
uint8 llCsSecGenerateIN(uint8* csIN);

/*******************************************************************************
 * @fn          llCsSecGeneratePV
 *
 * @brief       This function is used to generate a device's half of the CS_PV.
 * Random numbers generated using the requirements for random
 * number generation defined in specification[Vol 2] Part H, Section 2
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       csPV - pointer to personalization vector

 *
 * output parameters
 *
 * @param       csPV.
 *
 * @return      None
 */
uint8 llCsSecGeneratePV(uint8* csPV);

/*******************************************************************************
 * @fn          llCsSecGenRandomBits
 *
 * @brief       Generates Random bits using the CS DRBG
 * based on the transactionId this function shall set the generated random
 * bits in the CS DB.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       transactionId  - CS Transaction ID.
 * @param       rndBits - random bits pointer
 *
 * output parameters
 *
 * @param       rndBits.
 *
 * @return      None
 */
uint8 llCsSecGenRandomBits(uint8 transactionId, uint8* rndBits);

/*******************************************************************************
 * @fn          llCsProcedureInitDrbg
 *
 * @brief       Initialize the CS DRBG
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId   - connId
 * @param       configId - configId
 *
 * output parameters
 *
 * @param       rndBits.
 *
 * @return      None
 */
uint8 llCsProcedureInitDrbg(uint16 connId, uint8 configId);

/*******************************************************************************
 * @fn          llCsSecIncreaseStepCount
 *
 * @brief       Increase the step counter
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       rndBits.
 *
 * @return      None
 */
void llCsSecIncreaseStepCount(void);

/*******************************************************************************
 * @fn          llCsSecGetStepCount
 *
 * @brief       Get DRBG param step count
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      DRBG num steps
 */
uint16 llCsSecGetStepCount(void);

/*******************************************************************************
 * @fn          llCsSecResetStepCount
 *
 * @brief       Reset step counter
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
void llCsSecResetStepCount(void);

/*******************************************************************************
 * @fn          llCsSecIncProcCounter
 *
 * @brief       Increment CS procedure counter
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
void llCsSecIncProcCounter(void);

/*******************************************************************************
 * @fn          hr1
 *
 * @brief       Channel Sounding random number generation function.
 * The CS random number generation function hr1 as defined below is
 * used to generate random numbers within an arbitrary range.
 *
 * @design      BLE_LOKI-506
 * input parameters
 *
 * @param       r - arbitrary range 0 to R-1 from which a random number is to be
 *              generated
 * @param       tId - transactionID
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Rout - random number
 */
uint8 hr1(uint8 r, csTransactionId_e tId);

/*******************************************************************************
 * @fn          csDrbg
 *
 * @brief       Random Bit Generation Function
 * Returns random bits depending on the number of required bits
 * from the DRBG outout vector of the specific transaction ID.
 * In CS SPEC see Random bit generation function CS_DRBG
 * And Channel Sounding random bit generation.
 *              CS_DRBG( number of required bits ) ⊆ randomBits[0:127]
 *
 * @design      BLE_LOKI-506
 * input parameters
 *
 * @param       randomBitsRequired - num of required bits
 * @param       pRndBits - random bits result
 * @param       transactionId - the CS transaction Id
 *
 * output parameters
 *
 * @param       pRndBits.
 *
 * @return      Rout - random number
 */
uint32 csDrbg(uint8 randomBitsRequired, uint8* pRndBits,
              csTransactionId_e transactionId);

/*******************************************************************************
 * @fn          cr1
 *
 * @brief       Shuffling function cr1
 * When deriving the channel index selection within a CS procedure,
 * it is needed to shuffle the list of available channels.
 * The channel index shuffling function cr1 is defined for this
 * purpose.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       pChannelArray - output array
 * @param       filterdArr    - input array
 * @param       nChannels     - size of array (number of channels)
 * @param       trId          - transaction Id
 *
 * output parameters
 *
 * @param       pChannelArray - result of shuffling filteredArr
 *
 * @return      None.
 */
void cr1(uint8* pChannelArray, uint8* filterdArr, uint8 nChannels,
         csTransactionId_e trId);
