/******************************************************************************

 @file  ll_isr.c

 @brief This file contains the Interrupt Service Routines (ISR) for the
        Bluetooth Low Energy CC26xx RF Core Firmware
        Specification.

        Note: Currently, the ISR is not built in ROM. Consequently, all
        MAP_ calls do not do not incur any overhead using the
        ROM-to-ROM Jump Table.

        Note: All calls to LL_PM_ do not use MAP_.

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
#ifndef USE_RCL
#include <ti/drivers/rf/RF.h>
#include "rf_api.h"
#include "rf_hal.h"
#else
#include <ti/drivers/rcl/commands/ble5.h>
#endif
#include "../../ll/inc/ble.h"
#include "osal_bufmgr.h"
#include "osal_cbtimer.h"
#include "../../ll/inc/ble_isr.h"
#include "../../ll/inc/ll.h"
#include "../../ll/inc/ll_common.h"
#include "../../ll/inc/ll_enc.h"
#include "../../ll/inc/ll_config.h"
#include "../../ll/inc/ll_rat.h"
#include "hci_event.h"
#include "../../ll/inc/ll_privacy.h"
#include "icall.h"
#include "hal_gpio_wrapper.h"
#include "../../ll/inc/ll_ae.h"
//
#include "rom_jt.h"

// SW Tracer
#ifdef DEBUG_SW_TRACE
#include <ti/drivers/rf/RF.h>
#define DBG_ENABLE
#include "dbgid_sys_mst.h"
#endif // DEBUG_SW_TRACE

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
// indication for prevent handling the next interrupt
uint8 llUnhandleNextIntFlag = FALSE;
uint8 llUnhandleNextIntFlag_Init = FALSE;
/*******************************************************************************
 * GLOBAL VARIABLES
 */
#ifndef USE_RCL
// handle to radio driver for BLE client
extern RF_Handle    rfHandle;

// command handle for radio driver calls
extern RF_CmdHandle rfCmdHandle;

extern rfOpCmd_runImmedCmd_t fwParDtmCmd;
#endif

void LL_TxDoneCback( void );
void LL_TxEntryDoneCback( void );
void LL_RxEmptyCback( void );
uint32_t LL_RxIgnoredCback( void );
void LL_RxEntryDoneCback( uint8 crcError );
void LL_RxEntryDoneCback_allEntries( uint8 crcError );
uint32_t LL_LastCmdDoneCback( void );
void LL_DoorbellErrorCback( void );
uint32_t LL_AbortedCback( uint8 );
void llCmdStartedEventHandle( void );

#ifdef USE_RCL
void LL_rclAdvRxEntryDone( void );
void LL_rclScanRxEntryDone( void );
void LL_rclInitRxEntryDone( void );
void LL_rclUpdateExtAl( RCL_FilterList *filterList,
                     uint16 flags,
                     uint8* rpaAddr,
                     uint8 rlIndex );

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
static void llCheckPeerAddrTypeAndUpdate( llConnState_t *connPtr,
                                   uint8 *cmdDevAddr,
                                   uint8 *peerAddr,
                                   uint8 rlIndex );

#endif // CTRL_CONFIG & INIT_CFG
#endif // USE_RCL

#define STATE_CASE_NOT_HANDLED  0
#define STATE_CASE_HANDLED      1

const uint8 ctrlPktLenTable[] =
  {
    LL_CONN_UPDATE_IND_PAYLOAD_LEN,
    LL_CHAN_MAP_IND_PAYLOAD_LEN,
    LL_TERM_IND_PAYLOAD_LEN,
    LL_ENC_REQ_PAYLOAD_LEN,
    LL_ENC_RSP_PAYLOAD_LEN,
    LL_START_ENC_REQ_PAYLOAD_LEN,
    LL_START_ENC_RSP_PAYLOAD_LEN,
    LL_UNKNOWN_RSP_PAYLOAD_LEN,
    LL_FEATURE_REQ_PAYLOAD_LEN,
    LL_FEATURE_RSP_PAYLOAD_LEN,
    LL_PAUSE_ENC_REQ_PAYLOAD_LEN,
    LL_PAUSE_ENC_RSP_PAYLOAD_LEN,
    LL_VERSION_IND_PAYLOAD_LEN,
    LL_REJECT_IND_PAYLOAD_LEN,
    LL_PERIPHERAL_FEATURE_REQ_PAYLOAD_LEN,
    LL_CONN_PARAM_REQ_PAYLOAD_LEN,
    LL_CONN_PARAM_RSP_PAYLOAD_LEN,
    LL_REJECT_EXT_IND_PAYLOAD_LEN,
    LL_PING_REQ_PAYLOAD_LEN,
    LL_PING_RSP_PAYLOAD_LEN,
    LL_LENGTH_REQ_PAYLOAD_LEN,
    LL_LENGTH_RSP_PAYLOAD_LEN,
    LL_PHY_REQ_PAYLOAD_LEN,
    LL_PHY_RSP_PAYLOAD_LEN,
    LL_PHY_UPDATE_REQ_PAYLOAD_LEN,
    LL_MIN_USED_CHANNELS_IND_LEN,
    LL_CTE_REQ_PAYLOAD_LEN,
    LL_CTE_RSP_PAYLOAD_LEN
  };

#ifdef USE_RCL
/*
** CC23X0 BLE RF Driver Callback
*/
void LL_rclRescheduleCommand(RCL_Command *cmd)
{
  if (cmd != NULL)
  {
    cmd->status = RCL_CommandStatus_Idle;
  }
  taskEndAction = MAP_llScheduler;
  (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * This is the common RCL callback used for advertise command.
 *
 */
void LL_rclAdvCallback(RCL_Command *cmd,
                    LRF_Events lrfEvents,
                    RCL_Events events)
{
  if (llState != LL_STATE_EXT_ADV)
  {
    MAP_llHaltRadio( (uint32)cmd );
    LL_rclRescheduleCommand(cmd);
    return;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Command Started
  //////////////////////////////////////////////////////////////////////////////
  if ( events.cmdStarted )
  {
    llCmdStartedEventHandle();
  }

  //////////////////////////////////////////////////////////////////////////////
  // Rx Entry Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.rxEntryAvail )
  {
    if (lrfEvents.rxOk)
    {
      LL_rclAdvRxEntryDone();
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    MAP_llLastCmdDoneEventHandleStateAdv();
  }

  return;
}


/*******************************************************************************
 * This function is used by LL_rclAdvCallback to handle reception of SCAN_REQ packets
 * In case a callback was registered by the application, it will be called
 *
 */
void LL_rclAdvRxEntryDone( void )
{
  RCL_Buffer_DataEntry     *rxEntry;
  RCL_MultiBuffer_ListInfo listInfo;

  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );
  uint8    *pPkt;
  uint8    peerAddrType;
  uint8    peerAddr[B_ADDR_LEN];
  uint8    sendReq = TRUE;

  // Get MultiBuffer
  RCL_MultiBuffer_ListInfo_init(&listInfo, &((aeLegacyRf_t *)pAdvSet->pRfCmds)->advParam.rxBuffers);
  rxEntry = RCL_MultiBuffer_RxEntry_next(&listInfo);

  // Get the LL PDU header
  pPkt = LL_GET_PDU_HEADER(rxEntry->data, rxEntry->numPad);

  if ( LL_SCAN_REQ_PDU( *pPkt ) )
  {
    MAP_osal_memcpy( peerAddr, &pPkt[2], B_ADDR_LEN );
    peerAddrType = LL_ADV_HDR_GET_TX_ADD(*pPkt);

    // check if the ScanA is an RPA
    if ( MAP_LL_PRIV_IsRPA( peerAddrType, peerAddr ) )
    {
      uint8 rlIndex = MAP_LL_PRIV_IsResolvable( peerAddr, resolvingList );

      // Check filter policy
      if ( (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_SCAN_REQ) ||
           (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_ALL_REQ) )
      {
        // see if the Peer Address resolved
        if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
        {
          // check if resolved RPA's ID is in the AL
          if ( MAP_AL_FindEntry( alTable,
                                 resolvingList[rlIndex].idAddr,
                                 resolvingList[rlIndex].idAddrType ) != alTable->numAlEntries )
          {
            // update the peer RPA in the extended accept list table
            MAP_LL_PRIV_UpdateExtALEntry( alTable,
                                          resolvingList[rlIndex].RPA,
                                          peerAddr );

            // update RL RPA for this peer with ScanA
            MAP_osal_memcpy( resolvingList[rlIndex].RPA, peerAddr, B_ADDR_LEN );
            MAP_osal_memcpy( peerAddr, resolvingList[rlIndex].idAddr, B_ADDR_LEN);
          }
          else
          {
            sendReq = FALSE;
          }
        }
        else
        {
          // We didn't find this peer in the resolving list and we are using accept lists
          sendReq = FALSE;
        }
      }
      else
      {
        // see if the Peer Address resolved
        if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
        {
          // Update the RPA in the resolving list
          MAP_osal_memcpy( resolvingList[rlIndex].RPA, peerAddr, B_ADDR_LEN );
          // it is, so use ID address and address type
          peerAddrType = resolvingList[rlIndex].idAddrType | LL_DEV_ADDR_TYPE_ID_MASK;
          MAP_osal_memcpy( peerAddr, resolvingList[rlIndex].idAddr, B_ADDR_LEN );
        }
      }
    }

    // check if there's a register callback and if a Scan Request Report is needed
    if (( MAP_llCheckCBack(LL_CBACK_EXT_SCAN_REQ_RECEIVED) == FALSE) ||
        (( pAdvSet->pAdvParam->notifyEnableFlags & AE_NOTIFY_ENABLE_SCAN_REQUEST ) == 0) )
    {
       sendReq = FALSE;
    }

    if ( sendReq )
    {
      aeScanReqReceived_t *scanRequestRpt;

      // Use allocLimited to avoid overflowing the heap...
      scanRequestRpt = MAP_osal_mem_allocLimited( sizeof(aeScanReqReceived_t) );

      // check if we got the memory
      if ( scanRequestRpt )
      {
        scanRequestRpt->subCode      = AE_ADV_HCI_BLE_SCAN_REQUEST_RECEIVED_EVENT;
        scanRequestRpt->handle       = aeCurHandle;
        scanRequestRpt->scanAddrType = peerAddrType;
        scanRequestRpt->channel      = GET_CHANNEL_IDX(RCL_BLE5_getRxChannel(rxEntry));
        scanRequestRpt->rssi         = RCL_BLE5_getRxRssi(rxEntry);

        MAP_osal_memcpy( scanRequestRpt->scanAddr, peerAddr, B_ADDR_LEN );
        MAP_llExtAdvCBack( LL_CBACK_EXT_SCAN_REQ_RECEIVED, (void *)scanRequestRpt );
      }
      else // out of memory
      {
        MAP_llExtAdvCBack( LL_CBACK_OUT_OF_MEMORY, NULL );
      }
    }
  }
}
#endif // (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * This is the common RCL callback used for scan command.
 *
 */
void LL_rclScanCallback(RCL_Command *cmd,
                     LRF_Events lrfEvents,
                     RCL_Events events)
{
  if (llState != LL_STATE_SCAN)
  {
    MAP_llHaltRadio( (uint32)cmd );
    LL_rclRescheduleCommand(cmd);
    return;
  }
  //////////////////////////////////////////////////////////////////////////////
  // Command Started
  //////////////////////////////////////////////////////////////////////////////
  if ( events.cmdStarted )
  {
    llCmdStartedEventHandle();
  }

  if ( events.rxEntryAvail )
  {
    //////////////////////////////////////////////////////////////////////////////
    // Rx Entry Done
    //////////////////////////////////////////////////////////////////////////////
    if (lrfEvents.rxOk)
    {
      LL_rclScanRxEntryDone();
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    MAP_llLastCmdDoneEventHandleStateScan();
  }

  return;
}

/*******************************************************************************
 * This is the common RCL callback used for scan command. This function is used
 * to check if the packet should be ignoed due to NPM mode restrictions or not.
 * If it does, remove the packet from the RX queue, otherwise, process the received advertising packet.
 */
void LL_rclScanRxEntryDone( void )
{
  RCL_Buffer_DataEntry *rxEntry;
  uint8 *pAdvPkt;
  uint8 rlIndex;
  uint8 peerAddrType = LL_INVALID_DEV_ADDR_TYPE;
  uint8 peerAddr[B_ADDR_LEN];

  RCL_MultiBuffer_ListInfo listInfo;
  RCL_MultiBuffer_ListInfo_init(&listInfo, &extScanParam.rxBuffers);
  rxEntry = RCL_MultiBuffer_RxEntry_next(&listInfo);
  pAdvPkt = (uint8 *)&rxEntry->data[2];

  // copy addresses so there's no race condition or overwrite
  MAP_osal_memcpy( peerAddr, &pAdvPkt[LL_PKT_HDR_LEN], B_ADDR_LEN );

  // copy address types
  peerAddrType = LL_ADV_HDR_GET_TX_ADD(*pAdvPkt);

  //  According to the spec Core5.4 Vol6 Part B 4.7 RESOLVING LIST -
  //  when network privacy mode is used: the Controller shall only
  //  accept resolvable private addresses generated by the peer
  //  device using its distributed IRK.
  //  Relevant Test: LL/DDI/SCN/BV-26-C
  if ( !MAP_LL_PRIV_IsRPA( peerAddrType, peerAddr ))
  {
      rlIndex = MAP_LL_PRIV_FindPeerInRL(resolvingList, peerAddrType, peerAddr);

      // check if the Peer's ID address was found in the RL && Peer in NPM mode
      if ((rlIndex != INVALID_RESOLVE_LIST_INDEX) &&
          (resolvingList[rlIndex].privMode == LL_NETWORK_PRIVACY_MODE) &&
          (!MAP_LL_PRIV_IsZeroIRK( resolvingList[rlIndex].IRK)))
      {
          // This advertising report should be ignored, so remove the packet from the RX queue
          rxEntry = RCL_MultiBuffer_RxEntry_get(&extInitParam.rxBuffers, NULL);
          return;
      }
  }
  MAP_llProcessExtScanRxFIFO();
  return;
}
#endif // (CTRL_CONFIG & SCAN_CFG)

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * This is the common RCL callback used for init command.
 *
 */
void LL_rclInitCallback(RCL_Command *cmd,
                     LRF_Events lrfEvents,
                     RCL_Events events)
{
  if (llState != LL_STATE_INIT)
  {
    MAP_llHaltRadio( (uint32)cmd );
    LL_rclRescheduleCommand(cmd);
    return;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Rx Entry Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.rxEntryAvail )
  {
    if ( lrfEvents.rxOk )
    {
      LL_rclInitRxEntryDone();
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    MAP_llLastCmdDoneEventHandleStateInit();
  }

  return;
}

/*******************************************************************************
 * This is the common RCL callback used for init command. This function is used
 * to resolve the RPA addresses received as part of an advertising report
 *
 */
void LL_rclInitRxEntryDone( void )
{
  RCL_Buffer_DataEntry *rxEntry;
  uint8 *pAdvPkt;
  uint8 peerAddrType = LL_INVALID_DEV_ADDR_TYPE;
  uint8 peerAddr[B_ADDR_LEN];
  uint8 ownAddrType = LL_INVALID_DEV_ADDR_TYPE;
  uint8 ownAddr[B_ADDR_LEN];

  RCL_MultiBuffer_ListInfo listInfo;
  RCL_MultiBuffer_ListInfo_init(&listInfo, &extInitParam.rxBuffers);
  rxEntry = RCL_MultiBuffer_RxEntry_next(&listInfo);
  pAdvPkt = (uint8 *)&rxEntry->data[2];

  RCL_Ble5_RxPktStatus rclStatus = RCL_BLE5_getRxStatus(rxEntry);
  if ( rclStatus.ignoredRpa )
  {
    // This is an ignored advertising report and thus not containing the advertising
    // report a connect_ind was sent on so remove the packet from the RX queue
    rxEntry = RCL_MultiBuffer_RxEntry_get(&extInitParam.rxBuffers, NULL);
    MAP_osal_memcpy( peerAddr, &pAdvPkt[LL_PKT_HDR_LEN], B_ADDR_LEN );
    peerAddrType = LL_ADV_HDR_GET_TX_ADD(*pAdvPkt);

    if ( LL_ADV_IND_PDU( *pAdvPkt ) ||
         LL_ADV_DIRECT_IND_PDU( *pAdvPkt ) )
    {
      // check if there is an InitA address
      if ( LL_ADV_DIRECT_IND_PDU( *pAdvPkt ) )
      {
        // copy addresses so there's no race condition or overwrite
        MAP_osal_memcpy( ownAddr, &pAdvPkt[LL_PKT_HDR_LEN+B_ADDR_LEN], B_ADDR_LEN );

        // copy address types
        ownAddrType = LL_ADV_HDR_GET_RX_ADD( *pAdvPkt );
      }
    }
    else // not a packet we're interested in
    {
      return;
    }

    // check if the own address type is valid
    if ( ownAddrType != LL_INVALID_DEV_ADDR_TYPE )
    {
      // check if the InitA is an RPA
      if ( MAP_LL_PRIV_IsRPA( ownAddrType, ownAddr ) )
      {
        // check if Host specified addr type as Identity with a valid IRK
        // Note: If the Host specified Own address type of 0 | 1, or
        //       2 | 3 with an invalid Local IRK, then PHY address is
        //       set in Create Connection, and should not be changed!
        //       So unless the Rx Ignore is due to an RPA when our
        //       Own address type is 2 | 3 with a valid Local IRK, we
        //       should reject this packet.
        if ( LL_IS_ADDR_TYPE_RPA(extInitInfo->ownAddrType) &&
             (!MAP_LL_PRIV_IsZeroIRK( resolvingList[LOCAL_RL_INDEX].IRK)) )
        {
          // check if InitA resolves against the Local IRK
          // Note: The idea here is that the Rx Ignored interrupt was
          //       generated because InitA RPA didn't match our address.
          if ( MAP_LL_PRIV_ResolveRPA( ownAddr, resolvingList[LOCAL_RL_INDEX].IRK ) )
          {
            //NOTE: this section which is copying the OwnA into the LOCAL ADDRESS
            //      is left for the case customer will need backwards compatability
            //      other wise it's not used.
            // update pointer to our device address and set own addr type
            // Note: initParam.pDeviceAddr points to initInfo->ownAddr.
            MAP_osal_memcpy( extInitInfo->ownAddr, ownAddr, B_ADDR_LEN );
            //SETVAR_INIT_CFG_DEV_ADDR_TYPE( extInitParam.initCfg, LL_DEV_ADDR_TYPE_RANDOM_ID );
            // update RL RPA for this peer AdvA
            MAP_osal_memcpy( resolvingList[LOCAL_RL_INDEX].RPA, ownAddr, B_ADDR_LEN );
          }
          else // InitA RPA failed to resolve
          {
            return;
          }
        }
        else // InitA is RPA with no key, so reject
        {
          return;
        }
      }
      else // not an RPA, but is it our own address?
      {
        // Per Vol 6, Part B, Section 6.4, if the Host specifies
        // to use RPA (i.e. Own Address Type of 2 or 3), then the Init
        // shall not respond to Directed Connectable Adv events that
        // contain Public or Static address as the TargetA.
        // Note: Exception made to rejection clause: If the Local IRK is
        //       invalid, we will accept Public/Static address as the
        //       TargetA if it matches our device.

        // In both of these cases, our PHY device address is set and won't
        // by changed, so the CM0 will either match if it is our address
        // or it will reject if it is not. The problem is, we could be in
        // this Rx Ignore because the TargetA was rejected, and in that
        // case, we don't want to process the PeerA. On the other hand,
        // the PHY might have accept the TargetA, and in that case, we
        // don't want to reject the TargetA. So we are basically checking
        // here whether the CM0 accepted the TargetA or not. We can do this
        // using our address type and IRK. If we don't meet the rules in
        // the previous paragraph, we reject this non-RPA TargetA.

        if ( !((LL_IS_ADDR_TYPE_RPA(extInitInfo->ownAddrType) &&
                MAP_LL_PRIV_IsZeroIRK(resolvingList[LOCAL_RL_INDEX].IRK)) ||
                LL_IS_ADDR_IDENTITY_TYPE(extInitInfo->ownAddrType)) )
        {
          return;
        }
      }
    }

    // check if the peer address type is valid
    if ( peerAddrType != LL_INVALID_DEV_ADDR_TYPE )
    {
      // check if the peer is an RPA
      if ( MAP_LL_PRIV_IsRPA( peerAddrType, peerAddr ) )
      {
        uint8 rlIndex = MAP_LL_PRIV_IsResolvable( peerAddr, resolvingList );

        // see if the Peer Address is resolvable
        // Note: The idea here is that the Rx Ignored interrupt was
        //       generated because the peer RPA expired.
        if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
        {
          // check if filter policy is to use Peer address
          if ( extInitInfo->pCreateConn->initFilterPolicy == LL_INIT_AL_POLICY_USE_PEER_ADDR )
          {
            llConnState_t *connPtr = MAP_llDataGetConnPtr( extInitInfo->connId );

            // Check if the peer address is RPA is so, update the command, connection pointer,
            // and the resolving list with the new RPA given a valid RL index
            llCheckPeerAddrTypeAndUpdate( connPtr, (uint8 *)extInitParam.peerA, peerAddr, rlIndex );
          }
          else // initInfo->alPolicy == LL_INIT_AL_POLICY_USE_ACCEPT_LIST
          {
            // The command is running, the RCL should update the accept list
            // otherwise, the RCL cannot use the changes in t he AL until
            // submitting a new command
            uint16 flags = 0;

            // Set the ignore random, and busy flag on the new
            SET_AL_ENTRY_RANDOM( flags );
            SET_AL_ENTRY_BUSY( flags );

            LL_rclUpdateExtAl( extInitParam.filterList, flags, peerAddr, rlIndex );
          }
        }
        else // failed to resolve RPA
        {
          // check if filter policy is to use PEER
          // Note: If filter policy is to use AL, unresolved RPA is rejected.
          if ( extInitInfo->pCreateConn->initFilterPolicy == LL_INIT_AL_POLICY_USE_PEER_ADDR )
          {
            llConnState_t *connPtr = MAP_llDataGetConnPtr( extInitInfo->connId );
            // check if received address matches Host provided peer address
            if ( (peerAddrType == extInitInfo->pCreateConn->peerAddrType) &&
                 MAP_osal_memcmp( peerAddr, extInitInfo->pCreateConn->peerAddr, B_ADDR_LEN ) )
            {
              // update hardware peer address
              // Note: The initParam.pAcceptList is pointing to
              //       connExt->peerInfo.peerAddr.
              MAP_osal_memcpy( connPtr->peerInfo.peerAddr,
                               peerAddr,
                               B_ADDR_LEN );
            }
          }
        }
      }
      else // not an RPA
      {
        // Note: Could be a Peer ID that is in the RL with an invalid IRK, or
        //       in the RL with a valid IRK but using Device Privacy Mode.

        // check if filter policy is to use Peer address
        if ( extInitInfo->pCreateConn->initFilterPolicy == LL_INIT_AL_POLICY_USE_PEER_ADDR )
        {
          llConnState_t *connPtr = MAP_llDataGetConnPtr( extInitInfo->connId );

          // check if the input parameter is public ID or random ID
          if ( LL_IS_ADDR_TYPE_RPA(connPtr->peerInfo.peerAddrType) )
          {
            uint8 rlIndex = MAP_LL_PRIV_FindPeerInRL( resolvingList,
                                                      peerAddrType,
                                                      peerAddr );

            // check if the Peer's ID address was found in the RL
            if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
            {
              // accept peer ID for Device Privacy Mode or Invalid IRK
              if ( (resolvingList[rlIndex].privMode == LL_DEVICE_PRIVACY_MODE) ||
                   MAP_LL_PRIV_IsZeroIRK( resolvingList[rlIndex].IRK ) )
              {
                // accept only if peer address is as in the requested connect peer id
                if ( MAP_osal_memcmp( resolvingList[rlIndex].idAddr, extInitInfo->pCreateConn->peerAddr, B_ADDR_LEN ) )
                {
                  // update hardware peer address
                  // Note: The initParam.pAcceptList is pointing to
                  //       connExt->peerInfo.peerAddr.
                  MAP_osal_memcpy( connPtr->peerInfo.peerAddr,
                                   peerAddr,
                                   B_ADDR_LEN );
                }
              }
            }
          }
        }
      }
    }
  }
}
#endif // (CTRL_CONFIG & INIT_CFG)

/*******************************************************************************
 * This is the common RCL callback used for peripheral command.
 *
 */
void LL_rclPeripheralCallback(RCL_Command *cmd,
                     LRF_Events lrfEvents,
                     RCL_Events events)
{
  if (llState != LL_STATE_CONN_PERIPHERAL)
  {
    MAP_llHaltRadio( (uint32)cmd );
    LL_rclRescheduleCommand(cmd);
    return;
  }
  //////////////////////////////////////////////////////////////////////////////
  // Tx_Entry_Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.txBufferFinished )
  {
    // get connection information
    llConnState_t *connPtr = MAP_llDataGetConnPtr( llConns.currentConn );

    MAP_llProcessTxData( connPtr, LL_TX_DATA_CONTEXT_TX_ISR );
  }

  //////////////////////////////////////////////////////////////////////////////
  // Rx Entry Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.rxEntryAvail )
  {
    if (lrfEvents.rxOk)
    {
      MAP_llRxEntryDoneEventHandleStateConnection( FALSE );
    }
    else if (lrfEvents.rxNok)
    {
      MAP_llRxEntryDoneEventHandleStateConnection( TRUE );
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    MAP_llLastCmdDoneEventHandleStatePeripheral();
  }

  return;
}

/*******************************************************************************
 * This is the common RCL callback used for central command.
 *
 */
void LL_rclCentralCallback(RCL_Command *cmd,
                       LRF_Events lrfEvents,
                       RCL_Events events)
{
  if (llState != LL_STATE_CONN_CENTRAL)
  {
    MAP_llHaltRadio( (uint32)cmd );
    LL_rclRescheduleCommand(cmd);
    return;
  }
  //////////////////////////////////////////////////////////////////////////////
  // Tx_Entry_Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.txBufferFinished )
  {
    // get connection information
    llConnState_t *connPtr = MAP_llDataGetConnPtr( llConns.currentConn );

    MAP_llProcessTxData( connPtr, LL_TX_DATA_CONTEXT_TX_ISR );
  }

  //////////////////////////////////////////////////////////////////////////////
  // Rx Entry Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.rxEntryAvail )
  {
    if (lrfEvents.rxOk)
    {
      MAP_llRxEntryDoneEventHandleStateConnection( FALSE );
    }
    else if (lrfEvents.rxNok)
    {
      MAP_llRxEntryDoneEventHandleStateConnection( TRUE );
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    MAP_llLastCmdDoneEventHandleStateCentral();
  }

  return;
}

/*******************************************************************************
 * This is the common RCL callback used for DTM TX command and
 * RX command
 *
 */
void LL_rclTestCallback(RCL_Command *cmd,
                     LRF_Events lrfEvents,
                     RCL_Events events)
{
  if ((llState != LL_STATE_DIRECT_TEST_MODE_TX) &&
      (llState != LL_STATE_DIRECT_TEST_MODE_RX) &&
      (llState != LL_STATE_MODEM_TEST_TX)       &&
      (llState != LL_STATE_MODEM_TEST_RX))
  {
    MAP_llHaltRadio( (uint32)cmd );
    LL_rclRescheduleCommand(cmd);
    return;
  }
  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    MAP_llLastCmdDoneEventHandleStateTest();
  }

  return;
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * @fn          llCheckPeerAddrTypeAndUpdate
 *
 * @brief       This function will will update the connection pointer, the command
 *              received, and the resolving list with a RPA address
 *
 * @param       connPtr    - Connection pointer to update
 *              cmdDevAddr - The pointer to the peer address in the command
 *              peerAddr   - The new RPA address
 *              rlIndex    - The index of the peer device entry in the resolving
 *                           list
 *
 * @return      None.
 */
static void llCheckPeerAddrTypeAndUpdate(llConnState_t *connPtr, uint8 *cmdDevAddr, uint8 *peerAddr, uint8 rlIndex)
{
  if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
  {

    // check if Host specified peer address as an Identity Address
    // Note: If the Host specified Peer address type of 0 | 1, or
    //       2 | 3 with an invalid Peer IRK, then the PHY will be
    //       set in Create Connection with no additional checks
    //       needed here for IDA or NRPA. If we get an RPA, we
    //       still have to check it so we can either update the
    //       PHY or the Extended AL for the next advertisement.
    if ( LL_IS_ADDR_TYPE_RPA(connPtr->peerInfo.peerAddrType) )
    {
      // check if the Host's parameters match what's received
      // also check if the internal peer address is invalid
      // Note: The internal peer address is set to invalid when
      //       the Host set's the Init Filter Policy to PEER, and
      //       the Peer's ID in the RL has a valid IRK and is using
      //       Network Privacy Mode. This is done to ensure the
      //       Peer's ID is not accepted.
      if ( resolvingList[rlIndex].idAddrType == MASK_ID_ADDRTYPE(connPtr->peerInfo.peerAddrType) )
      {
        // accept only if peer address is as in the requested connect peer id
        if ( MAP_osal_memcmp( resolvingList[rlIndex].idAddr, extInitInfo->pCreateConn->peerAddr, B_ADDR_LEN ) )
        {
          // update hardware peer address
          // Note: The initParam.pWhiteList is pointing to
          //       connPtr->peerInfo.peerAddr.
          MAP_osal_memcpy( connPtr->peerInfo.peerAddr, peerAddr, B_ADDR_LEN );

          // Update the RPA in the initiator command
          MAP_osal_memcpy( cmdDevAddr, peerAddr, B_ADDR_LEN );

          // update RL RPA for this peer AdvA
          MAP_osal_memcpy( resolvingList[rlIndex].RPA, peerAddr, B_ADDR_LEN );

#ifdef RCL_329
          /////////////////////////////////////////////////////////////////////////////
          // Temporary - Until RCL will provide a fix (an API) which allows to update
          // peer device RPA address while the command is running,
          // LL_INIT_AL_POLICY_USE_PEER_ADDR will use Accept list as well in order to
          // update the peer RPA
          ////////////////////////////////////////////////////////////////////////////
          uint16 flags = 0;

          // Set the ignore random, and busy flag on the new
          SET_AL_ENTRY_RANDOM( flags );
          SET_AL_ENTRY_BUSY( flags );

          // Prepare and update the RPA in the RCL Accept List
          MAP_llRclPrepareAndUpdateAlEntry(extInitParam.filterList, flags, resolvingList[rlIndex].RPA, RCL_PEER_ADDR_INDEX);
#endif // RCL_329
        }
      }
    }
  }
}

/*******************************************************************************
 * @fn          LL_rclUpdateExtAl
 *
 * @brief       This function will use the RCL command for updating the accept
 *              list while a command is running.
 *              This function will also update the resolving list with the new
 *              RPA address
 *
 * @param       filterList - Pointer to the filter list in use
 *              flags      - The accept list flags that needed to be marked
 *              rpaAddr    - The new RPA address
 *              rlIndex    - The index of the peer device entry in the resolving
 *                           list
 *
 * @return      None.
 */
void LL_rclUpdateExtAl( RCL_FilterList *filterList, uint16 flags, uint8* rpaAddr, uint8 rlIndex)
{
  alTable_t *pAlTable;

  pAlTable = GET_AL_TABLE_POINTER(filterList);

  if ( MAP_AL_FindEntry( pAlTable,
                         resolvingList[rlIndex].idAddr,
                         resolvingList[rlIndex].idAddrType ) != pAlTable->numAlEntries )
  {
    // Find the record in the extended accept list using the identity address
    // since LL_PRIV_CheckRLPeerIdEntry adds the identity to the extended AL
    // when LL_PRIV_SetupPrivacy is called
    uint8 alIndex = MAP_LL_PRIV_FindExtALEntry( pAlTable,
                                                resolvingList[rlIndex].idAddr,
                                                resolvingList[rlIndex].idAddrType );

    if ( alIndex == INVALID_EXT_ACCEPT_LIST_INDEX )
    {
      // This device RPA is not in the extended yet. Find the first empty index
      alIndex = MAP_LL_PRIV_FindEmptyExtALEntry(pAlTable);
    }

    // Update the RPA in the resolving list
    MAP_osal_memcpy(resolvingList[rlIndex].RPA, rpaAddr, B_ADDR_LEN);

    if( alIndex != INVALID_EXT_ACCEPT_LIST_INDEX )
    {
      // Prepare and update the RPA in the RCL Extended Accept List
      MAP_llRclPrepareAndUpdateAlEntry(filterList, flags, resolvingList[rlIndex].RPA, alIndex);
    }
  }
}
#endif // CTRL_CONFIG & INIT_CFG

#else // USE_RCL
/*
** CC26xx BLE RF Driver Callback
*/

/*******************************************************************************
 * This is the common RF Power Up callback used whenever the device wakes.
 *
 * Public function defined in RF.h.
 */
void rfPUpCallback( RF_Handle    rfHandle,
                    RF_CmdHandle cmdHandle,
                    RF_EventMask events )
{


#ifdef CC33xx
    GTRACE(GRP_BLE_DBG,"rfPUpCallback")
    if ( rfHandle != NULL )
    {
#define CMD_ENABLE_DBG_CONFIG   (  (0<<14) /* Prescaler (0 -> divide by 1 -> 24 Mhz)                */\
                                 | (1<<12) /* Timestamp enable                                      */\
                                 | (1<<11) /* Channel 3 mode (1: enabled, 0: disabled)              */\
                                 | (2<<9)  /* Channel 2 mode (2: backdoor, 1: enabled, 0: disabled) */\
                                 | (1<<8)  /* Channel 1 mode (1: enabled, 0: disabled)              */\
                                 | (1<<6)) /* Systick channel (0: disabled, 1-3: channel)           */
      RF_Stat rfStat = RF_runDirectCmd( rfHandle, BUILD_DIRECT_PARAM_EXT_CMD( CMD_ENABLE_DEBUG, CMD_ENABLE_DBG_CONFIG ) );

      if ( (rfStat != RF_StatSuccess) && (rfStat != RF_StatCmdDoneSuccess) )
      {
          GTRACE(GRP_BLE_ERROR,"unable to enable debug")
          ASSERT_GENERAL(0);

      }
    }
#else
#ifdef DEBUG_SW_TRACE
  // enable RF trace output for FPGA
  // Note: While doing this here saves the trouble of hacking the RF Driver to
  //       do it, if the CM0 is allowed to power off, a fault is still likely
  //       to occur when a CM3 only operation tries to touch the Tracer HW. The
  //       only way to prevent this is to keep the CM0 always powered on. But
  //       presently, there's no way to do this without another hack to the
  //       RF Driver.
  if ( rfHandle != NULL )
  {
    RF_Stat rfStat = RF_runDirectCmd( rfHandle, BUILD_DIRECT_PARAM_EXT_CMD( CMD_ENABLE_DEBUG, 0x1D40 ) );

    if ( (rfStat != RF_StatSuccess) && (rfStat != RF_StatCmdDoneSuccess) )
    {
      volatile uint8 i = 1;
      while(i);
    }
  }

  DBG_PRINT0(DBGSYS, "");
  DBG_PRINTL1(DBGSYS, "Power Up RAT = 0x%08X", MAP_llGetCurrentTime() );
  DBG_PRINT0(DBGSYS, "");
#endif // DEBUG_SW_TRACE

  // Note: This is a temporary workaround for CC26xxR2, which has values backwards.
  *((volatile uint16 *)CM0_RAM_CA0_CA1_OFFSET_ADDR) = LL_AUX_PTR_CA0_CA1;
#endif // !CC33xx
  return;
}


/*******************************************************************************
 * This is the common RF Error callback used whenever the RF Driver raises
 * this error. Note that this callback is first trapped by the Controller,
 * and then passed on to the user's callback function, as defined in llConfig.
 *
 * Public function defined in RF.h.
 */
void rfErrorCallback( RF_Handle    rfHandle,
                      RF_CmdHandle cmdHandle,
                      RF_EventMask events )
{
  // check if this is an RF Error Callback that the Controller needs to first
  // handle
  // ...
  // report failure to Host
  //MAP_llHardwareError( HW_FAIL_RF_DRIVER_ERROR );

  // invoke the user's Error Callback
  (*(llUserConfig.pErrCb))(rfHandle, cmdHandle, events);

  return;
}

/*******************************************************************************
 * This is the common RF callback used for all RF driver commands. Currently,
 * it will be used for all CPE0 and CPE1 interrupts.
 *
 * Public function defined in RF.h.
 */
void rfCallback( RF_Handle    rfHandle,
                 RF_CmdHandle cmdHandle,
                 RF_EventMask events )
{
  //////////////////////////////////////////////////////////////////////////////
  // Tx_Entry_Done
  // Note: Assumed to only be for Connection state.
  //////////////////////////////////////////////////////////////////////////////
  if ( events & RF_EventTxDone )
  {
    LL_TxDoneCback();
  }

  //////////////////////////////////////////////////////////////////////////////
  // Tx_Entry_Done
  // Note: Assumed to only be for Connection state.
  //////////////////////////////////////////////////////////////////////////////
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  if ( events & (uint64_t)RF_EventTxEntryDone )
  {
    MAP_LL_TxEntryDoneCback();
  }
#endif // (ADV_CONN_CFG | INIT_CFG)

  //////////////////////////////////////////////////////////////////////////////
  // Rx Ignored
  // Note: Assumes only be for Adv, Scan, and Init state.
  // Note: Assumes only generated if Address Resolution is enabled.
  //////////////////////////////////////////////////////////////////////////////
  if ( events & RF_EventRxIgnored )
  {
    LL_RxIgnoredCback();

#if defined(BLE_VS_FEATURES) && (BLE_VS_FEATURES & SCAN_REQ_RPT_CFG) &&        \
    defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
    // ensure no RxEmpty packet processing
    events &= ~(uint64_t)RF_EventRxEmpty;
#endif // SCAN_REQ_RPT_CFG & (ADV_NCONN_CFG | ADV_CONN_CFG)
  }

  //////////////////////////////////////////////////////////////////////////////
  // Rx Empty
  // Note: Assumed to only be for Scan.
  //////////////////////////////////////////////////////////////////////////////
#if defined(BLE_VS_FEATURES) && (BLE_VS_FEATURES & SCAN_REQ_RPT_CFG) &&        \
    defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  if ( events & RF_EventRxEmpty )
  {
    LL_RxEmptyCback();
  }
#endif // SCAN_REQ_RPT_CFG & (ADV_NCONN_CFG | ADV_CONN_CFG)

  //////////////////////////////////////////////////////////////////////////////
  // Rx Entry Done
  // Note: Assumed to only be for Scan and Connection state.
  //////////////////////////////////////////////////////////////////////////////
  if ( events & RF_EventRxEntryDone )
  {
    LL_RxEntryDoneCback_allEntries( FALSE );
  }
  // Case received packet with CRC error
  else if ( events & RF_EventRxNOk )
  {
    LL_RxEntryDoneCback( TRUE );
  }

  //////////////////////////////////////////////////////////////////////////////
  // Command Done or Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( (events & RF_EventCmdDone) || (events & RF_EventLastCmdDone) )
  {
    LL_LastCmdDoneCback();
  }

  //////////////////////////////////////////////////////////////////////////////
  // IQ samples ready
  //////////////////////////////////////////////////////////////////////////////
#ifdef RTLS_CTE
  if ( events & (uint32)RF_EventSamplesEntryDone )
  {
    llCteSamples.autoCopyCompleted ++;
  }
#endif // RTLS_CTE

  //////////////////////////////////////////////////////////////////////////////
  // Doorbell Error
  //////////////////////////////////////////////////////////////////////////////
  if ( events & (uint32)RF_EventInternalError )
  {
    LL_DoorbellErrorCback();
  }

  //////////////////////////////////////////////////////////////////////////////
  // Cancelled, Aborted, Stopped, and/or Preempted
  //////////////////////////////////////////////////////////////////////////////
  if ( events & ( RF_EventCmdCancelled | RF_EventCmdAborted |
                  RF_EventCmdStopped | RF_EventCmdPreempted ))
  {
    LL_AbortedCback((events & (RF_EventCmdPreempted|RF_EventCmdAborted)) ? TRUE : FALSE);
  }

  return;
}

/*
** CC26xx BLE RF Driver Callback Functions
*/
////////////////////////////////////////////////////////////////////////////////
// Command Cancelled, Aborted, Stopped, and/or Preempted
////////////////////////////////////////////////////////////////////////////////
uint32_t LL_AbortedCback( uint8 preempted )
{
  // disable RAT channel
  MAP_llClearRatCompare();

  // determine which radio task just ended
  switch( llState )
  {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
    case LL_STATE_EXT_ADV:
      MAP_llAbortEventHandleStateAdv(preempted);
      break;
#ifdef USE_PERIODIC_ADV
    case LL_STATE_PERIODIC_ADV:
      if ((preempted) || (llGetRfCmdPreemptionEnable()))
      {
        taskEndAction = MAP_llPeriodicAdv_PostProcess;
        // process RF End Cause
        (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
      }
      break;
#endif
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
#ifdef USE_PERIODIC_SCAN
    case LL_STATE_PERIODIC_SCAN:
      if ((preempted) || (llGetRfCmdPreemptionEnable()))
      {
        taskEndAction = MAP_llPeriodicScan_PostProcess;

        // process RF End Cause
        (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
      }
      break;
#endif
    case LL_STATE_SCAN:
      MAP_llAbortEventHandleStateScan(preempted);
      break;
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
    case LL_STATE_INIT:
      MAP_llAbortEventHandleStateInit(preempted);
      break;
#endif // INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
    case LL_STATE_CONN_PERIPHERAL:
      MAP_llAbortEventHandleStatePeripheral(preempted);
      break;
#endif // ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
    case LL_STATE_CONN_CENTRAL:
      MAP_llAbortEventHandleStateCentral(preempted);
      break;
#endif // INIT_CFG

    default:
      return 0;  // case not found/handled
  }
  return 1; // case handled
}

////////////////////////////////////////////////////////////////////////////////
// Doorbell Internal Error
////////////////////////////////////////////////////////////////////////////////
void LL_DoorbellErrorCback( void )
{
    LL_ASSERT( FALSE );

    // report failure to Host
    MAP_llHardwareError( HW_FAIL_FW_INTERNAL_ERROR );

    // need to reset the radio; for now, this is fatal
    MAP_llResetRadio();

    return;
}

////////////////////////////////////////////////////////////////////////////////
// Command Done or Last Command Done
////////////////////////////////////////////////////////////////////////////////
uint32_t LL_LastCmdDoneCback( void )
{
  // determine which radio task just ended
  switch( llState )
  {
    case LL_STATE_IDLE:
      break;

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
    case LL_STATE_EXT_ADV:
      MAP_llLastCmdDoneEventHandleStateAdv();
      break;
#ifdef USE_PERIODIC_ADV
    case LL_STATE_PERIODIC_ADV:
    {
      llPeriodicAdvSet_t *pPeriodicAdv = MAP_llGetCurrentPeriodicAdv();

      if (pPeriodicAdv == NULL)
      {
        break;
      }
      taskEndStatus = pPeriodicAdv->rfCmd.rfOpCmd.status;
      taskEndAction = MAP_llPeriodicAdv_PostProcess;
      // process RF End Cause
      (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
    }
    break;
#endif
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
    case LL_STATE_SCAN:
      MAP_llLastCmdDoneEventHandleStateScan();
      break;

#ifdef USE_PERIODIC_SCAN
    case LL_STATE_PERIODIC_SCAN:
    {
      llPeriodicScanSet_t *pPeriodicScan = MAP_llGetCurrentPeriodicScan(PERIODIC_SCAN_STATE_SYNCED);

      if (pPeriodicScan == NULL)
      {
        break;
      }
      do
      {
        taskEndStatus = pPeriodicScan->rfCmd.rfOpCmd.status;
      } while ((taskEndStatus & 0xFF00) == 0);

      taskEndAction = MAP_llPeriodicScan_PostProcess;
      // determine an action, if any
      switch( taskEndStatus )
      {
        case BLESTAT_IDLE:
        case BLESTAT_PENDING:
        case BLESTAT_ACTIVE:
        case BLESTAT_ERROR_PAR:
          // Sanity Check:
          // This is a fatal error as either the status doesn't make any
          // sense (in the Idle, Pending, or Active case), or we have a
          // status that should not have occurred:
          // - Bad Parameter (i.e. a programming error)
          // - OK (only valid when bEndOnRpt is enabled; not used)
          // - Rx Error (only valid when bEndOnRpt is enabled; not used)
          // - No Synch (only valid when bEndOnRpt is enabled; not used)
          //LL_ASSERT( FALSE );

          // report failure to Host
          MAP_llHardwareError( HW_FAIL_UNEXPECTED_RF_STATUS );

          break;
      }
      // process RF End Cause
      (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
    }
    break;
#endif
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
    case LL_STATE_INIT:
      MAP_llLastCmdDoneEventHandleStateInit();
      break;
#endif // INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
    case LL_STATE_CONN_PERIPHERAL:
      MAP_llLastCmdDoneEventHandleStatePeripheral();
      break;
#endif // ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
    case LL_STATE_CONN_CENTRAL:
      MAP_llLastCmdDoneEventHandleStateCentral();
      break;
#endif // INIT_CFG

    case LL_STATE_DIRECT_TEST_MODE_TX:
    case LL_STATE_DIRECT_TEST_MODE_RX:
    case LL_STATE_MODEM_TEST_TX:
    case LL_STATE_MODEM_TEST_RX:
    case LL_STATE_MODEM_TEST_TX_FREQ_HOPPING:
      MAP_llLastCmdDoneEventHandleStateTest();
      break;

    default:
      // Sanity Check:
      // This is a fatal error as either the status doesn't make any
      // sense (in the Idle, Pending, or Active case), or an unspecified
      // status was returned (in all other cases)!
      LL_ASSERT( FALSE );

      // report failure to Host
      MAP_llHardwareError( HW_FAIL_UNKNOWN_RF_STATUS );

      break;
  }

  return 1;
}

////////////////////////////////////////////////////////////////////////////////
// Tx Done
// Note: Assumed to only be for Connection state.
////////////////////////////////////////////////////////////////////////////////
void LL_TxDoneCback( void )
{
  switch( llState )
  {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
    case LL_STATE_EXT_ADV:
      MAP_llTxDoneEventHandleStateAdv();
    break;

#ifdef USE_PERIODIC_ADV
    case LL_STATE_PERIODIC_ADV:
    {
      MAP_llUpdatePeriodicAdvChainPacket();
    }
    break;
#endif
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
    // Assume rest is Connection related.
    default:
#if defined( TEST_MODE_DTM )
      GPIO_writeDio(HAL_GPIO_1, 1);
#endif // TEST_MODE_DTM

      if ( llState == LL_STATE_DIRECT_TEST_MODE_TX )
      {
#if defined( TEST_MODE_DTM )
        // toggle GPIO
        GPIO_writeDio(HAL_GPIO_1, 0);
#endif // TEST_MODE_DTM
      }
      break;
  }

  return;
}


////////////////////////////////////////////////////////////////////////////////
// Tx Entry Done
// Note: Assumed to only be for Connection state.
////////////////////////////////////////////////////////////////////////////////
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
void LL_TxEntryDoneCback( void )
{
  llConnState_t *connPtr;

  // check if the connection is still valid
  if ( llConns.currentConn == LL_INVALID_CONNECTION_ID )
  {
    // connection may have already been ended by a terminate or reset
    return;
  }

  // get connection information
  connPtr = MAP_llDataGetConnPtr( llConns.currentConn );

  MAP_llProcessTxData( connPtr, LL_TX_DATA_CONTEXT_TX_ISR );

  return;
}
#endif // (ADV_CONN_CFG | INIT_CFG)

////////////////////////////////////////////////////////////////////////////////
// Rx Ignored
// Note: Assumes only be for Adv, Scan, and Init state.
// Note: Assumes only generated if Address Resolution is enabled.
////////////////////////////////////////////////////////////////////////////////
uint32_t LL_RxIgnoredCback( void )
{
  // action based on link layer state
  switch( llState )
  {
    case LL_STATE_IDLE:
    break;

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
    case LL_STATE_EXT_ADV:
      MAP_llRxIgnoreEventHandleStateAdv();
      break;
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
    case LL_STATE_SCAN:
      MAP_llRxIgnoreEventHandleStateScan();
      break;
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
    case LL_STATE_INIT:
      MAP_llRxIgnoreEventHandleStateInit();
      break;
#endif // INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
    case LL_STATE_CONN_PERIPHERAL:
    case LL_STATE_CONN_CENTRAL:
      break;
#endif // ADV_CONN_CFG | INIT_CFG

    default:
      // Unexpected BLE state!
      LL_ASSERT( FALSE );

      // report failure to Host
      MAP_llHardwareError( HW_FAIL_UNKNOWN_LL_STATE );

      break;
  } // switch(llState)

  return 1;
}

////////////////////////////////////////////////////////////////////////////////
// Rx Empty
// Note: Assumed to only be for Scan Request.
////////////////////////////////////////////////////////////////////////////////
#if defined(BLE_VS_FEATURES) && (BLE_VS_FEATURES & SCAN_REQ_RPT_CFG) && defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
void LL_RxEmptyCback( void )
{
  // action based on link layer state
  switch( llState )
  {
    case LL_STATE_IDLE:
      break;

    case LL_STATE_EXT_ADV:
      MAP_llRxEmptyEventHandleStateAdv();
    break;

    default:
      break;
  }

  return;
}
#endif // SCAN_REQ_RPT_CFG & (ADV_NCONN_CFG | ADV_CONN_CFG)


////////////////////////////////////////////////////////////////////////////////
// Find number of ready entries with status FINISHED
// Note: Assumed for all states.
////////////////////////////////////////////////////////////////////////////////
uint8 getNumFinishedEntries( dataEntryQ_t *pRxQ )
{
  dataEntry_t *pStart;
  dataEntry_t *pNext;
  uint8 numFinishedEntries = 0;

  // get current pointer to ring buffer
  pStart = pNext = pRxQ->pCurEntry;

  // disconnect the ring buffer to prevent CM0 processing
  pRxQ->pCurEntry = NULL;

  // count finished entries
  do
  {
    if (pNext->status == DATASTAT_FINISHED)
    {
      numFinishedEntries++;
    }

    // on to next buffer in ring
    pNext = pNext->pNextEntry;

    // check if we're back to where we started
  } while ( pNext != pStart);

  // restore current pointer to ring buffer
  pRxQ->pCurEntry = pStart;

  return numFinishedEntries;
}

////////////////////////////////////////////////////////////////////////////////
// Rx Entry Done - for all ready entries
// Note: Assumed for all states.
////////////////////////////////////////////////////////////////////////////////
void LL_RxEntryDoneCback_allEntries( uint8 crcError )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG | SCAN_CFG))
  uint8 numFinishedEntries;
  uint8 i;
#endif
  // Fix Coex issue, handle all FINISHED queue entries
  // Do it only for CONN/SCAN states
  switch( llState )
  {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
    case LL_STATE_CONN_CENTRAL:
    case LL_STATE_CONN_PERIPHERAL:
    {
      llConnState_t *connPtr;

      // check if the connection is still valid
      if ( llConns.currentConn == LL_INVALID_CONNECTION_ID )
      {
        // connection may have already been ended by a terminate or reset
        break;
      }

      // get connection information
      connPtr = MAP_llDataGetConnPtr( llConns.currentConn );

      numFinishedEntries = getNumFinishedEntries(connPtr->pRxDataEntryQ);
      BLE_LOG_INT_INT(0, BLE_LOG_MODULE_RF_CMD, "CONN: numFinishedEntries=0x%x, 0x%x\n", numFinishedEntries, 0);
      for (i=0; i<numFinishedEntries; i++)
      {
        LL_RxEntryDoneCback( crcError );
      }
      break;
    }
#endif
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  case LL_STATE_SCAN:
    {
      numFinishedEntries = getNumFinishedEntries(extScanParam.pRXQ);
      BLE_LOG_INT_INT(0, BLE_LOG_MODULE_RF_CMD, "SCNE: numFinishedEntries=0x%x, 0x%x\n", numFinishedEntries, 0);
      for (i=0; i<numFinishedEntries; i++)
      {
        LL_RxEntryDoneCback( crcError );
      }
      break;
    }
#endif // SCAN_CFG

  default:
      LL_RxEntryDoneCback( crcError );
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Rx Entry Done
// Note: Assumed for all states.
////////////////////////////////////////////////////////////////////////////////
void LL_RxEntryDoneCback( uint8 crcError )
{
  // action based on link layer state
  switch( llState )
  {
    case LL_STATE_IDLE:
    break;
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
    case LL_STATE_EXT_ADV:
      MAP_llRxEntryDoneEventHandleStateAdv();
    break;
#endif //ADV_NCONN_CFG | ADV_CONN_CFG
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
    // process Advertisment Report (ADV*_IND or SCAN_RSP)
    case LL_STATE_SCAN:
      // process RX FIFO data
      MAP_llProcessExtScanRxFIFO();
    break;

#ifdef USE_PERIODIC_SCAN
    case LL_STATE_PERIODIC_SCAN:
    {
      MAP_llProcessPeriodicScanRxFIFO();
    }
    break;
#endif
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
    case LL_STATE_INIT:
      MAP_llRxEntryDoneEventHandleStateInit();
    break;
#endif // INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
      // process received data or control packets in Connection
    case LL_STATE_CONN_CENTRAL:
    case LL_STATE_CONN_PERIPHERAL:
      MAP_llRxEntryDoneEventHandleStateConnection(crcError);
    break;
#endif // (ADV_CONN_CFG | INIT_CFG)
    case LL_STATE_DIRECT_TEST_MODE_RX:
      MAP_llRxEntryDoneEventHandleStateTest();
    break;
    default:
      // Unexpected BLE state!
      LL_ASSERT( FALSE );

      // report failure to Host
      MAP_llHardwareError( HW_FAIL_UNKNOWN_LL_STATE );

      break;
  }
}
#endif // USE_RCL
/*
** Local Functions for Adv state
*/
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
////////////////////////////////////////////////////////////////////////////////
// LastCmdDone Event Handle Connect Request
////////////////////////////////////////////////////////////////////////////////
uint8 llLastCmdDoneEventHandleConnectRequest( advSet_t *pAdvSet )
{
#ifdef USE_RCL
  if (((aeLegacyRf_t *)pAdvSet->pRfCmds)->advCmd.common.status == RCL_CommandStatus_Connect)
  {
    RCL_MultiBuffer_ListInfo listInfo;
    RCL_MultiBuffer_ListInfo_init(&listInfo, &((aeLegacyRf_t *)pAdvSet->pRfCmds)->advParam.rxBuffers);
    RCL_Buffer_DataEntry *rxEntry = RCL_MultiBuffer_RxEntry_next(&listInfo);
    uint8 *pData = (uint8 *)&rxEntry->data[2];
    uint8 rlIndexA;
    uint8 rlIndexB;
    uint8 peerAddrType;
    uint8 peerAddr[B_ADDR_LEN];
    uint8 *pPkt;

    // verify the connect indication
    if ( MAP_llConnExists(LL_TASK_ID_CENTRAL,&pData[LL_CONN_IND_INITIATOR_ADDRESS_OFFSET],
                          MASK_ID_ADDRTYPE(pData[LL_CONN_IND_HEADER_OFFSET] >> LL_ADV_PDU_HDR_TXADDR))  ||
         !llValidateConnectIndPkt( pData ) )
    {
      // invalid connect_ind - continue with advertising
      return FALSE;
    }

    // Connection indication is valid, continue
    pPkt = LL_GET_PDU_HEADER(rxEntry->data, rxEntry->numPad);

    // Copy peer device address and address type
    peerAddrType = LL_ADV_HDR_GET_TX_ADD(*pPkt);
    MAP_osal_memcpy( peerAddr, &pPkt[2], B_ADDR_LEN );

    // check if the InitA is an RPA
    if ( MAP_LL_PRIV_IsRPA( peerAddrType, peerAddr ) )
    {
      if (privInfo.addrResolution == FALSE)
      {
        // The peer is an RPA but address resolution is disabled
        // Don't connect
        return FALSE;
      }

      // Check if peer address is resolvable
      rlIndexA = MAP_LL_PRIV_IsResolvable( peerAddr, resolvingList );

      // Check if directed advertisement
      if ( TST_AE_PROPS_DIR(pAdvSet->pAdvParam->eventProps) ||
           TST_AE_PROPS_HDC_DIR(pAdvSet->pAdvParam->eventProps) )
      {
        rlIndexB = MAP_LL_PRIV_IsResolvable( pAdvSet->peerAddr, resolvingList );
        if ( rlIndexA != rlIndexB )
        {
          // The peer device address doesn't match the advertising
          // parameter peer address
          return FALSE;
        }
      }

      // Check if we managed to resolve
      if ( rlIndexA != INVALID_RESOLVE_LIST_INDEX )
      {
        // The address was resolved
        if ( (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_CONNECT_IND) ||
             (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_ALL_REQ) )
        {
          // check if RPA's ID is NOT found in the AL
          if ( MAP_AL_FindEntry( alTable,
                                 resolvingList[rlIndexA].idAddr,
                                 resolvingList[rlIndexA].idAddrType ) == alTable->numAlEntries )
          {
            // don't continue to Legacy connection
            return FALSE;
          }
          else
          {
            // Update the Initiators RPA in our Accept List (it might have changed)
            MAP_LL_PRIV_UpdateExtALEntry( alTable,
                                          resolvingList[rlIndexA].RPA,
                                          peerAddr );
            // update RL RPA for this peer with AnitA
            MAP_osal_memcpy( resolvingList[rlIndexA].RPA, peerAddr, B_ADDR_LEN );

            // continue to legacy connection
            return TRUE;
          }
        }
      }
      else
      {
        // Check the advertising filter policy in case the peer RPA is not resolved
        if ((pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_CONNECT_IND) ||
            (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_ALL_REQ))
        {
          // When the advertising filter policy is using AL with connection
          // and the peer adress is not resolved don't accept the connect indication
          return FALSE;
        }
      }
    }
    else // InitA not RPA
    {
      // Check if the ID address can be found in the resolving List
      uint8 rlIndex = MAP_LL_PRIV_FindPeerInRL( resolvingList,
                                                peerAddrType,
                                                peerAddr );

      if( rlIndex != INVALID_RESOLVE_LIST_INDEX )
      {
        // If the IRK is not all zeros and the device privacy mode
        // is Network privacy mode, don't connect to that device since
        // its uses the public address and not the RPA
        if ( !MAP_LL_PRIV_IsZeroIRK( resolvingList[rlIndex].IRK ) &&
             (resolvingList[rlIndex].privMode == LL_NETWORK_PRIVACY_MODE) )
        {
           return FALSE;
        }
      }
    }

    // Connect no matter what - if we got to this point then the following applies:
    // 1. This is an RPA we could not resolve -> this means it is not in our accept list - just connect
    // 2. If a device using a Public Address was added to the Accept List then the RCL will report the CONNECT_IND and we will get here
    //    If a device using a Public Address was not added then the RCL will filter the CONNECT_IND and we will not get here anyway
    return TRUE;
  }

  // RCL Command status is not RCL_CommandStatus_Connect, continue Advertising
  return FALSE;
#else
  uint8 status = FALSE;

  if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
  {
    aeLegacyRf_t *pRfCmds = (aeLegacyRf_t *)pAdvSet->pRfCmds;

    // check for a connection (verification of the connection indication is being done at the end of the legacy adv section)
    if ( (pRfCmds->advCmd[0].rfOpCmd.status == BLESTAT_DONE_CONNECT) ||
         (pRfCmds->advCmd[1].rfOpCmd.status == BLESTAT_DONE_CONNECT) ||
         (pRfCmds->advCmd[2].rfOpCmd.status == BLESTAT_DONE_CONNECT) ||
         ((pAdvSet->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT)&&
         (pRfCmds->advCmd[3].rfOpCmd.status == BLESTAT_DONE_CONNECT)) )
    {
      // set status
      taskEndStatus = BLESTAT_DONE_CONNECT;

      // this is a Undirected or Directed Connect, so setup routine to post-process
      taskEndAction = MAP_llAdv_TaskConnect;
      status = TRUE;
    }
    else if ( (pRfCmds->advCmd[0].rfOpCmd.status == BLESTAT_DONE_CONNECT_CHSEL0) ||
              (pRfCmds->advCmd[1].rfOpCmd.status == BLESTAT_DONE_CONNECT_CHSEL0) ||
              (pRfCmds->advCmd[2].rfOpCmd.status == BLESTAT_DONE_CONNECT_CHSEL0) ||
              ((pAdvSet->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT)&&
              (pRfCmds->advCmd[3].rfOpCmd.status == BLESTAT_DONE_CONNECT_CHSEL0)) )
    {
      // set status
      taskEndStatus = BLESTAT_DONE_CONNECT_CHSEL0;

      // this is a Undirected or Directed Connect, so setup routine to post-process
      taskEndAction = MAP_llAdv_TaskConnect;
      status = TRUE;
    }
    if(taskEndAction == MAP_llAdv_TaskConnect)
    {
      uint8 *pData = (uint8 *)(pRfCmds->advParam.pRXQ->pCurEntry) + sizeof( dataEntry_t );

      // verify the connect indication
      if(MAP_llConnExists(LL_TASK_ID_CENTRAL,&pData[LL_CONN_IND_INITIATOR_ADDRESS_OFFSET],
                          MASK_ID_ADDRTYPE(pData[LL_CONN_IND_HEADER_OFFSET] >> LL_ADV_PDU_HDR_TXADDR))  ||
         (pRfCmds->advParam.pRXQ->pCurEntry->status != DATASTAT_FINISHED)                               ||
         (pRfCmds->advOutput.nRxConnReq != 1)                                                           ||
         (!llValidateConnectIndPkt( pData )))
      {
        //invalid connect_ind - continue with advertising
        // in all cases, treat the same as a Task Done Okay
        taskEndStatus = BLESTAT_DONE_OK;

        // and setup routine to post-process
        taskEndAction = MAP_llExtAdv_PostProcess;
        status = FALSE;

        // mark entry as free
        pRfCmds->advParam.pRXQ->pCurEntry->status = DATASTAT_PENDING;
      }
    }
  }
#ifdef USE_AE
  else // !legacy
  {
    // get the status of the Secondary RF command
    taskEndStatus = ((aeRf_t *)pAdvSet->pRfCmds)->auxRfCmd.rfOpCmd.status;

    // determine an action, if any
    if ((taskEndStatus == BLESTAT_DONE_CONNECT) ||
        (taskEndStatus == BLESTAT_DONE_CONNECT_CHSEL0))
    {
        taskEndAction = MAP_llAdv_TaskConnect;
        status = TRUE;
    }
  }
#endif
#ifdef QUAL_TEST
  // the fix is under QUAL_TEST definition because the address resolution
  // is enabled by default in host application
  // in case of connection request and the address resolution is disable,
  // check the peer address and if it is RPA, dismiss the connection request
  // fix for the Qualification test LL.SEC.ADV.BV-08
  // Jira ticket - BLE_AGAMA-2058
  if ((taskEndAction == MAP_llAdv_TaskConnect) && (privInfo.addrResolution == FALSE))
  {
    uint8 peerAddrType;
    uint8 peerAddr[LL_DEVICE_ADDR_LEN];
    uint8 *pData;

    if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
    {
      pData = (uint8 *)((aeLegacyRf_t *)pAdvSet->pRfCmds)->advParam.pRXQ->pCurEntry + sizeof( dataEntry_t );
    }
#ifdef USE_AE
    else
    {
      pData = (uint8 *)((aeRf_t *)pAdvSet->pRfCmds)->auxRfParam.pRXQ->pCurEntry + sizeof( dataEntry_t );
    }
#endif
    // read the initiator's address type from the header
    peerAddrType = (uint8)MASK_ID_ADDRTYPE(pData[0] >> LL_ADV_PDU_HDR_TXADDR);
    // read the initiator's address
    MAP_osal_memcpy( peerAddr, &pData[2], LL_DEVICE_ADDR_LEN );
    // check that the peer address is RPA
    if (MAP_LL_PRIV_IsRPA(peerAddrType, peerAddr))
    {
      // in case the peer address is RPA and the adress resolution is disable,
      // dismiss the connection request and continue in advertising
      taskEndStatus = BLESTAT_DONE_ENDED;
      taskEndAction = MAP_llExtAdv_PostProcess;
      status = FALSE;
    }
  }
#endif
  return status;
#endif //USE_RCL
}

////////////////////////////////////////////////////////////////////////////////
// Rx Ignore Event Handle Connect Request
////////////////////////////////////////////////////////////////////////////////
uint8 llRxIgnoreEventHandleConnectRequest( advSet_t *pAdvSet, uint8 *PeerA, uint8 PeerAdd, uint8 chSel )
{
#ifndef CC23X0
  uint8 rlIndex = INVALID_RESOLVE_LIST_INDEX;
  uint8 update = TRUE;
  uint8 connect = FALSE;

  // check if the InitA is an RPA
  if ( MAP_LL_PRIV_IsRPA( PeerAdd, PeerA ) )
  {
    // try to resolve address only if address resolution is enabled
    if (privInfo.addrResolution == TRUE)
    {
      // Note: ISR is not in ROM, so call doesn't need to use R2R JT.
      rlIndex = MAP_LL_PRIV_IsResolvable( PeerA, resolvingList );

      if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
      {
        // check the filter policy
        // Note: If the AL is used, then only respond to the Request if the
        //       peer's ID address is in the AL.
        // Note: If the AL is not used, then always accept! That is, the
        //       rpaMode bit is disabled.
        if ( (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_CONNECT_IND) ||
             (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_ALL_REQ) )
        {
          // check if RPA's ID is NOT found in the AL
          if ( MAP_AL_FindEntry( alTable,
                                 resolvingList[rlIndex].idAddr,
                                 resolvingList[rlIndex].idAddrType ) == alTable->numAlEntries )
          {
            // don't continue to Legacy connection
            connect = FALSE;
            // don't update the ExtAL with received RPA
            update = FALSE;
          }
        }
        // if passed all the wanted filtering update the correct structures
        if (update)
        {
          // Update the ExtAL with the received InitA
          MAP_LL_PRIV_UpdateExtALEntry( alTable,
                                        resolvingList[rlIndex].RPA,
                                        PeerA );
          // update RL RPA for this peer with AnitA
          MAP_osal_memcpy( resolvingList[rlIndex].RPA, PeerA, B_ADDR_LEN );

          // continue to legacy connection
          connect = TRUE;
        }
      }
    }

    // address was not resolved - need to check filter policy
    if ( rlIndex == INVALID_RESOLVE_LIST_INDEX )
    {
      // Check the advertising filter policy in case the peer RPA is not resolved
      if (( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) ) &&
          ((pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_ANY_REQ) ||
           (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_SCAN_REQ)))
      {
        connect = TRUE;
      }
    }
  }

  // check if we should connect
  if ( connect )
  {
    // check if AE Legacy
    if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
    {
      // check if we support Algo 2
      if ( (deviceFeatureSet.featureSet[1] & (uint8)LL_FEATURE_CHAN_ALGO_2) &&
           (chSel == LL_CHANNEL_SELECT_ALGO_1) )
      {
        // set Adv status (all three) to connect, and let process
        // per usual (i.e. CPE0 interrupt should result)

        ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advCmd[0].rfOpCmd.status = BLESTAT_DONE_CONNECT_CHSEL0;
        ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advCmd[1].rfOpCmd.status = BLESTAT_DONE_CONNECT_CHSEL0;
        ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advCmd[2].rfOpCmd.status = BLESTAT_DONE_CONNECT_CHSEL0;

        // set status
        taskEndStatus = BLESTAT_DONE_CONNECT_CHSEL0;
      }
      else // either we don't support Algo2, or we and peer do
      {
        // set Adv status (all three) to connect, and let process
        // per usual (i.e. CPE0 interrupt should result)
        ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advCmd[0].rfOpCmd.status = BLESTAT_DONE_CONNECT;
        ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advCmd[1].rfOpCmd.status = BLESTAT_DONE_CONNECT;
        ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advCmd[2].rfOpCmd.status = BLESTAT_DONE_CONNECT;

        // set status
        taskEndStatus = BLESTAT_DONE_CONNECT;
      }

      aeLegacyRf_t *pRfCmds = (aeLegacyRf_t *)pAdvSet->pRfCmds;
      uint8 *pData = (uint8 *)(pRfCmds->advParam.pRXQ->pCurEntry) + sizeof( dataEntry_t );
      // validate the received connection indication
      if ((pRfCmds->advParam.pRXQ->pCurEntry->status == DATASTAT_FINISHED ) &&
          (llValidateConnectIndPkt( pData )) )
      {
        // set this flag in order to prevent handling the next RF_EventLastCmdDone interrupt
        llUnhandleNextIntFlag = TRUE;

        // nRxConnReq is increased by RF core only for packets received OK and not ignored
        // In case of ignored packet we need to increase it here
        ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advOutput.nRxConnReq++;

        // setup routine to post-process
        taskEndAction = MAP_llAdv_TaskConnect;

        // process RF End Cause
        (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );

      }
    }
    //else // !legacy
    // Note: Since the AUX_CONNECT_RSP is required, we cannot handle
    //       an immediate connection as was done in Legacy above.
    //       Instead, the AUX_CONNECT_REQ'S peer RPA will be resolved
    //       the next time, and we connect per usual.
  }
  else
  {
    // dismiss the connection request and continue in advertising
    taskEndStatus = BLESTAT_DONE_ENDED;
  }
#endif
  return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Rx Entry Done Event Handle Connect Request
////////////////////////////////////////////////////////////////////////////////
uint8 llRxEntryDoneEventHandleConnectRequest( advSet_t *pAdvSet, uint8 *PeerA, uint8 PeerAdd, uint8 chSel )
{
#ifndef CC23X0
  BLE_LOG_INT_STR(0, BLE_LOG_MODULE_CTRL, "CTRL: ll_isr llState=%d, RF_PDU=%s\n", llState, "LL_CONN_REQ_PDU");
  // check if the InitA is an RPA
  if ( MAP_LL_PRIV_IsRPA( PeerAdd, PeerA ) )
  {
    // resolve the received RPA PeerA to get its corresponding ID
    uint8 rlIndexA = MAP_LL_PRIV_IsResolvable( PeerA, resolvingList );
    // resolve the peerAddress which was generated by the controller when the AUX_ADV_IND was sent
    uint8 rlIndexB = MAP_LL_PRIV_IsResolvable( pAdvSet->peerAddr, resolvingList );
    // check if both addresses are not resolved to the same ID
    if ( rlIndexA != rlIndexB )
    {
      //NOTE: if we're using directed advertising and receiving a CONNECT_REQ / CONNECT_IND with RPA, we accept the
      //      packet automatically and CONNECT_RSP sent automatically.
      //      if the initA from the CONNECT_REQ / CONNECT_IND is not resolved to the correct expected id then:
      //      -  if AE is used then abort the RADIO to make sure that the RSP won't be sent.
      //      -  if LEGACY is used then mark a bit which will make the connection to disconnect with authenticaiton failure.

#ifdef USE_AE
      if ( !TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
      {
        MAP_llHaltRadio( CMD_ABORT );
      }
      else
#endif
      {
        pAdvSet->advHalted |= AE_ENABLE_DISCONNECT_AUTH_FAIL;
      }
    }
    else
    {
      MAP_osal_memcpy( resolvingList[rlIndexA].RPA, PeerA, B_ADDR_LEN );
      if  ((TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps)) &&
           (pAdvSet->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT))
      {
        // check if we support Algo 2
        if ( (deviceFeatureSet.featureSet[1] & (uint8)LL_FEATURE_CHAN_ALGO_2) &&
             (chSel == LL_CHANNEL_SELECT_ALGO_1) )
        {
          // set Adv status (all three) to connect, and let process
          // per usual (i.e. CPE0 interrupt should result)

          ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advCmd[0].rfOpCmd.status = BLESTAT_DONE_CONNECT_CHSEL0;
          ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advCmd[1].rfOpCmd.status = BLESTAT_DONE_CONNECT_CHSEL0;
          ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advCmd[2].rfOpCmd.status = BLESTAT_DONE_CONNECT_CHSEL0;
        }
        else // either we don't support Algo2, or we and peer do
        {
          // set Adv status (all three) to connect, and let process
          // per usual (i.e. CPE0 interrupt should result)
          ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advCmd[0].rfOpCmd.status = BLESTAT_DONE_CONNECT;
          ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advCmd[1].rfOpCmd.status = BLESTAT_DONE_CONNECT;
          ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advCmd[2].rfOpCmd.status = BLESTAT_DONE_CONNECT;
        }
      }
    }
  }
#endif
  return TRUE;
}

#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
#ifndef USE_RCL
////////////////////////////////////////////////////////////////////////////////
// Abort Event Handle for ADV state
////////////////////////////////////////////////////////////////////////////////
uint8 llAbortEventHandleStateAdv( uint8 preempted )
{
  // get current Adv Set
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );

  // check whether it is necessary to prevent handling this interrupt
  if (llUnhandleNextIntFlag)
  {
    llUnhandleNextIntFlag = FALSE;
    return FALSE;
  }

  if ( ( pAdvSet ) && (( preempted ) || (llGetRfCmdPreemptionEnable())) )
  {
    // Set DMM threshold
    MAP_llDmmSetThreshold(LL_STATE_EXT_ADV,aeCurHandle,FALSE);

    taskEndAction = MAP_llExtAdv_PostProcess;
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
  }
  return TRUE;
}
#endif
////////////////////////////////////////////////////////////////////////////////
// LastCmdDone Event Handle for ADV state
////////////////////////////////////////////////////////////////////////////////
uint8 llLastCmdDoneEventHandleStateAdv( void )
{
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );

  // check whether it is necessary to prevent handling this interrupt
  if ((llUnhandleNextIntFlag) || (pAdvSet == NULL))
  {
    llUnhandleNextIntFlag = FALSE;
    return FALSE;
  }
#ifndef USE_RCL
  // set default end cause to error handler
  taskEndAction = MAP_llTaskError;

  // Reset DMM threshold
  MAP_llDmmSetThreshold(LL_STATE_EXT_ADV,aeCurHandle,TRUE);
#endif // USE_RCL

  // check for receive connect request
  if (MAP_llLastCmdDoneEventHandleConnectRequest(pAdvSet) == TRUE)
  {
    taskEndAction = MAP_llAdv_TaskConnect;

    // process RF End Cause
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );

    return TRUE;
  }
#ifdef USE_RCL
  else
  {
    taskEndAction = MAP_llExtAdv_PostProcess;

    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );

    return FALSE;
  }
#else
  // check if this will be a legacy advertisement
  if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
  {
    aeLegacyRf_t *pRfCmds = (aeLegacyRf_t *)pAdvSet->pRfCmds;

    // search through the command statuses to find one final status/action
    for(uint8 i=0, done=FALSE; i<LL_MAX_NUM_ADV_CHAN && done==FALSE; i++)
    {
      // RF command appears to return PENDING as status,
      // before updating status to RFSTAT_ERROR_PAST_START.
      do
      {
        taskEndStatus = pRfCmds->advCmd[i].rfOpCmd.status;
      } while (taskEndStatus == BLESTAT_PENDING);

      // determine an action, if any
      switch( taskEndStatus )
      {
        case RFSTAT_DONE_OK:
          // check if this is an unused channel
          if ( pRfCmds->advCmd->rfOpCmd.cmdNum == CMD_NOP )
          {
            // it is, so skip it
            continue;
          }

          /* Drop Through */

        case BLESTAT_DONE_OK:
        case BLESTAT_DONE_RXERR:
        case BLESTAT_DONE_NOSYNC:
        case BLESTAT_DONE_ENDED:
        case BLESTAT_IDLE:
        case BLESTAT_ACTIVE:
        case BLESTAT_SKIPPED:
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
          // check if Directed
          if ( (pAdvSet->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT) ||
               (pAdvSet->advEvtType == LL_ADV_CONNECTABLE_LDC_DIRECTED_EVT) )
          {
            // this is a Directed timeout, so setup routine to post-process
            taskEndAction = MAP_llExtAdv_PostProcess;
          }
          else // okay to continue
#endif // ADV_CONN_CFG
          {
            // in all cases, treat the same as a Task Done Okay
            taskEndStatus = BLESTAT_DONE_OK;

            // and setup routine to post-process
            taskEndAction = MAP_llExtAdv_PostProcess;
          }
          // update coex counters
          MAP_llCoexUpdateCounters(TRUE);
          // exit loop
          done = TRUE;

          break;

        case BLESTAT_DONE_CONNECT:
        case BLESTAT_DONE_CONNECT_CHSEL0:
          // In case we got here with status of CONNECT it means stack has rejected
          // the connection request earlier above (in llLastCmdDoneEventHandleConnectRequest)
          // for a reason (i.e. address resolution was disabled for example).
          // Thus, Need to go to adv post-process.
          // Avoiding to handle this case will result in dropping through to the default case
          // and might results in LL_ASSERT.
          taskEndStatus = BLESTAT_DONE_ENDED;

          // and setup routine to post-process
          taskEndAction = MAP_llExtAdv_PostProcess;

          // exit loop
          done = TRUE;

          break;

        case RFSTAT_ERROR_PAST_START:
        case BLESTAT_ERROR_SYNTH_PROG:
        //Did not recieve Grant from Coex module
        case BLESTAT_ERROR_NO_GRANT:
          //MAP_llHardwareError( HW_FAIL_PAST_START_TRIG );
          //return;
          if ( pAdvSet->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT )
          {
            // treat as if a 1.28s HDC Directed Adv event expired
            pRfCmds->advCmd[i].rfOpCmd.status = BLESTAT_DONE_NOSYNC;
          }
          else
          {
            pRfCmds->advCmd[i].rfOpCmd.status = BLESTAT_DONE_OK;
          }
#ifdef USE_COEX
          if (taskEndStatus == BLESTAT_ERROR_NO_GRANT)
          {
            MAP_llCoexUpdateCounters(FALSE);
          }
#endif // USE_COEX
          // and setup routine to post-process
          taskEndAction = MAP_llExtAdv_PostProcess;

          // exit loop
          done = TRUE;

          break;

        case BLESTAT_ERROR_RXBUF:
          // clear the status
          ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advParam.pRXQ->pCurEntry->status = DATASTAT_PENDING;

          // and setup routine to post-process
          taskEndAction = MAP_llExtAdv_PostProcess;

          // exit loop
          done = TRUE;

          break;

        case BLESTAT_ERROR_PAR:
          // Sanity Check:
          // This is a fatal error as either the status doesn't make any
          // sense (in the Idle, Pending, or Active case), or an unspecified
          // status was returned (in all other cases)!
          LL_ASSERT( FALSE );

          // report failure to Host
          MAP_llHardwareError( HW_FAIL_INVAILD_RF_COMMAND );
          return 1;
          break;

        default:
          // Sanity Check:
          // We should not be able to traverse all radio operation commands without
          // either an error or handling the Last Command Done interrupt.
          LL_ASSERT( FALSE );

          // report failure to Host
          MAP_llHardwareError( HW_FAIL_UNKNOWN_RF_STATUS );
          return 1;
          break;
      } // switch taskEndStatus
    } // for loop
  }
#ifdef USE_AE
  else // !legacy
  {
    // update Coex counters
    // Receive "No Grant" error only on Primary RF command status
#ifdef USE_COEX
    if (((aeRf_t *)pAdvSet->pRfCmds)->extRfCmd[0].rfOpCmd.status == BLESTAT_ERROR_NO_GRANT)
    {
      MAP_llCoexUpdateCounters(FALSE);
    }
    else
    {
      MAP_llCoexUpdateCounters(TRUE);
    }
#endif // USE_COEX
    taskEndStatus = BLESTAT_DONE_ENDED;
    taskEndAction = MAP_llExtAdv_PostProcess;
  }
#endif // USE_AE

  // process RF End Cause
  (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );

  return TRUE;
#endif // USE_RCL
}

#ifdef USE_AE
////////////////////////////////////////////////////////////////////////////////
// TxDone Event Handle for Extended ADV state
////////////////////////////////////////////////////////////////////////////////
uint8 llTxDoneEventHandleStateExtAdv( advSet_t *pAdvSet )
{
#ifndef USE_RCL
  aeRf_t *pRf = (aeRf_t *)pAdvSet->pRfCmds;
#ifdef USE_PERIODIC_ADV
  llPeriodicAdvSet_t *pPeriodicAdv = NULL;
#endif

  // check if we're still processing primary channel commands
  if ( pAdvSet->txCount < pAdvSet->numPrimChans )
  {
    // do not handle the primary channel - not using RF count command any more
  }
  // check if done with the last primary channel command
  else if ( pAdvSet->txCount == pAdvSet->numPrimChans )
  {
    // set the Extended Header Info
    pRf->comPkt.extHdrInfo  = pAdvSet->auxHdrInfo;

    // set the Extended Header Flags
    pRf->comPkt.extHdrFlags = pAdvSet->auxHdrFlags;

    // check if data should be part of AUX_ADV_IND
    // Note: Data is never part of of AUX_ADV_IND when scannable.
    if ( TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) )
    {
      // check if AUX_ADV_IND has an AuxPtr
      if ( TST_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_AUXPTR) )
      {
        pRf->comPkt.extHdrInfo  -= EXTHDR_FLAG_AUXPTR_SIZE;
        pRf->comPkt.extHdrFlags &= ~EXTHDR_FLAG_AUXPTR;
      }

      // no auxPtr for Scannable
      // Note: Set trigger to NOW when there is no subordinate packet.
      pRf->auxRfParam.auxPtrTgtTime = 0;
      pRf->auxRfParam.auxPtrTgtType = TRIGTYPE_NOW;

      // build the Extended Header Buffer for ADV_EXT_IND
      // Note: Since the CM0 auto-inserts AdvA and TargetA if needed, we can
      //       exclude them from the Extended Header buffer.
      MAP_llSetupExtHdr( pAdvSet,
                         pRf->comPkt.extHdrFlags & ~(EXTHDR_FLAG_ADVA), // & ~(EXTHDR_FLAG_ADVA | EXTHDR_FLAG_TARGETA),
                         AE_AUX_OFFSET_AUTO_INSERT );
    }
    else // !Scannable
    {
      // only allow data (if any) when AdvA is not anonymous
      if ( !TST_AE_PROPS_OMIT_ADVA(pAdvSet->pAdvParam->eventProps) )
      {
        // setup Advertising Data (if any) for the first fragment
        pRf->comPkt.advDataLen = (pAdvSet->dataLen)?pAdvSet->fragLen:0;
        pRf->comPkt.pAdvData   = (pAdvSet->dataLen)?pAdvSet->pData:NULL;
      }

      // adjust data length if connectable
      // Note: Cannot omit AdvA when connectable, so advDataLen valid.
      if ( TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) )
      {
        // check if there data to be sent
        if ( pAdvSet->dataLen && pAdvSet->pData )
        {
          // set pointer
          pRf->comPkt.pAdvData = pAdvSet->pData;

          // cap the data length based on available space in AUX PDU
          pRf->comPkt.advDataLen =
            (pAdvSet->dataLen > pAdvSet->maxAvailData) ?
             pAdvSet->maxAvailData                     :
             pAdvSet->dataLen;
        }
      }

      // check if AUX_ADV_IND has an AuxPtr
      if ( TST_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_AUXPTR) )
      {
        pRf->auxRfCmd.rfOpCmd.startTime += US_TO_RAT_TICKS(pAdvSet->minTimeAdj);

        // set the auxPtr time and type
        // Note: CM0 expects time in RAT ticks.
        pRf->auxRfParam.auxPtrTgtTime = pRf->auxRfCmd.rfOpCmd.startTime         +
                                        US_TO_RAT_TICKS( pAdvSet->otaTimeAuxAdv +
                                                         START_SYNTH_TO_RAT_OFFSET );

        pRf->auxRfParam.auxPtrTgtType = TRIGTYPE_AT_ABS_TIME;
     }
      else // no auxPtr needed
      {
        // default aux time/type
        // Note: Set trigger to NOW when there is no subordinate packet.
        pRf->auxRfParam.auxPtrTgtTime = 0;
        pRf->auxRfParam.auxPtrTgtType = TRIGTYPE_NOW;
      }

      // Sync info could be only on AUX_ADV_IND in not connactable and not scannable mode
      if ((!TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps)) &&
          (!TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps)))
      {
#ifdef USE_PERIODIC_ADV
        pPeriodicAdv = MAP_llGetPeriodicAdv(pAdvSet->pAdvParam->handle);
        if (pPeriodicAdv != NULL)
        {
          if ((pPeriodicAdv->state == PERIODIC_ADV_STATE_ENABLE) && (!TST_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_SYNCINFO)))
          {
            SET_EXTHDR_FLAG( pAdvSet->auxHdrFlags,EXTHDR_FLAG_SYNCINFO );
            //pRf->comPkt.extHdrInfo  += EXTHDR_FLAG_SYNCINFO_SIZE;
            pRf->comPkt.extHdrFlags |= EXTHDR_FLAG_SYNCINFO;
          }
          else if ((pPeriodicAdv->state == PERIODIC_ADV_STATE_DISABLE) && (TST_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_SYNCINFO)))
          {
            CLR_EXTHDR_FLAG( pAdvSet->auxHdrFlags,EXTHDR_FLAG_SYNCINFO );
            pRf->comPkt.extHdrInfo  -= EXTHDR_FLAG_SYNCINFO_SIZE;
            pRf->comPkt.extHdrFlags &= ~EXTHDR_FLAG_SYNCINFO;
          }
        }
#endif
      }

      // build the Extended Header Buffer for ADV_EXT_IND
      // Note: Since the CM0 auto-inserts AdvA and TargetA if needed, we can
      //       exclude them from the Extended Header buffer.
      MAP_llSetupExtHdr( pAdvSet,
                         pAdvSet->auxHdrFlags & ~(EXTHDR_FLAG_ADVA | EXTHDR_FLAG_TARGETA),
                         AE_AUX_OFFSET_AUTO_INSERT );
#ifdef USE_PERIODIC_ADV
      if ((pPeriodicAdv != NULL) &&
          (TST_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_SYNCINFO)) &&
          (pPeriodicAdv->state == PERIODIC_ADV_STATE_PENDING_TRIGGER))
      {
        MAP_llTrigPeriodicAdv(pAdvSet, pPeriodicAdv);
      }
#endif
    }
  }
  else // txCount > numPrimChans: AUX_ADV_IND Transmitted
  {
    // Sync info could be only on AUX_ADV_IND and NOT on AUX_CHAIN_IND
    if ( TST_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_SYNCINFO) )
    {
      CLR_EXTHDR_FLAG( pAdvSet->auxHdrFlags,EXTHDR_FLAG_SYNCINFO );
      pRf->comPkt.extHdrInfo  -= EXTHDR_FLAG_SYNCINFO_SIZE;
      pRf->comPkt.extHdrFlags &= ~EXTHDR_FLAG_SYNCINFO;
    }

    if ( TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) )
    {
      // update header info and flags in case a AUX_CONNECT_RSP is needed
      // Note: Connectable Advertising never includes ADI.
      pRf->comPkt.extHdrInfo  -= EXTHDR_FLAG_ADI_SIZE;
      pRf->comPkt.extHdrFlags &= ~EXTHDR_FLAG_ADI;

      // change Adv Mode to 00b for the possible AUX_CONNECT_RSP
      pRf->comPkt.extHdrInfo  &= ~ADV_MODE_MASK;

      // add Target Address if not already included
      if ( !TST_EXTHDR_FLAG( pRf->comPkt.extHdrFlags, EXTHDR_FLAG_TARGETA ) )
      {
        pRf->comPkt.extHdrInfo  += EXTHDR_FLAG_TARGETA_SIZE;
        pRf->comPkt.extHdrFlags |= EXTHDR_FLAG_TARGETA;
      }

      // remove Tx Power if included
      if ( TST_EXTHDR_FLAG( pRf->comPkt.extHdrFlags, EXTHDR_FLAG_TXPWR ) )
      {
        pRf->comPkt.extHdrInfo  -= EXTHDR_FLAG_TXPWR_SIZE;
        pRf->comPkt.extHdrFlags &= ~EXTHDR_FLAG_TXPWR;
      }

      // no data in AUX_CONNECT_RSP
      pRf->comPkt.pAdvData     = NULL;
      pRf->comPkt.advDataLen   = 0;
    }
    else // !Connectable
    {
      uint8 remFrag;
      uint8 auxPktCnt;

      // number of aux packets sent
      auxPktCnt = pAdvSet->txCount - pAdvSet->numPrimChans;

      // Scannable?
      if ( TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) )
      {
        // check if this is the AUX_ADV_IND packet
        if ( auxPktCnt == 1 )
        {
          // just transmitted the AUX_ADV_IND, so update the start time
          // of secondary command in case an AUX_SCAN_RSP will be sent
          // Note: This start time is also needed to find the auxPtr
          //       time, in case an AUX_CHAIN_IND PDU is also sent.
          pRf->auxRfCmd.rfOpCmd.startTime += US_TO_RAT_TICKS(pAdvSet->otaTimeAuxAdv);

          // clear AdvMode
          // Note: Subsequent AdvMode for AUX_SCAN_RSP and AUX_CHAIN_IND
          //       are zero.
          CLR_ADV_MODE( pAdvSet->auxHdrInfo );

          // set the Extended Header Info
          // Note: Already contains auxExtHdrSize+1, but not data!
          pRf->comPkt.extHdrInfo  = pAdvSet->auxHdrInfo;

          // set the Extended Header Flags
          pRf->comPkt.extHdrFlags = pAdvSet->auxHdrFlags;

#ifdef EXCLUDE_V51_FEATURES
          // update header info and flags in case a AUX_SCAN_RSP is needed
          // Note: Scannable Advertising never includes ADI or TargetA.
          pRf->comPkt.extHdrInfo  -= EXTHDR_FLAG_ADI_SIZE;
          pRf->comPkt.extHdrFlags &= ~EXTHDR_FLAG_ADI;
#endif
          // check if TargetA included
          if ( TST_AE_PROPS_DIR(pAdvSet->pAdvParam->eventProps) )
          {
            // remove TargetA
            pRf->comPkt.extHdrInfo  -= EXTHDR_FLAG_TARGETA_SIZE;
            pRf->comPkt.extHdrFlags &= ~EXTHDR_FLAG_TARGETA;
          }

          // build the Extended Header Buffer for AUX_SCAN_RSP
          // Note: Since the CM0 auto-inserts AdvA and TargetA if needed, we can
          //       exclude them from the Extended Header buffer.
          //MAP_llSetupExtHdr( pAdvSet,
          //                   pRf->comPkt.extHdrFlags & ~(EXTHDR_FLAG_ADVA | EXTHDR_FLAG_TARGETA),
          //                   AE_AUX_OFFSET_AUTO_INSERT );

          // convert the OTA size of AUX packet to time based on the PHY
          // Note: The AUX PHY field in AuxPtr is minus one the value of the parameters.
          // Note: The expected phy and coding by llOctets2Time:
          //         BLE5_1M_PHY, BLE5_2M_PHY, BLE5_CODED_PHY
          //         BLE5_CODED_S8_DEFAULT, BLE5_CODED_S2_DEFAULT
          pAdvSet->otaTimeAuxAdv = MAP_llOctets2Time( pRf->auxRfCmd.phyMode & 0x03,      // first two bits only
                                                      (pRf->auxRfCmd.phyMode>>2) & 0x01, // scheme
                                                      GET_EXT_HDR_LEN(pRf->comPkt.extHdrInfo)+pAdvSet->fragLen+1,
                                                      MIC_NOT_ENABLED );

          // for Scannable, don't include the T_MAFS because it is not needed to find
          // the start time of the AUX_SCAN_RSP
          pAdvSet->otaTimeAuxAdv += AE_MIN_T_MAFS_IN_US;

          // build the Extended Header Buffer for ADV_EXT_IND
          // Note: Since the CM0 auto-inserts AdvA and TargetA if needed, we can
          //       exclude them from the Extended Header buffer.
          // Note: Since the CM0 re-reads the auxPtrTgtTime/Type too quickly, we
          //       have to manually calculate the auxPtr, if the flag is set.
          MAP_llSetupExtHdr( pAdvSet,
                             pRf->comPkt.extHdrFlags & ~(EXTHDR_FLAG_ADVA), // & ~(EXTHDR_FLAG_ADVA | EXTHDR_FLAG_TARGETA),
                             (pAdvSet->otaTimeAuxAdv / AE_AUX_OFFSET_30_US_UNIT_VALUE ) );


         // tell PHY not to touch the auxPtr
         ((aeRf_t *)pRf)->auxRfParam.auxPtrTgtType = TRIGTYPE_NOW;

          // setup Scan Response Data (if any) for the first fragment
          pRf->comPkt.advDataLen = (pAdvSet->dataLen)?pAdvSet->fragLen:0;
          pRf->comPkt.pAdvData   = (pAdvSet->dataLen)?pAdvSet->pData:NULL;

          return TRUE;
        }
        // check if we got a AUX_SCAN_REQ and sent a AUX_SCAN_RSP
        else if ( auxPktCnt == 2 )
        {
          // set start time of AUX_CHAIN_IND, if there is one
          pRf->auxRfCmd.rfOpCmd.startTime += US_TO_RAT_TICKS(pAdvSet->otaTimeAuxAdv);

          // check if AdvA included, and if so, remove from AUX_CHAIN_IND
          if ( TST_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_ADVA) )
          {
            // remove AdvA
            pRf->comPkt.extHdrInfo  -= EXTHDR_FLAG_ADVA_SIZE;
            pRf->comPkt.extHdrFlags &= ~EXTHDR_FLAG_ADVA;
          }

          // convert the OTA size of AUX packet to time based on the PHY
          // Note: The AUX PHY field in AuxPtr is minus one the value of the parameters.
          // Note: The expected phy and coding by llOctets2Time:
          //         BLE5_1M_PHY, BLE5_2M_PHY, BLE5_CODED_PHY
          //         BLE5_CODED_S8_DEFAULT, BLE5_CODED_S2_DEFAULT
          pAdvSet->otaTimeAuxAdv = MAP_llOctets2Time( pRf->auxRfCmd.phyMode & 0x03,      // first two bits only
                                                      (pRf->auxRfCmd.phyMode>>2) & 0x01, // scheme
                                                      GET_EXT_HDR_LEN(pRf->comPkt.extHdrInfo)+pAdvSet->fragLen+1,
                                                      MIC_NOT_ENABLED );

          // for Scannable, don't include the T_MAFS because it is not needed to find
          // the start time of the AUX_SCAN_RSP
          pAdvSet->otaTimeAuxAdv += AE_MIN_T_MAFS_IN_US;

          // build the Extended Header Buffer for ADV_EXT_IND
          // Note: Since the CM0 auto-inserts AdvA and TargetA if needed, we can
          //       exclude them from the Extended Header buffer.
          // Note: Since the CM0 re-reads the auxPtrTgtTime/Type too quickly, we
          //       have to manually calculate the auxPtr, if the flag is set.
          MAP_llSetupExtHdr( pAdvSet,
                             pRf->comPkt.extHdrFlags & ~(EXTHDR_FLAG_ADVA), // & ~(EXTHDR_FLAG_ADVA | EXTHDR_FLAG_TARGETA),
                             AE_AUX_OFFSET_AUTO_INSERT );

          // we sent an AUX_CHAIN_IND, so adjust the next command
          // counter based on fragmentation
          // Note: If Scannable, we just send a AUX_ADV_IND, so pAdvSet->numFrags
          //       is always at least one, but can be more if there's fragmented
          //       AUX_SCAN_RSP data.
          pRf->countCmd.counter = pAdvSet->numFrags;
        }
        else // all subsequent CHAIN packets remain the same
        {
          // set start time of AUX_CHAIN_IND, if there is one
          pRf->auxRfCmd.rfOpCmd.startTime += US_TO_RAT_TICKS(pAdvSet->otaTimeAuxAdv);
        }

        // find the remaining number of fragments
        remFrag = (pAdvSet->numFrags + 1) - auxPktCnt;

        // only update auxPtr timing if needed
        if ( remFrag > 1 )
        {
          // set the auxPtr time and type
          // Note: CM0 expects time in RAT ticks.
          pRf->auxRfParam.auxPtrTgtTime =
              pRf->auxRfCmd.rfOpCmd.startTime         +
              US_TO_RAT_TICKS( pAdvSet->otaTimeAuxAdv +
                               START_SYNTH_TO_RAT_OFFSET );

          pRf->auxRfParam.auxPtrTgtType = TRIGTYPE_AT_ABS_TIME;
        }
      }
      else // !Scannable
      {
        // update start time of secondary command in case of AUX_CHAIN_IND
        pRf->auxRfCmd.rfOpCmd.startTime += US_TO_RAT_TICKS(pAdvSet->otaTimeAuxAdv);

        // find the remaining number of fragments
        remFrag = pAdvSet->numFrags - auxPktCnt;

        // check if possible upcoming AUX_CHAIN_IND has an AuxPtr
        if ( TST_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_AUXPTR) )
        {
          // set the auxPtr time and type
          // Note: CM0 expects time in RAT ticks.
          pRf->auxRfParam.auxPtrTgtTime = pRf->auxRfCmd.rfOpCmd.startTime         +
                                          US_TO_RAT_TICKS( pAdvSet->otaTimeAuxAdv +
                                                           START_SYNTH_TO_RAT_OFFSET );

          pRf->auxRfParam.auxPtrTgtType = TRIGTYPE_AT_ABS_TIME;
        }
      }

      // check if there are more AUX_CHAIN_IND packets to follow
      if ( remFrag )
      {
        uint8 auxHdrFlags = pAdvSet->auxHdrFlags;
        uint8 auxHdrInfo  = pAdvSet->auxHdrInfo;
        uint8 auxExtHdrSize;

        // advance the pointer
        pRf->comPkt.pAdvData += pAdvSet->fragLen;

        // remove AdvA
        CLR_EXTHDR_FLAG( auxHdrFlags,
                         EXTHDR_FLAG_ADVA );

        // check if TargetA included
        if ( TST_AE_PROPS_DIR(pAdvSet->pAdvParam->eventProps) )
        {
          CLR_EXTHDR_FLAG( auxHdrFlags,
                           EXTHDR_FLAG_TARGETA );
        }

        // check if the next fragment will be the last fragment
        if ( remFrag == 1 )
        {
          // setup the next frag, which is the last frag

          // remove AuxPtr
          CLR_EXTHDR_FLAG( auxHdrFlags,
                           EXTHDR_FLAG_AUXPTR );

          // setup last fragment length
          pRf->comPkt.advDataLen = pAdvSet->lastFragLen;

          // force auxPtr offset to zero
          pRf->auxRfParam.auxPtrTgtTime = 0;
          pRf->auxRfParam.auxPtrTgtType = TRIGTYPE_NOW;
        }

        // update size of Extended Header buffer
        auxExtHdrSize = MAP_llGetExtHdrLen( auxHdrFlags );

        // add length to Extended Header Info
        SET_EXTHDR_LEN( auxHdrInfo,
                        auxExtHdrSize );

        // set the Extended Header Info
        pRf->comPkt.extHdrInfo = auxHdrInfo;

        // set the Extended Header Flags
        pRf->comPkt.extHdrFlags = auxHdrFlags;

        // build the Extended Header Buffer for AUX_CHAIN_IND
        // Note: Since the CM0 auto-inserts AdvA and TargetA if needed, we can
        //       exclude them from the Extended Header buffer.
        MAP_llSetupExtHdr( pAdvSet,
                           auxHdrFlags & ~(EXTHDR_FLAG_ADVA | EXTHDR_FLAG_TARGETA),
                           AE_AUX_OFFSET_AUTO_INSERT );
      }
    }
  }
#endif
  return TRUE;
}
#endif

#ifndef USE_RCL
////////////////////////////////////////////////////////////////////////////////
// TxDone Event Handle for ADV state
////////////////////////////////////////////////////////////////////////////////
uint8 llTxDoneEventHandleStateAdv( void )
{
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );

  if ( pAdvSet == NULL )
  {
    return FALSE;
  }
  // check if this is the first Adv event
  if ( pAdvSet->txCount == 0 )
  {
    // indicate first event has complete
    pAdvSet->firstAdvEvt = 1;
  }

  // bump the number of Tx Done interrupts received
  pAdvSet->txCount++;

#ifdef USE_AE
  // only non-legacy allowed for this ISR
  if ( !TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
  {
    MAP_llTxDoneEventHandleStateExtAdv(pAdvSet);
  }
  else //legacy
#endif
  {
#if defined(CC13X2P)
    // get pointer to RF command
    aeLegacyRf_t *pRf = (aeLegacyRf_t *)pAdvSet->pRfCmds;
    uint8 i = (pAdvSet->firstPrimChan - LL_ADV_BASE_CHAN) + pAdvSet->txCount;
    if ( pAdvSet->txCount < pAdvSet->numPrimChans )
    {
      if (pRf->advCmd[i].rfOpCmd.cmdNum == CMD_NOP)
      {
        i++;
      }
      // setup RF Setup and Radio Command for Tx Power PA based on current Tx Power
      MAP_llTxPwrSwitchPA( pAdvSet->txPowerIndex, (uint32 *)&(pRf->advCmd[i]) );
    }
#endif
  }
  return TRUE;
}
#endif
////////////////////////////////////////////////////////////////////////////////
// Rx Ignore Event Handle for ADV state
////////////////////////////////////////////////////////////////////////////////
uint8 llRxIgnoreEventHandleStateAdv( void )
{
#ifndef CC23X0
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );
  uint8 *pPkt;
  uint8 PeerAdd;
  uint8 PeerA[B_ADDR_LEN];

  if (pAdvSet == NULL)
  {
    return FALSE;
  }
  // check if AE Legacy
  if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
  {
    // get a pointer directly to the packet (i.e. data entry payload)
    pPkt = (uint8 *)((aeLegacyRf_t *)pAdvSet->pRfCmds)->advParam.pRXQ->pCurEntry + sizeof( dataEntry_t );
  }
#ifdef USE_AE
  else // !legacy
  {
    // get a pointer directly to the packet (i.e. data entry payload)
    pPkt = (uint8 *)((aeRf_t *)pAdvSet->pRfCmds)->auxRfParam.pRXQ->pCurEntry + sizeof( dataEntry_t );
  }
#endif
  // copy peer address so there's no race condition or overwrite
  MAP_osal_memcpy( PeerA, &pPkt[2], B_ADDR_LEN );

  // copy address types
  PeerAdd = LL_ADV_HDR_GET_TX_ADD( *pPkt );

  // read PDU type in header
  // Note: Sent in response to a Scannable Undirected or Connectable
  //       Undirected Advertisement.
  // Note: LL_PKT_TYPE_SCAN_REQ and LL_PKT_TYPE_AUX_SCAN_REQ have the same value.
  if ( LL_SCAN_REQ_PDU( *pPkt ) )
  {
    BLE_LOG_INT_STR(0, BLE_LOG_MODULE_CTRL, "CTRL: ll_isr llState=%d, RF_PDU=%s\n", llState, "LL_SCAN_REQ_PDU");
    // check if the ScanA is an RPA
    if ( MAP_LL_PRIV_IsRPA( PeerAdd, PeerA ) )
    {
      // check the filter policy
      // Note: If the AL is used, then only respond to the Request if the
      //       peer's ID address is in the AL.
      // Note: If the AL is not used, then always accept! That is, the
      //       rpaMode bit is disabled.
      if ( (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_SCAN_REQ) ||
           (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_ALL_REQ) )
      {
        // try to resolve the RPA
        uint8 rlIndex = MAP_LL_PRIV_IsResolvable( PeerA, resolvingList );

        if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
        {
          // check if resolved RPA's ID is in the AL
          if ( MAP_AL_FindEntry( alTable,
                                 resolvingList[rlIndex].idAddr,
                                 resolvingList[rlIndex].idAddrType ) != alTable->numAlEntries )
          {
            // update the peer RPA in the extended accept list table
            MAP_LL_PRIV_UpdateExtALEntry( alTable,
                                          resolvingList[rlIndex].RPA,
                                          PeerA );

            // update RL RPA for this peer with ScanA
            MAP_osal_memcpy( resolvingList[rlIndex].RPA, PeerA, B_ADDR_LEN );
          }
        } // failed to resolve when using AL, so reject
      } // FP is ANY, so PHY will accept
    } // not RPA, so PHY will accept if FP=ANY, and not to be ignrored due to Privacy Mode
  }
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  else if ( LL_CONN_REQ_PDU( *pPkt ) )
  {
    // get the Channel Selection Algorithm bit, in case we connect
    uint8 chSel = LL_ADV_HDR_GET_CHSEL( *pPkt );

    MAP_llRxIgnoreEventHandleConnectRequest(pAdvSet,PeerA,PeerAdd,chSel);
  }
#endif // ADV_CONN_CFG
  // check if AE Legacy
  if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
  {
    // clear the status, only in case we didn't start the post-process routine
    if ( llUnhandleNextIntFlag == FALSE )
    {
      ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advParam.pRXQ->pCurEntry->status = DATASTAT_PENDING;
    }
  }
#ifdef USE_AE
  else // !legacy
  {
    // clear the status
    ((aeRf_t *)pAdvSet->pRfCmds)->auxRfParam.pRXQ->pCurEntry->status = DATASTAT_PENDING;
  }
#endif
#endif
  return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Rx Empty Event Handle for ADV state
////////////////////////////////////////////////////////////////////////////////
uint8 llRxEmptyEventHandleStateAdv( void )
{
#ifndef CC23X0
  // process RX FIFO data
  uint8    *pPkt;
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );

  // got pointer
  if ( pAdvSet && TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) )
  {
    uint8 *pRf = pAdvSet->pRfCmds;

    // check if this will be a legacy advertisement
    if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
    {
      pPkt = (uint8 *)((aeLegacyRf_t *)pRf)->advParam.pRXQ->pCurEntry + sizeof( dataEntry_t );
    }
#ifdef USE_AE
    else // !legacy
    {
      uint16 extraOtaTime = 0;

      pPkt = (uint8 *)((aeRf_t *)pRf)->auxRfParam.pRXQ->pCurEntry + sizeof( dataEntry_t );

      // find the additional time needed for Scan Response AuxPtr based on PHY
      switch ( pPkt[LL_PKT_HDR_LEN + (2*LL_DEVICE_ADDR_LEN) + SUFFIX_RSSI_SIZE + 1] )
      {
        case BLE5_1M_PHY:
          extraOtaTime = 176;  // in us
          break;

        case BLE5_2M_PHY:
          extraOtaTime = 92;   // in us
          break;

        case BLE5_S2_PHY:
          extraOtaTime = 654;  // in us
          break;

        case BLE5_S8_PHY:
          extraOtaTime = 1488; // in us
          break;

        default:
          LL_ASSERT( FALSE );
          break;
      }

      // add two T_IFS
      extraOtaTime += (2 * AE_T_IFS_US); // in us

      // update the start time of AUX_SCAN_RSP with the time it took to
      // receive the AUX_SCAN_REQ
      // extraOtaTime =
      // - 150us T_IFS
      // - AUX_SCAN_REQ @ PHY
      // - 150us T_IFS
      ((aeRf_t *)pRf)->auxRfCmd.rfOpCmd.startTime += US_TO_RAT_TICKS(extraOtaTime);

      // This is a fix for the race condition in case of power saving.
      // CM0 raise TX done interrupt after sent the AUX_SCAN_RSP data.
      // In the TX done contex, Controller change the counter from 1 to numFrags
      // as indication for the CM0 that there are more data to send in AUX_CHAIN_IND
      // But in case power save is enable, it is too late because the CM0 already
      // read the previous counter which is 1 and the remain data would not be send.
      // So we update it here in the Rx Empty interrupt context which CM0 raise
      // before it send the the AUX_SCAN_RSP data.
      if (pAdvSet->numFrags > 1)
      {
        ((aeRf_t *)pRf)->countCmd.counter = pAdvSet->numFrags;
      }
    }
#endif
    // check if there's a register callback
    if ( MAP_llCheckCBack(LL_CBACK_EXT_SCAN_REQ_RECEIVED) )
    {
      // check if a Scan Request Report is needed
      if ( pAdvSet->pAdvParam->notifyEnableFlags & AE_NOTIFY_ENABLE_SCAN_REQUEST )
      {
        aeScanReqReceived_t *scanReqRpt;

        // Use allocLimited to avoid overflowing the heap...
        scanReqRpt = MAP_osal_mem_allocLimited( sizeof(aeScanReqReceived_t) );

        // check if we got the memory
        if ( scanReqRpt )
        {
          scanReqRpt->subCode      = AE_ADV_HCI_BLE_SCAN_REQUEST_RECEIVED_EVENT;
          scanReqRpt->handle       = aeCurHandle;
          scanReqRpt->scanAddrType = LL_ADV_HDR_GET_TX_ADD(*pPkt);

          MAP_osal_memcpy( scanReqRpt->scanAddr, &pPkt[2], B_ADDR_LEN );
#ifdef USE_RCL
          scanReqRpt->rssi         = (LRF_RSSI_INVALID == connOutput.lastRssi) ? LL_RF_RSSI_INVALID : connOutput.lastRssi;
          scanReqRpt->channel      = GET_CHANNEL_IDX(RCL_BLE5_getRxChannel(pPkt));
#else
          scanReqRpt->channel      = GET_CHANNEL_IDX(pPkt[15]);
          scanReqRpt->rssi         = LL_CHECK_LAST_RSSI(pPkt[14]);
#endif

          // check if the ScanA is an RPA
          if ( MAP_LL_PRIV_IsRPA( scanReqRpt->scanAddrType, scanReqRpt->scanAddr ) )
          {
            uint8 rlIndex = MAP_LL_PRIV_IsResolvable( scanReqRpt->scanAddr, resolvingList );

            // see if the Peer Address resolved
            if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
            {
              // it is, so use ID address and address type
              scanReqRpt->scanAddrType = resolvingList[rlIndex].idAddrType | LL_DEV_ADDR_TYPE_ID_MASK;
              MAP_osal_memcpy( scanReqRpt->scanAddr, resolvingList[rlIndex].idAddr, B_ADDR_LEN );
            }
          }

          MAP_llExtAdvCBack( LL_CBACK_EXT_SCAN_REQ_RECEIVED, (void *)scanReqRpt );
        }
        else // out of memory
        {
          MAP_llExtAdvCBack( LL_CBACK_OUT_OF_MEMORY, NULL );
        }
      }

      // check if this will be a legacy advertisement
      if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
      {
        // clear the status
        ((aeLegacyRf_t *)pRf)->advParam.pRXQ->pCurEntry->status = DATASTAT_PENDING;
      }
#ifdef USE_AE
      else // !legacy
      {
        // clear the status
        ((aeRf_t *)pRf)->auxRfParam.pRXQ->pCurEntry->status = DATASTAT_PENDING;
      }
#endif
    }
  }
#endif
  return TRUE;
}
////////////////////////////////////////////////////////////////////////////////
// Rx Entry Done Event Handle for ADV state
////////////////////////////////////////////////////////////////////////////////
uint8 llRxEntryDoneEventHandleStateAdv( void )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
#ifndef CC23X0
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );
  uint8 *pPkt;
  uint8 PeerAdd;            //Peer address type
  uint8 PeerA[B_ADDR_LEN];  //Peer address

  if (pAdvSet == NULL)
  {
    return FALSE;
  }

  // We should be here only in case of directed advertising
  if ( (!TST_AE_PROPS_DIR(pAdvSet->pAdvParam->eventProps)) && (!TST_AE_PROPS_HDC_DIR(pAdvSet->pAdvParam->eventProps)) )
  {
    return FALSE;
  }

  // check if AE Legacy
  if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
  {
    // get a pointer directly to the packet (i.e. data entry payload)
    pPkt = (uint8 *)((aeLegacyRf_t *)pAdvSet->pRfCmds)->advParam.pRXQ->pCurEntry + sizeof( dataEntry_t );
  }
#ifdef USE_AE
  else // !legacy
  {
    // get a pointer directly to the packet (i.e. data entry payload)
    pPkt = (uint8 *)((aeRf_t *)pAdvSet->pRfCmds)->auxRfParam.pRXQ->pCurEntry + sizeof( dataEntry_t );
  }
#endif
  // copy peer address so there's no race condition or overwrite
  MAP_osal_memcpy( PeerA, &pPkt[2], B_ADDR_LEN );

  // copy address types
  PeerAdd = LL_ADV_HDR_GET_TX_ADD( *pPkt );
  if ( LL_CONN_REQ_PDU( *pPkt ) )
  {
    // get the Channel Selection Algorithm bit, in case we connect
    uint8 chSel = LL_ADV_HDR_GET_CHSEL( *pPkt );

    MAP_llRxEntryDoneEventHandleConnectRequest(pAdvSet, PeerA, PeerAdd, chSel);
  }
#endif
#endif
  return TRUE;
}
#endif //(ADV_NCONN_CFG | ADV_CONN_CFG)

/*
** Local Functions for Scan state
*/
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
#ifndef USE_RCL
////////////////////////////////////////////////////////////////////////////////
// Abort Event Handle for Scan state
////////////////////////////////////////////////////////////////////////////////
uint8 llAbortEventHandleStateScan( uint8 preempted )
{
  // if preempted, post process and call Scheduler
  if ((preempted) || (llGetRfCmdPreemptionEnable()))
  {
    // Set DMM threshold
    MAP_llDmmSetThreshold(LL_STATE_SCAN,0,FALSE);
    taskEndAction = MAP_llExtScan_PostProcess;

    // post-process
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
  }
  return TRUE;
}
#endif
////////////////////////////////////////////////////////////////////////////////
// LastCmdDone Event Handle for SCAN state
////////////////////////////////////////////////////////////////////////////////
uint8 llLastCmdDoneEventHandleStateScan( void )
{
#ifdef USE_RCL
  taskEndAction = MAP_llExtScan_PostProcess;
#else
  // TEMP: RF command appears to return zero as status, before updating status.
  do
  {
    taskEndStatus = extScanCmd.rfOpCmd.status;
  } while ((taskEndStatus & 0xFF00) == 0);

  // set default end cause to error handler
  taskEndAction = MAP_llTaskError;

  // Reset DMM threshold
  MAP_llDmmSetThreshold(LL_STATE_SCAN,0,TRUE);

  // determine an action, if any
  switch( taskEndStatus )
  {
    case BLESTAT_DONE_ENDED:
    case BLESTAT_DONE_RXTIMEOUT:
    case BLESTAT_DONE_OK:
    case BLESTAT_DONE_AUX:
    case BLESTAT_DONE_RXERR:
    case BLESTAT_DONE_NOSYNC:
      // update coex counters
      MAP_llCoexUpdateCounters(TRUE);
      // either the command was stopped by the user, ended due to a
      // combination cutoff, or ended normally; in all cases, just
      // post process and call Scheduler
      taskEndAction = MAP_llExtScan_PostProcess;
      break;

    // handle occasional modem issues when using 2M
    case RFSTAT_ERROR_MODEM_TX_UNDF:
    case RFSTAT_ERROR_MODEM_RX_OVRF:
    case BLESTAT_ERROR_TX_UNDERFLOW:
    case BLESTAT_ERROR_RX_OVERFLOW:
    case RFSTAT_ERROR_PAST_START:
    //Did not recieve Grant from Coex module
    case BLESTAT_ERROR_NO_GRANT:
      //MAP_llHardwareError( HW_FAIL_PAST_START_TRIG );
      //return;
      extScanCmd.rfOpCmd.status = BLESTAT_DONE_ENDED;

      // decrement the next Scan channel so we ensure the reject channel will
      // be used when rescheduled
      extScanCmd.chan = LL_ADV_BASE_CHAN + llGetNextOrPreviousExtScanChannelIndex(LL_GET_PREV_SCAN_CHAN);

      // and setup routine to post-process
      taskEndAction = MAP_llExtScan_PostProcess;

      if (taskEndStatus == BLESTAT_ERROR_NO_GRANT)
      {
        MAP_llCoexUpdateCounters(FALSE);
      }
      break;

    case BLESTAT_ERROR_RXBUF:
    case BLESTAT_ERROR_SYNTH_PROG:
      // either there's no space to receive an Adv or Scan Response
      // packet, or there was a Frequency Synthesizer Programming Error
      // Note: Out of Rx buffer space is an error, but the only
      //       recoverable action is to continue the Scan.
      // Note: Frequency Synthesizer Programming Error is an error in the
      //       radio, but the only recoverable action is to continue with
      //       the Scan (i.e. the Scan has to be restarted in order to be
      //       able to receive again).
      taskEndAction = MAP_llExtScan_PostProcess;
      break;

    case BLESTAT_IDLE:
    case BLESTAT_PENDING:
    case BLESTAT_ACTIVE:
    case BLESTAT_ERROR_PAR:
      // Sanity Check:
      // This is a fatal error as either the status doesn't make any
      // sense (in the Idle, Pending, or Active case), or we have a
      // status that should not have occurred:
      // - Bad Parameter (i.e. a programming error)
      // - OK (only valid when bEndOnRpt is enabled; not used)
      // - Rx Error (only valid when bEndOnRpt is enabled; not used)
      // - No Synch (only valid when bEndOnRpt is enabled; not used)
      LL_ASSERT( FALSE );

      // report failure to Host
      MAP_llHardwareError( HW_FAIL_UNEXPECTED_RF_STATUS );

      break;

    default:
      // Sanity Check:
      // Unexpected status!
      LL_ASSERT( FALSE );

      // report failure to Host
      MAP_llHardwareError( HW_FAIL_UNKNOWN_RF_STATUS );

      break;
  }
#endif
  // process RF End Cause
  (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );

  return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Rx Ignore Event Handle for SCAN state
////////////////////////////////////////////////////////////////////////////////
uint8 llRxIgnoreEventHandleStateScan( void )
{
#ifndef CC23X0
  uint8 *pPkt;
  uint8 PeerA[B_ADDR_LEN];
  uint8 OwnA[B_ADDR_LEN];
  uint8 PeerAdd = LL_INVALID_DEV_ADDR_TYPE; // use address type to indicate if directed or not;
  uint8 OwnAdd  = LL_INVALID_DEV_ADDR_TYPE; // use address type to indicate if directed or not

  // get a pointer directly to the packet (i.e. data entry payload)
  pPkt = (uint8 *)extScanParam.pRXQ->pCurEntry + sizeof( dataEntry_t );

  // check if AE, and there's an extended header
  if ( LL_AUX_PDU( *pPkt ) &&
       GET_EXT_HDR_LEN(pPkt[LL_PKT_HDR_LEN] != 0) )
  {
    // check if there's a peer address
    if ( TST_EXTHDR_FLAG(pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE], EXTHDR_FLAG_ADVA) )
    {
      // copy addresses so there's no race condition or overwrite
      MAP_osal_memcpy( PeerA,
                       &pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE+AE_EXT_HDR_FLAGS_SIZE],
                       B_ADDR_LEN );

      // copy address types
      PeerAdd = LL_ADV_HDR_GET_TX_ADD( *pPkt );
    }

    // check if there's an InitA address
    if ( TST_EXTHDR_FLAG(pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE], EXTHDR_FLAG_TARGETA) )
    {
      // copy addresses so there's no race condition or overwrite
      MAP_osal_memcpy( OwnA,
                       &pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE+AE_EXT_HDR_FLAGS_SIZE +
                            (TST_EXTHDR_FLAG(pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE], EXTHDR_FLAG_ADVA)*B_ADDR_LEN)],
                       B_ADDR_LEN );

      // copy address types
      OwnAdd = LL_ADV_HDR_GET_RX_ADD( *pPkt );
    }
  }
  // read PDU type in header of non-Direct advertisement
  // Note: The Scan Response can only cause this interrupt if the AdvA
  //       doesn't match the address originally sent in the
  //       Advertisement.
  else if ( LL_ADV_IND_PDU( *pPkt )         ||
            LL_ADV_SCAN_IND_PDU( *pPkt )    ||
            LL_ADV_NONCONN_IND_PDU( *pPkt ) ||
            LL_ADV_DIRECT_IND_PDU( *pPkt ) )
  {
    // copy addresses so there's no race condition or overwrite
    MAP_osal_memcpy( PeerA, &pPkt[LL_PKT_HDR_LEN], B_ADDR_LEN );

    // copy address types
    PeerAdd = LL_ADV_HDR_GET_TX_ADD( *pPkt );

    // check if there is an InitA address
    if ( LL_ADV_DIRECT_IND_PDU( *pPkt ) )
    {
      // copy addresses so there's no race condition or overwrite
      MAP_osal_memcpy( OwnA, &pPkt[LL_PKT_HDR_LEN+B_ADDR_LEN], B_ADDR_LEN );

      // copy address types
      OwnAdd = LL_ADV_HDR_GET_RX_ADD( *pPkt );
    }
  }
  else // not a packet we're interested in
  {
    return TRUE;
  }

  // check if the own address type is valid
  if ( OwnAdd != LL_INVALID_DEV_ADDR_TYPE )
  {
    // check if ScanA is an RPA
    // Note: Vol 6, Part B, Section 6.3 says nothing about Directed
    //       Advertisements, so the following is based solely on the
    //       the filter policy, Vol 6, Part B, Section 4.3.3, which
    //       states the TargetA shall not be ignored if an RPA. As this
    //       does not indicate whether the RPA resolves or not, it will
    //       be accepted without bothering to resolve (but note that the
    //       Extended Advertisement Report will either display the RPA
    //       with address type 0x7E to indicate it's an unresolved
    //       TargetA, or it will display the ID address and address type.
    //       If the filter policy is not Extended, the RPA will be
    //       rejected.
    if ( MAP_LL_PRIV_IsRPA( OwnAdd, OwnA ) )
    {
      // an RPA, but only process if Extended Scanner Filter policy used
      if ( (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_ANY_ADV_PKTS_EXT) ||
#ifdef QUAL_TEST
           /* Note: test case LL/DDI/SCN/BV-33-C */
           /* Need to check if the test spec should configure the scan filter policy = 1 */
           (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST) ||
#endif
           (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST_EXT) )
      {
        if ((MAP_LL_PRIV_ResolveRPA(OwnA, resolvingList[LOCAL_RL_INDEX].IRK)) ||
            (extScanInfo->pScanParam->extScanParam[0].scanType == LL_SCAN_PASSIVE))
        {
          // accept the RPA either way
          extScanInfo->ownAddrType =  LL_DEV_ADDR_TYPE_RANDOM;
          SETVAR_SCAN_CFG_DEV_ADDR_TYPE( extScanParam.scanCfg, LL_DEV_ADDR_TYPE_RANDOM );
          MAP_osal_memcpy( extScanInfo->ownAddr, OwnA, B_ADDR_LEN );
        }
        else
        {
          return TRUE;
        }
      }

      else if (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_ANY_ADV_PKTS)
      {
        if (MAP_LL_PRIV_ResolveRPA(OwnA, resolvingList[LOCAL_RL_INDEX].IRK))
        {
          extScanInfo->ownAddrType =  LL_DEV_ADDR_TYPE_RANDOM;
          SETVAR_SCAN_CFG_DEV_ADDR_TYPE( extScanParam.scanCfg, LL_DEV_ADDR_TYPE_RANDOM );
          MAP_osal_memcpy( extScanInfo->ownAddr, OwnA, B_ADDR_LEN );
        }
        else
        {
          return TRUE;
        }
      }
      else // an RPA without extended scan filter policy or basic filter and the RPA is not resolvable - is rejected
      {
        return TRUE;
      }
    }
    else // not an RPA, but is it our own address?
    {
      // check if the address matches our device
      if ( (OwnAdd == resolvingList[LOCAL_RL_INDEX].idAddrType) &&
           (osal_memcmp(OwnA, resolvingList[LOCAL_RL_INDEX].idAddr, B_ADDR_LEN)) )
      {
        // check the Privacy Mode for extended filtering only
        switch( extScanInfo->pScanParam->scanFilterPolicy )
        {
          case LL_SCAN_AL_POLICY_ANY_ADV_PKTS_EXT:
          case LL_SCAN_AL_POLICY_USE_ACCEPT_LIST_EXT:
            // it is, but not allowed if NPM with valid IRK
            if ( (resolvingList[LOCAL_RL_INDEX].privMode == LL_NETWORK_PRIVACY_MODE) &&
                 !MAP_LL_PRIV_IsZeroIRK(resolvingList[LOCAL_RL_INDEX].IRK) )
            {
              break;
            }

            /* Drop Through! */

          case LL_SCAN_AL_POLICY_ANY_ADV_PKTS:
          case LL_SCAN_AL_POLICY_USE_ACCEPT_LIST:
            // it is, so update PHY address pointer
            SETVAR_SCAN_CFG_DEV_ADDR_TYPE( extScanParam.scanCfg, resolvingList[LOCAL_RL_INDEX].idAddrType );
            osal_memcpy( extScanInfo->ownAddr, resolvingList[LOCAL_RL_INDEX].idAddr, B_ADDR_LEN );
            break;
        }
      }
    }
  }

  // check if the peer address type is valid
  if ( PeerAdd != LL_INVALID_DEV_ADDR_TYPE )
  {
    // check if the AdvA is an RPA
    if ( MAP_LL_PRIV_IsRPA( PeerAdd, PeerA ) )
    {
      // check if AdvA is already in the extended AL
      // Note: Due to duplicate filtering, we may get this interrupt
      //       simply because the AL entry is being ignored. If the
      //       AdvA matches what's already in the Extended AL, then
      //       nothing more needs to be done for this address.
      if ( !MAP_LL_PRIV_FindExtALEntry( GET_AL_TABLE_POINTER(extScanParam.pAcceptList),
                                        PeerA,
                                        LL_DEV_ADDR_TYPE_RANDOM ) )
      {
        // check if the AL is being used
        if ( (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST) ||
             (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST_EXT) )
        {
          // try to resolve RPA to find ID address
          // Note: ISR is not in ROM, so call doesn't need to use R2R JT.
          uint8 rlIndex = MAP_LL_PRIV_IsResolvable( PeerA, resolvingList );

          // see if the Peer Address is resolvable
          // Note: The idea here is that the Rx Ignored interrupt was
          //       generated because the peer RPA expired. The reason
          //       for this is that we have already verified that the Adv
          //       Address Type and Address are valid. So the only reason
          //       the PHY raised an Rx Ignored is if the ScanA was not
          //       found in the AL. Since we placed every peer RPA in the
          //       AL, this can only happen if the peer's RPA has changed.
          if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
          {
            // check if the ID address is in the AL
            // Note: If the AL is used, then only respond to the Request if
            //       the peer's Identity Address is in the AL. If the AL
            //       is not used, then it doesn't matter whether the RPA
            //       resolved or not.
            if ( MAP_AL_FindEntry( GET_AL_TABLE_POINTER(extScanParam.pAcceptList),
                                   resolvingList[rlIndex].idAddr,
                                   resolvingList[rlIndex].idAddrType ) != BLE_MAX_NUM_AL_ENTRIES  )
            {
              // replace now expired peer RPA from AL
              MAP_LL_PRIV_UpdateExtALEntry( GET_AL_TABLE_POINTER(extScanParam.pAcceptList),
                                            resolvingList[rlIndex].RPA,
                                            PeerA );
              // update RL RPA for this peer with ScanA
              MAP_osal_memcpy( resolvingList[rlIndex].RPA, PeerA, B_ADDR_LEN );
            } // ID found in AL
          } // failed to resolve
        } // AL not used; always accept RPA if AL not used
      } // RPA wasn't already in the Extended AL
    }
    else // not an RPA, or there isn't a Peer Address
    {
      // if an ID address, and Peer is Network Privacy Mode with valid IRK,
      // or a duplicate that's been marked Ignore, or ignored for some
      // other reason
    }
  }
#endif
  return TRUE;
}
#endif //(CTRL_CONFIG & SCAN_CFG)

/*
** Local Functions for Init state
*/
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
#ifndef USE_RCL
////////////////////////////////////////////////////////////////////////////////
// Rx Ignore Event Handle Connect Response
////////////////////////////////////////////////////////////////////////////////
uint8 llRxIgnoreEventHandleConnectResponse( uint8 *OwnA, uint8 OwnAdd, uint8 *PeerA, uint8 PeerAdd )
{
#ifndef CC23X0
  uint8 rlIndex = INVALID_RESOLVE_LIST_INDEX;
  uint8 connRlEntry = INVALID_RESOLVE_LIST_INDEX;
  llConnState_t *connPtr = MAP_llDataGetConnPtr( extInitInfo->connId );
  uint8 connect = FALSE;

  // check if the own address type is valid
  if ( OwnAdd != LL_INVALID_DEV_ADDR_TYPE )
  {
    // check if the InitA is an RPA
    if ( MAP_LL_PRIV_IsRPA( OwnAdd, OwnA ) )
    {
      if ( !MAP_LL_PRIV_ResolveRPA( OwnA, resolvingList[LOCAL_RL_INDEX].IRK ) )
      {
        //if not resolved then disconnect with authentication failure
        MAP_llConnTerminate(connPtr,LL_DISCONNECT_AUTH_FAILURE);
      }
    }
  }

  // check if the AdvA is an RPA
  if ( MAP_LL_PRIV_IsRPA( PeerAdd, PeerA ) )
  {
    // try to resolve address only if address resolution is enabled
    if (privInfo.addrResolution == TRUE)
    {
      // Note: ISR is not in ROM, so call doesn't need to use R2R JT.
      rlIndex = MAP_LL_PRIV_IsResolvable( PeerA, resolvingList );

      if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
      {
        // Check if the address type in the connection request is ID address
        if( LL_IS_ADDR_IDENTITY_TYPE(connPtr->peerInfo.peerAddrType) )
        {
          // If the connection request was sent to the identidity address - compare to the ID address
          if( MAP_osal_memcmp(connPtr->peerInfo.peerAddr, resolvingList[rlIndex].idAddr, B_ADDR_LEN) == TRUE )
          {
            connect = TRUE;
          }
        }
        else
        {
          // Resolve the RPA in the connection pointer and compare with the resolved
          connRlEntry = MAP_LL_PRIV_IsResolvable( connPtr->peerInfo.peerAddr, resolvingList );

          if( connRlEntry != INVALID_RESOLVE_LIST_INDEX )
          {
            if( MAP_osal_memcmp(resolvingList[connRlEntry].idAddr, resolvingList[rlIndex].idAddr, B_ADDR_LEN) == TRUE )
            {
              connect = TRUE;
            }
          }
        }
      }
    }
  }

  // check if we should connect
  if ( connect )
  {
    // Update the ExtAL with the received AdvA
    MAP_LL_PRIV_UpdateExtALEntry( alTable,
                                  resolvingList[rlIndex].RPA,
                                  PeerA );

    // update RL RPA for this peer with AdvA
    MAP_osal_memcpy( resolvingList[rlIndex].RPA, PeerA, B_ADDR_LEN );

    // set this flag in order to prevent handling the next RF_EventLastCmdDone interrupt
    llUnhandleNextIntFlag_Init = TRUE;

    // setup routine to post-process
    taskEndAction = MAP_llInit_TaskConnect;

    // process RF End Cause
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
  }
#endif
  return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Abort Event Handle for Init state
////////////////////////////////////////////////////////////////////////////////
uint8 llAbortEventHandleStateInit( uint8 preempted )
{
  // if preempted, post-process and call Scheduler
  if ((preempted) || (llGetRfCmdPreemptionEnable()))
  {
    // Set DMM threshold
    MAP_llDmmSetThreshold(LL_STATE_INIT,0,FALSE);
    taskEndAction = MAP_llExtInit_PostProcess;

    // post-process
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
  }
  return TRUE;
}
#endif
////////////////////////////////////////////////////////////////////////////////
// LastCmdDone Event Handle for Init state
////////////////////////////////////////////////////////////////////////////////
uint8 llLastCmdDoneEventHandleStateInit( void )
{
#ifdef USE_RCL
  if (extInitCmd.common.status == RCL_CommandStatus_Connect)
  {
    taskEndAction = MAP_llInit_TaskConnect;
  }
  else
  {
    taskEndAction = MAP_llExtInit_PostProcess;
  }
#else

  if( llUnhandleNextIntFlag_Init == TRUE )
  {
      llUnhandleNextIntFlag_Init = FALSE;
      return FALSE;
  }

  // TEMP: RF command appears to return PENDING as status,
  //       before updating status to RFSTAT_ERROR_PAST_START.
  do
  {
    // get the status of the commands
    taskEndStatus = extInitCmd.rfOpCmd.status;
  } while ((taskEndStatus & 0xFF00) == 0);

  // set default end cause to error handler
  taskEndAction = MAP_llTaskError;

  // Reset DMM threshold
  MAP_llDmmSetThreshold(LL_STATE_INIT,0,TRUE);

  // determine an action, if any
  switch( taskEndStatus )
  {
    case BLESTAT_DONE_CONNECT:
    case BLESTAT_DONE_CONNECT_CHSEL0:
      taskEndAction = MAP_llInit_TaskConnect;
      break;

    case BLESTAT_DONE_ENDED:
    case BLESTAT_DONE_RXTIMEOUT:
    case BLESTAT_DONE_OK:
    case BLESTAT_DONE_AUX:
    case BLESTAT_DONE_RXERR:
    case BLESTAT_DONE_NOSYNC:
      // update coex counters
      MAP_llCoexUpdateCounters(TRUE);
      // either the command was stopped by the user, ended due to a
      // combination cutoff, or ended normally; in all cases, just
      // post process and call Scheduler
      taskEndAction = MAP_llExtInit_PostProcess;
      break;

    // handle occasional modem issues when using 2M
    case RFSTAT_ERROR_MODEM_TX_UNDF:
    case RFSTAT_ERROR_MODEM_RX_OVRF:
    case BLESTAT_ERROR_TX_UNDERFLOW:
    case BLESTAT_ERROR_RX_OVERFLOW:
    case RFSTAT_ERROR_PAST_START:
    //Did not recieve Grant from Coex module
    case BLESTAT_ERROR_NO_GRANT:
      //MAP_llHardwareError( HW_FAIL_PAST_START_TRIG );
      //return;
      extInitCmd.rfOpCmd.status = BLESTAT_DONE_ENDED;

      // decrement the next Scan channel so we ensure the reject channel will
      // be used when rescheduled
      extInitCmd.chan = ((extInitCmd.chan==LL_SCAN_ADV_CHAN_37) ?
                         (LL_SCAN_ADV_CHAN_39)                  :
                         (extInitCmd.chan - 1));

      // and setup routine to post-process
      taskEndAction = MAP_llExtInit_PostProcess;
      if (taskEndStatus == BLESTAT_ERROR_NO_GRANT)
      {
        MAP_llCoexUpdateCounters(FALSE);
      }
      break;

    case BLESTAT_ERROR_RXBUF:
    case BLESTAT_ERROR_SYNTH_PROG:
      // either there's no space to receive a Connection Request packet
      // or there was a Frequency Synthesizer Programming Error
      // Note: Out of Rx buffer space is an error, but the only
      //       recoverable action is to continue the Scan.
      // Note: Frequency Synthesizer Programming Error is an error in the
      //       radio, but the only recoverable action is to continue with
      //       the Scan (i.e. the Scan has to be restarted in order to be
      //       able to receive again).
      taskEndAction = MAP_llExtInit_PostProcess;
      break;

    case BLESTAT_IDLE:
    case BLESTAT_PENDING:
    case BLESTAT_ACTIVE:
    case BLESTAT_ERROR_PAR:
      // Sanity Check:
      // This is a fatal error as either the status doesn't make any
      // sense (in the Idle, Pending, or Active case), or we have a
      // status that should not have occurred:
      // - Bad Parameter (i.e. a programming error)
      // - OK (only valid when bEndOnRpt is enabled; not used)
      // - Rx Error (only valid when bEndOnRpt is enabled; not used)
      // - No Synch (only valid when bEndOnRpt is enabled; not used)
      LL_ASSERT( FALSE );

      // report failure to Host
      MAP_llHardwareError( HW_FAIL_UNEXPECTED_RF_STATUS );

      break;

    default:
      // Sanity Check:
      // Unexpected status!
      LL_ASSERT( FALSE );

      // report failure to Host
      MAP_llHardwareError( HW_FAIL_UNKNOWN_RF_STATUS );
      break;
  }
#endif
  // process RF End Cause
  (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
  return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Rx Ignore Event Handle for Init state
////////////////////////////////////////////////////////////////////////////////
uint8 llRxIgnoreEventHandleStateInit( void )
{
#ifndef CC23X0
  uint8 *pPkt;
  uint8 PeerA[B_ADDR_LEN];
  uint8 OwnA[B_ADDR_LEN];
  uint8 PeerAdd = LL_INVALID_DEV_ADDR_TYPE; // use address type to indicate if directed or not
  uint8 OwnAdd  = LL_INVALID_DEV_ADDR_TYPE; // use address type to indicate if directed or not

  // get a pointer directly to the packet (i.e. data entry payload)
  pPkt = (uint8 *)extInitParam.pRXQ->pCurEntry + sizeof( dataEntry_t );

  // check if AE, connectable, and there's an extended header
  if ( LL_AUX_PDU( *pPkt )                                &&
       (GET_ADV_MODE(pPkt[LL_PKT_HDR_LEN]) == AE_ADV_MODE_CONNECTABLE) &&
       GET_EXT_HDR_LEN(pPkt[LL_PKT_HDR_LEN] != 0) )
  {
    // check if there's a peer address
    if ( TST_EXTHDR_FLAG(pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE], EXTHDR_FLAG_ADVA) )
    {
      // copy addresses so there's no race condition or overwrite
      MAP_osal_memcpy( PeerA,
                       &pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE+AE_EXT_HDR_FLAGS_SIZE],
                       B_ADDR_LEN );

      // copy address types
      PeerAdd = LL_ADV_HDR_GET_TX_ADD( *pPkt );
    }

    // check if there's an InitA address
    if ( TST_EXTHDR_FLAG(pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE], EXTHDR_FLAG_TARGETA) )
    {
      // copy addresses so there's no race condition or overwrite
      MAP_osal_memcpy( OwnA,
                       &pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE+AE_EXT_HDR_FLAGS_SIZE +
                       (TST_EXTHDR_FLAG(pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE], EXTHDR_FLAG_ADVA)*B_ADDR_LEN)],
                       B_ADDR_LEN );

      // copy address types
      OwnAdd = LL_ADV_HDR_GET_RX_ADD( *pPkt );
    }
  }
  // read PDU type in header of non-Direct advertisement
  // Note: The Scan Response can only cause this interrupt if the AdvA
  //       doesn't match the address originally sent in the
  //       Advertisement.
  else if ( LL_ADV_IND_PDU( *pPkt ) ||
            LL_ADV_DIRECT_IND_PDU( *pPkt ) )
  {
    BLE_LOG_INT_STR(0, BLE_LOG_MODULE_CTRL, "CTRL: ll_isr llState=%d, RF_PDU=%s\n", llState, "LL_ADV_IND_PDU");
    // copy addresses so there's no race condition or overwrite
    MAP_osal_memcpy( PeerA, &pPkt[LL_PKT_HDR_LEN], B_ADDR_LEN );

    // copy address types
    PeerAdd = LL_ADV_HDR_GET_TX_ADD( *pPkt );

    // check if there is an InitA address
    if ( LL_ADV_DIRECT_IND_PDU( *pPkt ) )
    {
      // copy addresses so there's no race condition or overwrite
      MAP_osal_memcpy( OwnA,  &pPkt[LL_PKT_HDR_LEN+B_ADDR_LEN], B_ADDR_LEN );

      // copy address types
      OwnAdd = LL_ADV_HDR_GET_RX_ADD( *pPkt );
    }
  }
  else if ( LL_AUX_CONN_RSP_PDU( *pPkt ) &&
          ( GET_ADV_MODE(pPkt[LL_PKT_HDR_LEN]) == AE_ADV_MODE_NONCONN_NONSCAN) &&
            GET_EXT_HDR_LEN(pPkt[LL_PKT_HDR_LEN] != 0) )
  {
    // check if there's a peer address
    if ( TST_EXTHDR_FLAG(pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE], EXTHDR_FLAG_ADVA) )
    {
      // copy addresses so there's no race condition or overwrite
      MAP_osal_memcpy( PeerA,
                       &pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE+AE_EXT_HDR_FLAGS_SIZE],
                       B_ADDR_LEN );

      // copy address types
      PeerAdd = LL_ADV_HDR_GET_TX_ADD( *pPkt );
    }

    // check if there's a InitA address
    if ( TST_EXTHDR_FLAG(pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE], EXTHDR_FLAG_TARGETA) )
    {
      // copy addresses so there's no race condition or overwrite
      MAP_osal_memcpy( OwnA,
           &pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE+AE_EXT_HDR_FLAGS_SIZE +
           (TST_EXTHDR_FLAG(pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE], EXTHDR_FLAG_ADVA)*B_ADDR_LEN)],
           B_ADDR_LEN );

      // copy address types
      OwnAdd = LL_ADV_HDR_GET_RX_ADD( *pPkt );
    }

    // handle the connection process here as peer stoppped the advertise
    MAP_llRxIgnoreEventHandleConnectResponse(OwnA,OwnAdd,PeerA,PeerAdd);

    return TRUE;
  }
  else // not a packet we're interested in
  {
    // clear the status
    extInitParam.pRXQ->pCurEntry->status = DATASTAT_PENDING;

    return TRUE;
  }

  // check if the own address type is valid
  if ( OwnAdd != LL_INVALID_DEV_ADDR_TYPE )
  {
    // check if the InitA is an RPA
    if ( MAP_LL_PRIV_IsRPA( OwnAdd, OwnA ) )
    {
      // check if Host specified addr type as Identity with a valid IRK
      // Note: If the Host specified Own address type of 0 | 1, or
      //       2 | 3 with an invalid Local IRK, then PHY address is
      //       set in Create Connection, and should not be changed!
      //       So unless the Rx Ignore is due to an RPA when our
      //       Own address type is 2 | 3 with a valid Local IRK, we
      //       should reject this packet.
      if ( LL_IS_ADDR_TYPE_RPA(extInitInfo->ownAddrType) &&
           (!MAP_LL_PRIV_IsZeroIRK( resolvingList[LOCAL_RL_INDEX].IRK)) )
      {
        // check if InitA resolves against the Local IRK
        // Note: The idea here is that the Rx Ignored interrupt was
        //       generated because InitA RPA didn't match our address.
        if ( MAP_LL_PRIV_ResolveRPA( OwnA, resolvingList[LOCAL_RL_INDEX].IRK ) )
        {
          //NOTE: this section which is copying the OwnA into the LOCAL ADDRESS
          //      is left for the case customer will need backwards compatability
          //      other wise it's not used.
          // update pointer to our device address and set own addr type
          // Note: initParam.pDeviceAddr points to initInfo->ownAddr.
          MAP_osal_memcpy( extInitInfo->ownAddr, OwnA, B_ADDR_LEN );
          SETVAR_INIT_CFG_DEV_ADDR_TYPE( extInitParam.initCfg, LL_DEV_ADDR_TYPE_RANDOM_ID );
          // update RL RPA for this peer AdvA
          MAP_osal_memcpy( resolvingList[LOCAL_RL_INDEX].RPA, OwnA, B_ADDR_LEN );
        }
        else // InitA RPA failed to resolve
        {
          // clear the status
          extInitParam.pRXQ->pCurEntry->status = DATASTAT_PENDING;
          return TRUE;
        }
      }
      else // InitA is RPA with no key, so reject
      {
        // clear the status
        extInitParam.pRXQ->pCurEntry->status = DATASTAT_PENDING;
        return TRUE;
      }
    }
    else // not an RPA, but is it our own address?
    {
      // Per Vol 6, Part B, Section 6.4, if the Host specifies
      // to use RPA (i.e. Own Address Type of 2 or 3), then the Init
      // shall not respond to Directed Connectable Adv events that
      // contain Public or Static address as the TargetA.
      // Note: Exception made to rejection clause: If the Local IRK is
      //       invalid, we will accept Public/Static address as the
      //       TargetA if it matches our device.

      // In both of these cases, our PHY device address is set and won't
      // by changed, so the CM0 will either match if it is our address
      // or it will reject if it is not. The problem is, we could be in
      // this Rx Ignore because the TargetA was rejected, and in that
      // case, we don't want to process the PeerA. On the other hand,
      // the PHY might have accept the TargetA, and in that case, we
      // don't want to reject the TargetA. So we are basically checking
      // here whether the CM0 accepted the TargetA or not. We can do this
      // using our address type and IRK. If we don't meet the rules in
      // the previous paragraph, we reject this non-RPA TargetA.

      if ( !((LL_IS_ADDR_TYPE_RPA(extInitInfo->ownAddrType) &&
              MAP_LL_PRIV_IsZeroIRK(resolvingList[LOCAL_RL_INDEX].IRK)) ||
              LL_IS_ADDR_IDENTITY_TYPE(extInitInfo->ownAddrType)) )
      {
        // clear the status
        extInitParam.pRXQ->pCurEntry->status = DATASTAT_PENDING;
        return TRUE;
      }
    }
  }

  // check if the peer address type is valid
  if ( PeerAdd != LL_INVALID_DEV_ADDR_TYPE )
  {
    // check if the peer is an RPA
    if ( MAP_LL_PRIV_IsRPA( PeerAdd, PeerA ) )
    {
      uint8 rlIndex = MAP_LL_PRIV_IsResolvable( PeerA, resolvingList );

      // see if the Peer Address is resolvable
      // Note: The idea here is that the Rx Ignored interrupt was
      //       generated because the peer RPA expired.
      if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
      {
        // check if filter policy is to use Peer address
        if ( extInitInfo->pCreateConn->initFilterPolicy == LL_INIT_AL_POLICY_USE_PEER_ADDR )
        {
          llConnState_t *connPtr = MAP_llDataGetConnPtr( extInitInfo->connId );

          // check if Host specified peer address as an Identity Address
          // Note: If the Host specified Peer address type of 0 | 1, or
          //       2 | 3 with an invalid Peer IRK, then the PHY will be
          //       set in Create Connection with no additional checks
          //       needed here for IDA or NRPA. If we get an RPA, we
          //       still have to check it so we can either update the
          //       PHY or the Extended AL for the next advertisement.
          if ( LL_IS_ADDR_TYPE_RPA(connPtr->peerInfo.peerAddrType) )
          {
            // check if the Host's parameters match what's received
            // also check if the internal peer address is invalid
            // Note: The internal peer address is set to invalid when
            //       the Host set's the Init Filter Policy to PEER, and
            //       the Peer's ID in the RL has a valid IRK and is using
            //       Network Privacy Mode. This is done to ensure the
            //       Peer's ID is not accepted.
            if ( resolvingList[rlIndex].idAddrType == MASK_ID_ADDRTYPE(connPtr->peerInfo.peerAddrType) )
            {
              // accept only if peer address is as in the requested connect peer id
              if ( MAP_osal_memcmp( resolvingList[rlIndex].idAddr, extInitInfo->pCreateConn->peerAddr, B_ADDR_LEN ) )
              {

                // update local and hardware peer address type
                //connPtr->peerInfo.peerAddrType = LL_DEV_ADDR_TYPE_RANDOM;

                SETVAR_INIT_CFG_PEER_ADDR_TYPE( extInitParam.initCfg, LL_DEV_ADDR_TYPE_RANDOM );

                // update hardware peer address
                // Note: The initParam.pAcceptList is pointing to
                //       connPtr->peerInfo.peerAddr.
                MAP_osal_memcpy( connPtr->peerInfo.peerAddr, PeerA, B_ADDR_LEN );

                // update RL RPA for this peer AdvA
                MAP_osal_memcpy( resolvingList[rlIndex].RPA, PeerA, B_ADDR_LEN );
              }
            }
          }
        }
        else // initInfo->alPolicy == LL_INIT_AL_POLICY_USE_ACCEPT_LIST
        {
          if ( MAP_AL_FindEntry( GET_AL_TABLE_POINTER(extInitParam.pAcceptList),
                                 resolvingList[rlIndex].idAddr,
                                 resolvingList[rlIndex].idAddrType ) != BLE_MAX_NUM_AL_ENTRIES )
          {
            // replace now expired peer RPA from AL
            MAP_LL_PRIV_UpdateExtALEntry( GET_AL_TABLE_POINTER(extInitParam.pAcceptList),
                                          resolvingList[rlIndex].RPA,
                                          PeerA );

            // update RL RPA for this peer AdvA
            MAP_osal_memcpy( resolvingList[rlIndex].RPA, PeerA, B_ADDR_LEN );
          }
        }
      }
      else // failed to resolve RPA
      {
        // check if filter policy is to use PEER
        // Note: If filter policy is to use AL, unresolved RPA is rejected.
        if ( extInitInfo->pCreateConn->initFilterPolicy == LL_INIT_AL_POLICY_USE_PEER_ADDR )
        {
          llConnState_t *connPtr = MAP_llDataGetConnPtr( extInitInfo->connId );
        // check if received address matches Host provided peer address
          if ( (PeerAdd == extInitInfo->pCreateConn->peerAddrType) &&
               MAP_osal_memcmp( PeerA, extInitInfo->pCreateConn->peerAddr, B_ADDR_LEN ) )
          {
            // update hardware peer address type
            // Note: The connection peer address type is left alone as
            //       to preserve whether the Host specified an identity
            //       address or not.
            SETVAR_INIT_CFG_PEER_ADDR_TYPE( extInitParam.initCfg, PeerAdd );
            // update hardware peer address
            // Note: The initParam.pAcceptList is pointing to
            //       connExt->peerInfo.peerAddr.
            MAP_osal_memcpy( connPtr->peerInfo.peerAddr,
                             PeerA,
                             B_ADDR_LEN );
          }
        }
      }
    }
    else // not an RPA
    {
      // Note: Could be a Peer ID that is in the RL with an invalid IRK, or
      //       in the RL with a valid IRK but using Device Privacy Mode.

      // check if filter policy is to use Peer address
      if ( extInitInfo->pCreateConn->initFilterPolicy == LL_INIT_AL_POLICY_USE_PEER_ADDR )
      {
        llConnState_t *connPtr = MAP_llDataGetConnPtr( extInitInfo->connId );

        // check if the input parameter is public ID or random ID
        if ( LL_IS_ADDR_TYPE_RPA(connPtr->peerInfo.peerAddrType) )
        {
          uint8 rlIndex = MAP_LL_PRIV_FindPeerInRL( resolvingList,
                                                    PeerAdd,
                                                    PeerA );

          // check if the Peer's ID address was found in the RL
          if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
          {
            // accept peer ID for Device Privacy Mode or Invalid IRK
            if ( (resolvingList[rlIndex].privMode == LL_DEVICE_PRIVACY_MODE) ||
                 MAP_LL_PRIV_IsZeroIRK( resolvingList[rlIndex].IRK ) )
            {
              // accept only if peer address is as in the requested connect peer id
              if ( MAP_osal_memcmp( resolvingList[rlIndex].idAddr, extInitInfo->pCreateConn->peerAddr, B_ADDR_LEN ) )
              {
                // update local and hardware peer address type
                // WHY LOCAL? WE'RE HERE BECAUSE PeerAdd is in RL, so ID
                //connPtr->peerInfo.peerAddrType = PeerAdd;

                // update hardware peer address type
                // Note: The connection peer address type is left alone as
                //       to preserve whether the Host specified an identity
                //       address or not.
                SETVAR_INIT_CFG_PEER_ADDR_TYPE( extInitParam.initCfg, PeerAdd );
                // update hardware peer address
                // Note: The initParam.pAcceptList is pointing to
                //       connExt->peerInfo.peerAddr.
                MAP_osal_memcpy( connPtr->peerInfo.peerAddr,
                                 PeerA,
                                 B_ADDR_LEN );
              }
            }
          }
        }
      }
    }
  }

  // clear the status
  extInitParam.pRXQ->pCurEntry->status = DATASTAT_PENDING;
#endif
  return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Rx Entry Done Event Handle for Init state
////////////////////////////////////////////////////////////////////////////////
uint8 llRxEntryDoneEventHandleStateInit( void )
{
#ifndef CC23X0
  // get pointer to the init Rx queue
  dataEntry_t *pDataEntry = (dataEntry_t *)MAP_RFHAL_GetNextDataEntry( extInitParam.pRXQ );
  // get pointer to BLE PDU packet
  uint8 *pPkt = (uint8 *)(pDataEntry + 1);

  // check if it is valid, otherwise, nothing to do here
  if ( (pDataEntry == NULL) || (pDataEntry->status != DATASTAT_FINISHED) )
  {
    return FALSE;
  }

  if ( LL_ADV_EXT_IND_PDU(*pPkt) )
  {
    // check if more than one PHY was specified
    // Note: If only one PHY is specified, then it is possible to have
    //       different connection parameters based on the secondary PHY.
    //       Otherwise, the connection parameters are the same for all
    //       secondary PHYs, and there's no need to switch the connection
    //       request.
    if ( extInitInfo->pCreateConn->initPhys &
         (extInitInfo->pCreateConn->initPhys-1) )
    {

      // check if channel this packet was received on is a primary channel
      // Note: This is done by checking the lower five bits of the first
      //       suffix status byte, which is located at the end of the packet
      //       after the RSSI. So +2 because two bytes for the packet header
      //       and packet length, and one for RSSI byte, minus one.
      // Note: We're only interested in an ADV_EXT_IND packet, which is only
      //       sent on the primary (i.e. advertising) channels.
      if ( (*(pPkt+(*(pPkt+1) + 2)) & AE_CHAN_INDEX_MASK) >= LL_ADV_BASE_CHAN )
      {

        // advance packet pointer to Extended Header information field
        pPkt += 2;

        // must be Connectable, with Extended Header data
        if ( (GET_ADV_MODE(*pPkt) == AE_ADV_MODE_CONNECTABLE) && GET_EXT_HDR_LEN(*pPkt) )
        {
          uint8 phy;

          // advance to Flags
          pPkt++;

          // create pointer to the extended header flags
          uint8 *extHdrFlag = pPkt;

          llConnState_t *connPtr = MAP_llDataGetConnPtr( extInitInfo->connId );
          pPkt += TST_EXTHDR_FLAG(*extHdrFlag, EXTHDR_FLAG_ADVA)    ? B_ADDR_LEN              : 0;
          pPkt += TST_EXTHDR_FLAG(*extHdrFlag, EXTHDR_FLAG_TARGETA) ? B_ADDR_LEN              : 0;
          pPkt += TST_EXTHDR_FLAG(*extHdrFlag, EXTHDR_FLAG_ADI)     ? EXTHDR_FLAG_ADI_SIZE    : 0;
          pPkt += TST_EXTHDR_FLAG(*extHdrFlag, EXTHDR_FLAG_AUXPTR)  ? EXTHDR_FLAG_AUXPTR_SIZE : 0;

          // get the secondary PHY
          phy = *pPkt >> 5;

          // set connection request parameters based on PHY
          // Note: Could check if an AE packet was received and if the the event
          //       properties are connectable, but faster this way.
          extInitParam.pConnReqData = (uint8 *)&connReqData[phy];

          // also update the connection parameters
          connPtr->curParam.connInterval = connReqData[phy].connInterval;
          connPtr->curParam.peripheralLatency = connReqData[phy].latency;
          connPtr->curParam.connTimeout  = connReqData[phy].timeout;
        }
      }
    }
  }

  // check if AUX_CONNECT_RSP
  else if ( LL_AUX_CONN_RSP_PDU( *pPkt ) &&
          ( GET_ADV_MODE(pPkt[LL_PKT_HDR_LEN]) == AE_ADV_MODE_NONCONN_NONSCAN) &&
            GET_EXT_HDR_LEN(pPkt[LL_PKT_HDR_LEN] != 0) )
  {
    uint8 OwnA[B_ADDR_LEN];
    uint8 OwnAdd = LL_INVALID_DEV_ADDR_TYPE;
    llConnState_t *connPtr = MAP_llDataGetConnPtr( extInitInfo->connId );
    // check if there's an InitA address
    if ( TST_EXTHDR_FLAG(pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE], EXTHDR_FLAG_TARGETA) )
    {
      // copy addresses so there's no race condition or overwrite
      MAP_osal_memcpy( OwnA,
           &pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE+AE_EXT_HDR_FLAGS_SIZE +
           (TST_EXTHDR_FLAG(pPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE], EXTHDR_FLAG_ADVA)*B_ADDR_LEN)],
           B_ADDR_LEN );
      // copy address types
      OwnAdd = LL_ADV_HDR_GET_RX_ADD( *pPkt );
    }
    // check if the own address type is valid
    if ( OwnAdd != LL_INVALID_DEV_ADDR_TYPE )
    {
      // check if the InitA is an RPA
      if ( MAP_LL_PRIV_IsRPA( OwnAdd, OwnA ) )
      {
        if ( !MAP_LL_PRIV_ResolveRPA( OwnA, resolvingList[LOCAL_RL_INDEX].IRK ) )
        {
          //if not resolved then disconnect with authentication failure
          MAP_llConnTerminate(connPtr,LL_DISCONNECT_AUTH_FAILURE);
        }
      }
    }
  }

  // Sanity Check
  LL_ASSERT( pDataEntry->status == DATASTAT_FINISHED );
  // in all cases, mark the RX queue data entry as free
  // as we  already used it in the case of CONNECT_RSP or EXT_ADV_IND
  MAP_RFHAL_NextDataEntryDone( extInitParam.pRXQ );
#endif
  return TRUE;
}
#endif //(CTRL_CONFIG & INIT_CFG)

/*
** Local Functions for Peripheral state
*/
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
#ifndef USE_RCL
////////////////////////////////////////////////////////////////////////////////
// Abort Event Handle for Peripheral state
////////////////////////////////////////////////////////////////////////////////
uint8 llAbortEventHandleStatePeripheral( uint8 preempted )
{
  // check whether it is necessary to prevent handling this interrupt
  if (llUnhandleNextIntFlag)
  {
    llUnhandleNextIntFlag = FALSE;
    return FALSE;
  }
  // check if the connection is still valid and the command is preempted
  if ( llConns.currentConn != LL_INVALID_CONNECTION_ID && (preempted || (llGetRfCmdPreemptionEnable())) )
  {
    taskEndAction = MAP_llPeripheral_TaskEnd;

    // process RF End Cause
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
  }
  return TRUE;
}
#endif
////////////////////////////////////////////////////////////////////////////////
// LastCmdDone Event Handle for Peripheral state
////////////////////////////////////////////////////////////////////////////////
uint8 llLastCmdDoneEventHandleStatePeripheral( void )
{
  // check whether it is necessary to prevent handling this interrupt
  if (llUnhandleNextIntFlag)
  {
    llUnhandleNextIntFlag = FALSE;
    return FALSE;
  }
  // check if the connection is still valid
  if ( llConns.currentConn == LL_INVALID_CONNECTION_ID )
  {
    // connection may have already been ended by a reset
    return FALSE;
  }

  // Handle the case in which we must terminate an already "Created" connection in the future
  if (llConns.llConnection[llConns.currentConn].extFeatureMask & EXT_FEATURE_DISCONNECT_ENABLE)
  {
    // Connection needs to be cleaned up in llScheduler
    return FALSE;
  }
#ifdef USE_RCL
   taskEndAction = MAP_llPeripheral_TaskEnd;
#else
  switch ( llConns.llConnection[llConns.currentConn].connId )
  {
    case 0:
      HAL_GPIO_CLR( HAL_GPIO_1 );
      break;

    case 1:
      HAL_GPIO_CLR( HAL_GPIO_2 );
      break;

    case 2:
      HAL_GPIO_CLR( HAL_GPIO_3 );
      break;

    case 3:
      HAL_GPIO_CLR( HAL_GPIO_4 );
      break;

    case 4:
      HAL_GPIO_CLR( HAL_GPIO_5 );
      break;

    case 5:
      HAL_GPIO_CLR( HAL_GPIO_6 );
      break;

    case 6:
      HAL_GPIO_CLR( HAL_GPIO_7 );
      break;

    case 7:
      HAL_GPIO_CLR( HAL_GPIO_8 );
      break;

    default:
      // only using eight GPIOs
      break;
  }

  // set default end cause to error handler
  taskEndAction = MAP_llTaskError;

  // get the command status
  // TEMP: RF command appears to return PENDING as status,
  //       before updating status to RFSTAT_ERROR_PAST_START.
  do
  {
    taskEndStatus = linkCmd[llConns.currentConn].rfOpCmd.status;
  } while ((taskEndStatus & 0xFF00) == 0);

  // determine action based on command status
  switch ( taskEndStatus )
  {
    case BLESTAT_DONE_OK:
    case BLESTAT_DONE_NOSYNC:
    case BLESTAT_DONE_RXERR:
    case BLESTAT_DONE_MAXNACK:
    case BLESTAT_DONE_RXTIMEOUT:
    case BLESTAT_DONE_ENDED:
    case BLESTAT_ERROR_SYNTH_PROG:
      // update coex counters
      MAP_llCoexUpdateCounters(TRUE);
      taskEndAction = MAP_llPeripheral_TaskEnd;
      break;

    case RFSTAT_ERROR_PAST_START:
      //MAP_llHardwareError( HW_FAIL_PAST_START_TRIG );
      //return;
      linkCmd[llConns.currentConn].rfOpCmd.status = BLESTAT_DONE_RXTIMEOUT;

      // and setup routine to post-process
      taskEndAction = MAP_llPeripheral_TaskEnd;
      break;

    // handle occasional modem issues when using 2M
    case RFSTAT_ERROR_MODEM_TX_UNDF:
    case RFSTAT_ERROR_MODEM_RX_OVRF:
    case BLESTAT_ERROR_TX_UNDERFLOW:
    case BLESTAT_ERROR_RX_OVERFLOW:
    //Did not recieve Grant from Coex module
    case BLESTAT_ERROR_NO_GRANT:
      linkCmd[llConns.currentConn].rfOpCmd.status = BLESTAT_DONE_OK;

      // and setup routine to post-process
      taskEndAction = MAP_llPeripheral_TaskEnd;
      if (taskEndStatus == BLESTAT_ERROR_NO_GRANT)
      {
        MAP_llCoexUpdateCounters(FALSE);
      }
      break;
    case BLESTAT_IDLE:
    case BLESTAT_PENDING:
    case BLESTAT_ACTIVE:
    case BLESTAT_ERROR_PAR:
    default:
      // Sanity Check:
      // This is a fatal error as either the status doesn't make any
      // sense (in the Idle, Pending, or Active case), or an unspecified
      // status was returned (in all other cases)!
      LL_ASSERT( FALSE );

      // report failure to Host
      MAP_llHardwareError( HW_FAIL_UNEXPECTED_RF_STATUS );

      break;
  }
#endif
  // process RF End Cause
  (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );

  // ALT: Check if the active connection has any missing buffers in the
  //      Rx ring buffer (in case the heap ran out when processing Rx
  //      packets), and if so, try to replace here. Make sure the length
  //      is reset from zero.
  return TRUE;
}
#endif //(CTRL_CONFIG & ADV_CONN_CFG)

/*
** Local Functions for Peripheral state
*/
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
#ifndef USE_RCL
////////////////////////////////////////////////////////////////////////////////
// Abort Event Handle for Central state
////////////////////////////////////////////////////////////////////////////////
uint8 llAbortEventHandleStateCentral( uint8 preempted )
{
  // check if the connection is still valid and the command is preempted
  if ( llConns.currentConn != LL_INVALID_CONNECTION_ID && (preempted || (llGetRfCmdPreemptionEnable())) )
  {
    taskEndAction = MAP_llCentral_TaskEnd;

    // process RF End Cause
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
  }
  return TRUE;
}
#endif
////////////////////////////////////////////////////////////////////////////////
// LastCmdDone Event Handle for Central state
////////////////////////////////////////////////////////////////////////////////
uint8 llLastCmdDoneEventHandleStateCentral( void )
{
  // check if the connection is still valid
  if ( llConns.currentConn == LL_INVALID_CONNECTION_ID )
  {
    // connection may have already been ended by a reset
    return FALSE;
  }
#ifdef USE_RCL
  taskEndAction = MAP_llCentral_TaskEnd;
#else
  switch ( llConns.llConnection[llConns.currentConn].connId )
  {
    case 0:
      HAL_GPIO_CLR( HAL_GPIO_1 );
      break;

    case 1:
      HAL_GPIO_CLR( HAL_GPIO_2 );
      break;

    case 2:
      HAL_GPIO_CLR( HAL_GPIO_3 );
      break;

    case 3:
      HAL_GPIO_CLR( HAL_GPIO_4 );
      break;

    case 4:
      HAL_GPIO_CLR( HAL_GPIO_5 );
      break;

    case 5:
      HAL_GPIO_CLR( HAL_GPIO_6 );
      break;

    case 6:
      HAL_GPIO_CLR( HAL_GPIO_7 );
      break;

    case 7:
      HAL_GPIO_CLR( HAL_GPIO_8 );
      break;

    default:
      // only using eight GPIOs
      break;
  }

  // set default end cause to error handler
  taskEndAction = MAP_llTaskError;

  // get the command status
  // TEMP: RF command appears to return PENDING as status,
  //       before updating status to RFSTAT_ERROR_PAST_START.
  do
  {
    taskEndStatus = linkCmd[llConns.currentConn].rfOpCmd.status;
  } while ((taskEndStatus & 0xFF00) == 0);

  // determine action based on command status
  switch ( taskEndStatus )
  {
    case BLESTAT_DONE_OK:
    case BLESTAT_DONE_NOSYNC:
    case BLESTAT_DONE_RXERR:
    case BLESTAT_DONE_MAXNACK:
    case BLESTAT_DONE_ENDED:
    case BLESTAT_ERROR_SYNTH_PROG:
      // update coex counters
      MAP_llCoexUpdateCounters(TRUE);
      taskEndAction = MAP_llCentral_TaskEnd;
      break;

    case RFSTAT_ERROR_PAST_START:
      linkCmd[llConns.currentConn].rfOpCmd.status = BLESTAT_DONE_NOSYNC;

      // and setup routine to post-process
      taskEndAction = MAP_llCentral_TaskEnd;
      break;

    // handle occasional modem issues when using 2M
    case RFSTAT_ERROR_MODEM_TX_UNDF:
    case RFSTAT_ERROR_MODEM_RX_OVRF:
    case BLESTAT_ERROR_TX_UNDERFLOW:
    case BLESTAT_ERROR_RX_OVERFLOW:
    //Did not recieve Grant from Coex module
    case BLESTAT_ERROR_NO_GRANT:
      linkCmd[llConns.currentConn].rfOpCmd.status = BLESTAT_DONE_OK;

      // and setup routine to post-process
      taskEndAction = MAP_llCentral_TaskEnd;
      if (taskEndStatus == BLESTAT_ERROR_NO_GRANT)
      {
        MAP_llCoexUpdateCounters(FALSE);
      }
      break;
    case BLESTAT_IDLE:
    case BLESTAT_PENDING:
    case BLESTAT_ACTIVE:
    case BLESTAT_ERROR_PAR:
    default:
      // Sanity Check:
      // This is a fatal error as either the status doesn't make any
      // sense (in the Idle, Pending, or Active case), or an unspecified
      // status was returned (in all other cases)!
      LL_ASSERT( FALSE );

      // report failure to Host
      MAP_llHardwareError( HW_FAIL_UNEXPECTED_RF_STATUS );

      break;
  }
#endif
  // process RF End Cause
  (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );

  // ALT: Check if the active connection has any missing buffers in the
  //      Rx ring buffer (in case the heap ran out when processing Rx
  //      packets), and if so, try to replace here. Make sure the length
  //      is reset from zero.
  return TRUE;
}
#endif //(CTRL_CONFIG & INIT_CFG)

/*
** Local Functions for Central and Peripheral state
*/
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
////////////////////////////////////////////////////////////////////////////////
// Rx Entry Done Event Handle for Central and Peripheral state
////////////////////////////////////////////////////////////////////////////////
uint8 llRxEntryDoneEventHandleStateConnection( uint8 crcError )
{
  halIntState_t  cs;
  llConnState_t *connPtr;

#ifdef USE_RCL
  RCL_Buffer_DataEntry *pDataEntry;
#else
  dataEntry_t   *pDataEntry;
#endif
  uint8         *pPkt;
  uint8          pktLen;
  uint8          pktHdr;
  uint8          pktHdrInfo;
#ifdef RTLS_CTE
  uint8          cteInfo;
#endif
  uint8          recvCte = FALSE;

  // check if the connection is still valid
  if ( llConns.currentConn == LL_INVALID_CONNECTION_ID )
  {
    // connection may have already been ended by a terminate or reset
    return FALSE;
  }

  // get connection information
  connPtr = MAP_llDataGetConnPtr( llConns.currentConn );

#ifdef USE_RCL
  /*
  * RX Buffers (rxDataQ) are a in fact a unique shared queue for all connections (peripheral and/or central).
  *
  * When one of the connections receive data, it sets the rcl command with a pointer to the first available entry in the queue (head).
  * When the rf callback will occur, the connection should read the data from the queue - from the head pointer.
  *
  * The connection then will process the data, and when it's done, it should do 2 things:
  * 1. Clear the used buffers and enqueue those back to the queue
  *    (done with RCL_MultiBuffer_get, RCL_MultiBuffer_clear and RCL_MultiBuffer_put sequence)
  * 2. All active connections should be updated with the new head and tail pointers, so all will be ready for their next command.
  *
  * We do the whole processing (receiving RCL_MultiBuffer_RxEntry_get(), clearing and updating) on the rxDataQ instead of the
  * command pointer (linkParam[].rxBuffers) so the rxDataQ is always the most updated structure,
  * while the connections receive the parameters update from the rxDataQ queue.
  * */
  // get all data entries
  while ((pDataEntry = RCL_MultiBuffer_RxEntry_get(&rxDataQ.multiBuffers, &rxDataQ.finishedBuffers)) != NULL)
  {
    // get pointer to BLE PDU packet
    pPkt = pDataEntry->data + (pDataEntry->numPad - 1);
#else
  // get pointer to packet
  pDataEntry = MAP_RFHAL_GetNextDataEntry( connPtr->pRxDataEntryQ );

  // check that the packet is really finished while CRC is OK
  // Note: It has been been observed that this callback occurs even when
  //       no buffer in the ring buffer is finished!
  if (( pDataEntry->status != DATASTAT_FINISHED ) && (!crcError))
  {
    // ignore callback
    return FALSE;
  }

  // get pointer to BLE PDU packet
  pPkt = ((dataEntryPtr_t *)pDataEntry)->pData;
#endif
  // get the packet header
  pktHdrInfo = *pPkt++;
  pktHdr = pktHdrInfo & LL_DATA_PDU_HDR_LLID_MASK;

  // Check that header includes CTE header
  if ( (((pktHdrInfo) & BV(LL_DATA_PDU_HDR_CP_BIT)) != 0) )
  {
    recvCte = TRUE;
  }

  // when receive CRC Error - no need to mark the buffer as available
  if (( crcError ) && ( !recvCte ))
  {
    return TRUE;
  }

  // check if the LLID is invalid
  if ( LL_INVALID_LLID(pktHdr) )
  {
    // it is, so mark buffer as available, and advance to next entry
#ifdef USE_RCL
    /*
    * RX Buffers (rxDataQ) are a in fact a unique shared queue for all connections (peripheral and/or central).
    *
    * When one of the connections receive data, it sets the rcl command with a pointer to the first available entry in the queue (head).
    * When the rf callback will occur, the connection should read the data from the queue - from the head pointer.
    *
    * The connection then will process the data, and when it's done, it should do 2 things:
    * 1. Clear the used buffers and enqueue those back to the queue
    *    (done with RCL_MultiBuffer_get, RCL_MultiBuffer_clear and RCL_MultiBuffer_put sequence)
    * 2. All active connections should be updated with the new head and tail pointers, so all will be ready for their next command.
    *
    * We do the whole processing (receiving RCL_MultiBuffer_RxEntry_get(), clearing and updating) on the rxDataQ instead of the
    * command pointer (linkParam[].rxBuffers) so the rxDataQ is always the most updated structure,
    * while the connections receive the parameters update from the rxDataQ queue.
    * */
    llClearRxDataEntry(&rxDataQ.multiBuffers, &rxDataQ.finishedBuffers);
    // Align the global Rx buffer list
    llUpdateRxBuffersForActiveConnections(&rxDataQ.multiBuffers);
#else
    MAP_RFHAL_NextDataEntryDone( connPtr->pRxDataEntryQ );
#endif
    return TRUE;
  }

  // get the packet length
  pktLen = *pPkt++;

  // Case header includes CTE header
  if ( recvCte )
  {
#ifdef RTLS_CTE
    // get the CTE info
    cteInfo = *pPkt++;
    //save the CTE info received from peer
    llCte[connPtr->connId].initiator.recvCte = TRUE;
    llCte[connPtr->connId].initiator.recvInfo.length = cteInfo & LL_CTE_INFO_TIME_MASK;
    llCte[connPtr->connId].initiator.recvInfo.type = (cteInfo & LL_CTE_INFO_TYPE_MASK) >> LL_CTE_INFO_TYPE_OFFSET;
#endif
    // CTE Received with CRC Error
    if (crcError)
    {
      return TRUE;
    }
  }

  // check if we have received a data packet during an encryption procedure
  // Note: This requirement based on ESR05 V1.0, Erratum 3565.
  // Note: Technically, an empty packet will never be received since they
  //       are flushed.
  // Note: Vol 6, Part B, Section 5.1.3 says any Data Channel PDU received
  //       during Pause shall terminate the connection with MIC error. A
  //       Data Channel PDU is a non-empty data packet or a control packet.
  //       Control packets are allowed here, otherwise we won't be able
  //       to complete the encryption control procedure.
  // Note: For the Central, Rx data is not allowed after the ENC_RSP is received or after a
  // PAUSE_ENC_RSP is received. For the Peripheral, Rx data is not allowed after the ENC_REQ is received or
  // after the PAUSE_ENC_REQ is received
  if ( (LL_DATA_PDU(pktHdr) && (pktLen != 0)) &&
       ((connPtr->rxDataEnabled == FALSE) ||
       ((connPtr->encInfo.encRestart == TRUE) &&
       ((llState == LL_STATE_CONN_PERIPHERAL) ||
       ((llState == LL_STATE_CONN_CENTRAL) && (connPtr->encInfo.pauseEncRspRcved == TRUE))))) )
  {
    // non-empty data packet or an invalid packet received during an
    // encryption procedure, so terminate with MIC error
    // Note: When using PM, it is possible this routine could terminate
    //       the connection and try to shutdown the RF Core while the
    //       radio is still running (as we are in the context of an
    //       ISR). To prevent this, we wait until the connection ends
    //       before terminating by letting the connection event
    //       complete.

    // set termination reason code
    connPtr->termInfo.reason = LL_MIC_FAILURE_TERM;

    // set flag to indicate a termination indication was received
    connPtr->termInfo.termIndRcvd = TRUE;

    // ALT: Halt the radio first, then terminate the connection.
    // MAP_llHaltRadio( CMD_ABORT );
    // MAP_llConnTerminate( connPtr, LL_MIC_FAILURE_TERM );

    return TRUE;
  }

  // check if a data packet and the receive flow control is enabled
  // Note: This is in support of Controller to Host flow control.
  if ( LL_DATA_PDU(pktHdr) && (rxFifoFlowCtrl == LL_RX_FLOW_CONTROL_ENABLED) )
  {
    // yep, so nothing to do here
    return TRUE;
  }

  // check if decryption is necessary
  if ( connPtr->encEnabled )
  {
    // exclude the MIC size
    pktLen -= LL_PKT_MIC_LEN;

    // decrypt/authenticate PDU
    if ( MAP_LL_ENC_Decrypt( connPtr,
                             pktHdrInfo,
                             pktLen,
                             pPkt ) != SUCCESS )
    {
      // decrypt failed due to MIC error, so terminate connection
      // Note: When using PM, it is possible this routine could terminate
      //       the connection and try to shutdown the RF Core while the
      //       radio is still running (as we are in the context of an
      //       ISR). To prevent this, we wait until the connection ends
      //       before terminating by letting the connection event
      //       complete.
      // set termination reason
      connPtr->termInfo.reason = LL_MIC_FAILURE_TERM;

      // set flag to indicate a termination indication was received
      connPtr->termInfo.termIndRcvd = TRUE;

      // ALT: Halt the radio first, then terminate the connection.
      // MAP_llHaltRadio( CMD_ABORT );
      // MAP_llConnTerminate( connPtr, LL_MIC_FAILURE_TERM );

      // possible problem with IRK, so force update of all RPA in RL
#ifndef CC23X0
      MAP_LL_PRIV_UpdateRL( resolvingList );
#endif

      return TRUE;
    }

    HAL_ENTER_CRITICAL_SECTION(cs);

    // reset the expiration toggle flag
    llConns.llConnection[connPtr->connId].numAptoExp = 0;

    // restart the APTO timer
    MAP_osal_CbTimerUpdate( connPtr->aptoTimerId,
                            (connPtr->aptoValue / 2) );

    HAL_EXIT_CRITICAL_SECTION(cs);
  }
  // check packet type
  // ALT: This macro could also check first/continue packet.
  if ( LL_DATA_PDU(pktHdr) )
  {
#ifdef CC23X0
    connPtr->lastRssi = RCL_readRssi();
#else
    // obtain the RSSI, if present
    if ( RSSI_SUFFIX_PRESENT() )
    {
      // RSSI available, so figure out its offset
      // Note: Order is always: CRC, RSSI, Status, Timestamp.
      connPtr->lastRssi = pPkt[pktLen + ((connPtr->encEnabled)?LL_PKT_MIC_LEN:0) + ((CRC_SUFFIX_PRESENT())?SUFFIX_CRC_SIZE:0)];
    }
    else // RSSI is not present
    {
      connPtr->lastRssi = LL_RF_RSSI_INVALID;
    }
#endif

#ifdef CONTROLLER_ONLY
    uint8 *pBuf = MAP_LL_RX_bm_alloc( pktLen );

    if (pBuf != NULL)
    {
      // copy the data
      MAP_osal_memcpy( pBuf, pPkt, pktLen );
      // call the HCI to process the received data
      // Note: Check RSSI, and if valid, correct.
      MAP_LL_RxDataCompleteCback( (uint16)llConns.currentConn,
                                   pBuf,
                                   pktLen,
                                   pktHdr,
                                   LL_CHECK_LAST_RSSI(connPtr->lastRssi) );
    }
    else // out of heap!
    {
      // report failure to Host
      MAP_HCI_HardwareErrorEvent( HW_FAIL_OUT_OF_MEMORY );
    }
#else //!CONTROLLER_ONLY
#ifdef LL_TEST_MODE
    switch( llTestMode.testCase )
    {
      case LL_TEST_MODE_TP_CON_MAS_BV03:
      case LL_TEST_MODE_TP_CON_MAS_BV04:
      case LL_TEST_MODE_TP_CON_MAS_BV05:
      case LL_TEST_MODE_TP_CON_SLA_BV04:
      case LL_TEST_MODE_TP_CON_SLA_BV05:
        {
          uint8 *pBuf = MAP_LL_RX_bm_alloc( pktLen );

          if (pBuf != NULL)
          {
            // copy the data
            MAP_osal_memcpy( pBuf, pPkt, pktLen );
            // call the HCI to process the received data
            // Note: Check RSSI, and if valid, correct.
            MAP_LL_RxDataCompleteCback( (uint16)llConns.currentConn,
                                         pBuf,
                                         pktLen,
                                         pktHdr,
                                         LL_CHECK_LAST_RSSI(connPtr->lastRssi) );
          }
          else // out of heap!
          {
            // report failure to Host
            MAP_HCI_HardwareErrorEvent( HW_FAIL_OUT_OF_MEMORY );
          }
        }
        break;

      default:
        MAP_llCombinePDU( (uint16)llConns.currentConn,
                           pPkt,
                           pktLen,
                           pktHdr );
        break;
    }
#else // !LL_TEST_MODE
    //// check if fragmentation is to be used based on max PDU size
    //if ( maxPduSize <= connPtr->lenInfo.connEffectiveMaxRxOctets )
    //{
    //  // call the HCI to process the received data
    //  // Note: Check RSSI, and if valid, correct.
    //  MAP_LL_RxDataCompleteCback( (uint16)llConns.currentConn,
    //                               pPkt,
    //                               pktLen,
    //                               pktHdr,
    //                               LL_CHECK_LAST_RSSI(connPtr->lastRssi) );
    //}
    //else // fragmentation supported
    {
      MAP_llCombinePDU( (uint16)llConns.currentConn,
                         pPkt,
                         pktLen,
                         pktHdr );
    }
#endif // CONTROLLER_ONLY
#endif // LL_TEST_MODE
  }
  else // LL_CTRL_PDU(pktHdr)
  {
    // in case RF does not support CTE , we will receive the CTE response len as 2
    if (( recvCte ) &&
       ((*pPkt == LL_CTRL_CTE_RSP) && (pktLen == (LL_CTE_RSP_PAYLOAD_LEN + 1))))
    {

      pktLen = LL_CTE_RSP_PAYLOAD_LEN;
    }

    // check if opcode and pktLen are valid
    // Note: Since the opcode values are sequential, we can compare it
    //       directly to the size of the table to see if it is valid.
    if ( (*pPkt >= sizeof(ctrlPktLenTable)) ||
         (pktLen != ctrlPktLenTable[*pPkt]) )
    {
      // control packet received for features that are not supported
      connPtr->unknownCtrlType = *pPkt;

      // queue up an unknown response
      MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP );

      // ALT: setup/send an Unknown Response immediately
      //(void)MAP_llSetupUnknownRsp( connPtr );
    }
    else // control packet length is okay
    {
      // process control packet
      if ( llState == LL_STATE_CONN_CENTRAL )
      {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
        MAP_llProcessCentralControlPacket( connPtr, pPkt );
#endif // INIT_CFG
      }
      else // LL_STATE_CONN_PERIPHERAL
      {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
        MAP_llProcessPeripheralControlPacket( connPtr, pPkt );
#endif // ADV_CONN_CFG
      }
    }
  }
#ifdef USE_RCL
  } // while
#endif

  // mark data entry as free
#ifdef USE_RCL
  /*
  * RX Buffers (rxDataQ) are a in fact a unique shared queue for all connections (peripheral and/or central).
  *
  * When one of the connections receive data, it sets the rcl command with a pointer to the first available entry in the queue (head).
  * When the rf callback will occur, the connection should read the data from the queue - from the head pointer.
  *
  * The connection then will process the data, and when it's done, it should do 2 things:
  * 1. Clear the used buffers and enqueue those back to the queue
  *    (done with RCL_MultiBuffer_get, RCL_MultiBuffer_clear and RCL_MultiBuffer_put sequence)
  * 2. All active connections should be updated with the new head and tail pointers, so all will be ready for their next command.
  *
  * We do the whole processing (receiving RCL_MultiBuffer_RxEntry_get(), clearing and updating) on the rxDataQ instead of the
  * command pointer (linkParam[].rxBuffers) so the rxDataQ is always the most updated structure,
  * while the connections receive the parameters update from the rxDataQ queue.
  * */
  llClearRxDataEntry(&rxDataQ.multiBuffers, &rxDataQ.finishedBuffers);
  // Align the global Rx buffer list
  llUpdateRxBuffersForActiveConnections(&rxDataQ.multiBuffers);
#else
  MAP_RFHAL_NextDataEntryDone( connPtr->pRxDataEntryQ );
#endif
  return TRUE;
}
#endif //(CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))

/*
** Local Functions for Test state
*/
////////////////////////////////////////////////////////////////////////////////
// Rx Entry Done Event Handle for Test state
////////////////////////////////////////////////////////////////////////////////
uint8 llLastCmdDoneEventHandleStateTest( void )
{
#ifdef USE_RCL
  if (llState == LL_STATE_DIRECT_TEST_MODE_TX)
  {
    // get the command status
    taskEndStatus = txDtmTestCmd.common.status;

    if (taskEndStatus == RCL_CommandStatus_Error_Synth)
    {
      // The command will automatically stop if there is a Synth error
      // Post the command again in such a case and continue executing
      RCL_Command_submit(rfHandle, (RCL_Command_Handle)&txDtmTestCmd);
    }
    else
    if (dtmInfo->txPktCnt != LL_EXT_DTM_TX_CONTINUOUS)
    {
      // generate a callback for the packet report
      // Note: For TX, the number of received packets is always zero.
      MAP_LL_DirectTestEndDoneCback( 0, LL_DIRECT_TEST_MODE_TX );

      // back to Idle
      llState = LL_STATE_IDLE;
    }
  }
  else
  if (llState == LL_STATE_DIRECT_TEST_MODE_RX)
  {
    // get the command status
    taskEndStatus = rxTestCmd.common.status;
    if (taskEndStatus == RCL_CommandStatus_Error_Synth)
    {
      // post the command
      RCL_Command_submit(rfHandle, (RCL_Command_Handle)&rxTestCmd);
    }
  }
  else
  if ((llState == LL_STATE_MODEM_TEST_TX) ||
      (llState == LL_STATE_MODEM_TEST_RX))
  {
    /* We should never get into LastCmdDone event while running MODEM_TEST commands,
     * as those commands shouldn't stop unless ended by EndModemTestCmd API.
     * Thus, if we got here - an error occurred.
     * So, return with error. */
    LL_ASSERT( FALSE );
    return FALSE;
  }
  else
  {
    // Unexpected state
    LL_ASSERT( FALSE );
    return FALSE;
  }

#else
  if ((llState == LL_STATE_DIRECT_TEST_MODE_TX) ||
      (llState == LL_STATE_DIRECT_TEST_MODE_RX))
  {
#ifndef RF_SINGLEMODE
    RF_ScheduleCmdParams cmdParams = {
      0,
      RF_StartNotSpecified,
      RF_AllowDelayAny,
      0,
      RF_EndNotSpecified,
      0,
      0,
      RF_PriorityCoexDefault,
      RF_RequestCoexDefault
    };
#endif // !RF_SINGLEMODE
    // TEMP: RF command appears to return PENDING as status,
    //       before updating status to RFSTAT_ERROR_PAST_START.
    do
    {
      // get the command status
      taskEndStatus = trxTestCmd.rfOpCmd.status;
    } while ((taskEndStatus & 0xFF00) == 0);

    // determine action based on command status
    switch ( taskEndStatus )
    {
      case BLESTAT_ERROR_SYNTH_PROG:
      {

#ifdef DEBUG_SW_TRACE
        DBG_PRINT0(DBGSYS, "");
        DBG_PRINT0(DBGSYS, "*** ERROR: SYNTH PROG FAILED! ***");
        DBG_PRINT0(DBGSYS, "");
#endif // DEBUG_SW_TRACE

#ifdef RF_SINGLEMODE
        // re-issue radio command
        rfCmdHandle = RF_postCmd( rfHandle,
                                  (RF_Op *)&trxTestCmd,
                                  RF_PriorityHighest,
                                  (RF_Callback)MAP_rfCallback,
                                  (RF_EventLastCmdDone | RF_EventInternalError) );
#else // !RF_SINGLEMODE
        // Set the Coex params
        MAP_llCoexSetParams(CMD_BLE5_RX_TEST,&cmdParams);
        // re-issue radio command
        rfCmdHandle = RF_scheduleCmd( rfHandle,
                                      (RF_Op *)&trxTestCmd,
                                      &cmdParams,
                                      (RF_Callback)MAP_rfCallback,
                                      RF_EventLastCmdDone | RF_EventInternalError );
#endif // RF_SINGLEMODE

        return TRUE;
        break;
      }

      case BLESTAT_DONE_OK:
      case BLESTAT_DONE_RXERR:
        if ( (llState == LL_STATE_DIRECT_TEST_MODE_TX) &&
             (dtmInfo->txPktCnt != LL_EXT_DTM_TX_CONTINUOUS) )
        {
          // restore Tx power setting
          MAP_llSetTxPower( curTxPowerVal );

          // generate a callback for the packet report
          // Note: For TX, the number of received packets is always zero.
          MAP_LL_DirectTestEndDoneCback( 0, LL_DIRECT_TEST_MODE_TX );

          // back to Idle
          llState = LL_STATE_IDLE;

          break;
        }
#ifdef RTLS_CTE
        else if ( (llState == LL_STATE_DIRECT_TEST_MODE_RX) && (llCteTest.testMode) )
        {
          if (llCteTest.inProgress == FALSE)
          {
            llCteTest.inProgress = TRUE;
            // send the CTE samples to Host
            if ((llCteSamples.autoCopyCompleted) > 0 && (llCteTest.recvCte == TRUE))
            {
              MAP_llGetCteInfo( CTE_TASK_ID_TEST, NULL );
            }

            #ifdef RF_SINGLEMODE
              // issue radio command asynchrously
              rfCmdHandle = RF_postCmd( rfHandle,
                                        (RF_Op *)&fwParDtmCmd,
                                        RF_PriorityHighest,
                                        (RF_Callback)MAP_rfCallback,
                                        (RF_EventLastCmdDone | RF_EventInternalError | RF_EventRxEntryDone | RF_EventSamplesEntryDone) );
            #else // !RF_SINGLEMODE
              // issue radio command asynchrously
              rfCmdHandle = RF_scheduleCmd( rfHandle,
                                            (RF_Op *)&fwParDtmCmd,
                                            &cmdParams,
                                            (RF_Callback)MAP_rfCallback,
                                            RF_EventLastCmdDone | RF_EventInternalError | RF_EventRxEntryDone | RF_EventSamplesEntryDone);
            #endif // RF_SINGLEMODE

            llCteTest.inProgress = FALSE;
            llCteTest.recvCte = FALSE;
          }
          break;
        }
#endif // RTLS_CTE
        // else: either not DTM Tx or DTM Tx using continuous transmit
        // Note: Since command should never end until stopped, the rest
        //       of these values are unexpected.

        /* DROP THROUGH */

      case BLESTAT_IDLE:
      case BLESTAT_PENDING:
      case BLESTAT_ACTIVE:
      case BLESTAT_ERROR_PAR:
      case BLESTAT_DONE_ENDED:
      case BLESTAT_ERROR_RXBUF:
      case BLESTAT_ERROR_NO_GRANT:
      default:
        // Sanity Check:
        // This is a fatal error as either the status doesn't make any
        // sense (in the Idle, Pending, or Active case), or an unspecified
        // status was returned (in all other cases)!
        LL_ASSERT( FALSE );

        // report failure to Host
        MAP_llHardwareError( HW_FAIL_UNEXPECTED_RF_STATUS );

        break;
    }
  }
  else if ((llState == LL_STATE_MODEM_TEST_TX) ||
           (llState == LL_STATE_MODEM_TEST_RX))
  {
    // TEMP: RF command appears to return PENDING as status,
    //       before updating status to RFSTAT_ERROR_PAST_START.
    do
    {
      // get the command status
      taskEndStatus = trxTestCmd.rfOpCmd.status;
    } while ((taskEndStatus & 0xFF00) == 0);

    // determine action based on command status
    switch ( taskEndStatus )
    {
      default:
      // Sanity Check:
      // This is a fatal error as either the status doesn't make any
      // sense (in the Idle, Pending, or Active case), or an unspecified
      // status was returned (in all other cases)!
      LL_ASSERT( FALSE );

      // report failure to Host
      MAP_llHardwareError( HW_FAIL_UNEXPECTED_RF_STATUS );
    }
  }
  else if (llState == LL_STATE_MODEM_TEST_TX_FREQ_HOPPING)
  {
    // get the command status
    taskEndStatus = trxTestCmd.rfOpCmd.status;

    switch ( taskEndStatus )
    {
      case BLESTAT_ERROR_SYNTH_PROG:
#ifdef DEBUG_SW_TRACE
        DBG_PRINT0(DBGSYS, "");
        DBG_PRINT0(DBGSYS, "*** ERROR: SYNTH PROG FAILED! ***");
        DBG_PRINT0(DBGSYS, "");
#endif // DEBUG_SW_TRACE

        // DROP THROUGH!

      case BLESTAT_DONE_OK:
      case BLESTAT_IDLE:
      case BLESTAT_PENDING:
      case BLESTAT_ACTIVE:
        HAL_GPIO_SET( HAL_GPIO_1 );

        // bump the Tx channel frequency
        // Note: This frequency channel is adjusted. See header file.
        // Note: It is assumed this can be done before the start of the
        //       next Tx command, whcih occurs every 625us.
        // Note: Assumed this interrupt only occurs from Modem Tx Hop test!
        trxTestCmd.chan += 2;

        // handle wrap
        if ( trxTestCmd.chan > LL_LAST_RF_CHAN_ADJ )
        {
          trxTestCmd.chan = LL_FIRST_RF_CHAN_ADJ;
        }

        // clear status
        trxTestCmd.rfOpCmd.status = RFSTAT_IDLE;

        HAL_GPIO_CLR( HAL_GPIO_1 );

        //return;
        break;

      //case RFSTAT_ERROR_PAST_START:
      // re-issue radio command
      // Note: Must not use SendCommandSynch.
      //  MAP_MB_SendCommand( (uint32)&trxTestCmd );
      //  break;

      case BLESTAT_ERROR_PAR:
      case BLESTAT_DONE_RXERR:
      case BLESTAT_DONE_ENDED:
      case BLESTAT_ERROR_RXBUF:
      case BLESTAT_ERROR_NO_GRANT:
      default:
        // Sanity Check:
        // This is a fatal error as either the status doesn't make any
        // sense (in the Idle, Pending, or Active case), or an unspecified
        // status was returned (in all other cases)!
        LL_ASSERT( FALSE );

        // report failure to Host
        MAP_llHardwareError( HW_FAIL_UNEXPECTED_RF_STATUS );

        break;
    }
  }
#endif
  return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Rx Entry Done Event Handle for Test state
////////////////////////////////////////////////////////////////////////////////
uint8 llRxEntryDoneEventHandleStateTest( void )
{
#ifndef CC23X0
#ifdef RTLS_CTE
  uint8         *pPkt;
  uint8          pktHdr;
  uint8          cteInfo;
#endif //RTLS_CTE
  dataEntry_t   *pDataEntry;

#ifdef RTLS_CTE
  if (llCteTest.testMode == FALSE)
  {
    return FALSE;
  }
#endif // RTLS_CTE

  // get pointer to packet
  pDataEntry = (dataEntry_t *)MAP_RFHAL_GetNextDataEntry( rxTestParam.pRXQ );

  // check if it is valid, otherwise, nothing to do here
  if ( (pDataEntry == NULL) || (pDataEntry->status != DATASTAT_FINISHED) )
  {
    return FALSE;
  }

#ifdef RTLS_CTE
  // get pointer to BLE PDU packet
  pPkt = (uint8 *)(pDataEntry + 1);

  // get the packet header
  pktHdr = *pPkt++;
  // jump over packet length
  pPkt++;

  // case header includes CTE header
  if ( (((pktHdr) & BV(LL_DATA_PDU_HDR_CP_BIT)) != 0) )
  {
    // get the CTE info
    cteInfo = *pPkt++;

    //check the CTE info received from peer
    if ((llCteTest.type == ((cteInfo & LL_CTE_INFO_TYPE_MASK) >> LL_CTE_INFO_TYPE_OFFSET)) &&
        (llCteTest.length == (cteInfo & LL_CTE_INFO_TIME_MASK)) &&
        (llCteTest.recvCte == FALSE) && (llCteTest.inProgress == FALSE))
    {
      llCteTest.recvCte = TRUE;
    }
  }
#endif // RTLS_CTE

  // free RX queue data entry for radio use.
  MAP_RFHAL_NextDataEntryDone( rxTestParam.pRXQ );
#endif // !CC23X0
  return TRUE;
}
/*******************************************************************************
 */
