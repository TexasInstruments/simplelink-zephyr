/******************************************************************************

 @file  hci_event.c

 @brief This file send HCI events for the controller. It implements all the
        LL event callback and HCI events that send to host as struct message.

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
#include "bcomdef.h"
#include "hci_event.h"
#include "hci_event_internal.h"
#include "ble.h"

#include "rom_jt.h"

extern uint8 hciPTMenabled;

/*
** HCI Events
*/

/*******************************************************************************
 * This function sends the Number of Completed Packets Event to the Host.
 *
 * Public function defined in hci_c_event.h.
 */
void HCI_NumOfCompletedPacketsEvent( uint8   numHandles,
                                     uint16 *handles,
                                     uint16 *numCompletedPkts )
{
  hciEvt_NumCompletedPkt_t *pkt =
    (hciEvt_NumCompletedPkt_t *)MAP_osal_msg_allocate( sizeof(hciEvt_NumCompletedPkt_t) +
                           (numHandles * 2 * sizeof(uint16)) );
  if ( pkt )
  {
    pkt->hdr.event = HCI_DATA_EVENT; // packet type
    pkt->hdr.status = HCI_NUM_OF_COMPLETED_PACKETS_EVENT_CODE; // event code
    pkt->numHandles = numHandles;
    pkt->pConnectionHandle = (uint16 *)(pkt+1);
    pkt->pNumCompletedPackets = (uint16 *)( (uint8 *)(pkt+1) + ( numHandles * sizeof(uint16) ) );

    // for each handle, there's a handle number and a number of
    // completed packets for that handle
    for ( uint8 i = 0; i < numHandles; i++ )
    {
      pkt->pConnectionHandle[i] = handles[i];
      pkt->pNumCompletedPackets[i] = numCompletedPkts[i];
    }

    (void)MAP_osal_msg_send( hciL2capTaskID, (uint8 *)pkt );

   }
}

/*******************************************************************************
 * This function sends a Command Complete Event to the Host.
 *
 * Public function defined in hci_c_event.h.
 */
void HCI_CommandCompleteEvent( uint16 opcode,
                               uint8  numParam,
                               uint8  *param )
{
  // check if this is for the Host
  if ( ((hciGapTaskID != 0) || (hciSmpTaskID != 0)) && (hciPTMenabled == FALSE) )
  {
    hciEvt_CmdComplete_t *pkt =
      (hciEvt_CmdComplete_t *)MAP_osal_msg_allocate( sizeof(hciEvt_CmdComplete_t) +
                                                 numParam );

    if ( pkt )
    {
      uint8 taskID;

      if ( (opcode == HCI_LE_RAND || opcode == HCI_LE_ENCRYPT) && (hciSmpTaskID) )
      {
        taskID         = hciSmpTaskID;
        pkt->hdr.event = HCI_SMP_EVENT_EVENT;
      }
      else
      {
        taskID         = hciGapTaskID;
        pkt->hdr.event = HCI_GAP_EVENT_EVENT;
      }
      pkt->hdr.status   = HCI_COMMAND_COMPLETE_EVENT_CODE;
      pkt->numHciCmdPkt = 1;
      pkt->cmdOpcode    = opcode;
      pkt->pReturnParam = (uint8 *)(pkt+1);

      (void)MAP_osal_memcpy( pkt->pReturnParam, param, numParam );

      (void)MAP_osal_msg_send( taskID, (uint8 *)pkt );
    }
    else
    {
      MAP_HCI_SendCommandCompleteEvent( HCI_COMMAND_COMPLETE_EVENT_CODE,
                                        opcode,
                                        numParam,
                                        param );
    }
  }
}

/*******************************************************************************
 * @fn          LL_EXT_ChanMapUpdateCback Callback
 *
 * @brief       This LL callback is used to generate a vendor specific channel map
 *              update event
 *
 * input parameters
 *
 * @param       connHandle - connection for which channel map was updated
 * @param       newChanMap - new channel map
 * @param       nextDataChan - the next channel we will be using
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_EXT_ChanMapUpdateCback(uint16 connHandle, uint8 *newChanMap, uint8 nextDataChan)
{
  hciEvt_BLEChanMapUpdate_t *pkt;

  pkt = (hciEvt_BLEChanMapUpdate_t *)MAP_osal_msg_allocate(sizeof(hciEvt_BLEChanMapUpdate_t));

  if ( pkt )
  {
    pkt->hdr.event  = HCI_GAP_EVENT_EVENT;
    pkt->hdr.status = HCI_LE_EVENT_CODE;
    pkt->BLEEventCode = HCI_BLE_CHANNEL_MAP_UPDATE_EVENT;

    pkt->connHandle = connHandle;
    pkt->nextDataChan = nextDataChan;

    (void)MAP_osal_memcpy(pkt->newChanMap, newChanMap, LL_NUM_BYTES_FOR_CHAN_MAP);

    (void)MAP_osal_msg_send(hciGapTaskID, (uint8 *)pkt);
  }
}

/*******************************************************************************
 * This function sends a Vendor Specific Command Complete Event to the Host.
 *
 * Public function defined in hci_c_event.h.
 */
void HCI_VendorSpecifcCommandCompleteEvent( uint16  opcode,
                                            uint8   numParam,
                                            uint8  *param )
{
  // check if this is for the Host
  if ( (hciGapTaskID != 0) && (hciPTMenabled == FALSE) )
  {
    hciEvt_VSCmdComplete_t *pkt =
      (hciEvt_VSCmdComplete_t *)MAP_osal_msg_allocate( sizeof(hciEvt_VSCmdComplete_t) +
                                                   numParam );

    if ( pkt )
    {
      pkt->hdr.event   = HCI_GAP_EVENT_EVENT;
      pkt->hdr.status  = HCI_VE_EVENT_CODE;
      pkt->length      = numParam;
      pkt->cmdOpcode   = opcode;
      pkt->pEventParam = (uint8 *)(pkt+1);

      (void)MAP_osal_memcpy( pkt->pEventParam, param, numParam );

      (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)pkt );
    }
  }
  else
  {
    MAP_HCI_SendCommandCompleteEvent( HCI_VE_EVENT_CODE,
                                      opcode,
                                      numParam,
                                      param );
  }
}

/*******************************************************************************
 * This function sends a Command Status Event to the Host.
 *
 * Public function defined in hci_c_event.h.
 */
void HCI_CommandStatusEvent( hciStatus_t status,
                             uint16      opcode )
{
  // check if this is for the Host
  if ( (hciGapTaskID != 0) && (hciPTMenabled == FALSE) )
  {
    hciEvt_CommandStatus_t *pMsg;
    uint8 totalLength;

    totalLength = sizeof(hciEvt_CommandStatus_t);

    pMsg = (hciEvt_CommandStatus_t *)MAP_osal_msg_allocate( totalLength );

    if ( pMsg )
    {
      // message type, HCI event type
      pMsg->hdr.event = HCI_GAP_EVENT_EVENT;

      // use the OSAL status field for HCI event code
      pMsg->hdr.status   = HCI_COMMAND_STATUS_EVENT_CODE;
      pMsg->cmdStatus    = status;
      pMsg->numHciCmdPkt = 1;
      pMsg->cmdOpcode    = opcode;

      // send the message
      (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)pMsg );
    }
  }
  else
  {
    MAP_HCI_SendCommandStatusEvent( HCI_COMMAND_STATUS_EVENT_CODE,
                                    status,
                                    opcode );
  }
}

/*******************************************************************************
 * This function sends a Hardware Error Event to the Host.
 *
 * Public function defined in hci_c_event.h.
 */
void HCI_HardwareErrorEvent( uint8 hwErrorCode )
{
  // check the event mask to see if this event has been masked by Host
  if ( MAP_HCI_CheckEventMaskPage1(BT_EVT_HARDWARE_ERROR_BIT) )
  {
    if ( (hciGapTaskID != 0) && (hciPTMenabled == FALSE) )
    {
      hciEvt_HardwareError_t *pMsg;
      uint8 totalLength;

      totalLength = sizeof(hciEvt_HardwareError_t);

      pMsg = (hciEvt_HardwareError_t *)MAP_osal_msg_allocate( totalLength );

      if ( pMsg )
      {
        // message type, HCI event type
        pMsg->hdr.event = HCI_GAP_EVENT_EVENT;

        // use the OSAL status field for HCI event code
        pMsg->hdr.status   = HCI_BLE_HARDWARE_ERROR_EVENT_CODE;
        pMsg->hardwareCode = hwErrorCode;

        // send the message
        (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)pMsg );
      }
    }
    else
    {
      HCI_HardwareErrorEvent_raw(hwErrorCode);
    }
  }
}

/*******************************************************************************
 * @fn          LL_DataLenExceedEventCback
 *
 * @brief       This EXT LL callback is used to generate an event after receiving
 *              L2CAP data packet and the L2CAP length was exceeded the MTU size.
 *
 * input parameters
 *
 * @param       status - error code:
 *                       HW_FAIL_PDU_SIZE_EXCEEDS_MTU or
 *                       HW_FAIL_PKT_LEN_EXCEEDS_PDU_SIZE
 * @param       handle - Connection handle.
 * @param       cid - L2CAP Channel ID.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_DataLenExceedEventCback( uint8 status, uint16 connHandle, uint16 cid )
{
  if ( hciL2capTaskID != 0 )
  {
    hciEvt_DataLenExceed_t *pkt =
      (hciEvt_DataLenExceed_t *)MAP_osal_msg_allocate( sizeof(hciEvt_DataLenExceed_t) );
    if ( pkt )
    {
      pkt->hdr.event  = HCI_DATA_EVENT;              // packet type
      pkt->hdr.status = status;                      // event code
      pkt->connHandle = connHandle;
      pkt->cid = cid;

      (void)MAP_osal_msg_send( hciL2capTaskID, (uint8 *)pkt );
    }
  }
  else
  {
    // in HCI test keep the same behavior as it was before
    MAP_HCI_HardwareErrorEvent(status);
  }
}

/*******************************************************************************
 * @fn          LL_AuthPayloadTimeoutExpiredCback Callback
 *
 * @brief       This LL callback is used to generate an Authenticated Payload
 *              Timeout event when the APTO expires.
 *
 * input parameters
 *
 * @param       connHandle - Connection handle.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_AuthPayloadTimeoutExpiredCback( uint16 connHandle )
{
  if ( MAP_HCI_CheckEventMaskPage1(BT_EVT_LE_META_EVENT_BIT) &&
       MAP_HCI_CheckEventMaskPage2(BT_EVT_AUTHENTICATED_PAYLOAD_TIMEOUT_EXPIRED_BIT) )
  {
    hciEvt_AptoExpired_t *pkt =
      (hciEvt_AptoExpired_t *)MAP_osal_msg_allocate( sizeof(hciEvt_AptoExpired_t) +
                                                     sizeof(uint16) );
    if ( pkt )
    {
      pkt->hdr.event  = HCI_DATA_EVENT;              // packet type
      pkt->hdr.status = HCI_APTO_EXPIRED_EVENT_CODE; // event code
      pkt->connHandle = connHandle;

      (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)pkt );
    }
  }
}

/*******************************************************************************
 * @fn          LL_RemoteConnParamReqCback Callback
 *
 * @brief       This LL callback is used to generate a Remote Connection
 *              Parameter Request meta event to provide to the Host the peer's
 *              connection parameter request parameters (min connection
 *              interval, max connection interval, peripheral latency, and connection
 *              timeout), and to request the Host's acceptance or rejection of
 *              this parameters.
 *
 * input parameters
 *
 * @param       connHandle   - Connection handle.
 * @param       Interval_Min - Lower limit for connection interval.
 * @param       Interval_Max - Upper limit for connection interval.
 * @param       Latency      - Peripheral latency.
 * @param       Timeout      - Connection timeout.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_RemoteConnParamReqCback( uint16 connHandle,
                                 uint16 Interval_Min,
                                 uint16 Interval_Max,
                                 uint16 Latency,
                                 uint16 Timeout )
{
  // check if LE Meta-Events are enabled and this event is enabled
  if (MAP_HCI_CheckEventMaskLe(LE_EVT_REMOTE_CONN_PARAM_REQUEST_BIT))
  {
    hciEvt_BLERemoteConnParamReq_t *msg;
    uint8 totalLength;

    totalLength = sizeof( hciEvt_BLERemoteConnParamReq_t );

    msg = (hciEvt_BLERemoteConnParamReq_t *)MAP_osal_msg_allocate(totalLength);

    if( msg )
    {
      // message header
      msg->hdr.event  = HCI_GAP_EVENT_EVENT;
      msg->hdr.status = HCI_LE_EVENT_CODE; // use status field to pass the HCI Event code

      // event packet
      msg->BLEEventCode = HCI_BLE_REMOTE_CONN_PARAM_REQUEST_EVENT;
      msg->status       = HCI_SUCCESS;
      msg->connHandle   = connHandle;
      msg->Interval_Min = Interval_Min;
      msg->Interval_Max = Interval_Max;
      msg->Latency      = Latency;
      msg->Timeout      = Timeout;

      // send the message
      (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)msg );
    }
  }
  else
  {
    // the event mask is not set for this event, so accept as is
    MAP_LL_RemoteConnParamReqReply( connHandle,
                                    Interval_Min,
                                    Interval_Max,
                                    Latency,
                                    Timeout,
                                    0,
                                    0 );
  }
}

/*******************************************************************************
 * @fn          LL_PhyUpdateCompleteEventCback Callback
 *
 * @brief       This LL callback is used to indicate that the Controller has
 *              has changed the transmitter or receiver PHY in use or that the
 *              LL_SetPhy command has failed.
 *
 * input parameters
 *
 * @param       status     - Status of LL_SetPhy command.
 * @param       connHandle - Connection handle.
 * @param       txPhy      - Bit map of PHY used for Tx.
 * @param       rxPhy      - Bit map of PHY used for Rx.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_PhyUpdateCompleteEventCback( llStatus_t status,
                                     uint16     connHandle,
                                     uint8      txPhy,
                                     uint8      rxPhy )
{
  // check if LE Meta-Events are enabled and this event is enabled
  if (MAP_HCI_CheckEventMaskLe(LE_EVT_PHY_UPDATE_COMPLETE_BIT))
  {
    // Qualification fix - Bit map of PHY could not be 0
    if ((status != LL_STATUS_SUCCESS) && (txPhy == 0) && (rxPhy == 0))
    {
      txPhy = LL_PHY_1_MBPS;
      rxPhy = LL_PHY_1_MBPS;
    }

    hciEvt_BLEPhyUpdateComplete_t *msg =
      (hciEvt_BLEPhyUpdateComplete_t *)MAP_osal_msg_allocate(sizeof( hciEvt_BLEPhyUpdateComplete_t ));

    if( msg )
    {
      // message header
      msg->hdr.event  = HCI_GAP_EVENT_EVENT;
      msg->hdr.status = HCI_LE_EVENT_CODE; // use status field to pass the HCI Event code

      // event packet
      msg->BLEEventCode = HCI_BLE_PHY_UPDATE_COMPLETE_EVENT;
      msg->status       = status;
      msg->connHandle   = connHandle;
      msg->txPhy        = LL_ConvertPhy(txPhy);
      msg->rxPhy        = LL_ConvertPhy(rxPhy);

      // send the message
      (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)msg );
    }
  }
}


/*******************************************************************************
 * @fn          LL_DataLengthChangeEventCback Callback
 *
 * @brief       This LL callback is used to indicate that the Controller has
 *              has changed the either the maximum payload length or the
 *              maximum transmit time of data channel PDUs in either direction.
 *              The values reported are the maximum taht will actually be used
 *              on the connection following the change.
 *
 * input parameters
 *
 * @param       connHandle  - Connection handle.
 * @param       maxTxOctets - Maximum number of transmit payload bytes.
 * @param       maxTxTime   - Maximum transmit time.
 * @param       maxRxOctets - Maximum number of receive payload bytes.
 * @param       maxRxTime   - Maximum receive time.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_DataLengthChangeEventCback( uint16 connHandle,
                                    uint16 maxTxOctets,
                                    uint16 maxTxTime,
                                    uint16 maxRxOctets,
                                    uint16 maxRxTime )
{
  // check if LE Meta-Events are enabled and this event is enabled
  if (MAP_HCI_CheckEventMaskLe(LE_EVT_DATA_LENGTH_CHANGE_BIT))
  {
    hciEvt_BLEDataLengthChange_t *msg =
      (hciEvt_BLEDataLengthChange_t *)MAP_osal_msg_allocate(sizeof( hciEvt_BLEDataLengthChange_t ));

    if( msg )
    {
      // message header
      msg->hdr.event  = HCI_GAP_EVENT_EVENT;
      msg->hdr.status = HCI_LE_EVENT_CODE; // use status field to pass the HCI Event code

      // event packet
      msg->BLEEventCode = HCI_BLE_DATA_LENGTH_CHANGE_EVENT;
      msg->connHandle   = connHandle;
      msg->maxTxOctets  = maxTxOctets;
      msg->maxTxTime    = maxTxTime;
      msg->maxRxOctets  = maxRxOctets;
      msg->maxRxTime    = maxRxTime;

      // send the message
      (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)msg );
    }
  }
}

/*******************************************************************************
 * @fn          LL_ReadLocalP256PublicKeyCompleteEventCback Callback
 *
 * @brief       This LL callback is used to indicate the Controller has
 *              completed the generation of the P256 public key.
 *
 * @param       None.
 *
 * input parameters
 *
 * output parameters
 *
 * @param       status   - Operation status.
 * @param       p256KeyX - P256 public key (first 32 bytes X=0..31).
 * @param       p256KeyY - P256 public key (second 32 bytes Y=32..63).
 *
 * @return      None.
 */
void LL_ReadLocalP256PublicKeyCompleteEventCback( uint8  status,
                                                  uint8 *p256KeyX,
                                                  uint8 *p256KeyY )
{
  // check if LE Meta-Events are enabled and this event is enabled
  if (MAP_HCI_CheckEventMaskLe(LE_EVT_READ_LOCAL_P256_PUBLIC_KEY_COMPLETE_BIT))
  {
    hciEvt_BLEReadP256PublicKeyComplete_t *msg =
      (hciEvt_BLEReadP256PublicKeyComplete_t *)MAP_osal_msg_allocate(sizeof( hciEvt_BLEReadP256PublicKeyComplete_t ));

    if( msg )
    {
      // message header
      msg->hdr.event  = HCI_SMP_META_EVENT_EVENT;
      msg->hdr.status = HCI_LE_EVENT_CODE;

      // event packet
      msg->BLEEventCode = HCI_BLE_READ_LOCAL_P256_PUBLIC_KEY_COMPLETE_EVENT;
      msg->status       = status;

      if ( status == HCI_SUCCESS )
      {
        // copy the P256 key
        (void)MAP_osal_memcpy( &msg->p256Key[0],  p256KeyX, LL_SC_P256_KEY_LEN/2 );
        (void)MAP_osal_memcpy( &msg->p256Key[32], p256KeyY, LL_SC_P256_KEY_LEN/2 );
      }
      else // clear the key to avoid confusion
      {
        (void)MAP_osal_memset( msg->p256Key, 0, LL_SC_P256_KEY_LEN );
      }

      // send the message
      (void)MAP_osal_msg_send( hciSmpTaskID, (uint8 *)msg );
    }
  }
}

/*******************************************************************************
 * @fn          LL_GenerateDHKeyCompleteEventCback Callback
 *
 * @brief       This LL callback is used to indicate teh Controller has
 *              completed the generation of the Diffie Hellman key.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       status - Operation status.
 * @param       dhKey  - Diffie Hellman key (32 bytes).
 *
 * @return      None.
 */
void LL_GenerateDHKeyCompleteEventCback( uint8  status,
                                         uint8 *dhKey )
{
  // check if LE Meta-Events are enabled and this event is enabled
  if (MAP_HCI_CheckEventMaskLe(LE_EVT_GENERATE_DHKEY_COMPLETE_BIT))
  {
    hciEvt_BLEGenDHKeyComplete_t *msg =
      (hciEvt_BLEGenDHKeyComplete_t *)MAP_osal_msg_allocate(sizeof( hciEvt_BLEGenDHKeyComplete_t ));

    if( msg )
    {
      // message header
      msg->hdr.event  = HCI_SMP_META_EVENT_EVENT;
      msg->hdr.status = HCI_LE_EVENT_CODE;

      // event packet
      msg->BLEEventCode = HCI_BLE_GENERATE_DHKEY_COMPLETE_EVENT;
      msg->status       = status;

      if ( status == HCI_SUCCESS )
      {
        // copy the DH key
        (void)MAP_osal_memcpy( &msg->dhKey, dhKey, LL_SC_DHKEY_LEN );
      }
      else // clear the key to avoid confusion
      {
        (void)MAP_osal_memset( &msg->dhKey, 0, LL_SC_DHKEY_LEN );
      }

      // send the message
      (void)MAP_osal_msg_send( hciSmpTaskID, (uint8 *)msg );
    }
  }
}

/*******************************************************************************
 * @fn          LL_EnhancedConnectionCompleteCback Callback
 *
 * @brief       This LL callback is used to generate an Enhanced Connection
 *              Complete meta event when a connection is established by either
 *              an Advertiser or an Initiator.
 *
 * input parameters
 *
 * @param       reasonCode    - Status of connection complete.
 * @param       connHandle    - Connection handle.
 * @param       role          - Connection formed as Central or Central.
 * @param       peerAddrType  - Peer address as Public or Random.
 * @param       peerAddr      - Pointer to peer device address.
 * @param       localRPA      - Pointer to local RPA.
 * @param       peerRPA       - Pointer to peer RPA.
 * @param       connInterval  - Connection interval.
 * @param       peripheralLatency  - Peripheral latency.
 * @param       connTimeout   - Connection timeout.
 * @param       clockAccuracy - Sleep clock accuracy (from Central only).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_EnhancedConnectionCompleteCback( uint8   reasonCode,
                                         uint16  connHandle,
                                         uint8   role,
                                         uint8   peerAddrType,
                                         uint8  *peerAddr,
                                         uint8  *localRPA,
                                         uint8  *peerRPA,
                                         uint16  connInterval,
                                         uint16  peripheralLatency,
                                         uint16  connTimeout,
                                         uint8   clockAccuracy )
{
  uint8 enhanceConn = FALSE;
  uint8 hciEvtType;

  if(MAP_HCI_CheckEventMaskLe(LE_EVT_ENH_CONN_COMPLETE_BIT))
  {
    hciEvtType = HCI_BLE_ENHANCED_CONNECTION_COMPLETE_EVENT;
    enhanceConn = TRUE;
  }
  else if(MAP_HCI_CheckEventMaskLe(LE_EVT_CONN_COMPLETE_BIT))
  {
    hciEvtType = HCI_BLE_CONNECTION_COMPLETE_EVENT;
  }
  else // event not enable in event mask
  {
    hciEvtType = FALSE;
  }

  if(hciEvtType != FALSE)
  {
    hciEvt_BLEEnhConnComplete_t *pkt;

    pkt = (hciEvt_BLEEnhConnComplete_t *)MAP_osal_msg_allocate( sizeof(hciEvt_BLEEnhConnComplete_t) );

    if ( pkt )
    {
      pkt->hdr.event  = HCI_GAP_EVENT_EVENT;
      pkt->hdr.status = HCI_LE_EVENT_CODE;

      if ( reasonCode == LL_STATUS_SUCCESS )
      {
        pkt->status = HCI_SUCCESS;
        (void)MAP_osal_memcpy( pkt->peerAddr, peerAddr, B_ADDR_LEN );
      }
      else
      {
        pkt->status = bleGAPConnNotAcceptable;
        (void)MAP_osal_memset( pkt->peerAddr, 0, B_ADDR_LEN );
      }
      pkt->BLEEventCode = hciEvtType;

      if ( enhanceConn )
      {
        // local and peer RPA
        (localRPA != NULL) ? MAP_osal_memcpy( pkt->localRPA, localRPA, B_ADDR_LEN ) :
                             MAP_osal_memset( pkt->localRPA, 0, B_ADDR_LEN );

        (peerRPA != NULL)  ? MAP_osal_memcpy( pkt->peerRPA, peerRPA, B_ADDR_LEN ) :
                             MAP_osal_memset( pkt->peerRPA, 0, B_ADDR_LEN );

        pkt->peerAddrType = peerAddrType;
      }
      else // LE_EVT_MASK_CONN_COMPLETE
      {
        MAP_osal_memset( pkt->localRPA, 0, B_ADDR_LEN );
        MAP_osal_memset( pkt->peerRPA, 0, B_ADDR_LEN );
        pkt->peerAddrType = peerAddrType & LL_DEV_ADDR_TYPE_MASK;
      }

      pkt->connectionHandle = connHandle;
      pkt->role             = role;
      pkt->connInterval     = connInterval;
      pkt->connLatency      = peripheralLatency;
      pkt->connTimeout      = connTimeout;
      pkt->clockAccuracy    = clockAccuracy;

      (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)pkt );
    }
  }

  /**** UPDATE DEBUG INFO MODULE ****/
  (void)MAP_DbgInf_addConnEst(connHandle, role, UFALSE);
}

/*******************************************************************************
 * @fn          LL_ChannelSelectionAlgorithmCback Callback
 *
 * @brief       This LL callback is used to indicate which channel selection
 *              algorithm is used on a data channel connection.
 *
 * input parameters
 *
 * @param       connHandle - Connection handle.
 * @param       chSelAlgo  - LL_CHANNEL_SELECT_ALGO_1 | LL_CHANNEL_SELECT_ALGO_2
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_ChannelSelectionAlgorithmCback( uint16 connHandle,
                                        uint8  chSelAlgo )
{

  // check if LE Meta-Events are enabled and this event is enabled
  if (MAP_HCI_CheckEventMaskLe(LE_EVT_CHANNEL_SELECTION_ALGORITHM_BIT))
  {
    hciEvt_BLEChanSelAlgo_t *msg =
      (hciEvt_BLEChanSelAlgo_t *)MAP_osal_msg_allocate(sizeof( hciEvt_BLEChanSelAlgo_t ));

    if( msg )
    {
      // message header
      msg->hdr.event  = HCI_GAP_EVENT_EVENT;
      msg->hdr.status = HCI_LE_EVENT_CODE; // use status field to pass the HCI Event code

      // event packet
      msg->BLEEventCode = HCI_BLE_CHANNEL_SELECTION_ALGORITHM_EVENT;
      msg->connHandle   = connHandle;
      msg->chSelAlgo    = chSelAlgo;

      // send the message
      (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)msg );
    }
  }
}

/*
** LL Callbacks for LE Meta-Events
*/

/*******************************************************************************
 * @fn          LL_AdvReportCback Callback
 *
 * @brief       This LL callback is used to generate a Advertisment Report meta
 *              event when an Advertisment or Scan Response is received by a
 *              Scanner.
 *
 * input parameters
 *
 * @param       advEvt      - Advertise event type, or Scan Response event type.
 * @param       advAddrType - Public or Random address type.
 * @param       advAddr     - Pointer to device address.
 * @param       dataLen     - Length of data in bytes.
 * @param       advData     - Pointer to data.
 * @param       rssi        - The RSSI of received packet.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_AdvReportCback( uint8 advEvt,
                        uint8 advAddrType,
                        uint8 *advAddr,
                        uint8 dataLen,
                        uint8 *advData,
                        int8  rssi )
{
  hciEvt_BLEAdvPktReport_t *pkt;
  hciEvt_DevInfo_t *devInfo;
  uint8 x;

  pkt = (hciEvt_BLEAdvPktReport_t *)MAP_osal_msg_allocate( sizeof( hciEvt_BLEAdvPktReport_t ) +
                                                           sizeof( hciEvt_DevInfo_t ) );

  if ( pkt )
  {
    pkt->hdr.event = HCI_GAP_EVENT_EVENT;
    pkt->hdr.status = HCI_LE_EVENT_CODE;
    pkt->BLEEventCode = HCI_BLE_ADV_REPORT_EVENT;
    pkt->numDevices = 1;  // assume one device for now
    pkt->devInfo = devInfo = (hciEvt_DevInfo_t *)(pkt+1);

    for ( x = 0; x < pkt->numDevices; x++, devInfo++ )
    {
      /* Fill in the device info */
      devInfo->eventType = advEvt;
      devInfo->addrType = advAddrType;
      (void)MAP_osal_memcpy( devInfo->addr, advAddr, B_ADDR_LEN );
      devInfo->dataLen = dataLen;
      (void)MAP_osal_memcpy( devInfo->rspData, advData, dataLen );
      devInfo->rssi = rssi;
    }

    (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)pkt );
  }
}

/*******************************************************************************
 * @fn          LL_ConnectionCompleteCback Callback
 *
 * @brief       This LL callback is used to generate a Connection Complete meta
 *              event when a connection is established by either an Advertiser
 *              or an Initiator.
 *
 * input parameters
 *
 * @param       reasonCode    - Status of connection complete.
 * @param       connHandle    - Connection handle.
 * @param       role          - Connection formed as Central or Central.
 * @param       peerAddrType  - Peer address as Public or Random.
 * @param       peerAddr      - Pointer to peer device address.
 * @param       connInterval  - Connection interval.
 * @param       peripheralLatency  - Peripheral latency.
 * @param       connTimeout   - Connection timeout.
 * @param       clockAccuracy - Sleep clock accuracy (from Central only).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_ConnectionCompleteCback( uint8  reasonCode,
                                 uint16 connHandle,
                                 uint8  role,
                                 uint8  peerAddrType,
                                 uint8  *peerAddr,
                                 uint16 connInterval,
                                 uint16 peripheralLatency,
                                 uint16 connTimeout,
                                 uint8  clockAccuracy )
{
  hciEvt_BLEConnComplete_t *pkt;

  pkt = (hciEvt_BLEConnComplete_t *)MAP_osal_msg_allocate( sizeof( hciEvt_BLEConnComplete_t ) );
  if ( pkt )
  {
    pkt->hdr.event    = HCI_GAP_EVENT_EVENT;
    pkt->hdr.status   = HCI_LE_EVENT_CODE;
    pkt->BLEEventCode = HCI_BLE_CONNECTION_COMPLETE_EVENT;

    if ( reasonCode == LL_STATUS_SUCCESS )
    {
      pkt->status = HCI_SUCCESS;
      (void)MAP_osal_memcpy( pkt->peerAddr, peerAddr, B_ADDR_LEN );
    }
    else
    {
      pkt->status = bleGAPConnNotAcceptable;
      (void)MAP_osal_memset( pkt->peerAddr, 0, B_ADDR_LEN );
    }
    pkt->connectionHandle = connHandle;
    pkt->role             = role;
    pkt->peerAddrType     = peerAddrType;
    pkt->connInterval     = connInterval;
    pkt->connLatency      = peripheralLatency;
    pkt->connTimeout      = connTimeout;
    pkt->clockAccuracy    = clockAccuracy;

    (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)pkt );
  }
}


/*******************************************************************************
 * @fn          LL_DisconnectCback Callback
 *
 * @brief       This LL callback is used to generate a Disconnect Complete meta
 *              event when a connection is disconnected by either a Central or
 *              a Central.
 *
 * input parameters
 *
 * @param       connHandle - Connection handle.
 * @param       reasonCode - Status of connection complete.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_DisconnectCback( uint16 connHandle,
                         uint8  reasonCode )
{
  hciEvt_DisconnComplete_t *pkt;

  pkt = (hciEvt_DisconnComplete_t *)MAP_osal_msg_allocate( sizeof( hciEvt_DisconnComplete_t ) );
  if ( pkt )
  {
    pkt->hdr.event  = HCI_GAP_EVENT_EVENT;
    pkt->hdr.status = HCI_DISCONNECTION_COMPLETE_EVENT_CODE;
    pkt->status     = HCI_SUCCESS;
    pkt->connHandle = connHandle;
    pkt->reason     = reasonCode;

    (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)pkt );
  }

  /**** UPDATE DEBUG INFO MODULE ****/
  (void)MAP_llDbgInf_addConnTerm(connHandle, reasonCode);
}



/*******************************************************************************
 * @fn          LL_ConnParamUpdateCback Callback
 *
 * @brief       This LL callback is used to generate a Connection Update
 *              Complete meta event when a connection's parameters are updated
 *              by the Central, or if an error occurs (e.g. during the
 *              Connection Parameter Request control procedure).
 *
 * input parameters
 *
 * @param       status       - Status of update complete event.
 * @param       connHandle   - Connection handle.
 * @param       connInterval - Connection interval.
 * @param       peripheralLatency - Peripheral latency.
 * @param       connTimeout  - Connection timeout.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_ConnParamUpdateCback( llStatus_t status,
                              uint16     connHandle,
                              uint16     connInterval,
                              uint16     connLatency,
                              uint16     connTimeout )
{
  // check if LE Meta-Events are enabled and this event is enabled
  if (MAP_HCI_CheckEventMaskLe(LE_EVT_CONN_UPDATE_COMPLETE_BIT))
  {
    hciEvt_BLEConnUpdateComplete_t *msg;
    uint8 totalLength;

    totalLength = sizeof( hciEvt_BLEConnUpdateComplete_t );

    msg = (hciEvt_BLEConnUpdateComplete_t *)MAP_osal_msg_allocate(totalLength);

    if( msg )
    {
      // message header
      msg->hdr.event  = HCI_GAP_EVENT_EVENT;
      msg->hdr.status = HCI_LE_EVENT_CODE; // use status field to pass the HCI Event code

      // event packet
      msg->BLEEventCode     = HCI_BLE_CONN_UPDATE_COMPLETE_EVENT;
      msg->status           = status;
      msg->connectionHandle = connHandle;
      msg->connInterval     = connInterval;
      msg->connLatency      = connLatency;
      msg->connTimeout      = connTimeout;

      // send the message
      (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)msg );
    }
  }
}

/*******************************************************************************
 * @fn          LL_ConnParamUpdateRejectCback Callback
 *
 * @brief       This function will send an event to the GAP after the device
 *              has rejected a peer device connection parameter update request
 *
 *
 * input parameters
 *
 * @param       connHandle    - Connection handle.
 * @param       status        - Link Layer error code.
 *
 * @param       connInterval  - Connection interval.
 * @param       connLatency   - Peripheral latency.
 * @param       connTimeout   - Connection timeout.
 *
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_ConnParamUpdateRejectCback( llStatus_t   status,
                                    uint16  connHandle,
                                    uint16  connInterval,
                                    uint16  connLatency,
                                    uint16  connTimeout )

{
  // Check if the feature was enabled by the application.
  if ( (hciVsEvtMask & PEER_PARAM_REJECT_ENABLED ) == 0 )
  {
    // the feature was not enabled
    return;
  }
  else
  {
    hciEvt_BLEConnUpdateComplete_t *msg;
    uint8 totalLength;

    totalLength = sizeof( hciEvt_BLEConnUpdateComplete_t );

    msg = (hciEvt_BLEConnUpdateComplete_t *)MAP_osal_msg_allocate(totalLength);

    if( msg )
    {
      // message header
      msg->hdr.event  = HCI_GAP_EVENT_EVENT;
      msg->hdr.status = HCI_LE_EVENT_CODE; // use status field to pass the HCI Event code

      // event packet
      msg->BLEEventCode     = HCI_BLE_CONN_UPDATE_REJECT_EVENT;
      msg->status           = status;
      msg->connectionHandle = connHandle;
      msg->connInterval     = connInterval;
      msg->connLatency      = connLatency;
      msg->connTimeout      = connTimeout;

      // send the message
      (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)msg );
    }
  }
}
/*******************************************************************************
 * @fn          LL_EncLtkReqCback Callback
 *
 * @brief       This LL callback is used to generate a Encryption LTK Request
 *              meta event to provide to the Host the Central's random number
 *              and encryption diversifier, and to request the Host's Long Term
 *              Key (LTK).
 *
 * input parameters
 *
 * @param       connHandle - The LL connection ID for new connection.
 * @param       randNum    - Random vector used in device identification.
 * @param       encDiv     - Encrypted diversifier.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_EncLtkReqCback( uint16  connHandle,
                        uint8  *randNum,
                        uint8  *encDiv )
{
  hciEvt_BLELTKReq_t *pkt;

  pkt = (hciEvt_BLELTKReq_t *)MAP_osal_msg_allocate( sizeof( hciEvt_BLELTKReq_t ) );
  if ( pkt )
  {
    pkt->hdr.event    = HCI_SMP_EVENT_EVENT;
    pkt->hdr.status   = HCI_LE_EVENT_CODE;
    pkt->BLEEventCode = HCI_BLE_LTK_REQUESTED_EVENT;
    pkt->connHandle   = connHandle;

    (void)MAP_osal_memcpy( pkt->random, randNum, B_RANDOM_NUM_SIZE );
    pkt->encryptedDiversifier = BUILD_UINT16( encDiv[0], encDiv[1] );

    (void)MAP_osal_msg_send( hciSmpTaskID, (uint8 *)pkt );
  }
}


/*******************************************************************************
 * @fn          LL_EncChangeCback Callback
 *
 * @brief       This Callback is used by the LL to indicate to the Host that
 *              an encryption change has taken place. This results when
 *              the host performs a LL_StartEncrypt when encryption is not
 *              already enabled.
 *
 *              Note: If the key request was rejected, then encryption will
 *                    remain off.
 *
 * input parameters
 *
 * @param       connHandle - The LL connection ID for new connection.
 * @param       reason     - LL_ENC_KEY_REQ_ACCEPTED or LL_ENC_KEY_REQ_REJECTED.
 * @param       encEnab    - LL_ENCRYPTION_OFF or LL_ENCRYPTION_ON.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_EncChangeCback( uint16 connHandle,
                        uint8  reason,
                        uint8  encEnab )
{
  hciEvt_EncryptChange_t *pkt;

  pkt = (hciEvt_EncryptChange_t *)MAP_osal_msg_allocate( sizeof( hciEvt_EncryptChange_t ) );
  if ( pkt )
  {
    pkt->hdr.event    = HCI_SMP_EVENT_EVENT;
    pkt->hdr.status   = HCI_LE_EVENT_CODE;
    pkt->BLEEventCode = HCI_ENCRYPTION_CHANGE_EVENT_CODE;
    pkt->connHandle   = connHandle;
    pkt->reason       = reason;
    pkt->encEnable    = encEnab;

    (void)MAP_osal_msg_send( hciSmpTaskID, (uint8 *)pkt );
  }
}


/*******************************************************************************
 * @fn          LL_EncKeyRefreshCback Callback
 *
 * @brief       This Callback is used by the LL to indicate to the Host that
 *              an encryption key refresh has taken place. This results when
 *              the host performs a LL_StartEncrypt when encryption is already
 *              enabled.
 *
 * input parameters
 *
 * @param       connHandle - The LL connection ID for new connection.
 * @param       reason    - LL_ENC_KEY_REQ_ACCEPTED, LL_CTRL_PKT_TIMEOUT_TERM
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void LL_EncKeyRefreshCback( uint16 connHandle,
                            uint8  reason )
{
  hciEvt_EncryptChange_t *pkt;

  pkt = (hciEvt_EncryptChange_t *)MAP_osal_msg_allocate( sizeof( hciEvt_EncryptChange_t ) );
  if ( pkt )
  {
    pkt->hdr.event    = HCI_SMP_EVENT_EVENT;
    pkt->hdr.status   = HCI_LE_EVENT_CODE;
    pkt->BLEEventCode = HCI_ENCRYPTION_CHANGE_EVENT_CODE;
    pkt->connHandle   = connHandle;
    pkt->reason       = reason;
    pkt->encEnable    = TRUE;

    (void)MAP_osal_msg_send( hciSmpTaskID, (uint8 *)pkt );
  }
}

#ifdef USE_PERIODIC_SCAN
/*********************************************************************
 * @fn      HCI_PeriodicAdvSyncEstablishedEvent
 *
 * @brief   This event indicates the scanner that the Controller has received
 *          the first periodic advertising packet from an advertiser after the
 *          HCI_LE_Periodic_Advertising_Create_Sync command has been sent to the Controller.
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param   status           - Periodic advertising sync HCI status
 * @param   syncHandle       - Handle identifying the periodic advertising train assigned by the Controller
 *                             (Range: 0x0000 to 0x0EFF)
 * @param   advSid           - Value of the Advertising SID subfield in the ADI field of the PDU
 * @param   advAddrType      - Advertiser address type
 *                             0x00 - Public
 *                             0x01 - Random
 *                             0x02 - Public Identity Address
 *                             0x03 - Random Identity Address
 * @param   advAddress       - Advertiser address
 * @param   advPhy           - Advertiser PHY
 *                             0x01 - LE 1M
 *                             0x02 - LE 2M
 *                             0x03 - LE Coded
 * @param   periodicAdvInt   - Periodic advertising interval Range: 0x0006 to 0xFFFF
 *                             Time = N * 1.25 ms (Time Range: 7.5 ms to 81.91875 s)
 * @param   advClockAccuracy - Accuracy of the periodic advertiser's clock
 *                             0x00 - 500 ppm
 *                             0x01 - 250 ppm
 *                             0x02 - 150 ppm
 *                             0x03 - 100 ppm
 *                             0x04 - 75 ppm
 *                             0x05 - 50 ppm
 *                             0x06 - 30 ppm
 *                             0x07 - 20 ppm
 *
 * @return  void
 */
void HCI_PeriodicAdvSyncEstablishedEvent( uint8  status,
                                          uint16 syncHandle,
                                          uint8  advSid,
                                          uint8  advAddrType,
                                          uint8  *advAddress,
                                          uint8  advPhy,
                                          uint16 periodicAdvInt,
                                          uint8  advClockAccuracy )
{
  // check if LE Meta-Events are enabled and this event is enabled
  if (HCI_CheckEventMaskLe(LE_EVT_PERIODIC_ADV_SYNC_ESTABLISHED_BIT))
  {
    hciEvt_BLEPeriodicAdvSyncEstablished_t *msg =
      (hciEvt_BLEPeriodicAdvSyncEstablished_t *)MAP_osal_msg_allocate(sizeof( hciEvt_BLEPeriodicAdvSyncEstablished_t ));

    if( msg )
    {
      // message header
      msg->hdr.event  = HCI_GAP_EVENT_EVENT;
      msg->hdr.status = HCI_LE_EVENT_CODE; // use status field to pass the HCI Event code

      // event packet
      msg->BLEEventCode = HCI_BLE_PERIODIC_ADV_SYNCH_ESTABLISHED_EVENT;
      msg->status       = status;
      msg->syncHandle   = syncHandle;
      msg->sid          = advSid;
      msg->addrType     = advAddrType;
      if (advAddress != NULL)
      {
        (void)MAP_osal_memcpy( msg->address, advAddress, B_ADDR_LEN );
      }
      msg->phy          = advPhy;
      msg->periodicInterval = periodicAdvInt;
      msg->clockAccuracy = advClockAccuracy;

      // send the message
      (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)msg );
    }
  }
}

/*********************************************************************
 * @fn      HCI_LE_PeriodicAdvertisingReportEvent
 *
 * @brief   This event indicates the scanner that the Controller has
 *          received a Periodic Advertising packet.
 *
 * @design  /ref did_286039104
 *
 * @param   syncHandle - Handle identifying the periodic advertising train
 * @param   txPower    - Tx Power information (Range: -127 to +20)
 * @param   rssi       - RSSI value for the received packet (Range: -127 to +20)
 *                       If the packet contains CTE, this value is not available
 * @param   cteType    - Constant Tone Extension type
 *                       0x00 - AoA Constant Tone Extension
 *                       0x01 - AoD Constant Tone Extension with 1us slots
 *                       0x02 - AoD Constant Tone Extension with 2us slots
 *                       0xFF - No Constant Tone Extension
 * @param   dataStatus - Data status
 *                       0x00 - Data complete
 *                       0x01 - Data incomplete, more data to come
 *                       0x02 - Data incomplete, data truncated, no more to come
 * @param   dataLen    - Length of the Data field (Range: 0 to 247)
 * @param   data       - Data received from a Periodic Advertising packet
 *
 * @return  void
 */
void HCI_PeriodicAdvReportEvent( uint16 syncHandle,
                                 int8   txPower,
                                 int8   rssi,
                                 uint8  cteType,
                                 uint8  dataStatus,
                                 uint8  dataLen,
                                 uint8  *data )
{
  // check if LE Meta-Events are enabled and this event is enabled
  if (HCI_CheckEventMaskLe(LE_EVT_PERIODIC_ADV_REPORT_BIT))
  {
    hciEvt_BLEPeriodicAdvReport_t *msg =
      (hciEvt_BLEPeriodicAdvReport_t *)MAP_osal_msg_allocate(sizeof( hciEvt_BLEPeriodicAdvReport_t ) + dataLen);

    if( msg )
    {
      // message header
      msg->hdr.event  = HCI_GAP_EVENT_EVENT;
      msg->hdr.status = HCI_LE_EVENT_CODE; // use status field to pass the HCI Event code

      // event packet
      msg->BLEEventCode = HCI_BLE_PERIODIC_ADV_REPORT_EVENT;
      msg->syncHandle   = syncHandle;
      msg->txPower      = txPower;
      msg->rssi         = rssi;
      msg->cteType      = cteType;
      msg->dataStatus   = dataStatus;
      msg->dataLen      = dataLen;
      if ((data != NULL) && (dataLen > 0))
      {
        msg->data = ((uint8 *)(msg)) + sizeof( hciEvt_BLEPeriodicAdvReport_t );
        MAP_osal_memcpy( msg->data, data, dataLen );
      }

      // send the message
      (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)msg );
    }
  }
}

/*********************************************************************
 * @fn      HCI_PeriodicAdvSyncLostEvent
 *
 * @brief   This event indicates the scanner that the Controller has not
 *          received a Periodic Advertising packet from the train identified
 *          by syncHandle within the timeout period.
 *
 * @design  /ref did_286039104
 *
 * @param   syncHandle - Handle identifying the periodic advertising train
 *
 * @return  void
 */
void HCI_PeriodicAdvSyncLostEvent( uint16 syncHandle )
{
  // check if LE Meta-Events are enabled and this event is enabled
  if (HCI_CheckEventMaskLe(LE_EVT_PERIODIC_ADV_SYNC_LOST_BIT))
  {
    hciEvt_BLEPeriodicAdvSyncLost_t *msg =
      (hciEvt_BLEPeriodicAdvSyncLost_t *)MAP_osal_msg_allocate(sizeof( hciEvt_BLEPeriodicAdvSyncLost_t ));

    if( msg )
    {
      // message header
      msg->hdr.event  = HCI_GAP_EVENT_EVENT;
      msg->hdr.status = HCI_LE_EVENT_CODE; // use status field to pass the HCI Event code

      // event packet
      msg->BLEEventCode = HCI_BLE_PERIODIC_ADV_SYNCH_LOST_EVENT;
      msg->syncHandle   = syncHandle;

      // send the message
      (void)MAP_osal_msg_send( hciGapTaskID, (uint8 *)msg );
    }
  }
}
#endif //USE_PERIODIC_SCAN
