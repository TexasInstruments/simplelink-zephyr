/******************************************************************************

 @file  ll_handover_sn.h

 @brief This file contains the data structures and APIs for handling
        Bluetooth Low Energy handover process

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

#ifndef LL_HANDOVER_SN_H
#define LL_HANDOVER_SN_H

/*******************************************************************************
 * INCLUDES
 */
#include "hal_types.h"
#include "ll_common.h"
#include "ll_handover.h"

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * TYPEDEFS
 */

/*
* @brief LL Handover Start Callback
*
* @Design: BLE_LOKI-1458
*
* @note
* This callback will trigger with a success status once it will finish filling
* the handover data. Otherwise, this callback will be called with an error code
*
* @param status - the controller status (LL_STATUS_SUCCESS,
LL_STATUS_ERROR_BAD_PARAMETER, LL_STATUS_ERROR_INACTIVE_CONNECTION)
*
* @return None
*/
typedef void(*pfnLLHandoverStartSNCB_t)(uint16 connHandle, uint32_t status);

/**
 *
 * @brief LL Handover SN callbacks
 *
 * @Design: BLE_LOKI-1458
 */
typedef struct
{
  pfnLLHandoverStartSNCB_t pfnHandoverStartSNCB;
} llHandoverSNCBs_t;


/**
 * @brief LL Handover SN parameters structure
 */
typedef struct
{
  uint16_t connHandle;          //!< Connection handle
  uint32_t handoverDataSize;    //!< The stack handover data size
  uint8_t *pHandoverData;       //!< Pointer to the buffer the application allocated
  uint32_t maxNumConnEvtTries;  //!< Number of connection events the candidate will try to follow before determine the handover failed or not, 0 - try until connection supervision timeout
  uint8_t  handoverSnMode;      //!< 0 - Terminate the connection immediately on the serving node side, 1 - wait for the candidate response
} llHandoverSNParams_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * FUNCTIONS
 */

/*
 * Serving Node functions
 */

/*******************************************************************************
 * @fn          LL_Handover_RegisterSNCb
 *
 * @brief       Register to the serving node callbacks
 *
 * @Design:     BLE_LOKI-1458
 *
 * input parameters
 *
 * @param       pCBs - pointer to the callback functions
 *
 * output parameters
 *
 * @param       None
 *
 * @return      LL_STATUS_SUCCESS, LL_STATUS_ERROR_BAD_PARAMETER
 */
uint8 LL_Handover_RegisterSNCb( const llHandoverSNCBs_t *pCBs );

/*******************************************************************************
 * @fn          LL_Handover_GetSNDataSize
 *
 * @brief       Return the LL handover data size
 *
 * @Design:     BLE_LOKI-1458
 *
 * input parameters
 *
 * @param       pParams - Pointer to the parameters structure
 *
 * output parameters
 *
 * @param       None
 *
 * @return      The controller handover data size
 */
uint32 LL_Handover_GetSNDataSize( llHandoverSNParams_t *pParams );

/*******************************************************************************
 * @fn          LL_Handover_StartSN
 *
 * @brief       Starts the Handover process on the serving node side at the LL.
 *              This function will save the upper layer data size and the connection
 *              handle on which the Handover should start
 *
 * @Design:     BLE_LOKI-1458
 *
 * input parameters
 *
 * @param       pParams - Pointer to the parameters structure
 *
 * output parameters
 *
 * @param       None
 *
 * @return      LL_STATUS_SUCCESS, LL_STATUS_ERROR_INACTIVE_CONNECTION,
 *              LL_STATUS_ERROR_COMMAND_DISALLOWED, LL_STATUS_ERROR_BAD_PARAMETER,
 *              LL_STATUS_ERROR_DUE_TO_LIMITED_RESOURCES
 */
uint8 LL_Handover_StartSN( llHandoverSNParams_t *pParams );

/*******************************************************************************
 * @fn          LL_Handover_CloseSN
 *
 * @brief       Close the Handover process on the serving node side at the LL.
 *              If the handover was successful this function will generate disconnect
 *              command complete event. Otherwise, it will re-enable the connection.
 *
 * @Design:     BLE_LOKI-1458
 *
 * input parameters
 *
 * @param       pParams        - Pointer to the serving node parameters
 * @param       handoverStatus - TRUE if the candidate was able to follow the
 *                               handover connection. Otherwise, FALSE
 *
 * @return      LL_STATUS_SUCCESS
 */
uint8 LL_Handover_CloseSN( llHandoverSNParams_t *pParams, uint8 handoverStatus );

/*******************************************************************************
 * @fn          llHandoverTriggerDataTransfer
 *
 * @brief       This function will check if the data copy can start. It will verify
 *              the TX queue is empty. If so, it will fill the data needed for the
 *              handover. At the end it will trigger the SN start CB to the upper
 *              layer with the controller status
 *
 * @Design:     BLE_LOKI-1458
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None
 *
 * @return      LL_STATUS_SUCCESS, LL_STATUS_ERROR_COMMAND_DISALLOWED,
 *              LL_STATUS_ERROR_INACTIVE_CONNECTION, LL_STATUS_ERROR_BAD_PARAMETER,
 *              FAILURE
 */
uint8_t llHandoverTriggerDataTransfer( void );

/*******************************************************************************
 * @fn          llHandoverPopulateSnData
 *
 * @brief       This function fills the buffer with the handover data
 *
 * @Design:     BLE_LOKI-1458
 *
 * input parameters
 *
 * @param       connPtr  - Connection pointer
 * @param       pContBuf - Pointer to the data buffer
 * @param       pLinkCmd - Pointer to the link command
 *
 * output parameters
 *
 * @param       pContBuf - Handover data
 *
 * @return      None
 */
void llHandoverPopulateSnData(llConnState_t *connPtr, handoverDataFull_t *pContBuf, RCL_CmdBle5Connection *pLinkCmd);

/*******************************************************************************
 * @fn          llIsHandoverInProgress
 *
 * @brief       This function return the handover progress bit for the serving node
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the connection
 *
 * @return      TRUE - Handover is in progress, otherwise, FALSE
 */
uint8 llIsHandoverInProgress( llConnState_t *connPtr );

/*******************************************************************************
 * @fn          llReturnNonHandoverConn
 *
 * @brief       In case there are two connections and one of the connections is
 *              in the middle of handover process it will return the connection
 *              handle of the other connection. If non of the connections are in
 *              middle of handover process it will return invalid connection handle
 *              to allow the connection selection process to continue as usual
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      Connection ID.
 */
uint16 llReturnNonHandoverConn( void );

/*******************************************************************************
 * @fn          llHandoverCheckTermConnAndTerm
 *
 * @brief       This function is used to check if the connection needed to be
 *              terminated. This function must be called after a command was
 *              finished and before the scheduler is called again
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llHandoverCheckTermConnAndTerm( void );
#endif /* LL_HANDOVER_SN_H */
