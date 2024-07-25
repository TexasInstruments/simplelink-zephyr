/******************************************************************************

 @file ll_csdrbg.c

 @brief

 ******************************************************************************
 $License: TISD 2009 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*******************************************************************************
 * INCLUDES
 */

#include "bcomdef.h"
#include <stddef.h>

#include "cs/ll_cs_logs.h"
#include "hci_data.h"
#include "ll_common.h"
#include "ll_csdrbg.h"
#include "ll_enc.h"
#include "rom_jt.h"

/*******************************************************************************
 * CONSTANTS
 */
#define CS_INVALID_AA (0x8E89BED6)
#define CS_S0_START_POSITION (0)
#define CS_S1_START_POSITION (1)
#define CS_S2_START_POSITION (2)
#define CS_S3_START_POSITION (3)
#define AESCTRDRBG_AES_BLOCK_SIZE_BYTES 16
#define uint64 long long unsigned int
#define UINT8_SIZE_64 8
#define UINT8_SIZE_128 16
#define UINT8_SIZE_256 32
#define UINT_SIZE_INPUT_F8 80
#define UINT_SIZE_INPUT_BIT_STRING 40
#define inputString_F8_OFFSET 24
#define OFFSET_192_BITS 24
#define MAX_CONNECTIONS 4
#define CS_STEP_COUNTER_SIZE 16
#define CS_TRANSACTION_ID_SIZE 8
#define CS_TRANSACTION_COUNTER_COUNTER 8
#define CS_PROCEDURE_COUNTER_SIZE 8
#define F7_BLOCK_NUM 5
#define MIN_TRAN_ID 0x00
#define MAX_TRAN_ID 0x09
#define BACKTRACKING_TRAN_ID 0x09

/*******************************************************************************
 * TYPEDEFS
 */
//typedef __size_t size_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * LOCAL FUNCTIONS
 */

/*******************************************************************************
 * GLOBAL VARIABLES this is a tessddt
 */

/****************************************************************************/
/****************************************************************************
 * @fn addBigendianCounter
 *
 * @brief the function sums to big numbers
 *
 * input parameters
 * @param counter - pointer to 128 bits number.
 * @param increment - number to add (size varies)
 * @param size - size of increment
 *
 * output parameters
 * @param counter - the sum of the numbers.
 *
 */
void CSDRBG_addBigendianCounter(uint8* counter, uint8* increment, uint8 size)
{
    /* first, make sure the pointers are valid */
    if (counter && increment)
    {
        /* loop over the each byte */
        for (uint8 i = 0; i < size; i++)
        {
            /* save the current byte value */
            uint8 tmp = counter[i];
            /* increment the curr byte */
            counter[i] += increment[i];
            /* check if there was a wraparound */
            if (counter[i] < tmp)
            {
                /* add carry to next byte */
                counter[i + 1]++;
            }
        }
    }
}

/****************************************************************************
 * @fn security function e()
 *
 * @brief the function encrypts the plain data using a key value.
 *
 * input parameters
 * @param key - pointer to 128 bits number key.
 *
 * @param plainText - pointer to 128 bits number data to be encrypted
 *
 * output parameters
 * @param cypherText - pointer to 128 bits encrypted data
 */
llStatus_t CSDRBG_security_function_e(uint8* key, uint8* plainText,
                                      uint8* cypherText)
{
    return LL_Encrypt(key, plainText, cypherText);
}

/****************************************************************************
 * @fn f7() DRBG chain function
 *
 * @brief this function will genetarte an output that is 128 bits length of a
 * key and a input string that has a length of multipale of 128 bits using a
 * cipher block chaining technique
 *
 * input parameters
 * @param key - pointer to 128 bit key (uint8 key[16])
 * @param inputString - pointer to a multiple of 128 bit string (uint8
 * inputString[16*len])
 * @param len - muliple of 128 len of inputString in octets
 * @param encryptedData - pointer to 128 bit string in which the encrtpted data
 * will be placed.
 *
 * output parameters
 * @param encryptedData - pointer to 128 bit data
 *
 * @return  LL_STATUS_ERROR_BAD_PARAMETER if one of the pointers is null or
 * LL_STATUS_SUCCESS.
 */
llStatus_t f7(uint8* key, uint8* inputString, uint8 len, uint8* encryptedData)
{
    uint8 temp[UINT8_SIZE_128] = {0};

    // check parameters
    if ((key == NULL) || (inputString == NULL) || (encryptedData == NULL))
    {
        return (LL_STATUS_ERROR_BAD_PARAMETER);
    }

    for (int p = 0; p < len; p++) // go over the n blocks
    {
        for (int i = 0; i < UINT8_SIZE_128; i++) // XOR each 128 bits
        {
            temp[i] = inputString[i + p * UINT8_SIZE_128] ^ temp[i];
        }

        CSDRBG_security_function_e(
            key, temp, encryptedData); // encrypt each xored block with the key

        MAP_osal_memcpy(temp, encryptedData, UINT8_SIZE_128);
    }
    return (LL_STATUS_SUCCESS);
}

/****************************************************************************

 * @fn f8() DRBG derivation function
 *
 * @brief this function will genetarte DRBG seed matirial that is 256 bits in
 length.
 * Both f7 and security function e() are invoked by f8.
 *
 * input parameters
 * @param input_bit_string - pointer to 320 bit string
 *
 * @param SM - 256 bits empty string in which we will genetarte DRBG seed
 matirial (uint8 SM [32] = {0})
 *
 * output parameters
 * @param SM - pointer to 256 bit data
 *
 * @return LL_STATUS_ERROR_BAD_PARAMETER if one of the pointers is null or
 LL_STATUS_SUCCESS.
 */
llStatus_t f8(uint8* input_bit_string, uint8* SM)
{
    uint8 f7_status;
    uint8 secuirty_e_status;
    // init 2 input strings as described in the spec. (has the const part as
    // implementted below concat with the variable input_bit_string - this part
    // is initialied to 0x00)
    uint8 input1[UINT_SIZE_INPUT_F8] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x20,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    uint8 input2[UINT_SIZE_INPUT_F8] = {
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x20,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    uint8 k[UINT8_SIZE_128] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                               0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    uint8 k2[UINT8_SIZE_128];
    uint8 X[UINT8_SIZE_128];
    uint8 encr[UINT8_SIZE_128] = {0};
    uint8 encr2[UINT8_SIZE_128] = {0};

    // check parameters.
    if ((input_bit_string == NULL) || (SM == NULL))
    {
        return (LL_STATUS_ERROR_BAD_PARAMETER);
    }

    // concatenate the 2 inputs to f7 withe tha input_bit_string as described in
    // the spec.
    MAP_osal_memcpy(input1 + inputString_F8_OFFSET, input_bit_string,
                    UINT_SIZE_INPUT_BIT_STRING);
    MAP_osal_memcpy(input2 + inputString_F8_OFFSET, input_bit_string,
                    UINT_SIZE_INPUT_BIT_STRING);

    // F7 function with input1 and k the output is k2
    f7_status = f7(k, input1, F7_BLOCK_NUM, k2);
    if (f7_status != LL_STATUS_SUCCESS)
    {
        return f7_status;
    }

    // F7 function with input2 and k the output is x
    f7(k, input2, F7_BLOCK_NUM, X);
    if (f7_status != LL_STATUS_SUCCESS)
    {
        return f7_status;
    }

    // encryption of X with k2 as a key - output encr
    secuirty_e_status = CSDRBG_security_function_e(k2, X, encr);
    if (secuirty_e_status != LL_STATUS_SUCCESS)
    {
        return secuirty_e_status;
    }

    // encryption of encr with k2 as a key - output encr2
    secuirty_e_status = CSDRBG_security_function_e(k2, encr, encr2);
    if (secuirty_e_status != LL_STATUS_SUCCESS)
    {
        return secuirty_e_status;
    }

    // conct encr and encr2 to genarate SM
    MAP_osal_memcpy(SM, encr, UINT8_SIZE_128);
    MAP_osal_memcpy(SM + UINT8_SIZE_128, encr2, UINT8_SIZE_128);
    return (LL_STATUS_SUCCESS);
}

/****************************************************************************
 * @fn f9() DRBG update function
 *
 * @brief DRBG update function is used to update and reefresh a current DRBG
 * 128-bit temporal key kDrbg and 128-bit nonce vector vDrbg using a 256-bit
 * seed material SM that may carry fresh entropy.
 *
 * input parameters
 * @param SM 256 bits string that is genetarted by f8()
 *
 * @param params - a pointer to drbgParams_t for updating params->kDrbg,
 * params->vDrbg
 *
 * output parameters
 *
 * changes the input K V
 *
 * @param params - with kDrbg, vDrbg updated
 *
 * @return LL_STATUS_ERROR_BAD_PARAMETER if one of the pointers is null or
 * LL_STATUS_SUCCESS.
 */
llStatus_t f9(uint8* SM, drbgParams_t* params)
{
    uint8 v1[UINT8_SIZE_128];
    uint8 v2[UINT8_SIZE_128];
    uint8 X[UINT8_SIZE_256];
    uint8 x1[UINT8_SIZE_128];
    uint8 x2[UINT8_SIZE_128];
    uint8 one = 1;

    // check parameters
    if ((SM == NULL) || (params == NULL))
    {
        return (LL_STATUS_ERROR_BAD_PARAMETER);
    }

    // reverse v in order to add as bid endian V1 = V+1, V2 = V+2 as descried in
    // spec.
    MAP_HCI_ReverseBytes(params->vDrbg, UINT8_SIZE_128);
    // v1 = v+1
    MAP_osal_memcpy(v1, params->vDrbg, UINT8_SIZE_128);
    CSDRBG_addBigendianCounter(v1, &one, sizeof(uint8));
    // v2 = v+2
    MAP_osal_memcpy(v2, v1, UINT8_SIZE_128);
    CSDRBG_addBigendianCounter(v2, &one, sizeof(uint8));

    MAP_HCI_ReverseBytes(v1, UINT8_SIZE_128);
    // security function x1 = e(k,v1)
    CSDRBG_security_function_e(params->kDrbg, v1, x1);
    // reverse v1 back to little endian
    MAP_HCI_ReverseBytes(v2, UINT8_SIZE_128);
    // security function x2 = e(k,v2)
    CSDRBG_security_function_e(params->kDrbg, v2, x2);

    // concat X = x1 || x2
    MAP_osal_memcpy(X, x1, UINT8_SIZE_128);
    MAP_osal_memcpy(X + UINT8_SIZE_128, x2, UINT8_SIZE_128);

    // X = X ^ SM
    for (int i = 0; i < UINT8_SIZE_256; i++)
    {
        X[i] = X[i] ^ SM[i];
    }
    // Kout = leftmost( X ) 128 bits
    MAP_osal_memcpy(params->kDrbg, X, UINT8_SIZE_128);
    // Vout = rightmost( X ) 128 bits
    MAP_osal_memcpy(params->vDrbg, X + UINT8_SIZE_128, UINT8_SIZE_128);
    return (LL_STATUS_SUCCESS);
}

/****************************************************************************
 * @fn DRBG instantiation function h9()
 *
 * @brief The DRBG instantiation function h9 is used to instantiate the DRBG
 * temporal key KDRBG and a nonce vector vDrbg for the first time. The inputs to
 * the h9 are the 128-bit CS_IV, 64-bit CS_IN, and 128-bit CS_PV values that are
 * the result of the CS Security Start procedure.
 *
 * input parameters
 * @param CS_IV 128 bits result of the CS Security Start procedure.
 *
 * @param CS_IN 64 bits result of the CS Security Start procedure.
 *
 * @param CS_PV 128 bits result of the CS Security Start procedure.
 *
 * output parameters
 * @param kDrbg 128 bits
 *
 * @param vDrbg 128 bits
 *
 * @return status - LL_STATUS_ERROR_BAD_PARAMETER or LL_STATUS_SUCCESS.
 */
llStatus_t h9(uint8* CS_IV, uint8* CS_IN, uint8* CS_PV, drbgParams_t* params)
{
    uint8 f8_status;
    uint8 f9_status;
    uint8 input_bit_string[UINT_SIZE_INPUT_BIT_STRING];
    uint8 SM[UINT8_SIZE_256] = {0};

    // check parameters
    if ((CS_IV == NULL) || (CS_IN == NULL) || (params == NULL))
    {
        return (LL_STATUS_ERROR_BAD_PARAMETER);
    }

    // concatenate CS_IV, CS_IN, CS_PV to create input_bit_string
    MAP_osal_memcpy(input_bit_string, CS_IV, UINT8_SIZE_128);
    MAP_osal_memcpy(input_bit_string + UINT8_SIZE_128, CS_IN, UINT8_SIZE_64);
    MAP_osal_memcpy(input_bit_string + OFFSET_192_BITS, CS_PV, UINT8_SIZE_128);

    // f8 is used to generatre SM from input_bit_string
    f8_status = f8(input_bit_string, SM);
    if (f8_status != LL_STATUS_SUCCESS)
    {
        return f8_status;
    }

    // f9 uses SM and K=0, V=0 TO create the first K_DRBG, V_DRBG
    f9_status = f9(SM, params);
    if (f9_status != LL_STATUS_SUCCESS)
    {
        return f9_status;
    }
    return (LL_STATUS_SUCCESS);
}

/****************************************************************************
 * @fn csGetConuter
 *
 * @brief help function creates: counter = CS Step Counter (16 bits) ||
 * Transaction ID (8 bits) || Transaction Counter (8 bits)
 *
 * input parameters
 * @param params (see typedef)
 *
 *  output parameters
 * @return counter
 */
uint64 csGetConuter(drbgParams_t* params)
{
    uint64 counter = 0;
    counter = counter | params->CSStepCounter;
    counter = counter << CS_STEP_COUNTER_SIZE / 2;
    counter = counter | params->TransactionID;
    counter = counter << CS_TRANSACTION_ID_SIZE;
    counter = counter | params->TransactionCounter;
    return counter;
}

/****************************************************************************
 * @fn CSDRBG_generate
 *
 * @brief The CSDRBG_generate function is used to generate 128 random bits
 * prandomBits[0:127] = CSDRBG_security_function_e( KDRBG, vDrbg + counters )
 *
 * counters:
 * CS Step_Counter = (vDrbg[31:16] + CSStepCount)
 * CS Transaction_Identifier = (vDrbg[15:8] + CSTransactionID)
 * CS Transaction_Counter = (vDrbg[7:0] + CSTransactionCounter)
 *
 * input parameters
 * @param params (see typedef)
 *
 * @param prandomBits - pointer to the 128 bits block empty.
 *
 *  output parameters
 * @param prandomBits - pointer to the 128 random bits block full.
 *
 * @return LL_STATUS_ERROR_BAD_PARAMETER if one of the pointers is null or
 * LL_STATUS_SUCCESS.
 */
llStatus_t CSDRBG_generate(drbgParams_t* params, uint8* prandomBits)
{
    uint8 securityFunc_e_status;
    uint64 count = 0;
    uint8 V[UINT8_SIZE_128] = {0};

    // check parameters
    if ((params == NULL) || (prandomBits == NULL))
    {
        return (LL_STATUS_ERROR_BAD_PARAMETER);
    }

    // count = CS Step Counter (16 bits) || Transaction ID (8 bits) ||
    // Transaction Counter (8 bits)
    count = csGetConuter(params);
    MAP_osal_memcpy(V, params->vDrbg, UINT8_SIZE_128);

    // V = vDrbg + counter (big endian)
    MAP_HCI_ReverseBytes(V, UINT8_SIZE_128);
    CSDRBG_addBigendianCounter(V, (uint8*)&count, sizeof(uint64));
    MAP_HCI_ReverseBytes(V, UINT8_SIZE_128);

    // prandomBits = CSDRBG_security_function_e( KDRBG, vDrbg + counters )
    securityFunc_e_status =
        CSDRBG_security_function_e(params->kDrbg, V, prandomBits);
    if (securityFunc_e_status != LL_STATUS_SUCCESS)
    {
        return securityFunc_e_status;
    }

    return (LL_STATUS_SUCCESS);
}

/****************************************************************************
 * @fn LL_CSDRBG_Init
 *
 * @brief the function initalize the first kDrbg, vDrbg from CS_IV, CS_IN,
 * CS_PV.
 *
 * input parameters
 * @param CS_IV 128 bits result of the CS Security Start procedure.
 *
 * @param CS_IN 64 bits result of the CS Security Start procedure.
 *
 * @param CS_PV 128 bits result of the CS Security Start procedure.
 *
 * @param params - pointer to drbgParams_t. see typedef
 *
 * output parameters
 * @param params - params->kDrbg, params->vDrbg are initialized.
 *
 * @return LL_STATUS_ERROR_BAD_PARAMETER if one of the pointers is null or
 * LL_STATUS_SUCCESS.
 */
llStatus_t LL_CSDRBG_Init(uint8* CS_IV, uint8* CS_IN, uint8* CS_PV,
                          drbgParams_t* params)
{
    // check parameters
    if ((CS_IV == NULL) || (CS_IN == NULL) || (CS_PV == NULL))
    {
        return (LL_STATUS_ERROR_BAD_PARAMETER);
    }

    // csProcedureCounter should be 0 as this function is only for init K, V in
    // the first proc.
    params->csProcedureCounter = 0;

    // initializing K = 0 , V = 0
    MAP_osal_memset(params->kDrbg, 0, UINT8_SIZE_128);
    MAP_osal_memset(params->vDrbg, 0, UINT8_SIZE_128);

    // h9 is a function that generate the first K_DRBG, V_DRBG from CS_IV,
    // CS_IN, CS_PV
    h9(CS_IV, CS_IN, CS_PV, params);

    llCsLogDrbgInitResult(CS_IV, CS_IN, CS_PV, params);

    return (LL_STATUS_SUCCESS);
}

/****************************************************************************
 * @fn LL_CSDRBG_GetDrbg
 *
 * @brief the function create a new DRBG
 *
 *  input parameters
 *
 * @param prandomBits - pointer to the 128 bits block.
 *
 * @param params - pointer to drbgParams_t. see typedef
 *
 * output parameters:
 *
 * @param prandomBits 128 bits of random numbers
 *
 * @return LL_STATUS_ERROR_BAD_PARAMETER if one of the pointers is null or
 * LL_STATUS_SUCCESS.
 */
llStatus_t LL_CSDRBG_GetDrbg(uint8* prandomBits, drbgParams_t* params)
{

    // check parameters
    if ((prandomBits == NULL) || (params == NULL) ||
        (params->TransactionID > MAX_TRAN_ID))
    {
        return (LL_STATUS_ERROR_BAD_PARAMETER);
    }

    // generate 128 random bits
    CSDRBG_generate(params, prandomBits);

    llCsLogDrbgParamsAndResult(prandomBits, params);

    return (LL_STATUS_SUCCESS);
}

/****************************************************************************
 * @fn LL_CSDRBG_BackTracking
 *
 * @brief Backtracking resistance shall be invoked every time the CSProcCount is
 * incremented. the backtracting uses f9() to update kDrbg, vDrbg by taking SM =
 * {0}, kDrbg and vDrbg[15:8] + TransactionID and cs proc. counter increses
 * by 1.
 *
 * input parameters
 * @param params - pointer to drbgParams_t. see typedef
 *
 * output parameters
 * @param params - params->kDrbg, params->vDrbg are updated. cs proc. counter
 * + 1.
 *
 * @return
 */
llStatus_t LL_CSDRBG_BackTracking(drbgParams_t* params)
{
    uint8 SM[UINT8_SIZE_256] = {0};
    uint8 f9_status;

    if ((params == NULL) || (params->TransactionID != BACKTRACKING_TRAN_ID))
    {
        return (LL_STATUS_ERROR_BAD_PARAMETER);
    }

    // backtracking, update kDrbg, vDrbg.

    uint32 tranId = params->TransactionID;
    tranId = tranId << CS_TRANSACTION_ID_SIZE;

    // reverse is needed because the Vdrbg is defined as big-endian and not
    // little as all other variables and our stack
    MAP_HCI_ReverseBytes(params->vDrbg, UINT8_SIZE_128);
    // vDrbg[15:8] + TransactionID
    CSDRBG_addBigendianCounter(params->vDrbg, (uint8*)&tranId, sizeof(uint32));
    MAP_HCI_ReverseBytes(params->vDrbg, UINT8_SIZE_128);

    f9_status = f9(SM, params);
    if (f9_status != LL_STATUS_SUCCESS)
    {
        return f9_status;
    }
    // The CS_Step, CS Transaction_Counter fields shall be reset to zero
    params->TransactionCounter = 0;
    params->CSStepCounter = 0;
    // csProcedureCounter increses by 1
    params->csProcedureCounter += 1;

    return (LL_STATUS_SUCCESS);
}
