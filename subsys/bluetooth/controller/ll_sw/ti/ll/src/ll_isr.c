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
#include "hal_mcu.h"
#include <ti/drivers/rcl/commands/ble5.h>
#include "ble.h"
#include "osal_bufmgr.h"
#include "osal_cbtimer.h"
#include "ble_isr.h"
#include "ll.h"
#include "ll_common.h"
#include "ll_enc.h"
#include "ll_config.h"
#include "ll_rat.h"
#include "hci_event.h"
#include "ll_privacy.h"
#include "icall.h"
#include "ll_ae.h"
#include "cs/ll_cs_rcl.h"
#include "map_direct.h"

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

extern const uint8 ctrlPktLenTable[NUM_OF_CTRL_PKT];

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
static void LL_rclUpdateExtAl( RCL_FilterList *filterList,
                     uint16 flags,
                     uint8* rpaAddr,
                     uint8 rlIndex );

static void llCheckPeerAddrTypeAndUpdate( llConnState_t *connPtr,
                                   uint8 *cmdDevAddr,
                                   uint8 *peerAddr,
                                   uint8 rlIndex );

#endif // CTRL_CONFIG & INIT_CFG

#define STATE_CASE_NOT_HANDLED  0
#define STATE_CASE_HANDLED      1

/*
** CC23X0 BLE RF Driver Callback
*/
void LL_rclRescheduleCommand(RCL_Command *cmd)
{
  if (cmd != NULL)
  {
    cmd->status = RCL_CommandStatus_Idle;
  }
  (void)MAP_osal_set_event( LL_TaskID, LL_EVT_RESCHEDULE );
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * This is the common RCL callback used for advertise command.
 *
 * @Design: BLE_LOKI-1453
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
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_CMD_STARTED );
  }

  if ( events.txBufferFinished )
  {
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_ADV_TX_BUFF_FINISHED );
  }

  //////////////////////////////////////////////////////////////////////////////
  // Rx Entry Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.rxEntryAvail )
  {
    if (lrfEvents.rxOk)
    {
      (void)MAP_osal_set_event( LL_TaskID, LL_EVT_ADV_RX_AVAIL );
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_ADV_LAST_CMD_DONE );
  }

  return;
}

/*******************************************************************************
 * This function is used by LL_rclAdvCallback to handle reception of SCAN_REQ packets
 * In case a callback was registered by the application, it will be called
 * This function using with dynamic filter list
 */
void LL_rclAdvRxEntryDone( void )
{
  RCL_Buffer_DataEntry      *rxEntry;
  RCL_MultiBuffer_ListInfo  listInfo;
  privTestflags_t           privacyTestCriteria;
  privTestflags_t           privacyTestResults;
  RCL_FilterList            *pDynamicFL = NULL;
  rankDynamicFL_t           *pRankFLTable = NULL;
  rlEntry_t                 *pResolvingList = NULL;
  uint8                     rpaTypeAddr = FALSE;
  uint8                     usingAcceptListFilter = FALSE;

  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );
  uint8    *pPkt;
  uint8    peerAddrType;
  uint8    peerAddr[B_ADDR_LEN];
  uint8    sendReq = TRUE;
  uint8    rlIndex = INVALID_RESOLVE_LIST_INDEX;

  // Initiate test flags
  privacyTestCriteria.flags = POLICY_NO_FAILED_TEST;
  privacyTestResults.flags = POLICY_NO_FAILED_TEST;

  // Get MultiBuffer
  RCL_MultiBuffer_ListInfo_init(&listInfo, &((aeRf_t *)pAdvSet->pRfCmds)->advParam.rxBuffers);
  rxEntry = RCL_MultiBuffer_RxEntry_next(&listInfo);

  if( rxEntry != NULL )
  {
    // Get the LL PDU header
    pPkt = LL_GET_PDU_HEADER(rxEntry->data, rxEntry->numPad);

    if ( LL_SCAN_REQ_PDU( *pPkt ) )
    {
      memcpy( peerAddr, &pPkt[2], B_ADDR_LEN );
      peerAddrType = LL_ADV_HDR_GET_TX_ADD(*pPkt);

      // check if the scanA is an RPA address type
      if ( (MAP_LL_PRIV_IsRPA( peerAddrType, peerAddr )) == TRUE )
      {
        rpaTypeAddr = TRUE;
      }
      // Check advertiser filter policy
      if ( (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_SCAN_REQ) ||
          (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_ALL_REQ) )
      {
        usingAcceptListFilter = TRUE;
      }
      // Set flags of the required policy tests.
      privacyTestCriteria = LL_PRIV_SetPrivacyTests( peerAddr,
                                                     rpaTypeAddr,
                                                     usingAcceptListFilter );
      // check all the required test to determine whether to approve the packet
      // or not
      privacyTestResults = LL_PRIV_ValidatePrivacyCompliance( peerAddr,
                                                              peerAddrType,
                                                              (uint8*)&rlIndex,
                                                              privacyTestCriteria );

      if ( privacyTestResults.flags == POLICY_NO_FAILED_TEST )
      {
        // Get pointer to the dynamic filter list
        pDynamicFL = LL_DFL_GetDynamicFilterlist();
        pRankFLTable = LL_DFL_GetRankTable();

        if ( rpaTypeAddr == UTRUE )
        {
          if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
          {
            // Get resolving List pointer
            pResolvingList = LL_PRIV_GetResolvingList();

            // update the peer RPA in the dynamic filter list table
            (void)LL_DFL_UpdateEntry( pDynamicFL,
                                      pRankFLTable,
                                      pResolvingList[rlIndex].RPA,
                                      peerAddr );

            // update RL RPA for this peer with ScanA
            memcpy( pResolvingList[rlIndex].RPA, peerAddr, B_ADDR_LEN );
            memcpy( peerAddr, pResolvingList[rlIndex].idAddr, B_ADDR_LEN);

            // it is, so use ID address and address type
            peerAddrType = pResolvingList[rlIndex].idAddrType | LL_DEV_ADDR_TYPE_ID_MASK;
          }
        }
        else
        {
          (void)LL_DFL_AddEntry( pDynamicFL, pRankFLTable, peerAddr, peerAddrType);
        }
      }
      else
      {
        sendReq = FALSE;
      }

      // check if there's a register callback and if a Scan Request Report is needed
      if (( MAP_llCheckCBack(LL_CBACK_EXT_SCAN_REQ_RECEIVED) == UFALSE) ||
          (( pAdvSet->pAdvParam->notifyEnableFlags & (uint8)AE_NOTIFY_ENABLE_SCAN_REQUEST ) == UFALSE) )
      {
        sendReq = FALSE;
      }

      if ( sendReq == UTRUE )
      {
        aeScanReqReceived_t *scanReqRpt;

        // Use allocLimited to avoid overflowing the heap...
        scanReqRpt = MAP_osal_mem_allocLimited( sizeof(aeScanReqReceived_t) );

        // check if we got the memory
        if ( scanReqRpt != NULL )
        {
          scanReqRpt->subCode      = AE_ADV_HCI_BLE_SCAN_REQUEST_RECEIVED_EVENT;
          scanReqRpt->handle       = aeCurHandle;
          scanReqRpt->scanAddrType = peerAddrType;
          scanReqRpt->channel      = GET_CHANNEL_IDX(RCL_BLE5_getRxChannel(rxEntry));
          scanReqRpt->rssi         = RCL_BLE5_getRxRssi(rxEntry);

          memcpy( scanReqRpt->scanAddr, peerAddr, B_ADDR_LEN );
          MAP_llExtAdvCBack( LL_CBACK_EXT_SCAN_REQ_RECEIVED, (void *)scanReqRpt );
        }
        else // out of memory
        {
          MAP_llExtAdvCBack( LL_CBACK_OUT_OF_MEMORY, NULL );
        }
      }

    // We are finished with handling the scan request. Remove it from the RX queue
    rxEntry = RCL_MultiBuffer_RxEntry_get(&((aeRf_t *)pAdvSet->pRfCmds)->advParam.rxBuffers, NULL);
    }
  }
}

/*******************************************************************************
 * This function is used by LL_rclAdvCallback to manage the command TX queue
 *
 */
void LL_rclAdvTxFinished( void)
{
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );

  if ( pAdvSet == NULL )
  {
    return;
  }

#ifdef USE_AE
  // only non-legacy allowed for this ISR
  if ( !TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
  {
    aeRf_t *pRf = (aeRf_t *)pAdvSet->pRfCmds;
    uint8 remFrag = 0;
    uint8 auxChainExtHdrSize;
    uint8 payloadLen;
    uint8 auxChainHdrFlags = pAdvSet->auxHdrFlags;

    // Check if we have a chain
    if ( pAdvSet->numFrags > 1 )
    {
      // Calculate the number of the packets still needed to be added to the TX queue
      remFrag = pAdvSet->numFrags - (pAdvSet->txCount + 1);
    }

      // only scannable and NC/NS modes send AUX_CHAIN_IND
      // and will enter this section
      if ( remFrag )
      {
        // advance the pointer
        pRf->comPkt.pAdvData += pAdvSet->fragLen;
        pRf->comPkt.advDataLen = pAdvSet->fragLen;

        // AUX_CHAIN_IND pkt is not permitted to send ADV A , Target A or syncInfo
        CLR_EXTHDR_FLAG( auxChainHdrFlags, EXTHDR_FLAG_ADVA );
        CLR_EXTHDR_FLAG( auxChainHdrFlags, EXTHDR_FLAG_TARGETA );
        CLR_EXTHDR_FLAG( auxChainHdrFlags, EXTHDR_FLAG_SYNCINFO );

        // check if the next fragment will be the last fragment
        if ( remFrag == 1 )
        {
          // Clear AuxPtr
          CLR_EXTHDR_FLAG( auxChainHdrFlags, EXTHDR_FLAG_AUXPTR );

          // Setup last fragment length
          pRf->comPkt.advDataLen = pAdvSet->lastFragLen;
        }
        else
        {
          // Add auxPtr because its not the last pkt
          // we need to make sure its set because in scannable and
          // connectable mode this flag was cleared in AUX_ADV_IND pkt
          SET_EXTHDR_FLAG( auxChainHdrFlags, EXTHDR_FLAG_AUXPTR );
        }

        // Tx power is optional. add Tx Power if asked by App
        if(TST_AE_PROPS_TX_PWR(pAdvSet->pAdvParam->eventProps) == UTRUE)
        {
          SET_EXTHDR_FLAG(pRf->comPkt.extHdrFlags, EXTHDR_FLAG_TXPWR);
        }
        // Build AUX_CHAIN_IND packet and add it to the command TX queue
        MAP_llSetupExtHdr(pAdvSet, auxChainHdrFlags, 0 );
        auxChainExtHdrSize = MAP_llGetExtHdrLen( auxChainHdrFlags );
        payloadLen = 1 + auxChainExtHdrSize + pRf->comPkt.advDataLen;
        pRf->comPkt.extHdrFlags = auxChainHdrFlags;
        // AUX_CHAIN_IND pkt is needed to be sent as NC/NS mode
        SET_ADV_MODE( pRf->comPkt.extHdrInfo,
                      AE_ADV_MODE_NONCONN_NONSCAN );
        /* No need to check the status. If an error occured the RCL will not have another
         * packet in the chain to transmit and it will finish the advertiser command.
         * A new command will be prepared in llExtAdv_PostProcess */
        (void)MAP_llAddExtAdvPacketToTx(pAdvSet, LL_PKT_TYPE_AUX_CHAIN_IND, payloadLen);

        // if scannable mode change the state back to scannable
        if (TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps))
        {
          SET_ADV_MODE( pRf->comPkt.extHdrInfo,
                        AE_ADV_MODE_SCANNABLE );
        }
      }
    pAdvSet->txCount++;
  }
#endif // USE_AE
}

/*******************************************************************************
 * This is the common RCL callback used for periodic advertise command.
 *
 * @Design: BLE_LOKI-1453
 * @Design: BLE_LOKI-1795
 *
 */
void LL_rclPeriodicAdvCallback(RCL_Command *cmd,
                               LRF_Events lrfEvents,
                               RCL_Events events)
{
  if (llState != LL_STATE_PERIODIC_ADV)
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
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_CMD_STARTED );
  }

  if ( events.txBufferFinished )
  {
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_PERIODIC_ADV_TX_BUFF_FINISHED );
  }

  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_PERIODIC_ADV_LAST_CMD_DONE );
  }

  return;
}

#endif // (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * This is the common RCL callback used for scan command.
 *
 * @Design: BLE_LOKI-1455
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
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_CMD_STARTED );
  }

  if ( events.rxEntryAvail )
  {
    //////////////////////////////////////////////////////////////////////////////
    // Rx Entry Done
    //////////////////////////////////////////////////////////////////////////////
    if (lrfEvents.rxOk)
    {
      (void)MAP_osal_set_event( LL_TaskID, LL_EVT_SCAN_RX_AVAIL );
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_SCAN_LAST_CMD_DONE );
  }

  return;
}

/*******************************************************************************
 * This is the common RCL callback used for periodic scan command.
 *
 * @Design: BLE_LOKI-2022
 *
 */
void LL_rclPeriodicScanCallback(RCL_Command *cmd,
                     LRF_Events lrfEvents,
                     RCL_Events events)
{
  if (llState != LL_STATE_PERIODIC_SCAN)
  {
    MAP_llHaltRadio( (uint32)cmd );
    LL_rclRescheduleCommand(cmd);
    return;
  }
  if ( events.rxEntryAvail )
  {
    //////////////////////////////////////////////////////////////////////////////
    // Rx Entry Done
    //////////////////////////////////////////////////////////////////////////////
    if (lrfEvents.rxOk)
    {
      // process Periodic Scan Rx Avail
      (void)MAP_osal_set_event( LL_TaskID, LL_EVT_PERIODIC_SCAN_RX_AVAIL );
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    // process Periodic Scan Post Process
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_PERIODIC_SCAN_LAST_CMD_DONE );
  }

  return;
}
/*******************************************************************************
 * This is the common RCL function used for scan command. This function is used
 * to check if the packet should be ignored due to NPM mode restrictions or not.
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
  pAdvPkt = (uint8 *)&rxEntry->data[ADV_DATA_INDEX];

  // Extended advertising
  if (LL_ADV_EXT_IND_PDU( *pAdvPkt ))
  {
    // check if there's a peer address
    if ( TST_EXTHDR_FLAG(pAdvPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE], EXTHDR_FLAG_ADVA) )
    {
      // copy addresses so there's no race condition or overwrite
      MAP_osal_memcpy( peerAddr,
                       &pAdvPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE+AE_EXT_HDR_FLAGS_SIZE],
                       B_ADDR_LEN );
      // copy address types
      peerAddrType = LL_ADV_HDR_GET_TX_ADD(*pAdvPkt);

    }
  }
  else // Legacy
  {
    // copy addresses so there's no race condition or overwrite
    MAP_osal_memcpy( peerAddr, &pAdvPkt[LL_PKT_HDR_LEN], B_ADDR_LEN );

    // copy address types
    peerAddrType = LL_ADV_HDR_GET_TX_ADD(*pAdvPkt);
  }

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
        rxEntry = RCL_MultiBuffer_RxEntry_get(&extScanParam.rxBuffers, NULL);
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
 * @Design: BLE_LOKI-1468
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
      (void)MAP_osal_set_event( LL_TaskID, LL_EVT_INIT_RX_ENTRY_DONE );
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    if( extInitCmd.common.status == RCL_CommandStatus_RxErr)
    {
      (void)MAP_osal_set_event( LL_TaskID, LL_EVT_INIT_LAST_CMD_DONE_RX_ERR );
    }
    else if (extInitCmd.common.status == RCL_CommandStatus_Connect)
    {
      (void)MAP_osal_set_event( LL_TaskID, LL_EVT_INIT_LAST_CMD_DONE_CONNECT );
    }
    else
    {
      (void)MAP_osal_set_event( LL_TaskID, LL_EVT_INIT_LAST_CMD_DONE );
    }
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
  RCL_Buffer_DataEntry *rxEntry = NULL;
  uint8 *pAdvPkt = NULL;
  uint8 peerAddrType = LL_INVALID_DEV_ADDR_TYPE;
  uint8 peerAddr[B_ADDR_LEN];
  uint8 ownAddrType = LL_INVALID_DEV_ADDR_TYPE;
  uint8 ownAddr[B_ADDR_LEN];

  RCL_MultiBuffer_ListInfo listInfo;
  RCL_MultiBuffer_ListInfo_init(&listInfo, &extInitParam.rxBuffers);
  rxEntry = RCL_MultiBuffer_RxEntry_next(&listInfo);

  if ( rxEntry != NULL )
  {
    pAdvPkt = (uint8 *)&rxEntry->data[ADV_DATA_INDEX];

    // if its an ADV_EXT_IND packet, just extract it from the buffer
    if(LL_AUX_PDU( *pAdvPkt) && !TST_EXTHDR_FLAG(pAdvPkt[LL_PKT_HDR_LEN +
                                                         AE_EXT_HDR_LEN_SIZE],
                                                           EXTHDR_FLAG_ADVA))
    {
      rxEntry = RCL_MultiBuffer_RxEntry_get(&extInitParam.rxBuffers, NULL);
    }

    if ( rxEntry != NULL )
    {
      RCL_Ble5_RxPktStatus rclStatus = RCL_BLE5_getRxStatus(rxEntry);

      if ( rclStatus.ignoredAddr )
      {
        // This is an ignored advertising report and thus not containing the advertising
        // report a connect_ind was sent on so remove the packet from the RX queue
        rxEntry = RCL_MultiBuffer_RxEntry_get(&extInitParam.rxBuffers, NULL);
        peerAddrType = LL_ADV_HDR_GET_TX_ADD(*pAdvPkt);

        if ( LL_ADV_IND_PDU( *pAdvPkt ) ||
            LL_ADV_DIRECT_IND_PDU( *pAdvPkt ) )
        {
          MAP_osal_memcpy( peerAddr, &pAdvPkt[LL_PKT_HDR_LEN], B_ADDR_LEN );
          // check if there is an InitA address
          if ( LL_ADV_DIRECT_IND_PDU( *pAdvPkt ) )
          {
            // copy addresses so there's no race condition or overwrite
            MAP_osal_memcpy( ownAddr, &pAdvPkt[LL_PKT_HDR_LEN+B_ADDR_LEN], B_ADDR_LEN );

            // copy address types
            ownAddrType = LL_ADV_HDR_GET_RX_ADD( *pAdvPkt );
          }
        }
        // Checks if it's an AE packet
        else if(LL_AUX_PDU( *pAdvPkt))
        {
          // extract advA
          MAP_osal_memcpy( peerAddr, &pAdvPkt[AE_AUX_ADVA_INDEX], B_ADDR_LEN );

          // Check if Adv Pkt is Directed
          if ( TST_EXTHDR_FLAG(pAdvPkt[LL_PKT_HDR_LEN+AE_EXT_HDR_LEN_SIZE], EXTHDR_FLAG_TARGETA) )
          {
            // copy addresses so there's no race condition or overwrite
            MAP_osal_memcpy( ownAddr, &pAdvPkt[AE_AUX_TARGETA_INDEX ],
                            B_ADDR_LEN );
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
  }
}
#endif // (CTRL_CONFIG & INIT_CFG)

/*******************************************************************************
 * This is the common RCL callback used for peripheral command.
 *
 * @Design: BLE_LOKI-1470
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
  else
  {
        /* this else clause is required, even if the
           programmer expects this will never be reached
           Fix Misra-C Required: MISRA.IF.NO_ELSE */
  }
  // check if the connection is still valid
  if ( llConns.currentConn == LL_INVALID_CONNECTION_ID )
  {
    // connection may have already been ended by a reset
//      LL_rclRescheduleCommand(cmd);
      return;
  }
  // Handle the case in which we must terminate an already "Created" connection in the future
  if (llConns.llConnection[llConns.currentConn].extFeatureMask & EXT_FEATURE_DISCONNECT_ENABLE)
  {
    // Connection needs to be cleaned up in llScheduler
    return ;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Tx_Entry_Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.txBufferFinished )
  {
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_CONN_TX_BUFF_FINISHED  );
  }
  else
  {
        /* this else clause is required, even if the
           programmer expects this will never be reached
           Fix Misra-C Required: MISRA.IF.NO_ELSE */
  }

  //////////////////////////////////////////////////////////////////////////////
  // Rx Entry Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.rxEntryAvail )
  {
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_CONN_RX_AVAIL );
  }
  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    // Call last command done handling
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_CONN_RX_AVAIL | LL_EVT_PERIPHERAL_LAST_CMD_DONE );  }

  return;
}

/*******************************************************************************
 * This is the common RCL callback used for central command.
 *
 * @Design: BLE_LOKI-1470
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
  // check if the connection is still valid
  if ( llConns.currentConn == LL_INVALID_CONNECTION_ID )
  {
    // connection may have already been ended by a reset
//    LL_rclRescheduleCommand(cmd);
    return;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Tx_Entry_Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.txBufferFinished )
  {
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_CONN_TX_BUFF_FINISHED );
  }

  //////////////////////////////////////////////////////////////////////////////
  // Rx Entry Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.rxEntryAvail )
  {
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_CONN_RX_AVAIL );
  }
  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    // Call last command done handling
    // process Central Cause
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_CONN_RX_AVAIL | LL_EVT_CENTRAL_LAST_CMD_DONE );
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
  if ((llState != LL_STATE_DIRECT_TEST_MODE_TX)         &&
      (llState != LL_STATE_DIRECT_TEST_MODE_RX)         &&
      (llState != LL_STATE_MODEM_TEST_TX)               &&
      (llState != LL_STATE_MODEM_TEST_RX)               &&
      (llState != LL_STATE_MODEM_TEST_TX_FREQ_HOPPING))
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

void ll_rclCsCallback(RCL_Command *cmd, LRF_Events lrfEvents, RCL_Events rclEvents)
{
  if (rclEvents.lastCmdDone)
  {
    taskEndAction = MAP_llCsSubevent_PostProcess;
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
  }
  else
  {
    taskEndAction = MAP_llCsSteps_PostProcess;
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
  }
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
          MAP_llPrepareAndUpdateAlEntry(extInitParam.filterList, flags, resolvingList[rlIndex].RPA, RCL_PEER_ADDR_INDEX);
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
static void LL_rclUpdateExtAl( RCL_FilterList *filterList, uint16 flags, uint8* rpaAddr, uint8 rlIndex)
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
      MAP_llPrepareAndUpdateAlEntry(filterList, flags, resolvingList[rlIndex].RPA, alIndex);
    }
  }
}
#endif // CTRL_CONFIG & INIT_CFG

/*
** Local Functions for Adv state
*/
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
////////////////////////////////////////////////////////////////////////////////
// LastCmdDone Event Handle Connect Request - using dynamic filter list
////////////////////////////////////////////////////////////////////////////////
uint8 llLastCmdDoneEventHandleConnectRequest( void )
{
  RCL_MultiBuffer_ListInfo listInfo;
  RCL_Buffer_DataEntry *rxEntry = NULL;
  uint8 *pData = NULL;
  uint8 connect = FALSE;
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );

  if ( pAdvSet != NULL ) 
  {
  privTestflags_t privacyTestCriteria;
  privTestflags_t privacyTestResults;
  RCL_FilterList *pDynamicFL = NULL;
  rankDynamicFL_t *pRankFLTable = NULL;
  rlEntry_t *pResolvingList = NULL;
  uint8 rpaTypeAddr = FALSE;
  uint8 usingAcceptListFilter = FALSE;
  uint8 rlIndexA = INVALID_RESOLVE_LIST_INDEX;
  uint8 rlIndexB = INVALID_RESOLVE_LIST_INDEX;
  uint8 peerAddrType;
  uint8 peerAddr[B_ADDR_LEN];
  uint8 *pPkt;

  // Init tests flags
  privacyTestCriteria.flags = POLICY_NO_FAILED_TEST;
  privacyTestResults.flags = POLICY_NO_FAILED_TEST;

  do
  {
    // check if RCL finished and ready to establish a connection
    if (((aeRf_t *)pAdvSet->pRfCmds)->advCmd.common.status == RCL_CommandStatus_Connect)
    {
      // initate RCL Rx buffer list
      RCL_MultiBuffer_ListInfo_init(&listInfo, &((aeRf_t *)pAdvSet->pRfCmds)->advParam.rxBuffers);
    }
    else
    {
      // RCL Command status is not RCL_CommandStatus_Connect, continue Advertising
      break;
    }
    rxEntry = RCL_MultiBuffer_RxEntry_next(&listInfo);
    if ( rxEntry == NULL )
    {
      break;
    }
    pData = (uint8 *)&rxEntry->data[ADV_DATA_INDEX];

    // verify the connect indication
    if ( !llValidateConnectIndPkt( pData ) ||
          (MAP_llConnExists(&pData[LL_CONN_IND_INITIATOR_ADDRESS_OFFSET],
                        MASK_ID_ADDRTYPE(pData[LL_CONN_IND_HEADER_OFFSET] >> LL_ADV_PDU_HDR_TXADDR)) == UTRUE))
    {
      // invalid connect_ind - continue with advertising
      break;
    }

    // Connection indication is valid, continue
    pPkt = LL_GET_PDU_HEADER(rxEntry->data, rxEntry->numPad);

    // Copy peer device address and address type
    peerAddrType = LL_ADV_HDR_GET_TX_ADD(*pPkt);
    memcpy( peerAddr, &pPkt[2], B_ADDR_LEN );

    // check if the InitA is an RPA address type
    if ( (MAP_LL_PRIV_IsRPA( peerAddrType, peerAddr )) == UTRUE )
    {
      rpaTypeAddr = TRUE;
    }

    // Check advetiser filter policy
    if ( (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_CONNECT_IND) ||
         (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_ALL_REQ) )
    {
      usingAcceptListFilter = TRUE;
    }
    // Set flags of the required policy tests.
    privacyTestCriteria = LL_PRIV_SetPrivacyTests( peerAddr,
                                                   rpaTypeAddr,
                                                   usingAcceptListFilter );

    // check all the required test to determine whether to approve the packet
    // or not
    privacyTestResults = LL_PRIV_ValidatePrivacyCompliance( peerAddr,
                                                            peerAddrType,
                                                            (uint8*)&rlIndexA,
                                                            privacyTestCriteria );

    // Get resolving List pointer
    pResolvingList = LL_PRIV_GetResolvingList();

    // Check if directed advertisement
    if ( TST_AE_PROPS_DIR(pAdvSet->pAdvParam->eventProps) ||
         TST_AE_PROPS_HDC_DIR(pAdvSet->pAdvParam->eventProps) )
    {
      rlIndexB = MAP_LL_PRIV_IsResolvable( pAdvSet->peerAddr, pResolvingList );
      if ( rlIndexA != rlIndexB )
      {
        // The peer device address doesn't match the advertising
        // parameter peer address
        break;
      }
    }

    // Verify that all the required tests passed in order to accept the packet
    // and connect to the device
    if ( privacyTestResults.flags == POLICY_NO_FAILED_TEST )
    {
      // All the requires policy test passed, thus connect
      connect = TRUE;
      // Get pointer to the dynamic filter list
      pDynamicFL = LL_DFL_GetDynamicFilterlist();
      pRankFLTable = LL_DFL_GetRankTable();
      if ( rpaTypeAddr == UTRUE )
      {
        if ( rlIndexA != INVALID_RESOLVE_LIST_INDEX )
        {
          // update the peer RPA in the dynamic filter list table
          (void)LL_DFL_UpdateEntry( pDynamicFL,
                                    pRankFLTable,
                                    pResolvingList[rlIndexA].RPA,
                                    peerAddr );
          // update the RPA address in the resolving list entry
          memcpy( pResolvingList[rlIndexA].RPA, peerAddr, B_ADDR_LEN );
        }
      }
      else
      {
        // add the identity device address to the dynamic filter list
        (void)LL_DFL_AddEntry( pDynamicFL, pRankFLTable, peerAddr, peerAddrType);
      }
    }
    else // some of the required tests failed, thus reject conn_ind
    {
      break;
    }
  } while ( (bool)FALSE );
  }

  // Connect when the peer device complies with the privacy policy (connect
  // variable is TRUE).
  // Otherwise, continue Advertising (connect variable is FALSE).
  return connect;
}

#endif

/*
** Local Functions for Central and Peripheral state
*/
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
////////////////////////////////////////////////////////////////////////////////
// Rx Entry Done Event Handle for Central and Peripheral state
////////////////////////////////////////////////////////////////////////////////
uint8 llRxEntryDoneEventHandleStateConnection()
{
  halIntState_t  cs;
  llConnState_t *connPtr;
  uint8          opcode;
  uint8         *pPkt;
  uint8          pktLen;
  uint8          pktHdr;
  uint8          pktHdrInfo;
  uint8          recvCte = FALSE;
  RCL_Buffer_DataEntry *pDataEntry = NULL;
  RCL_Ble5_RxPktStatus rxPktStatus;

  // check if the connection is still valid
  if ( llConns.currentConn == LL_INVALID_CONNECTION_ID )
  {
    // connection may have already been ended by a terminate or reset
    return FALSE;
  }

  // get connection information
  connPtr = MAP_llDataGetConnPtr( llConns.currentConn );

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
    // get the packet header
    pktHdrInfo = *pPkt++;
    pktHdr = pktHdrInfo & LL_DATA_PDU_HDR_LLID_MASK;

    // Check that header includes CTE header
    if ( (((pktHdrInfo) & BV(LL_DATA_PDU_HDR_CP_BIT)) != 0) )
    {
      recvCte = TRUE;
    }
    rxPktStatus = RCL_BLE5_getRxStatus(pDataEntry);

    // when receive CRC Error - no need to mark the buffer as available
    if (( rxPktStatus.crcError ) && ( !recvCte ))
    {
      return TRUE;
    }

    // Check if the LLID is invalid
    if ( LL_INVALID_LLID(pktHdr) )
    {
      // it is, so mark buffer as available, and advance to next entry
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
      return TRUE;
    }

    // get the packet length
    pktLen = *pPkt++;

    // Case header includes CTE header
    if ( recvCte )
    {
      // CTE Received with CRC Error
      if (rxPktStatus.crcError)
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

      // This connection is already marked for termination from the previous packet process,
      // we should not continue with the process this packet.
      if (connPtr->termInfo.termIndRcvd == TRUE)
      {
        return TRUE;
      }

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

        // After decryption failure, we should stop the RPA timer and post an LL event to change the RPA properly.
        if ( privInfo.addrResolution == LL_ENABLE_ADDR_RESOLUTION )
        {
          // stop timer
          (void) MAP_osal_stop_timerEx( LL_TaskID, LL_EVT_ADDRESS_RESOLUTION_TIMEOUT );

          // set the event
          (void) MAP_osal_set_event( LL_TaskID, LL_EVT_ADDRESS_RESOLUTION_TIMEOUT );
        }
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
      connPtr->lastRssi = RCL_readRssi();

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
      // Get the opcode
      opcode = *pPkt;

      // in case RF does not support CTE , we will receive the CTE response len as 2
      if (( recvCte ) &&
         ((opcode == LL_CTRL_CTE_RSP) && (pktLen == (LL_CTE_RSP_PAYLOAD_LEN + 1))))
      {
        pktLen = LL_CTE_RSP_PAYLOAD_LEN;
      }

      // Check if we failed to verify packet length
      if ( pktLen != ctrlPktLenTable[opcode] )
      {
        // Control packet received for features that are not supported or length did not match the opcode
        connPtr->unknownCtrlType = *pPkt;

        // Queue up an unknown response
        MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP );

        // ALT: setup/send an Unknown Response immediately
        //(void)MAP_llSetupUnknownRsp( connPtr );
      }
      else // control packet length is okay
      {
        // process control packet
        if ( llState == LL_STATE_CONN_CENTRAL )
        {
          MAP_llProcessCentralControlPacket( connPtr, pPkt );
        }
        else // LL_STATE_CONN_PERIPHERAL
        {
          MAP_llProcessPeripheralControlPacket( connPtr, pPkt );
        }
      }
    }
  } // while


  // mark data entry as free
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
  if (llState == LL_STATE_DIRECT_TEST_MODE_TX)
  {
    // get the command status
    taskEndStatus = txDtmTestCmd.common.status;

    if (taskEndStatus == RCL_CommandStatus_Error_Synth)
    {
      // The command will automatically stop if there is a Synth error
      // Post the command again in such a case and continue executing
      RCL_Command_submit(MAP_llScheduler_getHandle(LL_TASK_ID_STANDARD_BLE), (RCL_Command_Handle)&txDtmTestCmd);
    }
    else if (dtmInfo->txPktCnt != LL_EXT_DTM_TX_CONTINUOUS)
    {
      // generate a callback for the packet report
      // Note: For TX, the number of received packets is always zero.
      MAP_LL_DirectTestEndDoneCback( 0, LL_DIRECT_TEST_MODE_TX );

      // back to Idle
      llState = LL_STATE_IDLE;
    }
    else
    {
        /* this else clause is required, even if the
           programmer expects this will never be reached
           Fix Misra-C Required: MISRA.IF.NO_ELSE */
    }
  }
  else if (llState == LL_STATE_DIRECT_TEST_MODE_RX)
  {
    // get the command status
    taskEndStatus = rxTestCmd.common.status;
    if (taskEndStatus == RCL_CommandStatus_Error_Synth)
    {
      // post the command
      RCL_Command_submit(MAP_llScheduler_getHandle(LL_TASK_ID_STANDARD_BLE), (RCL_Command_Handle)&rxTestCmd);
    }
  }

  else if ( llState == LL_STATE_MODEM_TEST_TX_FREQ_HOPPING )
  {
      /**
       * This part is used for modem tests with channel frequency hopping, such as:
       * LL_EXT_EnhancedModemHopTestTx, LL_EXT_ModemHopTestTx.
       *
       * In those tests, a data packet is transmitted on a different frequency
       * (linearly stepping through all RF channels 0..39).
       * The channel update is taking place in this part.
       */

      // get the command status
      taskEndStatus = txDtmTestCmd.common.status;

      // command finished unsuccessfully
      if (taskEndStatus == RCL_CommandStatus_Error_Synth)
      {
        // post the command
        RCL_Command_submit(MAP_llScheduler_getHandle(LL_TASK_ID_STANDARD_BLE), (RCL_Command_Handle)&txDtmTestCmd);
      }

      // command finished successfully
      if ( taskEndStatus == RCL_CommandStatus_Finished )
      {
          // incrementing the Physical channel by 1 until we reach channel 39
          // then start the cycle all over again
          txDtmTestCmd.channel += 1;

          if ( txDtmTestCmd.channel > LL_LAST_RF_CHAN_ADJ )
          {
              txDtmTestCmd.channel = LL_FIRST_RF_CHAN_ADJ;
          }

          // clear status
          txDtmTestCmd.common.status = RCL_CommandStatus_Idle;

          // resubmit the command in the callback
          RCL_Command_submit(MAP_llScheduler_getHandle(LL_TASK_ID_STANDARD_BLE), (RCL_Command_Handle)&txDtmTestCmd);

          return TRUE;
      }
  }
  else if ( (llState == LL_STATE_MODEM_TEST_TX)  ||
            (llState == LL_STATE_MODEM_TEST_RX) )
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

  return TRUE;
}

/*******************************************************************************
 */
