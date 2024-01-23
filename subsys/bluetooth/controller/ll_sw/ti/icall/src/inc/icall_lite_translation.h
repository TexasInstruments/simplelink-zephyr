/******************************************************************************

 @file  icall_liteTranslation.h

 @brief icall LITE transaltion form icall message to stack API call..

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2016 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifdef ICALL_LITE

/*********************************************************************
 * INCLUDES
 */

/*********************************************************************
 * @fn      icall_liteTranslationInit
 *
 * @brief   Initialize the jump table value.
 *
 * @param   JT - pointer to the jump table containing the stack API to call.
 *
 * @return  None
 */
void icall_liteTranslationInit(uint32_t *JT);

/*********************************************************************
 * @fn      icall_liteTranslation
 *
 * @brief   Translate the icall direct API Message to a stack API call.
 *
 * @param   JT - pointer to the jump table containing the stack API to call.
 *
 * @return  None
 */
void icall_liteTranslation(icall_directAPIMsg_t *pMsg);
#endif /* ICALL_LITE */

/*********************************************************************
*********************************************************************/
