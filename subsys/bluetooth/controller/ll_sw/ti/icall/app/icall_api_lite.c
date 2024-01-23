/******************************************************************************

 @file  icall_api_lite.c

 @brief lite version of the icall dispatch mechanism.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2016 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifdef ICALL_LITE

/*******************************************************************************
 * INCLUDES
 */
#include "gatt.h"
#include "ble_dispatch.h"
#include <ti/drivers/cryptoutils/cryptokey/CryptoKeyPlaintext.h>
#ifndef USE_RCL
#include <ti/drivers/rf/RF.h>
#endif
#ifndef CC23X0
#include "trng_api.h"
#endif
#include "ll_common.h"
#include "ll_enc.h"
#include "rom_jt.h"

/*********************************************************************
 * GLOBAL VARIABLES
 */
// BM allocator and de-allocator functions
pfnBMAlloc_t pfnBMAlloc = NULL;
pfnBMFree_t  pfnBMFree  = NULL;

#ifndef USE_RCL // no error callback in rcl open
// RF Driver error callback
void rfErrCb(RF_Handle h, RF_CmdHandle ch, RF_EventMask e)
{
  HAL_ASSERT( HAL_ASSERT_CAUSE_RF_DRIVER_ERROR );
}

RF_Callback pRfErrCb = &rfErrCb;
#endif
uint32 lastAppOpcodeIdxSent = 0xFFFFFFFF;

#ifndef STACK_LIBRARY
/* This variable exit on teh stack side, and it is always equal to the maximum
 number of possible connection. It is also used in a file share between
application and stack. */
uint8 linkDBNumConns = MAX_NUM_BLE_CONNS;

/*********************************************************************
 * @fn      GATT_bm_alloc
 *
 * @brief   GATT implementation of the allocator functionality.
 *
 *          Note: This function should only be called by GATT and
 *                the upper layer protocol/application.
 *
 * @param   connHandle - connection that message is to be sent on.
 * @param   opcode - opcode of message that buffer to be allocated for.
 * @param   size - number of bytes to allocate from the heap.
 * @param   pSizeAlloc - number of bytes allocated for the caller from the heap.
 *
 * @return  pointer to the heap allocation; NULL if error or failure.
 */
void *GATT_bm_alloc(uint16 connHandle, uint8 opcode, uint16 size, uint16 *pSizeAlloc)
{
  if (pfnBMAlloc != NULL)
  {
    return (*pfnBMAlloc)(BM_MSG_GATT, size, connHandle, opcode, pSizeAlloc);
  }

  return ((void *)NULL);
}

/*********************************************************************
 * @fn      GATT_bm_free
 *
 * @brief   GATT implementation of the de-allocator functionality.
 *
 * @param   pMsg - pointer to the message containing the memory to free.
 * @param   opcode - opcode of the message.
 *
 * @return  none
 */
void GATT_bm_free(gattMsg_t *pMsg, uint8 opcode)
{
  if (pfnBMFree != NULL)
  {
    (*pfnBMFree)(BM_MSG_GATT, pMsg, opcode);
  }
}

/*********************************************************************
 * @fn      L2CAP_bm_alloc
 *
 * @brief   L2CAP implementation of the allocator functionality.
 *
 *          Note: This function should only be called by L2CAP and
 *                the upper layer protocol/application.
 *
 * @param   size - number of bytes to allocate from the heap.
 *
 * @return  pointer to the heap allocation; NULL if error or failure.
 */
void *L2CAP_bm_alloc( uint16 size )
{
  if (pfnBMAlloc != NULL)
  {
    return (*pfnBMAlloc)(BM_MSG_L2CAP, size, 0, 0, NULL);
  }

  return ((void *)NULL);
}
#endif /* !STACK_LIBRARY */

/*********************************************************************
 * @fn      BM_free
 *
 * @brief   Implementation of the de-allocator functionality.
 *
 * @param   payload_ptr - pointer to the memory to free.
 *
 * @return  none
 */
void BM_free(void *payload_ptr)
{
  if (pfnBMFree != NULL)
  {
    (*pfnBMFree)(BM_MSG_GENERIC, payload_ptr, 0);
  }
}

/*******************************************************************************
 * @fn          Util_GetTRNG
 *
 * @brief       This routine returns a 32 bit TRNG number.
 *
 * @param       None.
 *
 * @return      A 32 bit TRNG number.
 */
uint32_t Util_GetTRNG(void)
{
  uint32    trngVal;

  MAP_LL_ENC_GenerateTrueRandNum((uint8 *)(&trngVal), 4);

  return trngVal;
}

#endif /* ICALL_LITE */

/*********************************************************************
*********************************************************************/
