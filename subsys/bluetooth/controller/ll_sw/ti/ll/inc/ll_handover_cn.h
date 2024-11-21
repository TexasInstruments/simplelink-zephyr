/******************************************************************************

 @file  ll_handover_cn.h

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

#ifndef LL_HANDOVER_CN_H
#define LL_HANDOVER_CN_H

/*******************************************************************************
 * INCLUDES
 */
#include "hal_types.h"
#include "ll_common.h"
#include "ll_handover.h"

/*******************************************************************************
 * MACROS
 */
#define LL_HANDOVER_EARLIER_ST RAT_TICKS_IN_3MS // Guard time of 3ms

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * TYPEDEFS
 */

/*
* @brief LL Handover Start Candidate Callback
*
* @note
* This callback will trigger with a success status if the candidate was able to
* follow the new connection. Otherwise, a failure status will be returned
*
* @param status - the controller status (SUCCESS, FAILURE)
*
* @return None
*/
typedef void(*pfnLLHandoverStartCNCB_t)(uint16_t connHandle, uint32_t status);

/**
 * @brief LL Handover CN callbacks
 *
 */
typedef struct
{
  pfnLLHandoverStartCNCB_t pfnHandoverStartCNCB;
} llHandoverCNCBs_t;


/**
 * @brief LL Handover candidate parameter structure
 */
typedef struct
{
  handoverDataFull_t *pHandoverData;   //!< The handover data given by the serving node
  uint8  appTaskId;                    //!< Application task ID
  uint32 timeDelta;                    //!< The time it took for the data to be transferred from the serving node to the candidate node
  uint32 timeDeltaMaxErr;              //!< The maximum deviation time
  uint16 connHandle;                   //!< Connection handle given by the controller
  uint32 maxFailedConnEvents;          //!< Number of connection events the candidate node will try to follow the central. 0 - supervision timeout
} llHandoverCNParams_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * @fn          LL_Handover_RegisterCNCb
 *
 * @brief       Register to the candidate node callbacks
 *
 * @Design:      BLE_LOKI-1466
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
uint8 LL_Handover_RegisterCNCb( const llHandoverCNCBs_t *pCBs );

/*******************************************************************************
 * @fn          LL_Handover_StartCN
 *
 * @brief       This API check if it valid to start the handover process
 *              in the LL layer.
 *              If the controller can handle the new connection, all the
 *              advertising sets will be closed, creates a new connection
 *              with the new given data.
 *
 * @Design:     BLE_LOKI-1466
 *
 * input parameters
 *
 * @param   pParams - Pointer to the candidate node parameters
 *
 * output parameters
 *
 * @param       None
 *
 * @return      LL_STATUS_SUCCESS, LL_STATUS_ERROR_CONNECTION_LIMIT_EXCEEDED
 *              LL_STATUS_ERROR_COMMAND_DISALLOWED, LL_STATUS_ERROR_BAD_PARAMETER
 */
uint8 LL_Handover_StartCN( llHandoverCNParams_t *pParams );

/*******************************************************************************
 * @fn          llHandoverNotifyConnStatus
 *
 * @brief       This function is used to notify the upper layers with the handover
 *              status on the candidate node side
 *
 * input parameters
 *
 * @param       connHandle - Connection handle
 * @param       handoverStatus - Handover status
 *
 * output parameters
 *
 * @param       None
 *
 * @return      LL_STATUS_SUCCESS, LL_STATUS_ERROR_UNEXPECTED_PARAMETER
 */
uint8 llHandoverNotifyConnStatus(uint16_t connHandle, uint32_t handoverStatus);
#endif /* LL_HANDOVER_CN_H */
