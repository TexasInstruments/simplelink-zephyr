/******************************************************************************

 @file  ll_cs_ctrl_pkt_mgr.h

 @brief The LL CS Ctrl Packet manager contains the APIs that build CS control
        Packets, send CS control pakcets, handles received CS control pakcets.
        Manages the CS procedure that is started by the contol packets
        Uses the CS DB to read/write CS data.
        Notifies the host when a CS procedure was completed.

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
#include "ll_common.h"
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
 * @fn          llCsProcessCsControlPacket
 *
 * @brief       This API is used to handle the reception of a CS packet.
 * Once a Rx interrupt occurs, the BLE module is called and in case it's a
 * CS Control packet that was received, llCsProcessCsControlPacket
 * is called to parse and handle the received packet. The size of
 * pBuf depends on ctrlType.
 *
 * The size of the received packet is checked in the ISR function
 * prior to this function, therefore there is no need to check
 * it again. We assume that this function is called only when
 * ctrlType belongs to a CS control packet.
 *
 * This function also builds the response packet for the control
 * packets that require a response
 *
 * @design      BLE_LOKI-506
 *  parameters
 *
 * @param       ctrlType - Type of the received CS control packet
 * @param       connPtr - Pointer to connection
 * @param       pBuf  - Pointer to the data in the control packet
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Status
 *              CS_STATUS_INVALID_CONN_PTR connPtr is NULL
 *              CS_STATUS_INVALID_BUFFER pBuf is NULL
 *              CS_STATUS_UNSUPPORTED_FEATURE CS is not supported
 *              CS_STATUS_INSUFFICIENT_MEMORY insufficient memory to setup ctrl
 *              CS_STATUS_SUCCESS - if packet was processed successfully
 * @note it is assumed that this API is used only when ctrlType is a CS ctrl
 * opcode.
 */
csStatus_e llCsProcessCsControlPacket(uint8 ctrlType, llConnState_t* connPtr,
                                    uint8* pBuf);

/*******************************************************************************
 * @fn          llCsProcessCsCtrlProcedures
 *
 * @brief       Process CS Control Procedure Depending on type.
 * This processes the packets to be transmitted.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connPtr - connection information pointer
 * @param       ctrlPkt - control packet opcode
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Status
 *              CS_STATUS_INVALID_CONN_PTR - if connPts is NULL
 *              CS_STATUS_CONNECTION_TERMINATED - if connection got terminated
 *              CS_STATUS_SUCCESS - if packet was processed successfully
 * @note it is assumed that this API is used only when ctrlType is a CS ctrl
 * opcode.
 */
uint8 llCsProcessCsCtrlProcedures(llConnState_t* connPtr, uint8 ctrlPkt);
