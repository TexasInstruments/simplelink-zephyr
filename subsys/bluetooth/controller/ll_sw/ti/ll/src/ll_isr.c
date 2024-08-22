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
#include "hal_gpio_wrapper.h"
#include "ll_ae.h"
#include "cs/ll_cs_rcl.h"
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

extern sortedAdv_t *pNextAdvSet;

extern const uint8 ctrlPktLenTable[NUM_OF_CTRL_PKT];

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
void llRecordTxUsage( void );
void llHandleSDAALastCmdDone( void );

void LL_rclAdvRxEntryDone( void );
void LL_rclScanRxEntryDone( void );
void LL_rclAdvTxFinished( void);
void LL_rclPeriodicAdvTxFinished( void);
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
  taskEndAction = MAP_llScheduler;
  (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
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
    llCmdStartedEventHandle();
  }

  if ( events.txBufferFinished )
  {
    LL_rclAdvTxFinished();
  }

  //////////////////////////////////////////////////////////////////////////////
  // Rx Entry Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.rxEntryAvail )
  {
    if (lrfEvents.rxOk)
    {
      MAP_LL_rclAdvRxEntryDone();
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
  RCL_MultiBuffer_ListInfo_init(&listInfo,&((aeRf_t *)pAdvSet->pRfCmds)->advParam.rxBuffers);
  rxEntry = RCL_MultiBuffer_RxEntry_next(&listInfo);

  if( rxEntry != NULL )
  {
    // Get the LL PDU header
    pPkt = LL_GET_PDU_HEADER(rxEntry->data, rxEntry->numPad);

    // Note: LL_PKT_TYPE_SCAN_REQ and LL_PKT_TYPE_AUX_SCAN_REQ have the same value.
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
        aeScanReqReceived_t *scanReqRpt;

        // Use allocLimited to avoid overflowing the heap...
        scanReqRpt = MAP_osal_mem_allocLimited( sizeof(aeScanReqReceived_t) );

        // check if we got the memory
        if ( scanReqRpt )
        {
          scanReqRpt->subCode      = AE_ADV_HCI_BLE_SCAN_REQUEST_RECEIVED_EVENT;
          scanReqRpt->handle       = aeCurHandle;
          scanReqRpt->scanAddrType = peerAddrType;
          scanReqRpt->channel      = GET_CHANNEL_IDX(RCL_BLE5_getRxChannel(rxEntry));
          scanReqRpt->rssi         = RCL_BLE5_getRxRssi(rxEntry);

          MAP_osal_memcpy( scanReqRpt->scanAddr, peerAddr, B_ADDR_LEN );
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
 * This function is used by LL_rclAdvCallback to handle reception of SCAN_REQ packets
 * In case a callback was registered by the application, it will be called
 * This function using with dynamic filter list
 */
void LL_rclAdvRxEntryDoneDFL( void )
{
  RCL_Buffer_DataEntry      *rxEntry;
  RCL_MultiBuffer_ListInfo  listInfo;
  privTestflags_t           policyTests = POLICY_NO_FAILED_TEST;
  privTestflags_t           testResults = POLICY_NO_FAILED_TEST;
  dynamicFL_t               *pDynamicFL = NULL;
  rankDynamicFL_t           *pRankFLTable = NULL;
  rlEntry_t                 *pResolvingList = NULL;
  uint8                     rpaTypeAddr = FALSE;

  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );
  uint8    *pPkt;
  uint8    peerAddrType;
  uint8    peerAddr[B_ADDR_LEN];
  uint8    sendReq = TRUE;
  uint8    rlIndex = INVALID_RESOLVE_LIST_INDEX;

  // Get MultiBuffer
  RCL_MultiBuffer_ListInfo_init(&listInfo, &((aeRf_t *)pAdvSet->pRfCmds)->advParam.rxBuffers);
  rxEntry = RCL_MultiBuffer_RxEntry_next(&listInfo);

  // Get the LL PDU header
  pPkt = LL_GET_PDU_HEADER(rxEntry->data, rxEntry->numPad);

  if ( LL_SCAN_REQ_PDU( *pPkt ) )
  {
    (void)MAP_osal_memcpy( peerAddr, &pPkt[2], B_ADDR_LEN );
    peerAddrType = LL_ADV_HDR_GET_TX_ADD(*pPkt);

    // check if the InitA is an RPA address type
    if ( (MAP_LL_PRIV_IsRPA( peerAddrType, peerAddr )) == TRUE )
    {
      rpaTypeAddr = TRUE;

      // Get resolving List pointer
      pResolvingList = LL_PRIV_GetResolvingList();

      // Get resolving list entry index
      rlIndex = MAP_LL_PRIV_IsResolvable( peerAddr, pResolvingList );

      // set test flag of address resolution enable
      SET_ADDRESS_RESOLUTION_TEST( policyTests );
    }
    else
    {
      // set test flag of privacy mode and IRK validation
      SET_DPM_OR_INVALID_IRK_TEST( policyTests );
    }
    // Check advertiser filter policy
    if ( (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_SCAN_REQ) ||
         (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_ALL_REQ) )
    {
      // set test flag of existance in accept list
      SET_ADDRESS_IN_ACCEPT_LIST_TEST( policyTests );

      if(rpaTypeAddr == UTRUE)
      {
        // The RPA address shall be resolvable only when using accept list filter
        SET_RESOLVABLE_RPA_TEST( policyTests );
      }
    }
    // check all the required test to determine whether to approve the packet
    // or not
    testResults = LL_PRIV_PrivacyPolicyTests( peerAddr,
                                              peerAddrType,
                                              rlIndex,
                                              policyTests );

    if ( testResults == POLICY_NO_FAILED_TEST )
    {
      // Get pointer to the dynamic filter list
      pDynamicFL = LL_DFL_GetDynamicFilterlist();
      pRankFLTable = LL_DFL_GetRankTable();

      if ( rpaTypeAddr == UTRUE )
      {
        if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
        {
          // update the peer RPA in the dynamic filter list table
          (void)LL_DFL_UpdateEntry( pDynamicFL,
                              pRankFLTable,
                              pResolvingList[rlIndex].RPA,
                              peerAddr );

          // update RL RPA for this peer with ScanA
          (void)MAP_osal_memcpy( pResolvingList[rlIndex].RPA, peerAddr, B_ADDR_LEN );
          (void)MAP_osal_memcpy( peerAddr, pResolvingList[rlIndex].idAddr, B_ADDR_LEN);

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

        (void)MAP_osal_memcpy( scanReqRpt->scanAddr, peerAddr, B_ADDR_LEN );
        MAP_llExtAdvCBack( LL_CBACK_EXT_SCAN_REQ_RECEIVED, (void *)scanReqRpt );
      }
      else // out of memory
      {
        MAP_llExtAdvCBack( LL_CBACK_OUT_OF_MEMORY, NULL );
      }
    }

    // We are finished with handling the scan request. Remove it from the RX queue
    rxEntry = RCL_MultiBuffer_RxEntry_get(&((aeLegacyRf_t *)pAdvSet->pRfCmds)->advParam.rxBuffers, NULL);
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
    llCmdStartedEventHandle();
  }

  if ( events.txBufferFinished )
  {
    LL_rclPeriodicAdvTxFinished();
  }

  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    MAP_llLastCmdDoneEventHandleStatePeriodicAdv();
  }

  return;
}

/*******************************************************************************
 * This function is used by LL_rclPeriodicAdvCallback to manage the command TX queue
 *
 */
void LL_rclPeriodicAdvTxFinished( void )
{
#ifdef USE_PERIODIC_ADV
  MAP_llUpdatePeriodicAdvChainPacket( llPeriodicAdv.currentAdv );
#endif
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
      llProcessPeriodicScanRxFIFO();
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    MAP_llLastCmdDoneEventHandleStatePeriodicScan();
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
  pAdvPkt = (uint8 *)&rxEntry->data[ADV_DATA_INDEX];

  // if its an ADV_EXT_IND packet, just extract it from the buffer
  if(LL_AUX_PDU( *pAdvPkt) && !TST_EXTHDR_FLAG(pAdvPkt[LL_PKT_HDR_LEN +
                                                       AE_EXT_HDR_LEN_SIZE],
                                                         EXTHDR_FLAG_ADVA))
  {
    rxEntry = RCL_MultiBuffer_RxEntry_get(&extInitParam.rxBuffers, NULL);
  }

  RCL_Ble5_RxPktStatus rclStatus = RCL_BLE5_getRxStatus(rxEntry);
  if ( rclStatus.ignoredRpa )
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
  //////////////////////////////////////////////////////////////////////////////
  // Tx_Entry_Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.txBufferFinished )
  {
    // get connection information
    llConnState_t *connPtr = MAP_llDataGetConnPtr( llConns.currentConn );

    MAP_llProcessTxData( connPtr, LL_TX_DATA_CONTEXT_TX_ISR );
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
    if (lrfEvents.rxOk)
    {
      MAP_llRxEntryDoneEventHandleStateConnection( FALSE );
    }
    else if (lrfEvents.rxNok)
    {
      MAP_llRxEntryDoneEventHandleStateConnection( TRUE );
    }
    else
    {
        /* this else clause is required, even if the
           programmer expects this will never be reached
           Fix Misra-C Required: MISRA.IF.NO_ELSE */
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    // Check if there are anymore pakcets in the RX queue that needed to be processed
    (void)MAP_llRxEntryDoneEventHandleStateConnection( FALSE );

    // Call last command done handling
    MAP_llLastCmdDoneEventHandleStatePeripheral();
  }

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
    else
    {
        /* this else clause is required, even if the
           programmer expects this will never be reached
           Fix Misra-C Required: MISRA.IF.NO_ELSE */
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  // Last Command Done
  //////////////////////////////////////////////////////////////////////////////
  if ( events.lastCmdDone )
  {
    // Check if there are anymore pakcets in the RX queue that needed to be processed
    (void)MAP_llRxEntryDoneEventHandleStateConnection( FALSE );

    // Call last command done handling
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
// LastCmdDone Event Handle Connect Request
////////////////////////////////////////////////////////////////////////////////
uint8 llLastCmdDoneEventHandleConnectRequest( advSet_t *pAdvSet )
{
    RCL_MultiBuffer_ListInfo listInfo;
    // check if RCL finished and ready to establish a connection
    if (((aeRf_t *)pAdvSet->pRfCmds)->advCmd.common.status == RCL_CommandStatus_Connect)
    {
        // initate RCL Rx buffer list
        RCL_MultiBuffer_ListInfo_init(&listInfo, &((aeRf_t *)pAdvSet->pRfCmds)->advParam.rxBuffers);
    }
    else
   {
        // RCL Command status is not RCL_CommandStatus_Connect, continue Advertising
        return FALSE;
   }

    RCL_Buffer_DataEntry *rxEntry = RCL_MultiBuffer_RxEntry_next(&listInfo);
    uint8 *pData = (uint8 *)&rxEntry->data[ADV_DATA_INDEX];
    uint8 rlIndexA;
    uint8 rlIndexB;
    uint8 peerAddrType;
    uint8 peerAddr[B_ADDR_LEN];
    uint8 *pPkt;

    // verify the connect indication
    if ( !llValidateConnectIndPkt( pData )                                                        ||
         (MAP_llConnExists(&pData[LL_CONN_IND_INITIATOR_ADDRESS_OFFSET],
                          MASK_ID_ADDRTYPE(pData[LL_CONN_IND_HEADER_OFFSET] >> LL_ADV_PDU_HDR_TXADDR)) == UTRUE))
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

////////////////////////////////////////////////////////////////////////////////
// LastCmdDone Event Handle Connect Request - using dynamic filter list
////////////////////////////////////////////////////////////////////////////////
uint8 llLastCmdDoneEventHandleConnectRequestDFL( advSet_t *pAdvSet )
{
  uint8 connect = FALSE;
  if (((aeRf_t *)pAdvSet->pRfCmds)->advCmd.common.status == RCL_CommandStatus_Connect)
  {
    RCL_MultiBuffer_ListInfo listInfo;
    RCL_MultiBuffer_ListInfo_init(&listInfo, &((aeRf_t *)pAdvSet->pRfCmds)->advParam.rxBuffers);
    RCL_Buffer_DataEntry *rxEntry = RCL_MultiBuffer_RxEntry_next(&listInfo);
    privTestflags_t policyTests = POLICY_NO_FAILED_TEST;
    privTestflags_t testResults = POLICY_NO_FAILED_TEST;
    dynamicFL_t *pDynamicFL = NULL;
    rankDynamicFL_t *pRankFLTable = NULL;
    rlEntry_t *pResolvingList = NULL;
    uint8 rpaTypeAddr = FALSE;
    uint8 *pData = (uint8 *)&rxEntry->data[ADV_DATA_INDEX];
    uint8 rlIndexA = INVALID_RESOLVE_LIST_INDEX;
    uint8 rlIndexB = INVALID_RESOLVE_LIST_INDEX;
    uint8 peerAddrType;
    uint8 peerAddr[B_ADDR_LEN];
    uint8 *pPkt;
    do
    {
      // verify the connect indication
      if ( !llValidateConnectIndPkt( pData ) ||
            (MAP_llConnExists(&pData[LL_CONN_IND_INITIATOR_ADDRESS_OFFSET],
                          MASK_ID_ADDRTYPE(pData[LL_CONN_IND_HEADER_OFFSET] >> LL_ADV_PDU_HDR_TXADDR)) == UTRUE))
      {
        // invalid connect_ind - continue with advertising
        connect = FALSE;
        break;
      }

      // Connection indication is valid, continue
      pPkt = LL_GET_PDU_HEADER(rxEntry->data, rxEntry->numPad);

      // Copy peer device address and address type
      peerAddrType = LL_ADV_HDR_GET_TX_ADD(*pPkt);
      (void)MAP_osal_memcpy( peerAddr, &pPkt[2], B_ADDR_LEN );

      // Check if directed advertisement
      if ( TST_AE_PROPS_DIR(pAdvSet->pAdvParam->eventProps) ||
           TST_AE_PROPS_HDC_DIR(pAdvSet->pAdvParam->eventProps) )
      {
        rlIndexB = MAP_LL_PRIV_IsResolvable( pAdvSet->peerAddr, resolvingList );
        if ( rlIndexA != rlIndexB )
        {
          // The peer device address doesn't match the advertising
          // parameter peer address
          connect = FALSE;
          break;
        }
      }

      // check if the InitA is an RPA address type
      if ( (MAP_LL_PRIV_IsRPA( peerAddrType, peerAddr )) == UTRUE )
      {
        rpaTypeAddr = TRUE;

        // Get resolving List pointer
        pResolvingList = LL_PRIV_GetResolvingList();

        // Get resolving list entry index
        rlIndexA = MAP_LL_PRIV_IsResolvable( peerAddr, pResolvingList );

        // set test flag of address resolution enable
        SET_ADDRESS_RESOLUTION_TEST( policyTests );
      }
      else
      {
        // set test flag of privacy mode and IRK validation
        SET_DPM_OR_INVALID_IRK_TEST( policyTests );
      }
      // Check advetiser filter policy
      if ( (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_CONNECT_IND) ||
           (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_ALL_REQ) )
      {
        // set test flag of existance in accept list
        SET_ADDRESS_IN_ACCEPT_LIST_TEST( policyTests );

        if(rpaTypeAddr == UTRUE)
        {
          // The RPA address shall be resolvable only when using accept list filter
          SET_RESOLVABLE_RPA_TEST( policyTests );
        }
      }
      // check all the required test to determine whether to approve the packet
      // or not
      testResults = LL_PRIV_PrivacyPolicyTests( peerAddr,
                                                peerAddrType,
                                                rlIndexA,
                                                policyTests );

      // Verify that all the required tests passed in order to accept the packet
      // and connect to the device
      if ( testResults == POLICY_NO_FAILED_TEST )
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
                                resolvingList[rlIndexA].RPA,
                                peerAddr );

            // update the RPA address in the resolving list entry
            (void)MAP_osal_memcpy( resolvingList[rlIndexA].RPA, peerAddr, B_ADDR_LEN );
            // update the identity device address of InitA device in peer address.
            (void)MAP_osal_memcpy( peerAddr, resolvingList[rlIndexA].idAddr, B_ADDR_LEN);
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
        connect = FALSE;
        break;
      }

    } while (FALSE);
  }

  /* RCL Command status is not RCL_CommandStatus_Connect, continue Advertising
     Otherwise - connection is initiated - verify with Maxim
  */
  return connect;
}

#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
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

  // check for receive connect request
  if (MAP_llLastCmdDoneEventHandleConnectRequest(pAdvSet) == TRUE)
  {
    taskEndAction = MAP_llAdv_TaskConnect;

    // process RF End Cause
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );

    return TRUE;
  }
  else
  {
    taskEndAction = MAP_llExtAdv_PostProcess;

    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );

    return FALSE;
  }
}

#ifdef USE_AE
////////////////////////////////////////////////////////////////////////////////
// TxDone Event Handle for Extended ADV state
////////////////////////////////////////////////////////////////////////////////
uint8 llTxDoneEventHandleStateExtAdv( advSet_t *pAdvSet )
{
  return TRUE;
}
#endif

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
    pPkt = (uint8 *)((aeRf_t *)pAdvSet->pRfCmds)->advParam.pRXQ->pCurEntry + sizeof( dataEntry_t );
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

////////////////////////////////////////////////////////////////////////////////
// LastCmdDone Event Handle for Periodic ADV state
////////////////////////////////////////////////////////////////////////////////
uint8 llLastCmdDoneEventHandleStatePeriodicAdv( void )
{
  taskEndAction = MAP_llPeriodicAdv_PostProcess;

  (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );

  return TRUE;
}
#endif //(ADV_NCONN_CFG | ADV_CONN_CFG)

/*
** Local Functions for Scan state
*/
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
////////////////////////////////////////////////////////////////////////////////
// LastCmdDone Event Handle for SCAN state
////////////////////////////////////////////////////////////////////////////////
uint8 llLastCmdDoneEventHandleStateScan( void )
{
  taskEndAction = MAP_llExtScan_PostProcess;
  // process RF End Cause
  (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
  return TRUE;
}

/*
** Local Functions for Periodic Scan state
*/
#ifdef USE_PERIODIC_SCAN
////////////////////////////////////////////////////////////////////////////////
// LastCmdDone Event Handle for SCAN state
////////////////////////////////////////////////////////////////////////////////
uint8 llLastCmdDoneEventHandleStatePeriodicScan( void )
{
  taskEndAction = MAP_llPeriodicScan_PostProcess;
  // process RF End Cause
  (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
  return TRUE;
}

#endif

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
////////////////////////////////////////////////////////////////////////////////
// LastCmdDone Event Handle for Init state
////////////////////////////////////////////////////////////////////////////////
uint8 llLastCmdDoneEventHandleStateInit( void )
{
  if (extInitCmd.common.status == RCL_CommandStatus_Connect)
  {
    taskEndAction = MAP_llInit_TaskConnect;
  }
  else
  {
    taskEndAction = MAP_llExtInit_PostProcess;
  }
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
   taskEndAction = MAP_llPeripheral_TaskEnd;
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
  taskEndAction = MAP_llCentral_TaskEnd;

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

  RCL_Buffer_DataEntry *pDataEntry;
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

  // when receive CRC Error - no need to mark the buffer as available
  if (( crcError ) && ( !recvCte ))
  {
    return TRUE;
  }

  // check if the LLID is invalid
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
    // First check for CS since it is not sequential like the rest of the pkts
    if( (*pPkt >= LL_CTRL_CS_SEC_RSP) &&
        (pktLen != ctrlPktLenTable[*pPkt-LL_CS_CTRL_DLTA]) &&
    // Note: Since the opcode values are sequential, we can compare it
    //       directly to the size of the table to see if it is valid.
      ( (*pPkt >= NUM_OF_CTRL_PKT) ||
         (pktLen != ctrlPktLenTable[*pPkt]) ))
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

////////////////////////////////////////////////////////////////////////////////
// llRecordTxUsage
// Note: This function is called after every RF_EventTxDone and
//       checks whether the next send will cross the limit
////////////////////////////////////////////////////////////////////////////////

void llRecordTxUsage()
{
#ifdef SDAA_ENABLE //NOTE: USE RFLIB API!!
    // if SDAA module is disable return
    if(!MAP_LL_Is_SDAA_Enable())
    {
      return;
    }
    else
    {
      taskInfo_t *llTask = MAP_llGetCurrentTask();

      if(llTask == NULL)
      {
        // Unexpected BLE state!
        LL_ASSERT( FALSE );
        return;
      }

      uint8 byteLength =0xFF;
      uint8 phyType = 0xFF;
      uint8 curChannel = 0xFF;
#ifdef USE_PERIODIC_ADV
      llPeriodicAdvSet_t *pPeriodicAdv;
#endif //USE_PERIODIC_ADV
      int8 txPower = llGetTxPower();
      ble5OpCmd_t *currCmd  = ((ble5OpCmd_t *)llTask->command);
      switch (llTask->taskID)
      {
        case LL_TASK_ID_CENTRAL:
        case LL_TASK_ID_PERIPHERAL:
        {
          llConnState_t * connPtr = MAP_llDataGetConnPtr( llConns.currentConn );

          if(connPtr != NULL)
          {
            linkParam_t *currRFCmd = (linkParam_t *)(currCmd->pParams);
            phyType = connPtr->phyInfo.curPhy;
            curChannel = currCmd->chan;
            if(currRFCmd->pTXQ != NULL && currRFCmd->pTXQ->pCurEntry != NULL)
            {
              byteLength = currRFCmd->pTXQ->pCurEntry->length;
            }
            else
            {
              byteLength = 0;
            }
          }
          break;
        }
#ifdef USE_PERIODIC_ADV
        case LL_TASK_ID_PERIODIC_ADVERTISER:
        {
          pPeriodicAdv = MAP_llGetCurrentPeriodicAdv();
          byteLength = pPeriodicAdv->dataCmd.dataLen;
          curChannel = pPeriodicAdv->currentChan;
          phyType = pPeriodicAdv->phy;
          break;
        }
#endif //USE_PERIODIC_ADV
        case LL_TASK_ID_ADVERTISER:
        {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
#ifdef USE_AE
          if(TST_AE_PROPS_LEGACY(pNextAdvSet->AdvEntry->pAdvParam->eventProps) == FALSE)
          {
            // extended Adv
            byteLength = pNextAdvSet->AdvEntry->dataLen;
            curChannel = pNextAdvSet->AdvEntry->auxChanIndex;
            phyType = pNextAdvSet->AdvEntry->pAdvParam->secPhy;
          }
#endif //USE_AE
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
          break;
        }
        default:
          break;
      }
      if(curChannel != 0xFF)
      {
        MAP_LL_SDAA_RecordTxUsage(byteLength, phyType, txPower, curChannel);
      }
    }

#endif //SDAA_ENABLE
}

////////////////////////////////////////////////////////////////////////////////
// llHandleSDAALastCmdDone
// Note: This function is called after every RF_EventLastCmdDone and
//       handle SDAA record dwell time and combine TX queue if is neccessry
////////////////////////////////////////////////////////////////////////////////
void llHandleSDAALastCmdDone()
{

  // if SDAA module is disable return
  if(!MAP_LL_Is_SDAA_Enable())
  {
    return;
  }
  else
  {
#ifdef SDAA_ENABLE //NOTE: USE RFLIB API!!
    taskInfo_t *llTask = MAP_llGetCurrentTask();
    llConnState_t* connPtr = NULL;
    if(llTask == NULL)
    {
      // Unexpected BLE state!
      LL_ASSERT( FALSE );
      return;
    }

    switch (llTask->taskID)
    {
      case LL_TASK_ID_CENTRAL:
      case LL_TASK_ID_PERIPHERAL:
      {
        connPtr = MAP_llDataGetConnPtr( llConns.currentConn );

        if(connPtr != NULL)
        {
          ble5OpCmd_t * rfCmd = (ble5OpCmd_t *)llTask->command;
          linkParam_t * pParam = (linkParam_t *)rfCmd->pParams;

          //If the TX queue is split - merge it
          if(pParam->pTXQ == NULL)
          {
            pParam->pTXQ = connPtr->pTxDataEntryQ;
          }

          MAP_LL_SDAA_AddDwtRecord(MAP_llGetCurrentTime()-llTask->startTime, llTask->taskID ,connPtr->connId);
        }
        break;
      }
      case LL_TASK_ID_PERIODIC_ADVERTISER:
      {
        MAP_LL_SDAA_AddDwtRecord(MAP_llGetCurrentTime()-llTask->startTime, llTask->taskID ,0);
        break;
      }
      case LL_TASK_ID_ADVERTISER:
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
        if(TST_AE_PROPS_LEGACY(pNextAdvSet->AdvEntry->pAdvParam->eventProps) == FALSE)
        {
          MAP_LL_SDAA_AddDwtRecord(MAP_llGetCurrentTime()-llTask->startTime,llTask->taskID, 0);
          break;
        }
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
        default:
          break;
    }
#endif //SDAA_ENABLE
  }
}

/*******************************************************************************
 */
