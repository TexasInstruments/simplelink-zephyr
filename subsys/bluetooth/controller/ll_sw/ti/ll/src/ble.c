/******************************************************************************

 @file  ble.c

 @brief This file contains the data structures and APIs for CC26xx
        RF Core Firmware Specification for Bluetooth Low Energy.

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
#include <ti/drivers/rcl/RCL.h>
#include <ti/drivers/rcl/commands/ble5.h>
#include "osal_bufmgr.h"
#include "osal_cbtimer.h"
#include "ble.h"
#include "ll.h"
#include "ll_ae.h"
#include "ll_common.h"
#include "ll_enc.h"
#include "ll_config.h"
#include "hci_event.h"
#include "hal_gpio_wrapper.h"
#include "cs/ll_cs_ctrl_pkt_mgr.h"
#include "cs/ll_cs_db.h"

//
#include "rom_jt.h"

/*******************************************************************************
 * MACROS
 */

#ifdef DEBUG
#define RFHAL_ASSERT(cond) {volatile uint8 i = (cond); while(!i);}
#else // !DEBUG
// Note: Use HALNODEBUG to eliminate HAL assert handling (i.e. no assert).
// Note: If HALNODEBUG is not used, use ASSERT_RESET to reset system on assert.
//       Otherwise, evaluation board hazard lights are used.
// Note: Unused input parameter possible when HALNODEBUG; PC-Lint error 715.
#define RFHAL_ASSERT(cond) HAL_ASSERT(cond)
#endif // DEBUG

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

//
// Initiator
//

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
struct
{
  List_Elem            __elem__;
  RCL_BufferState      state;       ///< Buffer state
  uint16_t             length;      ///< Number of bytes in the data field
  uint16_t             headIndex;   ///< Number of bytes consumed
  uint16_t             tailIndex;   ///< Number of bytes written
  union
  {
    /* When using AE, the initiator will receive 3 packets, so the buffer need
       to contain all of them. it's possible to clean the buffer after receiving each packet,
       but it's not recommended because the RCL command is still active. */
    uint8  data[ MAX_EXT_ADV_PKT_SIZE + AUX_CONN_RSP_PKT_SIZE + MAX_BLE_ADV_PKT_SIZE +
                 3 * (LL_PKT_HDR_LEN + RCL_BUFFER_RX_HEADER_ENTRY_SIZE + SUFFIX_MAX_SIZE) ];
    uint32 reserved;
  };
} initDataEntry;

// Init Data finished buffers
RCL_MultiBuffer *initDataQueue = NULL;

connReqData_t connReqData[LL_PHY_NUMBER_OF_PHYS];
#endif // INIT_CFG

//
// Connection - Central/Peripheral
//

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
// Connection Command and Parameters
RCL_CmdBle5Connection *linkCmd;
RCL_CtxConnection     *linkParam;
#endif // ADV_CONN_CFG | INIT_CFG

//
// Connection Data
//
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
// Connection Receive Queue
rxDataQ_t rxDataQ = {0};
txDataQ_t *txDataQ;
// Connection Output
RCL_StatsConnection connOutput;

#endif // ADV_CONN_CFG | INIT_CFG

//
// Scanner
//

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
struct
{
  List_Elem            __elem__;
  RCL_BufferState      state;       ///< Buffer state
  uint16_t             length;      ///< Number of bytes in the data field
  uint16_t             headIndex;   ///< Number of bytes consumed
  uint16_t             tailIndex;   ///< Number of bytes written
  union
  {
    uint8  data[ LL_PKT_HDR_LEN + MAX_BLE_ADV_PKT_SIZE + RCL_BUFFER_RX_HEADER_ENTRY_SIZE + SUFFIX_MAX_SIZE ];
    uint32 reserved;
  };
} scanDataEntry[ NUM_RX_SCAN_ENTRIES ];

// Scan  and Periodic Scan Data finished buffers
List_List     scanDataQueue;

#ifdef USE_PERIODIC_SCAN
//
// Periodic Scanner
//


#endif // USE_PERIODIC_SCAN
#endif // SCAN_CFG

//
// Advertiser
//

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
typedef struct
{
  List_Elem            __elem__;
  RCL_BufferState      state;       ///< Buffer state
  uint16_t             length;      ///< Number of bytes in the data field
  uint16_t             headIndex;   ///< Number of bytes consumed
  uint16_t             tailIndex;   ///< Number of bytes written
  union
  {
    uint8  data[ LL_PKT_HDR_LEN + MAX_BLE_CONNECT_IND_SIZE + RCL_BUFFER_RX_HEADER_ENTRY_SIZE + SUFFIX_MAX_SIZE ];
    uint32 reserved;
  };
} advDataEntry_t;

advDataEntry_t advDataEntry[RCL_NUM_ADV_RX];
RCL_MultiBuffer *pAdvDataEntry = NULL;

#endif // ADV_NCONN_CFG | ADV_CONN_CFG

//
// Direct Test Mode
//
// DTM Command, Parameters and Output
RCL_CmdBle5DtmTx      txDtmTestCmd;
RCL_CmdBle5GenericRx  rxTestCmd;
RCL_CtxGenericRx      rxTestParam;
RCL_StatsGenericRx    rxTestOut;
RCL_CmdBle5TxTest     txTestCmd;

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llReplaceRxBuffers
 *
 * @brief       This routine is used to free all unused data buffers on
 *              the Rx ring buffer. This is done by marking these buffers
 *              as "Finished".
 *
 *              Note: This routine is either called when a LL_LENGTH_REQ or
 *                    LL_LENGTH_RSP is received, or at the end of the
 *                    connection event. In the former case, we should have a
 *                    minimum of 380us before receive a data packet. In the
 *                    latter case, the radio is done so not an issue.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llReplaceRxBuffers( llConnState_t *connPtr )
{
  uint8 i;
  uint16 length = sizeof(RCL_MultiBuffer) + sizeof(RCL_Buffer_DataEntry) +
                  RCL_BUFFER_MAX_HEADER_PAD_BYTES + SUFFIX_MAX_SIZE +
                  LL_PKT_HDR_LEN + connPtr->lenInfo.connEffectiveMaxRxOctets + LL_PKT_MIC_LEN;

  // check the shared rx buffers size
  if (rxDataQ.length >= length)
  {
    // No need to increase the shared rx buffer - only to update the connection command
    llUpdateRxBuffersForActiveConnections(&rxDataQ.multiBuffers);
    return;
  }

  // clear the buffers lists
  List_clearList(&linkParam[connPtr->connId].rxBuffers);
  List_clearList(&rxDataQ.finishedBuffers);
  List_clearList(&rxDataQ.multiBuffers);

  // realese the buffers
  for (i=0; i<NUM_RX_DATA_ENTRIES; i++)
  {
    // free the buffer
    MAP_osal_bm_free( rxDataQ.dataBuffers[i] );
  }

  // set the new buffer size
  rxDataQ.length = length;

  // create the buffers
  for (i=0; i<NUM_RX_DATA_ENTRIES; i++)
  {
    rxDataQ.dataBuffers[i] = MAP_osal_bm_alloc( rxDataQ.length );
    if (rxDataQ.dataBuffers[i] != NULL)
    {
      RCL_MultiBuffer_init(rxDataQ.dataBuffers[i], rxDataQ.length);
      RCL_MultiBuffer_put(&rxDataQ.multiBuffers, rxDataQ.dataBuffers[i]);
    }
  }

  llUpdateRxBuffersForActiveConnections(&rxDataQ.multiBuffers);

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * @fn          llSetupScanDataEntryQueue
 *
 * @brief       This routine is used to setup a static ring buffer.
 *
 * input parameters
 *
 * @param
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to the Rx Data Entry Queue.
 */
void *llSetupScanDataEntryQueue( void )
{
  RCL_MultiBuffer *multiBuffer;

  // set the Scan receive buffers
  for (uint8 i=0; i<NUM_RX_SCAN_ENTRIES; i++)
  {
    multiBuffer = (RCL_MultiBuffer *)&scanDataEntry[i];

    // Init scan buffers if they are not initialized yet
    if(scanDataEntry[i].length == 0)
    {
      RCL_MultiBuffer_init(multiBuffer, sizeof(scanDataEntry[0]));
    }
    RCL_MultiBuffer_put(&extScanParam.rxBuffers, multiBuffer);
  }

  /* Prepare list of RX buffers that are done */
  List_clearList(&scanDataQueue);

  return (multiBuffer);
}

#ifdef USE_PERIODIC_SCAN
/*******************************************************************************
 * @fn          llSetupPeriodicScanDataEntryQueue
 *
 * @brief       This routine is used to setup a static ring buffer
 *              for the periodic Scanner. it will use the same buffers
 *              as the Scan command to reduce memory use
 *
 * input parameters
 *
 * @param
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to the Rx Data Entry Queue.
 */
void *llSetupPeriodicScanDataEntryQueue( void )
{
  RCL_MultiBuffer *multiBuffer;

  // set the Scan receive buffers
  for (uint8 i=0; i<NUM_RX_SCAN_ENTRIES; i++)
  {
    multiBuffer = (RCL_MultiBuffer *)&scanDataEntry[i];

    // Init scan buffers if they are not initialized yet
    if(scanDataEntry[i].length == 0)
    {
      RCL_MultiBuffer_init(multiBuffer, sizeof(scanDataEntry[0]));
    }
    RCL_MultiBuffer_put(&llPeriodicScan.rxBuffers, multiBuffer);
  }

  /* Prepare list of RX buffers that are done */

  List_clearList(&scanDataQueue);

  return (multiBuffer);

}

#endif // USE_PERIODIC_SCAN
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * @fn          llSetupInitDataEntryQueue
 *
 * @brief       This routine is used to setup a single buffer queue statically
 *              to receive a connectable Adv packet.
 *
 * input parameters
 *
 * @param
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 */
void *llSetupInitDataEntryQueue( void )
{
  initDataQueue = (RCL_MultiBuffer *)&initDataEntry;
  // Provide buffer for storing received packet
  RCL_MultiBuffer_init(initDataQueue, sizeof(initDataEntry));
  RCL_MultiBuffer_put(&extInitParam.rxBuffers, initDataQueue);

  return (void *)(initDataQueue);
}
#endif // INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llSetupAdvDataEntryQueue
 *
 * @brief       This routine is used to setup a single buffer queue statically
 *              to receive a Adv packets.
 *
 * input parameters
 *
 * @param
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 */
void *llSetupAdvDataEntryQueue( void )
{
  // Provide buffer for storing received packet
  // one is enough since we will only receive a CONNECT_IND
  if (pAdvDataEntry == NULL)
  {
    pAdvDataEntry = (RCL_MultiBuffer *)&advDataEntry;
    RCL_MultiBuffer_init(pAdvDataEntry, sizeof(advDataEntry)*RCL_NUM_ADV_RX);
  }
  return (void *)(pAdvDataEntry);
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llSetupConnRxDataEntryQueue
 *
 * @brief       This routine is used to setup a RX static ring buffer.
 *
 *              Note: It will create one shared queue for all connections
 *
 * input parameters
 *
 * @param       connId - Connection ID.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to the Rx Data Entry Queue.
 */
void *llSetupConnRxDataEntryQueue( uint8 connId )
{
  llConnState_t *connPtr = MAP_llDataGetConnPtr( connId );
  uint8 i;

  // check that the shared rx queue was created
  if (rxDataQ.length > 0)
  {
    // check if need to increase the shared rx buffers
    MAP_llReplaceRxBuffers( connPtr );
  }
  else
  {
    // set the buffer size
    rxDataQ.length = sizeof(RCL_MultiBuffer) + sizeof(RCL_Buffer_DataEntry) +
                     RCL_BUFFER_MAX_HEADER_PAD_BYTES + SUFFIX_MAX_SIZE +
                     LL_PKT_HDR_LEN + connPtr->lenInfo.connEffectiveMaxRxOctets + LL_PKT_MIC_LEN;

    // Prepare list of RX buffers
    List_clearList(&rxDataQ.finishedBuffers);
    List_clearList(&rxDataQ.multiBuffers);

    // init data entries
    for (i = 0; i< NUM_RX_DATA_ENTRIES; i++)
    {
      rxDataQ.dataBuffers[i] = MAP_osal_bm_alloc( rxDataQ.length );
      if (rxDataQ.dataBuffers[i] != NULL)
      {
        RCL_MultiBuffer_init(rxDataQ.dataBuffers[i], rxDataQ.length);
        RCL_MultiBuffer_put(&rxDataQ.multiBuffers, rxDataQ.dataBuffers[i]);
      }
      else
      {
        return NULL;
      }
    }

    llUpdateRxBuffersForActiveConnections(&rxDataQ.multiBuffers);
  }

  return (&rxDataQ.multiBuffers);
}
#endif // ADV_CONN_CFG | INIT_CFG

#ifdef RTLS_CTE
/*******************************************************************************
 * @fn          llSetRfReportAodPackets
 *
 * @brief       This routine is used to set the RF to Report samples from AoD packets
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llSetRfReportAodPackets( void )
{
  //Report samples from AoD packets
  llCteSamples.autoCopy.samplesConfig.bFlushAod1us = 0;
  llCteSamples.autoCopy.samplesConfig.bFlushAod2us = 0;
}

/*******************************************************************************
 * @fn          llSetupCteSamplesEntryQueue
 *
 * @brief       This routine is used to setup a queue with single buffer queue
 *              dynamically to receive a CTE samples packet.
 *
 * input parameters
 *
 * @param       numBuffers - number of auto copy buffers to allocate (between 1 to 16)
 *                           in connection mode this value is 1.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE or FALSE
 */
uint8 llSetupCteSamplesEntryQueue( uint8 numBuffers )
{
  dataEntry_t *pEntryQ;
  uint8 *pBuffer;
  uint16 bufSize;

  if (numBuffers > LL_CTE_COUNT_MAX)
  {
    return FALSE;
  }

  llCteSamples.autoCopyCompleted = 0;
  //Report samples regardless of CRC result
  llCteSamples.autoCopy.samplesConfig.bFlushCrcErr = 0;
  //Do not report samples from packets with invalid CTEInfo
  llCteSamples.autoCopy.samplesConfig.bFlushCteInfoErr = 1;
  //Report samples from AoA packets
  llCteSamples.autoCopy.samplesConfig.bFlushAoa = 0;
  //Do not Report samples from AoD packets - default setting
  //will be overwrite in case of CTE test mode
  llCteSamples.autoCopy.samplesConfig.bFlushAod1us = 1;
  llCteSamples.autoCopy.samplesConfig.bFlushAod2us = 1;
  //Report gain as single-bit value in status field only
  llCteSamples.autoCopy.samplesConfig.bIncludeRfGain = 1;
  //Report RSSI in status field
  llCteSamples.autoCopy.samplesConfig.bIncludeRssi = 1;
  //Minimum value of CTETime for packets to report
  llCteSamples.autoCopy.minReportSize = LL_CTE_MIN_LEN;
  //Maximum value of CTETime for packets to report
  llCteSamples.autoCopy.maxReportSize = LL_CTE_MAX_LEN;
  //Disable the CTE limit counter
  llCteSamples.autoCopy.cteCopyLimitCount = 0xFF;
  //In case of CTE test - configure the RF to report samples from AoD packets
  MAP_llSetRfReportAodPackets();
  //allocate the RF IQ samples buffer = ~2.5kb
  // buffer size = 32 bit size * max sample rate *((max cte Length * 8) - CTE_OFFSET)
  bufSize = sizeof(dataEntry_t) + sizeof(llCteSamplesRfHeader_t) + (sizeof(uint32) * LL_CTE_NUM_RF_SAMPLES(LL_CTE_MAX_LEN));
  // allocate the complete memory - theoretically could be up to ~2.5KB * 16 = ~40KB
  llCteSamples.pAutoCopyBuffers = MAP_osal_mem_alloc( numBuffers * bufSize );

  if (llCteSamples.pAutoCopyBuffers == NULL)
  {
    return( FALSE );
  }
  pEntryQ = llCteSamples.pAutoCopyBuffers;
  pBuffer = (uint8 *)llCteSamples.pAutoCopyBuffers;
  for (uint8 i=0; i < numBuffers; i++)
  {
    pEntryQ->status     = DATASTAT_PENDING;
    pEntryQ->config     = DATA_ENTRY_TYPE_GENERAL | DATA_ENTRY_LEN_SIZE_2;
    pEntryQ->length     = bufSize;
    // last entry will points to the first - in case of one entry, it will point to itself.
    pEntryQ->pNextEntry = (i+1 < numBuffers)?(dataEntry_t *)(pBuffer + bufSize):llCteSamples.pAutoCopyBuffers;
    pBuffer += bufSize;
    pEntryQ = (dataEntry_t *)pBuffer;
  }
  // initialize data queue
  llCteSamples.queue.dataEntryQ.pCurEntry  = llCteSamples.pAutoCopyBuffers;
  llCteSamples.queue.dataEntryQ.pLastEntry = NULL;
  llCteSamples.queue.pNextDataEntry        = llCteSamples.pAutoCopyBuffers;
  llCteSamples.queue.pTempDataEntry        = NULL;

  // set the queue pointer
  llCteSamples.autoCopy.pSamplesQueue = (uint32_t *)&llCteSamples.queue.dataEntryQ;
  return ( TRUE );
}

/*******************************************************************************
 * @fn          llFreeCteSamplesEntryQueue
 *
 * @brief       This routine is used to free the buffer queue
 *              which hold the received CTE samples packet.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE or FALSE
 */
uint8 llFreeCteSamplesEntryQueue( void )
{
  uint8 tasksCounter = 0;

  if (llCteSamples.pAutoCopyBuffers == NULL)
  {
    return FALSE;
  }
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  // search on connection list
  for (uint8 i = 0; i < maxNumConns; i++)
  {
    // check if Connection CTE sampling is or was enable
    if (llCte[i].initiator.samplingEnable != LL_CTE_SAMPLING_NOT_INIT)
    {
      tasksCounter++;
    }
  }
#endif
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  // get the number of periodic scanners with active CTE sampling
  tasksCounter += MAP_llGetPeriodicScanCteTasks();
#endif
  if (tasksCounter > 0)
  {
    return FALSE;
  }
  // reset data queue
  llCteSamples.queue.dataEntryQ.pCurEntry  = NULL;
  llCteSamples.queue.dataEntryQ.pLastEntry = NULL;
  llCteSamples.queue.pNextDataEntry        = NULL;
  llCteSamples.queue.pTempDataEntry        = NULL;
  llCteSamples.autoCopyCompleted = 0;
  llCteSamples.autoCopy.pSamplesQueue = NULL;
  // reset the struct pointer in RF memory
  llRfOverrideCteValue(0,RFC_FWPAR_CTE_AUTO_COPY,RFC_CTE_AUTO_COPY_OFFSET);
  // disable the antenna switch
  llRfOverrideCteValue(0,RFC_FWPAR_CTE_ANT_SWITCH,RFC_CTE_ANT_SWITCH_OFFSET);
  // release the buffers
  MAP_osal_mem_free(llCteSamples.pAutoCopyBuffers);
  llCteSamples.pAutoCopyBuffers = NULL;

  return TRUE;
}
#endif //RTLS_CTE

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llMoveTempTxDataEntries
 *
 * @brief       This routine is used to encrypt (if necessary) the temp data
 *              entries, then move them to the internal Tx data entry list and
 *              queue them for Tx RF.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to connection.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return
 */
void llMoveTempTxDataEntries( llConnState_t *connPtr )
{
  RCL_Buffer_TxBuffer *pEntry = RCL_TxBuffer_get(&((txDataQ_t *)(connPtr->pTxDataEntryQ))->tmpDataBuffers);

  while( pEntry != NULL )
  {
    // check if encryption is enabled
    if ( connPtr->encEnabled )
    {
      //pData = [byte 0 = LL_PKT_HDR, byte 1 = dataLen, byte 2:x = data, byte x+1:byte x+5 = LL_PKT_MIC]
      // point to start of the packet, including header
      uint8 *pData  = pEntry->data + RCL_BUFFER_MAX_HEADER_PAD_BYTES;
      // get the dataLen value (remove the additional length related to the pEntry)
      // Note: This length added by llWriteTxData()
      uint8 dataLen = pEntry->length - (pEntry->numPad + LL_PKT_HDR_LEN + 1);
      // save header value
      uint8  pktHdr = *pData++;
      // add MIC length to the data length
      (*pData) += LL_PKT_MIC_LEN;
      // point to payload
      pData++;
      // adjust length for MIC
      pEntry->length += LL_PKT_MIC_LEN;

      // and encrypt
      MAP_LL_ENC_Encrypt( connPtr,
                          pktHdr,
                          dataLen,
                          pData );
    }

    // queue it on connection TX list and queue for RF
    MAP_llAddTxDataEntry( connPtr->pTxDataEntryQ,
                          pEntry );

    // move to next entry, if any
    pEntry = RCL_TxBuffer_get(&((txDataQ_t *)(connPtr->pTxDataEntryQ))->tmpDataBuffers);
  }

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && ((CTRL_CONFIG & ADV_CONN_CFG) || (CTRL_CONFIG & INIT_CFG))
/*******************************************************************************
 * @fn          llManageControlPacketQueue
 *
 * @brief       This routine is used to check the control packets queue and
 *              in case of collision it manage the entries
 *
 * input parameters
 *
 * @param       connPtr - Pointer to connection.
 * @param       llCtrlPacket - current control packet.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return
 */
void llManageControlPacketQueue( llConnState_t *connPtr, uint8 llCtrlPacket )
{
  uint8 swapCtrlPacket = LL_CTRL_INVALID_OPCODE;

  // in case CtrlPacket queue is empty
  if ( connPtr->ctrlPktInfo.ctrlPktCount == 0 )
  {
    // send the control packet
    MAP_llEnqueueCtrlPkt( connPtr, llCtrlPacket );
  }
  else // handle the collision
  {
    if ( connPtr->llTask->taskID == LL_TASK_ID_PERIPHERAL )
    {
      switch (llCtrlPacket)
      {
        case LL_CTRL_PHY_RSP:
        case LL_CTRL_CONNECTION_PARAM_RSP:
        case LL_CTRL_DUMMY_PLACE_HOLDER_TRANSMIT:
          if (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CTE_REQ)
          {
            swapCtrlPacket = LL_CTRL_CTE_REQ;
          }
          break;
        case LL_CTRL_ENC_RSP:
          if ( (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CONNECTION_PARAM_REQ) ||
               (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_LENGTH_REQ)           ||
               (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_PERIPHERAL_FEATURE_REQ)    ||
               (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_VERSION_IND)          ||
               (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CTE_REQ)              ||
               (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_PHY_REQ) )
          {
            swapCtrlPacket = connPtr->ctrlPktInfo.ctrlPkts[0];
          }
      }
    }
    else if ( connPtr->llTask->taskID == LL_TASK_ID_CENTRAL )
    {
      switch (llCtrlPacket)
      {
        case LL_CTRL_PHY_UPDATE_REQ:
        case LL_CTRL_CONNECTION_UPDATE_IND:
        case LL_CTRL_LENGTH_RSP:
        case LL_CTRL_DUMMY_PLACE_HOLDER_TRANSMIT:
          if (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CTE_REQ)
          {
            swapCtrlPacket = LL_CTRL_CTE_REQ;
          }
          break;
      }
    }
    // check if need to replace the first control packet
    if (swapCtrlPacket != LL_CTRL_INVALID_OPCODE)
    {
      uint8 pktSent = connPtr->ctrlPktInfo.ctrlPktActive;

      // Replace the first control packet
      MAP_llReplaceCtrlPkt( connPtr, llCtrlPacket, LL_CTRL_UNDEFINED_PKT );

      // check if the packet was placed on the Tx queue
      if ( pktSent == TRUE )
      {
        // signal the restoration of the active control packet
        connPtr->ctrlPktInfo.ctrlPktPending = swapCtrlPacket;

        MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_DUMMY_PLACE_HOLDER_TX_PENDING );
      }
      else // just requeue the control packet
      {
        MAP_llEnqueueCtrlPkt( connPtr, swapCtrlPacket );
      }
    }
    else
    {
      // just requeue the control packet
      MAP_llEnqueueCtrlPkt( connPtr, llCtrlPacket );
    }
  }
}
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
/*******************************************************************************
 * @fn          llProcessPeripheralControlPacket
 *
 * @brief       This routine is used to process a LL Peripheral PDU Control packet.
 *
 * input parametersd
 *
 * @param       connPtr - Pointer to BLE LL Connection.
 * @param       pBuf    - Pointer to Control packet payload.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llProcessPeripheralControlPacket( llConnState_t *connPtr,
                                  uint8         *pBuf )
{
  uint8 opcode = *pBuf++;
  uint8 status;

  // check the type of control packet
  if ((opcode >= LL_CTRL_CS_SEC_RSP) &&
      (opcode <= LL_CTRL_CS_SEC_REQ) )
  {
      MAP_llCsProcessCsControlPacket(opcode, connPtr, pBuf);
      return;
  }
  switch( opcode )
  {
    // Update Connection Parameters
    case LL_CTRL_CONNECTION_UPDATE_IND:
      // Note: It is assumed that we have automatically ACK'ed this
      //       packet since the only time the nR sends a NACK is when
      //       the RX FIFO is too full to receive a packet. The fact
      //       that we received this packet means this wasn't the case.
      // Note: What we don't know here is whether or not the Central
      //       in fact received the ACK. The only way to know that is
      //       if the Central's next packet is an ACK. For now, we are
      //       going to assume we only have to verify that we sent an
      //       ACK to the Central, not that the Central actually received
      //       it.
      // Note: The spec limits the number of control procedures that
      //       the Peripheral has to handle to one. It is assumed that the
      //       Central will ensure this isn't violated, so the Peripheral
      //       only need keep track of control procedures it starts.

      // save the connection udpate parameters
      connPtr->paramUpdate.winSize = *pBuf++;
      pBuf = MAP_llMemCopySrc( (uint8 *)&connPtr->paramUpdate.winOffset, pBuf, 2 );
      pBuf = MAP_llMemCopySrc( (uint8 *)&connPtr->paramUpdate.connInterval, pBuf, 2 );
      pBuf = MAP_llMemCopySrc( (uint8 *)&connPtr->paramUpdate.peripheralLatency, pBuf, 2 );
      pBuf = MAP_llMemCopySrc( (uint8 *)&connPtr->paramUpdate.connTimeout, pBuf, 2 );

      // convert this data into units of 625us
      connPtr->paramUpdate.winSize      <<= 1;
      connPtr->paramUpdate.winOffset    <<= 1;
      connPtr->paramUpdate.connInterval <<= 1;
      connPtr->paramUpdate.connTimeout  <<= 4;

      // connection event when update is activated
      pBuf = MAP_llMemCopySrc( (uint8 *)&connPtr->paramUpdateEvent, pBuf, 2 );

      // check if update event count is still valid
      // Note: The spec indicates the connection should be termainted when
      //       the instant is in the past.
      if ( ((connPtr->paramUpdateEvent - connPtr->currentEvent) & 0xFFFF) >= LL_MAX_UPDATE_COUNT_RANGE )
      {
        // instant past, so terminate the connection
        // Note: When using PM, it is possible this routine could terminate
        //       connection and try to shutdown the RF Core while the radio
        //       is still running (as we are in the context of an ISR). To
        //       prevent this, we wait until the connection ends before
        //       terminating by letting the connection event complete.

        // set the terminate reason code
        connPtr->termInfo.reason = LL_CTRL_PKT_INSTANT_PASSED_PEER_TERM;

        // set flag to indicate a termination indication was received
        connPtr->termInfo.termIndRcvd = TRUE;

        // ALT: Halt the radio first, then terminate the connection.
        // MAP_llHaltRadio( CMD_ABORT );
        // MAP_llConnTerminate( connPtr, LL_CTRL_PKT_INSTANT_PASSED_PEER_TERM );

        return;
      }

      // check that the LSTO is valid (i.e. meets the requirements)
      // Note: LSTO > (1 + Peripheral Latency) * (Connection Interval * 2)
      // Note: The CI * 2 requirement based on ESR05 V1.0, Erratum 3904.
      // Note: All times are in 625us.
      if ( (uint32)connPtr->paramUpdate.connTimeout <=
           ((uint32)(1 + connPtr->paramUpdate.peripheralLatency) *
            (uint32)(connPtr->paramUpdate.connInterval << 1)) )
      {
        // invalid connection parameters, so terminate
        // Note: When using PM, it is possible this routine could terminate
        //       the connection and try to shutdown the RF Core while the radio
        //       is still running (as we are in the context of an ISR). To
        //       prevent this, we wait until the connection ends before
        //       terminating by letting the connection event complete.

        // set termination reason code
        connPtr->termInfo.reason = LL_UNACCEPTABLE_CONN_INTERVAL_TERM;

        // set flag to indicate a termination indication was received
        connPtr->termInfo.termIndRcvd = TRUE;

        // ALT: Halt the radio first, then terminate the connection.
        // MAP_llHaltRadio( CMD_ABORT );
        // MAP_llConnTerminate( connPtr, LL_UNACCEPTABLE_CONN_INTERVAL_TERM );

        return;
      }

      // set flag in current connection to indicate an param update is valid
      connPtr->pendingParamUpdate = PARAM_UPDATE_PENDING;

      // disable peripheral latency so peripheral can listen to every connection
      // event, per the spec
      // ALT: Technically this is only required until an ACK of this Update is
      //      confirmed, or if at the connection event before the instant, or
      //      at the connection event instant itself.
      connPtr->peripheralLatency = 0;

      // set flag to monitor for Central confirmation of Peripheral's ACK for update
      connPtr->updateSLPending = UPDATE_RX_CTRL_ACK_PENDING;

      break;

    // Update Data Channel Map
    case LL_CTRL_CHANNEL_MAP_IND:
      // Note: It is assumed that we have automatically ACK'ed this
      //       packet since the only time the NR sends a NACK is when
      //       the RX FIFO is too full to receive a packet. The fact
      //       that we received this packet means this wasn't the case.
      // Note: What we don't know here is whether or not the Central
      //       in fact received the ACK. The only way to know that is
      //       if the Central's next packet is an ACK. For now, we are
      //       going to assume we only have to verify that we sent an
      //       ACK to the Central, not that the Central actually received
      //       it.
      // Note: The spec limits the number of control procedures that
      //       the Peripheral has to handle to one. It is assumed that the
      //       Central will ensure this isn't violated, so the Peripheral
      //       only need keep track of control procedures it starts.
      // Note: Can also check if RX FIFO full error bit is set.

      // save the connection udpate data channel parameters for this connection
      pBuf = MAP_llMemCopySrc( connPtr->curChanMap.chanMap, pBuf, LL_NUM_BYTES_FOR_CHAN_MAP );

      // connection event when update is activated
      pBuf = MAP_llMemCopySrc( (uint8 *)&connPtr->chanMapUpdateEvent, pBuf, 2 );

      // check if update event count is still valid
      // Note: The spec indicates the connection should be termainted when
      //       the instant is in the past.
      if ( ((connPtr->chanMapUpdateEvent - connPtr->currentEvent) & 0xFFFF) >= LL_MAX_UPDATE_COUNT_RANGE )
      {
        // instant past, so terminate the connection
        // Note: When using PM, it is possible this routine could terminate
        //       the connection and try to shutdown the RF Core while the radio
        //       is still running (as we are in the context of an ISR). To
        //       prevent this, we wait until the connection ends before
        //       terminating by letting the connection event complete.

        // set termination reason code
        connPtr->termInfo.reason = LL_CTRL_PKT_INSTANT_PASSED_PEER_TERM;

        // set flag to indicate a termination indication was received
        connPtr->termInfo.termIndRcvd = TRUE;

        // ALT: Halt the radio first, then terminate the connection.
        // MAP_llHaltRadio( CMD_ABORT );
        // MAP_llConnTerminate( connPtr, LL_CTRL_PKT_INSTANT_PASSED_PEER_TERM );

        return;
      }

      // set flag in current connection to indicate an data channel update is valid
      connPtr->pendingChanUpdate = TRUE;

      // disable peripheral latency so peripheral can listen to every connection
      // event, per the spec
      // ALT: Technically this is only required until an ACK of this Update is
      //      confirmed, or if at the connection event before the instant, or
      //      at the connection event instant itself.
      connPtr->peripheralLatency = 0;

      // set flag to monitor for Central confirmation of Peripheral's ACK for update
      connPtr->updateSLPending = UPDATE_RX_CTRL_ACK_PENDING;

      break;

    // Encryption Request - Peripheral Only; Sent by the Central
    case LL_CTRL_ENC_REQ:
#if defined( LL_TEST_MODE )
      if ( llTestMode.testCase == LL_TEST_MODE_TP_SEC_MAS_BV_03 )
      {
        // force encryption support off to force a Rejection Indication
        connPtr->featureSetInfo.featureSet[0] &= ~LL_FEATURE_ENCRYPTION;
      }
#endif // LL_TEST_MODE

      // check if encryption is a supported feature set item
      if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_ENCRYPTION) )
      {
        // set the encryption rejection error code
        connPtr->encInfo.encRejectErrCode = LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE;

        // so reject the encryption request; check for pause encryption procedure
        if ( connPtr->encInfo.encRestart == TRUE )
        {
          // it is, so replace the DUMMY packet at head of queue
          MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_REJECT_IND,
                                LL_CTRL_UNDEFINED_PKT );
        }
        else
        {
          // schedule the output of the control packet
          MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_REJECT_IND );
        }

        break;
      }

#if defined( LL_TEST_MODE )
      if ( llTestMode.testCase == LL_TEST_MODE_TP_SEC_MAS_BV_04 )
      {
        uint8 *buf = (uint8 *)LL_TX_bm_alloc( 4 );

        // set the L2CAP header with the length and connection ID
        // Note: The L2CAP Header Length is -4 the payload size.
        *((uint32 *)buf) = 0;

        MAP_LL_TxData( connPtr->connId, buf, 4, LL_DATA_FIRST_PKT_HOST_TO_CTRL );
      }
#endif // LL_TEST_MODE

      // set flag to stop all outgoing transmissions
      connPtr->txDataEnabled = FALSE;

      // set flag to discard all incoming data transmissions
      connPtr->rxDataEnabled = FALSE;

      // indicate an Encryption Request has been received
      // Note: This is used in a Pause Encryption procedure to allow
      //       the control procedure to restart the timer. In a normal
      //       encryption procedure, this flag is not used.
      connPtr->encInfo.encReqRcved = TRUE;

      // copy the random vector
      // Note: The RAND will be left in LSO..MSO order as this is
      //       assumed to be the order of the bytes at the Host API
      //       interface.
      pBuf = MAP_llMemCopySrc( &connPtr->encInfo.RAND[0], pBuf, LL_ENC_RAND_LEN );

      // copy the encrypted diversifier
      // Note: The EDIV will be left in LSO..MSO order as this is
      //       assumed to be the order of the bytes at the Host API
      //       interface.
      pBuf = MAP_llMemCopySrc( &connPtr->encInfo.EDIV[0], pBuf, LL_ENC_EDIV_LEN );

      // copy the central's portion of the session key identifier
      // Note: The SKDm LSO is the LSO of the SKD.
      pBuf = MAP_llMemCopySrc( &connPtr->encInfo.SKD[LL_ENC_SKD_M_OFFSET], pBuf, LL_ENC_SKD_M_LEN );

      // bytes are received LSO..MSO, but need to be maintained as
      // MSO..LSO, per FIPS 197 (AES), so reverse the bytes
      MAP_LL_ENC_ReverseBytes( &connPtr->encInfo.SKD[LL_ENC_SKD_M_OFFSET], LL_ENC_SKD_M_LEN );

      // copy the central's portion of the initialization vector
      // Note: The IVm LSO is the LSO of the IV.
      pBuf = MAP_llMemCopySrc( &connPtr->encInfo.IV[LL_ENC_IV_M_OFFSET], pBuf, LL_ENC_IV_M_LEN );

      // bytes are received LSO..MSO, but need to be maintained as
      // MSO..LSO, per FIPS 197 (AES), so reverse the bytes
      // ALT: Maintain the IV in LSO..MSO order as the Nonce is formed that way.
      MAP_LL_ENC_ReverseBytes( &connPtr->encInfo.IV[LL_ENC_IV_M_OFFSET], LL_ENC_IV_M_LEN );

      // generate SKDs
      // Note: The SKDs MSO is the MSO of the SKD.
      // Note: Placement of result forms concatenation of SKDm and SKDs.
      // Note: Leave the SKDs in LSO..MSO order for now since it has to
      //       be sent OTA in this way. Reservse bytes after that.
      MAP_LL_ENC_GenDeviceSKD( &connPtr->encInfo.SKD[ LL_ENC_SKD_S_OFFSET ] );

      // generate IVs
      // Note: The IVs MSO is the MSO of the IV.
      // Note: Placement of result forms concatenation of IVm and IVs.
      // Note: Leave the SKDs in LSO..MSO order for now since it has to
      //       be sent OTA in this way. Reservse bytes after that.
      MAP_LL_ENC_GenDeviceIV( &connPtr->encInfo.IV[ LL_ENC_IV_S_OFFSET ] );

      // schedule a cache update of FIPS TRNG values for next SKD/IV usage
      postRfOperations |= LL_POST_RADIO_CACHE_RANDOM_NUM;

      // check if this is a pause encryption procedure
      if ( connPtr->encInfo.encRestart == TRUE )
      {
        // it is, so replace the DUMMY packet at head of queue
        MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_ENC_RSP,
                              LL_CTRL_UNDEFINED_PKT );
      }
      else
      {
#ifdef LL_TEST_MODE
        if ( llTestMode.testCase == LL_TEST_MODE_TP_CON_MAS_BV_28 )
        {
          // force what looks to the Central like a collision
          MAP_llSetupCtrlPkt( connPtr, LL_CTRL_CONNECTION_PARAM_REQ);

          // then continue with encryption
          MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_ENC_RSP );

          // Note: When ENC ends, the test mode will set ctrlPktActive so that
          //       the connection parameter request isn't sent again.
          MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_CONNECTION_PARAM_REQ );
        }
        else if ( llTestMode.testCase == LL_TEST_MODE_TP_SEC_MAS_BV_12 )
        {
          // force what looks to the Central like a collision
          MAP_llSetupCtrlPkt( connPtr, LL_CTRL_VERSION_IND);

          // then continue with encryption
          MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_ENC_RSP );

          // put the request on the control queue
          // Note: When ENC ends, the test mode will set ctrlPktActive so that
          //       the version indication isn't sent again.
          MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_VERSION_IND );
        }

        else if ( llTestMode.testCase == LL_TEST_MODE_TP_SEC_MAS_BV_13 )
        {
          // write control opcode based on connection role
          if ( llState == LL_STATE_CONN_CENTRAL )
          {
            // force what looks to the Central like a collision
            MAP_llSetupCtrlPkt( connPtr, LL_CTRL_FEATURE_REQ);
          }
          else // LL_STATE_CONN_PERIPHERAL
          {
            // force what looks to the Central like a collision
            MAP_llSetupCtrlPkt( connPtr, LL_CTRL_PERIPHERAL_FEATURE_REQ);
          }

          // then continue with encryption
          MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_ENC_RSP );

          // put the request on the control queue
          // Note: When ENC ends, the test mode will set ctrlPktActive so that
          //       the feature request isn't sent again.
          MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_PERIPHERAL_FEATURE_REQ );
        }
        else
          if ( llTestMode.testCase == LL_TEST_MODE_JIRA_4785 )
          {
            // Not our first encryption
            if ( connPtr->ctrlPktInfo.ctrlPktCount == 0 )
            {
              MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_LENGTH_REQ );
              MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_VERSION_IND);
            }

            if ( connPtr->ctrlPktInfo.ctrlPktCount == 1)
            {
              MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_LENGTH_REQ );
            }
          }
#endif // LL_TEST_MODE
        {
          // check for a collision with encryption
          llManageControlPacketQueue( connPtr, LL_CTRL_ENC_RSP );
        }
      }

      break;

    // FIRST CENTRAL THEN PERIPHERAL SENDS
    case LL_CTRL_START_ENC_RSP:

#ifdef LL_TEST_MODE
      if ( llTestMode.testCase == LL_TEST_MODE_TP_SEC_MAS_BI_04 )
      {
        break;
      }
#endif // LL_TEST_MODE
      // indicate we've received the start encryption response
      connPtr->encInfo.startEncRspRcved = TRUE;

      break;

    // ONLY CENTRAL SENDS, ONLY PERIPHERAL RECEIVES
    case LL_CTRL_PAUSE_ENC_REQ:

#ifdef LL_TEST_MODE
      if ( llTestMode.testCase == LL_TEST_MODE_TP_SEC_MAS_BV05 )
      {
        uint8 *buf = (uint8 *)LL_TX_bm_alloc( 1 );

        // send one byte per the spec
        *buf = 0xFF;

        LL_TxData( connPtr->connId, buf, 1, LL_DATA_FIRST_PKT_HOST_TO_CTRL );
      }
#endif // LL_TEST_MODE

      // set flag to stop all outgoing transmissions
      connPtr->txDataEnabled = FALSE;

      // set flag to discard all incoming data transmissions
      connPtr->rxDataEnabled = FALSE;

      // invalidate the existing session key
      connPtr->encInfo.SKValid = FALSE;

      // set a flag to indicate this is a restart
      connPtr->encInfo.encRestart = TRUE;

      // indicate the LTK is no longer valid
      connPtr->encInfo.LTKValid = FALSE;

      // schedule the pause encryption response control packet
      MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_PAUSE_ENC_RSP );

      break;

    // FIRST CENTRAL THEN PERIPHERAL
    case LL_CTRL_PAUSE_ENC_RSP:
      // set a flag to indicate the pause encryption response arrived
      connPtr->encInfo.pauseEncRspRcved = TRUE;

      break;

    // Controller Feature Setup
    case LL_CTRL_FEATURE_REQ:
#ifdef LL_TEST_MODE
      switch( llTestMode.testCase )
      {
        case LL_TEST_MODE_TP_CON_MAS_BV_19:
          return;

          // Note: Unreachable statement generates compiler warning!
          //break;

        default:
          break;
      }
#endif // LL_TEST_MODE

      // Create a feature set for response PDU
      llCreateCommonFeatureSet(connPtr, pBuf);

      // Note: If a feature request arrives between sending the encryption
      //       request and before receiving the encryption response, then there
      //       was a collision and the Controller should respond. But if the
      //       feature request arrived anytime after the encryption response,
      //       then it is considered an illegal control packet and the
      //       connection is disconnected with a MIC error.

      // check if encryption is in progress
      if ( connPtr->rxDataEnabled == FALSE )
      {
        // set termination reason code
        connPtr->termInfo.reason = LL_MIC_FAILURE_TERM;

        // set flag to indicate a termination indication was received
        connPtr->termInfo.termIndRcvd = TRUE;

        // ALT: Halt the radio first, then terminate the connection.
        // MAP_llHaltRadio( CMD_ABORT );
        // MAP_llConnTerminate( connPtr, LL_MIC_FAILURE_TERM );
      }
      else // okay to respond
      {
        // setup/send a Feature Request Response
        // Note: This control packet is not queued, but merely sent.
        // Note: Features to be used will be taken on the next connection
        //       event after the response is successfully transmitted.
        if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_FEATURE_RSP ) == FALSE )
        {
          // unable to malloc a packet!
          (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
        }
      }
      break;

    // Controller Feature Setup
    case LL_CTRL_FEATURE_RSP:
      // Create a feature set for response PDU
      llCreateCommonFeatureSet(connPtr, pBuf);

      // check if the peer does not support Coded
      if ( !(connPtr->featureSetInfo.featureSet[1] & LL_FEATURE_CODED_PHY) )
      {
        // they do not, so limit max Tx/Rx time to no greater than 2120us
        // Note: Per Vol 6, Part B, Section 5.1.9.
        connPtr->lenInfo.connMaxTxTime = LL_MAX_LINK_DATA_TIME_UNCODED;
        connPtr->lenInfo.connMaxRxTime = LL_MAX_LINK_DATA_TIME_UNCODED;
      }

      if ( deviceFeatureSet.featureSet[0] & LL_FEATURE_DATA_PACKET_LENGTH_EXTENSION )
      {
        // check if the default max number of Tx/Rx bytes is not the min OTA size
        // of 27, or the max Tx/Rx time is not the min OTA time of 328us
        // Note: Will need to adjust these time checks when AE is added.
        if ( (connPtr->lenInfo.connMaxTxOctets != LL_MIN_LINK_DATA_LEN)  ||
             (connPtr->lenInfo.connMaxRxOctets != LL_MIN_LINK_DATA_LEN)  ||
             (connPtr->lenInfo.connMaxTxTime   != LL_MIN_LINK_DATA_TIME) ||
             (connPtr->lenInfo.connMaxRxTime   != LL_MIN_LINK_DATA_TIME) )
        {
          // schedule a data length update control procedure as soon as possible
          MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_LENGTH_REQ );
        }
      }

      break;

    // Version Information Indication
    case LL_CTRL_VERSION_IND:
#ifdef LL_TEST_MODE
      switch( llTestMode.testCase )
      {
        case LL_TEST_MODE_TP_CON_MAS_BI_04:
          return;

          // Note: Unreachable statement generates compiler warning!
          //break;

        default:
          break;
      }
#endif // LL_TEST_MODE

      // check if the peer's version information has not yet been obtained
      if ( connPtr->verExchange.peerInfoValid != TRUE )
      {
        // get the peer's version information and save it
        connPtr->verInfo.verNum = *pBuf++;
        pBuf = MAP_llMemCopySrc( (uint8 *)&connPtr->verInfo.comId, pBuf, 2 );
        pBuf = MAP_llMemCopySrc( (uint8 *)&connPtr->verInfo.subverNum, pBuf, 2 );

        // set a flag to indicate it is now valid
        connPtr->verExchange.peerInfoValid = TRUE;
      }

      // Note: If the version indication arrived between sending the encryption
      //       request and before receiving the encryption response, then there
      //       was a collision and the Controller should respond (if a version
      //       indication exchange hasn't already occurred). But if the version
      //       indication arrived anytime after the encryption response, then
      //       it is considered an illegal control packet and the connection
      //       is disconnected with a MIC error.
      // Note: Vol 6, Part B, Section 5.1.3.1 implies only START_ENC_RSP,
      //       TERMINATE_IND, and REJECT can be sent by the Central, but
      //       here, the interpretation taken is the Central is replying.
      // Note: Vol 6, Part B, Section 5.1.3.1, paragraph 8 says the Peripheral
      //       should accept UNKNOWN_RSP after ENC_RSP is sent, which implies
      //       the Central can also send this PDU after sending ENC_REQ.

      // check if encryption is in progress
      if ( connPtr->rxDataEnabled == FALSE )
      {
        // set termination reason code
        connPtr->termInfo.reason = LL_MIC_FAILURE_TERM;

        // set flag to indicate a termination indication was received
        connPtr->termInfo.termIndRcvd = TRUE;

        // ALT: Halt the radio first, then terminate the connection.
        // MAP_llHaltRadio( CMD_ABORT );
        // MAP_llConnTerminate( connPtr, LL_MIC_FAILURE_TERM );
      }
      else // okay to respond
      {
        // check if a version indication has been sent or if the host not already ask for it
        if ( (connPtr->verExchange.verInfoSent == FALSE) && (connPtr->verExchange.hostRequest == FALSE) )
        {
          // send peer's request for our version information
          // Note: This control packet is not queued, but merely sent.
          if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_VERSION_IND) == FALSE )
          {
            // unable to malloc a packet!
            (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
            break;
          }
          // version indication has been sent
          connPtr->verExchange.verInfoSent = TRUE;
        }
      }
      break;

    // Ping Request
    case LL_CTRL_PING_REQ:
#ifdef LL_TEST_MODE
      switch( llTestMode.testCase )
      {
        case LL_TEST_MODE_TP_SEC_MAS_BV_08:
          return;

          // Note: Unreachable statement generates compiler warning!
          //break;

        default:
          break;
      }
#endif // LL_TEST_MODE

      // check if the Ping Feature is a supported feature set item
      if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_PING) )
      {
        // unknown data PDU control packet received so save the type
        connPtr->unknownCtrlType = opcode;

        // setup/send an Unknown Response
        // Note: This control packet is not queued, but merely sent.
        if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == FALSE )
        {
          // unable to malloc a packet!
          (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
        }
      }
      else // this feature is supported
      {
        // setup/send a Ping Response
        // Note: This control packet is not queued, but merely sent.
        if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_PING_RSP) == FALSE )
        {
          // unable to malloc a packet!
          (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
        }
      }
      break;

    // Ping Response
    case LL_CTRL_PING_RSP:
      connPtr->pingReqPending = FALSE;
      break;

    // Terminate Indication
    case LL_CTRL_TERMINATE_IND:
      // received a terminate from peer host, so terminate immediately
      // Note: It is assumed that we have automatically ACK'ed this
      //       packet since the terminate indication was correctly received
      //       (i.e. no NACK from either a CRC error or RX FIFO too full
      //       to receive error).
      // Note: What we don't know here is whether or not the Central
      //       in fact received the ACK. The only way to know that is
      //       if the Central's next packet is an ACK. But if the Central
      //       received the ACK, then it would terminate, so we may not
      //       ever receive another packet! In any case, the spec has been
      //       updated such that, when a terminate indication is received,
      //       we only need confirm we've ACK'ed the packet.
      // Note: The spec limits the number of control procedures that
      //       the Peripheral has to handle to one. It is assumed that the
      //       Central will ensure this isn't violated, so the Peripheral
      //       only need keep track of control procedures it starts.
      // Note: When using PM, it is possible this routine could terminate
      //       the connection and try to shutdown the RF Core while the radio
      //       is still running (as we are in the context of an ISR). To
      //       prevent this, we wait until the connection ends before
      //       terminating by letting the connection event complete.
#if defined(LL_TEST_MODE)
      switch( llTestMode.testCase )
      {
        case LL_TEST_MODE_TP_CON_MAS_BI_02:
          return;

          //Note: Unreachable statement generates compiler warning!
          //break;

        default:
          break;
      }
#endif

      // read the reason code
      connPtr->termInfo.reason = *pBuf++;

      // set flag to indicate a termination indication was received
      connPtr->termInfo.termIndRcvd = TRUE;

      // ALT: Halt the radio first, then terminate the connection.
      // MAP_llHaltRadio( CMD_ABORT );
      // MAP_llConnTerminate( connPtr, connPtr->termInfo.reason );

      return;

      // Note: Unreachable statement generates compiler warning!
      //break;

    // Controller Extended Reject
    case LL_CTRL_REJECT_EXT_IND:
      // save rejected opcode and error code, and indicate RejectIndExt received
      connPtr->rejectIndExt.rejectOpcode = pBuf[0];
      connPtr->rejectIndExt.errorCode    = pBuf[1];

      if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
      {
         switch( connPtr->ctrlPktInfo.ctrlPkts[0] )
         {
           case LL_CTRL_CONNECTION_PARAM_REQ:
           case LL_CTRL_CONNECTION_PARAM_RSP:
             // set flag to indicate rejection was received
             connPtr->connParamReqFlags.rejectIndExtRcved = TRUE;
             break;

           case LL_CTRL_PHY_REQ:
             // set flag to indicate rejection was received
             SET_FEATURE_FLAG( connPtr->phyInfo.phyFlags,
                               REJECT_EXT_IND_RECEIVED );
             break;
#ifdef RTLS_CTE
           case LL_CTRL_CTE_REQ:
             llCte[connPtr->connId].initiator.requestEnable = FALSE;
             llCte[connPtr->connId].initiator.sendRequest = FALSE;
             HCI_CteRequestFailedEvent(connPtr->rejectIndExt.errorCode,connPtr->connId);
             break;
#endif // RTLS_CTE
           default:
             break;
         }
      }
      else // unexpected!
      {
        LL_ASSERT( FALSE );
      }

      break;

    // Connection Parameter Request or Response
    case LL_CTRL_CONNECTION_PARAM_REQ:
      // check if the Connection Parameter Request feature is supported
      if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_CONN_PARAMS_REQ) )
      {
        // control packet received for features that are not supported
        connPtr->unknownCtrlType = LL_CTRL_CONNECTION_PARAM_REQ;

        // setup/send an Unknown Response
        // Note: This control packet is not queued, but merely sent.
        if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == FALSE )
        {
          // unable to malloc a packet!
          (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
        }
      }
      else // okay to process parameters
      {
        // save parameters
        connPtr->connParams.intervalMin     = BUILD_UINT16(pBuf[0], pBuf[1]);
        connPtr->connParams.intervalMax     = BUILD_UINT16(pBuf[2], pBuf[3]);
        connPtr->connParams.latency         = BUILD_UINT16(pBuf[4], pBuf[5]);
        connPtr->connParams.timeout         = BUILD_UINT16(pBuf[6], pBuf[7]);
        connPtr->connParams.periodicity     = pBuf[8];
        connPtr->connParams.refConnEvtCount = BUILD_UINT16(pBuf[9], pBuf[10]);
        connPtr->connParams.offset0         = BUILD_UINT16(pBuf[11], pBuf[12]);
        connPtr->connParams.offset1         = BUILD_UINT16(pBuf[13], pBuf[14]);
        connPtr->connParams.offset2         = BUILD_UINT16(pBuf[15], pBuf[16]);
        connPtr->connParams.offset3         = BUILD_UINT16(pBuf[17], pBuf[18]);
        connPtr->connParams.offset4         = BUILD_UINT16(pBuf[19], pBuf[20]);
        connPtr->connParams.offset5         = BUILD_UINT16(pBuf[21], pBuf[22]);

        status = LL_STATUS_SUCCESS;
        // check LE event mask
        if ( MAP_HCI_CheckEventMaskLe(LE_EVT_REMOTE_CONN_PARAM_REQUEST_BIT) == 0 )
        {
          status = LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE;
        }
        else
        // check the basic connection parameters are valid
        // check the combination of the basic connection parameters are valid
        // check the parameters for the connection parameters request are valid
        // check the parameters for APTO are valid
        if ( MAP_llValidateConnParams( connPtr,
                                       connPtr->connParams.intervalMin,
                                       connPtr->connParams.intervalMax,
                                       connPtr->connParams.latency,
                                       connPtr->connParams.timeout,
                                       connPtr->currentEvent,
                                       connPtr->connParams.periodicity,
                                       connPtr->connParams.refConnEvtCount,
                                       (uint16 *)&connPtr->connParams.offset0 ) )
        {
          status = LL_STATUS_ERROR_INVALID_PARAMS;
        }

        if ( status != LL_STATUS_SUCCESS )
        {
          // check if the Reject Indication Extended feature is supported
          if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_REJECT_EXT_IND) )
          {
            // control packet received for features that are not supported
            connPtr->unknownCtrlType = LL_CTRL_CONNECTION_PARAM_REQ;

            // setup/send an Unknown Response
            // Note: This control packet is not queued, but merely sent.
            if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == FALSE )
            {
              // unable to malloc a packet!
              (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
            }
          }
          else // the Reject Indication Extended is at least supported
          {
            // ALT: If the parameters are invalid, then send Reject with
            //      LL_STATUS_ERROR_UNSUPPORTED_PARAM_VAL.
            // send a Reject Indication
            MAP_llSendReject( connPtr,
                              LL_CTRL_CONNECTION_PARAM_REQ,
                              status );

            MAP_LL_ConnParamUpdateRejectCback( status,
                                               connPtr->connId,
                                               connPtr->connParams.intervalMax,
                                               connPtr->connParams.latency,
                                               connPtr->connParams.timeout );
          }
        }
        else // parameters are okay
        {
          // TBD: Extend this feature by comparing/evaluating parameters, and
          //      determine the best connection interval between min/max,
          //      taking periodicity into account.
          // ALT: If the parameters are no good for LL, then send Reject with
          //      LL_STATUS_ERROR_UNSUPPORTED_PARAM_VAL.
          // for now, set the values as before.
          // Note: If indicated to the Host, these values could be changed by
          //       the Host's Reply.
          connPtr->paramUpdate.connInterval = connPtr->connParams.intervalMax; // Interval_Max;
          connPtr->paramUpdate.peripheralLatency = connPtr->connParams.latency;     // Latency;
          connPtr->paramUpdate.connTimeout  = connPtr->connParams.timeout;     // Timeout;

          // TBD: If a valid offset is used, then set window size and window
          //      offset based on this.

          // window size (units of 1.25ms)
          connPtr->paramUpdate.winSize = LL_WINDOW_SIZE;

          // set the window offset (units of 1.25ms)
          connPtr->paramUpdate.winOffset = LL_WINDOW_OFFSET;

          // set the relative offset of the number of events for the update to take place
          // Note: The absolute event number will be determined at the time the packet
          //       is placed in the TX FIFO.
          // Note: The central should allow a minimum of 6 connection events that the
          //       peripheral will be listening for before the instant occurs.
          connPtr->paramUpdateEvent = (connPtr->curParam.peripheralLatency+1) + LL_INSTANT_NUMBER_MIN;

          // determine if the LL should indicate this request to the Host
          // Note: If only an anchor point move, then don't notify the Host.
          // Note: If the CI/SL/LSTO has changed, but are still within what is
          //       requested, then don't notify the Host.

          // check for any CI/SL/LSTO change, and if so, pass params to Host
          // TBD: Determine how the current CI is supposed to be compared to a
          //      a min/max CI. Easy if outside the range, but what about when
          //      inside the range?
          // TBD: Determine if LL should first compare/evaluate parameters to
          //      obtain the CI/SL/LSTO values, then check if different from
          //      the current parameters, then indicate to them to the Host
          //      (which could of course change them again).
          if ( (connPtr->paramUpdate.connInterval != (connPtr->curParam.connInterval >> 1)) ||
               (connPtr->paramUpdate.peripheralLatency != connPtr->curParam.peripheralLatency) ||
               (connPtr->paramUpdate.connTimeout  != (connPtr->curParam.connTimeout >> 4)) )
          {
            // check if the Peripheral has queued a connection parameter request
            // Note: If the Peripheral has already queued a connection parameter
            //       request, but is now receiving a connection parameter
            //       request from the Central, then we have a collision and the
            //       Central's connection parameter request overrules.
            if ( (connPtr->ctrlPktInfo.ctrlPktActive == TRUE) &&
                 (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CONNECTION_PARAM_REQ) )
            {
              // Collision! Central's connection parameter request overrules.

#ifdef LL_TEST_MODE
              if ( llTestMode.testCase == LL_TEST_MODE_TP_CON_MAS_BV_26 )
              {
                // so continue with the Central's control procedure
                // Note: A received Reject will dequeue the Peripheral's request.
                MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_CONNECTION_PARAM_RSP );
              }
              else
#endif // !LL_TEST_MODE
              {
                // enqueue a place holder
                // Note: The LL_RemoteConnParamReqCback will result in an event
                //       being sent to the Host. The Host will respond with
                //       either a Reply or a Negative Reply command. That code
                //       will replace the control opcode at the head of the
                //       ctrlPktInfo.ctrlPkts queue. This is precisely how this
                //       works even when there isn't a collision.
                MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_DUMMY_PLACE_HOLDER_TRANSMIT );

                // this callback sends an event to the Host; the Host will
                // respond with either a Reply or a Negative Reply command
                MAP_LL_RemoteConnParamReqCback( connPtr->connId,
                                                connPtr->connParams.intervalMin,
                                                connPtr->connParams.intervalMax,
                                                connPtr->connParams.latency,
                                                connPtr->connParams.timeout );
              }
            }
            // check if any control procedure is currently active
            else if (( connPtr->ctrlPktInfo.ctrlPktCount > 0 ) &&
                     (connPtr->ctrlPktInfo.ctrlPkts[0] != LL_CTRL_CTE_REQ))
            {
              // Collision! we can't continue at this point, so save type
              connPtr->ctrlPktInfo.ctrlPktPending = LL_CTRL_CONNECTION_PARAM_REQ;

              // and delay the processing of this control procedure
              MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_DUMMY_PLACE_HOLDER_RECEIVE );
            }
            else // okay to continue now
            {
              // enqueue a place holder
              llManageControlPacketQueue( connPtr, LL_CTRL_DUMMY_PLACE_HOLDER_TRANSMIT );

              // Note: Will either get a Reply or a Negative Reply from Host
              MAP_LL_RemoteConnParamReqCback( connPtr->connId,
                                              connPtr->connParams.intervalMin,
                                              connPtr->connParams.intervalMax,
                                              connPtr->connParams.latency,
                                              connPtr->connParams.timeout );
            }
          }
          else // Host doesn't need to be involved
          {
            // queue control packet for processing based on update params
            llManageControlPacketQueue( connPtr, LL_CTRL_CONNECTION_PARAM_RSP );
          }
        }
      }

      break;

    /*
    ** PHY Request
    */

    case LL_CTRL_PHY_REQ:
      // check if the PHY Change feature is supported
      // Note: While this device necessarily supports 2M/Coded and Reject
      //       Indication Extended (as this code is conditionally included),
      //       it is still possible the feature exchange resulted in the
      //       connection not supporting these features! Why the peer
      //       would send this control packet when it already indicated it
      //       doesn't support this feature is unknown, but tester code
      //       such as Codenomics might try such things. In any case, since
      //       the spec is specific about the error codes to send when using
      //       Reject Indication Extended for this procedure, it seems better
      //       to just treat this as being an Unknown control packet.
      if ( !(connPtr->featureSetInfo.featureSet[1] & LL_FEATURE_2M_PHY) &&
           !(connPtr->featureSetInfo.featureSet[1] & LL_FEATURE_CODED_PHY) )

      {
        // control packet received for an unsupported connection feature
        connPtr->unknownCtrlType = LL_CTRL_PHY_REQ;

        // setup/send an Unknown Response
        // Note: This control packet is not queued, but merely sent.
        if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == FALSE )
        {
          // unable to malloc a packet!
          (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
        }
      }
#if defined( LL_TEST_MODE )
      else if ( llTestMode.testCase == LL_TEST_MODE_TP_CON_MAS_BV_46 )
      {
        break;
      }
#endif // LL_TEST_MODE
      else // okay to process this control packet
      {
        // check that there is at least one common phy between tx and rx
        // Note: Our device currently only supports symmetric connections.
        //       If there are no common PHYs, then phy will be LL_PHY_NONE.
        // Note: Reuse of pBuf[0] (TX_PHYS) to save needing runtime stack.
        pBuf[0] = (pBuf[0] & pBuf[1]) & LL_PHY_SUPPORTED_PHYS;

        // find common phys between our device and the peer
        // Note: This is needed to ensure our device does not cause asymmetric
        //       PHYs connections. For example, if our device supports both 1M
        //       and 2M for both Tx and Rx, and the peer supports 1M for Tx and
        //       2M for Rx, then the peer could choose 1M for Tx and 2M for Rx!
        //       To prevent this, only the peer's common PHYs are combined with
        //       our common PHYs. In the example given, the peer's common PHYs
        //       results in LL_PHY_NONE.
        connPtr->phyInfo.updatePhy = connPtr->phyInfo.phyPreference & pBuf[0];

        // check if there are more than one PHY common to our device and peer
        // Note: Again, this is needed to ensure our device does not cause an
        //       asymmetric PHY connection. This can result if our device and
        //       the peer device both set the preferred PHYs to 1M and 2M. The
        //       peer device could then do an update to an asymmetric PHY.
        if ( (connPtr->phyInfo.updatePhy != LL_PHY_NONE) &&
             (connPtr->phyInfo.updatePhy & (connPtr->phyInfo.updatePhy-1)) )
        {
          // there is, so need to down select M_TO_S/S_TO_M for Update Phy
          // check if our Host specified Coded in preferences
          // Note: If the Host performs a Set Default Phy or Set Phy with more
          //       than one phy that includes Coded, and given that Coded is
          //       one of the possible choices here, then Coded phy will be selected.
          //       Otherwise, the fast phy will be chosen.
          if (connPtr->phyInfo.updatePhy & LL_PHY_CODED)
          {
            // choose coded
            connPtr->phyInfo.updatePhy = LL_PHY_CODED;
          }
          else // find the fastest
          {
            // there is, so need to down select M_TO_S/S_TO_M for Update Phy
            // by finding the fastest PHY
            // Note: If the fastest PHY happens to be the current PHY, then the
            // next statement will check this and set the update to no PHY.
            for (uint8 i=LL_PHY_FASTEST_PHY; i>0; i>>=1)
            {
              if ( connPtr->phyInfo.updatePhy & i )
              {
                connPtr->phyInfo.updatePhy = i;

                break;
              }
            }
          }
        }

        // check if the update PHY is zero
        if ( connPtr->phyInfo.updatePhy == LL_PHY_NONE )
        {
          // it is, so set the update PHY to the current PHY
          // Note: Spec indicates REQ/RSP should have at least one bit set.
          connPtr->phyInfo.updatePhy = connPtr->phyInfo.curPhy;
        }

        // check if the update will be Coded
        // Note: If so, we have to update connEffectiveMaxTxTime so we can find
        //       the connActualMaxTxOctets.
        if ( connPtr->phyInfo.updatePhy == LL_PHY_CODED )
        {
          connPtr->lenInfo.connEffectiveMaxTxTime =
            MAP_llSetCodedMaxTxTime( connPtr );
        }

        // determine the slowest PHY based on Central's RX_PHYS and our TX_PHYS
        connPtr->lenInfo.connSlowestPhy =
          MAP_llGetSlowestPhy( connPtr->phyInfo.updatePhy );

        // update the effective Tx buffer size, factoring in Time
        // Note: Specfication indicates a Peripheral packet transmit time restriction
        //       when Extended Data Length is used whereby the Peripheral's max Tx
        //       Time was restricted by TX_PHYS when it initiated
        //       the PHY change, and by TX_PHYS and RX_PHYS when it responds
        //       to a PHY change. In either case, this ends when the Update
        //       is received.
        connPtr->lenInfo.connActualMaxTxOctets =
          MIN( connPtr->lenInfo.connEffectiveMaxTxOctets,
               MAP_llTime2Octets( connPtr->lenInfo.connSlowestPhy,
                                  connPtr->phyInfo.phyOpts,
                                  connPtr->lenInfo.connEffectiveMaxTxTime,
                                  MIC_ENABLED ) );

        // send PHY Update Response
        llManageControlPacketQueue( connPtr, LL_CTRL_PHY_RSP );
      }
      break;

    /*
    ** PHY Update Request
    */

    case LL_CTRL_PHY_UPDATE_REQ:
      // check if the parameters are valid and reject if not
      // Note: Error if not equal (only symmetric PHYs allowed by this device),
      //       or they are equal, are not zero, and more than one bit is set
      //       (which is invalid, per spec).
      if ( (pBuf[0] != pBuf[1]) ||
           (((pBuf[0] = ((pBuf[0] & pBuf[1]) & LL_PHY_SUPPORTED_PHYS)) != LL_PHY_NONE) &&
            (pBuf[0] & (pBuf[0]-1))) )
      {
        // so send hardware error event
        MAP_llSendReject( connPtr,
                          LL_CTRL_PHY_UPDATE_REQ,
                          LL_STATUS_ERROR_INVALID_PARAMS );

        // notify the Host
        MAP_LL_PhyUpdateCompleteEventCback( LL_STATUS_ERROR_INVALID_PARAMS,
                                            connPtr->connId,
                                            0,
                                            0 );

        // indicate an update has been received
        SET_FEATURE_FLAG( connPtr->phyInfo.phyFlags, UPDATE_PHY_RECEIVED );
      }
      else // M_TO_S == S_TO_M and either zero or only one phy is set
      {
        // check if there is any phy change
        if ( pBuf[0] != LL_PHY_NONE )
        {
          // set the update phy based M_TO_S_PHY/S_TO_M_PHY
          connPtr->phyInfo.updatePhy = pBuf[0];

          // get the update event count
          connPtr->phyUpdateEvent = BUILD_UINT16(pBuf[2], pBuf[3]);

          // check if update event count is still valid
          // Note: The spec indicates the connection should be terminated when
          //       the instant is in the past.
          if ( ((connPtr->phyUpdateEvent - connPtr->currentEvent) & 0xFFFF) >= LL_MAX_UPDATE_COUNT_RANGE )
          {
            // instant past, so terminate the connection
            // Note: When using PM, it is possible this routine could terminate
            //       connection and try to shutdown the RF Core while the radio
            //       is still running (as we are in the context of an ISR). To
            //       prevent this, we wait until the connection ends before
            //       terminating by letting the connection event complete.

            // set the terminate reason code
            connPtr->termInfo.reason = LL_CTRL_PKT_INSTANT_PASSED_PEER_TERM;

            // set flag to indicate a termination indication was received
            connPtr->termInfo.termIndRcvd = TRUE;

            // ALT: Halt the radio first, then terminate the connection.
            // MAP_llHaltRadio( CMD_ABORT );
            // MAP_llConnTerminate( connPtr, LL_CTRL_PKT_INSTANT_PASSED_PEER_TERM );

            return;
          }

          // set flag to perform update at instant
          connPtr->pendingPhyUpdate = TRUE;

        // check if the update will be Coded
        // Note: If so, we have to update connEffectiveMaxTxTime so we can find
        //       the connActualMaxTxOctets.
        if ( connPtr->phyInfo.updatePhy == LL_PHY_CODED )
        {
          connPtr->lenInfo.connEffectiveMaxTxTime =
            MAP_llSetCodedMaxTxTime( connPtr );
        }

          // set the slowest PHY based on update PHY
          connPtr->lenInfo.connSlowestPhy = connPtr->phyInfo.updatePhy;
        }
        else // no PHY change occurred
        {
          // if Peripheral initiated, then notify Host, else don't
          if ( connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_PHY_REQ )
          {
            // notify the Host
            MAP_LL_PhyUpdateCompleteEventCback( LL_STATUS_SUCCESS,
                                                connPtr->connId,
                                                connPtr->phyInfo.curPhy,
                                                connPtr->phyInfo.curPhy );
          }
          connPtr->pendingPhyUpdate = PHY_UPDATE_APPLIED;
          connPtr->phyUpdatedNoChange = TRUE;
          // set the slowest PHY based on update PHY
          connPtr->lenInfo.connSlowestPhy = connPtr->phyInfo.curPhy;
        }

        // update the effective Tx buffer size, factoring in Time
        // Note: Specfication indicates a Peripheral packet trasmit time restriction
        //       when Extended Data Length is used whereby the Peripheral's max Tx
        //       Time was restricted by TX_PHYS when it initiated the PHY
        //       the PHY change, and by TX_PHYS and RX_PHYS when it responds
        //       to a PHY change. In either case, this ends when the Update
        //       is received.
        connPtr->lenInfo.connActualMaxTxOctets =
          MIN( connPtr->lenInfo.connEffectiveMaxTxOctets,
              MAP_llTime2Octets( connPtr->lenInfo.connSlowestPhy,
                                 connPtr->phyInfo.phyOpts,
                                 connPtr->lenInfo.connEffectiveMaxTxTime,
                                 MIC_ENABLED ) );

        // indicate an update has been received
        SET_FEATURE_FLAG( connPtr->phyInfo.phyFlags, UPDATE_PHY_RECEIVED );

        // set flag to monitor for Central confirmation of Peripheral's ACK for update
        connPtr->updateSLPending = UPDATE_RX_CTRL_ACK_PENDING;
      }
      break;

    /*
    ** Extended Data Length
    */

    case LL_CTRL_LENGTH_REQ:
    case LL_CTRL_LENGTH_RSP:
    {
      uint16 newEffectiveVal;
      uint8  notifyHost;
      uint8  replaceBuffers;
      uint16 receivedRemoteMaxRxOctets;
      uint16 receivedRemoteMaxRxTime;
      uint16 receivedRemoteMaxTxOctets;
      uint16 receivedRemoteMaxTxTime;

      // last actions based on opcode
      if ( opcode == LL_CTRL_LENGTH_REQ )
      {
        // Note: If a data length request arrives between sending the encryption
        //       request and before receiving the encryption response, then
        //       there was a collision and the Controller should respond. But
        //       But if the data length request arrived anytime after the
        //       encryption response, then it is considered an illegal control
        //       packet, and the connection is disconnected with a MIC error.

        // check if encryption is in progress
        if ( connPtr->rxDataEnabled == FALSE )
        {
          // set termination reason code
          connPtr->termInfo.reason = LL_MIC_FAILURE_TERM;

          // set flag to indicate a termination indication was received
          connPtr->termInfo.termIndRcvd = TRUE;

          // ALT: Halt the radio first, then terminate the connection.
          // MAP_llHaltRadio( CMD_ABORT );
          // MAP_llConnTerminate( connPtr, LL_MIC_FAILURE_TERM );

          break;
        }
      }

      // get the received remote data length parameters to validate the values
      receivedRemoteMaxRxOctets = BUILD_UINT16(pBuf[0], pBuf[1]);
      receivedRemoteMaxRxTime   = BUILD_UINT16(pBuf[2], pBuf[3]);
      receivedRemoteMaxTxOctets = BUILD_UINT16(pBuf[4], pBuf[5]);
      receivedRemoteMaxTxTime   = BUILD_UINT16(pBuf[6], pBuf[7]);

      // last actions based on opcode
      if ( opcode == LL_CTRL_LENGTH_RSP )
      {
        // check if remote data length parameters are in valid range
        // in case a parameter is invalid - replace it with the nearest valid boundry value.
        // once replaced - this alone should not trigger a notification to host.
        // a notification to host should be sent only if a valid remote parameter recived and taking its value into
        // account causes a change of the current supported.
        if (receivedRemoteMaxRxOctets < LL_MIN_LINK_DATA_LEN) { receivedRemoteMaxRxOctets = LL_MIN_LINK_DATA_LEN; receivedRemoteMaxRxTime = LL_MIN_LINK_DATA_TIME;  }
        if (receivedRemoteMaxRxOctets > LL_MAX_LINK_DATA_LEN) { receivedRemoteMaxRxOctets = LL_MAX_LINK_DATA_LEN; receivedRemoteMaxRxTime = LL_MAX_LINK_DATA_TIME;  }

        if (receivedRemoteMaxRxTime < LL_MIN_LINK_DATA_TIME)  { receivedRemoteMaxRxTime = LL_MIN_LINK_DATA_TIME;  receivedRemoteMaxRxOctets = LL_MIN_LINK_DATA_LEN; }
        if (receivedRemoteMaxRxTime > LL_MAX_LINK_DATA_TIME)  { receivedRemoteMaxRxTime = LL_MAX_LINK_DATA_TIME;  receivedRemoteMaxRxOctets = LL_MAX_LINK_DATA_LEN; }

        if (receivedRemoteMaxTxOctets < LL_MIN_LINK_DATA_LEN) { receivedRemoteMaxTxOctets = LL_MIN_LINK_DATA_LEN; receivedRemoteMaxTxTime = LL_MIN_LINK_DATA_TIME;  }
        if (receivedRemoteMaxTxOctets > LL_MAX_LINK_DATA_LEN) { receivedRemoteMaxTxOctets = LL_MAX_LINK_DATA_LEN; receivedRemoteMaxTxTime = LL_MAX_LINK_DATA_TIME;  }

        if (receivedRemoteMaxTxTime < LL_MIN_LINK_DATA_TIME)  { receivedRemoteMaxTxTime = LL_MIN_LINK_DATA_TIME;  receivedRemoteMaxTxOctets = LL_MIN_LINK_DATA_LEN; }
        if (receivedRemoteMaxTxTime > LL_MAX_LINK_DATA_TIME)  { receivedRemoteMaxTxTime = LL_MAX_LINK_DATA_TIME;  receivedRemoteMaxTxOctets = LL_MAX_LINK_DATA_LEN; }
      }

      // save off peer's values
      connPtr->lenInfo.connRemoteMaxRxOctets = receivedRemoteMaxRxOctets;
      connPtr->lenInfo.connRemoteMaxRxTime   = receivedRemoteMaxRxTime;
      connPtr->lenInfo.connRemoteMaxTxOctets = receivedRemoteMaxTxOctets;
      connPtr->lenInfo.connRemoteMaxTxTime   = receivedRemoteMaxTxTime;

      //////////////////////////////////////////////////////////////////////////
      // Effective Maximum Rx Time
      // Note: Needed by Effective Maximum Tx Time!
      //////////////////////////////////////////////////////////////////////////

      // find the effective Rx time
      newEffectiveVal = MIN( connPtr->lenInfo.connMaxRxTime,
                             connPtr->lenInfo.connRemoteMaxTxTime );

      // based on current phy
      if ( connPtr->phyInfo.curPhy == LL_PHY_CODED )
      {
        newEffectiveVal = MAX( 2704, newEffectiveVal );
      }

      // check if length info has changed
      notifyHost = (newEffectiveVal != connPtr->lenInfo.connEffectiveMaxRxTime);

      // update the effective Rx buffer size
      connPtr->lenInfo.connEffectiveMaxRxTime = newEffectiveVal;

      //////////////////////////////////////////////////////////////////////////
      // Effective Maximum Tx Time
      // Note: Needed by Effective Maximum Tx Octets!
      //////////////////////////////////////////////////////////////////////////

      // based on current phy
      if ( connPtr->phyInfo.curPhy == LL_PHY_CODED )
      {
        newEffectiveVal =
          MAP_llSetCodedMaxTxTime( connPtr );

        // check if length info has changed
        notifyHost |= (newEffectiveVal != connPtr->lenInfo.connEffectiveMaxTxTime);

        connPtr->lenInfo.connEffectiveMaxTxTime = newEffectiveVal;
      }
      else // !LL_PHY_CODED
      {
        // find the effective Tx time
        newEffectiveVal = MIN( connPtr->lenInfo.connMaxTxTime,
                               connPtr->lenInfo.connRemoteMaxRxTime );

        // check if length info has changed
        notifyHost |= (newEffectiveVal != connPtr->lenInfo.connEffectiveMaxTxTime);

        // update the effective Rx buffer size
        connPtr->lenInfo.connEffectiveMaxTxTime = newEffectiveVal;
      }

      //////////////////////////////////////////////////////////////////////////
      // Effective Maximum Tx Octets
      //////////////////////////////////////////////////////////////////////////

      // find the effective Tx buffer size
      newEffectiveVal = MIN( connPtr->lenInfo.connMaxTxOctets,
                             connPtr->lenInfo.connRemoteMaxRxOctets );

      // check if length info has changed
      // Note: Sets variable to zero or one.
      notifyHost |= (newEffectiveVal != connPtr->lenInfo.connEffectiveMaxTxOctets);

      // update the effective Tx buffer size
      connPtr->lenInfo.connEffectiveMaxTxOctets = newEffectiveVal;

      // update the effective Tx buffer size, factoring in Time
      // Note: The specfication requires that the Controller not transmit PDUs
      //       with payload greater than connEffectiveMaxTxOctets OR take more
      //       than connEffectiveMaxTxTime in us to transmit. By converting
      //       connEffectiveMaxTxTime into octets at the current PHY data rate,
      //       connEffectiveMaxTxOctets can be the lesser of octets and time,
      //       but all in terms of octets. However, in order not to confuse
      //       the Host, connEffectiveMaxTxOctets has to be properly maintained
      //       (i.e. based only on its comparison to connRemoteMaxRxOctets),
      //       and reported, while a different internally used value can be
      //       used to cap Tx based on PHY and PHY changes (as given by
      //       connSlowestPhy).
      connPtr->lenInfo.connActualMaxTxOctets =
        MIN( connPtr->lenInfo.connEffectiveMaxTxOctets,
             MAP_llTime2Octets( connPtr->phyInfo.curPhy,
                                connPtr->phyInfo.phyOpts,
                                connPtr->lenInfo.connEffectiveMaxTxTime,
                                MIC_ENABLED ) );

      //////////////////////////////////////////////////////////////////////////
      // Effective Maximum Rx Octets
      //////////////////////////////////////////////////////////////////////////

      // find the effective Rx buffer size
      newEffectiveVal = MIN( connPtr->lenInfo.connMaxRxOctets,
                             connPtr->lenInfo.connRemoteMaxTxOctets );

      // check if length info has changed
      notifyHost |= (newEffectiveVal != connPtr->lenInfo.connEffectiveMaxRxOctets);


      // check if the new buffer size is larger than current buffer size
      // Note: If this is the case, the old buffers need to be replaced.
      replaceBuffers = (newEffectiveVal > connPtr->lenInfo.connEffectiveMaxRxOctets);

      // update the effective Rx buffer size
      connPtr->lenInfo.connEffectiveMaxRxOctets = newEffectiveVal;

      //////////////////////////////////////////////////////////////////////////
      // Remaining Action
      //////////////////////////////////////////////////////////////////////////

      // set max Rx packet length allowed on connection in PHY
      // check if the old buffers should be replaced
      if ( replaceBuffers )
      {
        // replace unused Rx buffers
        SET_FEATURE_FLAG( connPtr->lenInfo.lenFlags, REPLACE_RX_BUFFERS );
      }

      // last actions based on opcode
      if ( opcode == LL_CTRL_LENGTH_REQ )
      {
        // check if any length info has changed
        if ( notifyHost )
        {
          // yes, so notify Host now
          MAP_LL_DataLengthChangeEventCback( connPtr->connId,
                                             connPtr->lenInfo.connEffectiveMaxTxOctets,
                                             connPtr->lenInfo.connEffectiveMaxTxTime,
                                             connPtr->lenInfo.connEffectiveMaxRxOctets,
                                             connPtr->lenInfo.connEffectiveMaxRxTime );
        }

        // setup/send a Data Length Response
        // Note: This control packet is not queued, but merely sent.
        if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_LENGTH_RSP ) == FALSE )
        {
          // unable to malloc a packet!
          (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
        }
      }
      else // opcode == LL_CTRL_LENGTH_RSP
      {
        // check if any length info has changed
        if ( notifyHost )
        {
          // send data length update event to the Host
          SET_FEATURE_FLAG( connPtr->lenInfo.lenFlags, NOTIFY_HOST );
        }

        // set flag to indicate we received a response to our request
        SET_FEATURE_FLAG( connPtr->lenInfo.lenFlags, LEN_RSP_RECEIVED );
      }
    }
    break;
#ifdef RTLS_CTE
    /*
    ** CTE Request and Response
    */
    case LL_CTRL_CTE_REQ:
      {
        uint8 cteLen = pBuf[0] & LL_CTE_INFO_TIME_MASK;
        uint8 cteType = (pBuf[0] & LL_CTE_INFO_TYPE_MASK) >> LL_CTE_INFO_TYPE_OFFSET;

        // check if the CTE Feature is a supported feature set item
        if (( llCte[connPtr->connId].responder.responseEnable == FALSE ) ||
            ((llCte[connPtr->connId].responder.supportedTypes & BV(cteType)) == 0) ||
            (connPtr->phyInfo.curPhy == LL_PHY_CODED))
        {
           MAP_llSendReject( connPtr,LL_CTRL_CTE_REQ, LL_STATUS_ERROR_UNSUPPORTED_PARAM_VAL );
        }
        else // this feature is supported
        {
          llCte[connPtr->connId].responder.len = cteLen;
          llCte[connPtr->connId].responder.type = cteType;
          // setup/send a Response
          if ( MAP_llSetupCte( connPtr,FALSE ) == FALSE )
          {
            // unable to malloc a packet!
            (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
          }
        }
      }
      break;

    case LL_CTRL_CTE_RSP:
      if (llCte[connPtr->connId].initiator.requestInterval == 0)
      {
        llCte[connPtr->connId].initiator.requestEnable = FALSE;
      }
      llCte[connPtr->connId].initiator.sendRequest = FALSE;
      break;

    /*
    ** Unknown Response
    */
#endif // RTLS_CTE
    // Peer Device Received an Unknown Control Type
    case LL_CTRL_UNKNOWN_RSP:
      // Note: There doesn't appear to be any action for this message,
      //       other than to ACK it.

      // check if a BLE feature is not understood by the peer device
      switch( *pBuf )
      {
        case LL_CTRL_PING_REQ:
          connPtr->pingReqPending = FALSE;
          break;

        case LL_CTRL_CONNECTION_PARAM_REQ:
          connPtr->connParamReqFlags.unknownRspRcved = TRUE;
          // Clear remote feature bit
          connPtr->featureSetInfo.featureSet[0] &= ~LL_FEATURE_CONN_PARAMS_REQ;
          break;

        case LL_CTRL_PERIPHERAL_FEATURE_REQ:
          // indicate the procedure is over
          connPtr->featureSetInfo.featureRspRcved = LL_FEATURE_RSP_FAILED;
          break;

        case LL_CTRL_LENGTH_REQ:
          SET_FEATURE_FLAG( connPtr->lenInfo.lenFlags, UNKNOWN_RSP_RECEIVED );
          SET_FEATURE_FLAG( connPtr->lenInfo.lenFlags, DISABLE_LEN_REQUEST );
          break;

        case LL_CTRL_PHY_REQ:
          SET_FEATURE_FLAG( connPtr->phyInfo.phyFlags, UNKNOWN_RSP_RECEIVED );
          SET_FEATURE_FLAG( connPtr->phyInfo.phyFlags, DISABLE_PHY_REQUEST );

          break;

#ifdef RTLS_CTE
        case LL_CTRL_CTE_REQ:
          llCte[connPtr->connId].initiator.requestEnable = FALSE;
          llCte[connPtr->connId].initiator.sendRequest = FALSE;
          // Clear CTE response feature bit
          connPtr->featureSetInfo.featureSet[2] &= ~(LL_FEATURE_CONNECTION_CTE_RESPONSE);
          HCI_CteRequestFailedEvent(LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE,connPtr->connId);
          break;
#endif
        default:
          break;
      }
      break;

    /*
    ** Default
    */

    // Our Device Received an Unknown Control Type
    default:
      // unknown data PDU control packet received so save the type
      connPtr->unknownCtrlType = opcode;

      // send an Unknown Response
      // Note: This control packet is not queued, but merely sent.
      if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == FALSE )
      {
        // unable to malloc a packet!
        (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
      }

      break;
  } // switch control packet

  return;
}
#endif // ADV_CONN_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * @fn          llProcessCentralControlPacket
 *
 * @brief       This routine is used to process a LL Central PDU Control packet.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to BLE LL Connection.
 * @param       pBuf    - Pointer to Control packet payload.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llProcessCentralControlPacket( llConnState_t *connPtr,
                                   uint8         *pBuf )
{
  uint8 i = 0;
  uint8 opcode = *pBuf++;
  uint8 status = 0;

  // check the type of control packet
  if ((opcode >= LL_CTRL_CS_SEC_RSP) &&
      (opcode <= LL_CTRL_CS_SEC_REQ) )
  {
      MAP_llCsProcessCsControlPacket(opcode, connPtr, pBuf);
      return;
  }
  switch( opcode )
  {
    // Encryption Response
    case LL_CTRL_ENC_RSP:
#ifdef LL_TEST_MODE
      if ( llTestMode.testCase == LL_TEST_MODE_TP_SEC_SLA_BI_05 )
      {
        MAP_llSetupCtrlPkt( connPtr, LL_CTRL_VERSION_IND);
      }
#endif // LL_TEST_MODE

      // concatenate peripheral's SKDs with SKDm
      // Note: The SKDs MSO is the MSO of the SKD.
      pBuf = MAP_llMemCopySrc( (uint8 *)&connPtr->encInfo.SKD[LL_ENC_SKD_S_OFFSET], pBuf, LL_ENC_SKD_S_LEN );

      // bytes are received LSO..MSO, but need to be maintained as
      // MSO..LSO, per FIPS 197 (AES), so reverse the bytes
      MAP_LL_ENC_ReverseBytes( &connPtr->encInfo.SKD[LL_ENC_SKD_S_OFFSET], LL_ENC_SKD_S_LEN );

      // concatenate the peripheral's IVs with IVm
      // Note: The IVs MSO is the MSO of the IV.
      pBuf = MAP_llMemCopySrc( (uint8 *)&connPtr->encInfo.IV[LL_ENC_IV_S_OFFSET], pBuf, LL_ENC_IV_S_LEN );

      // bytes are received LSO..MSO, but need to be maintained as
      // MSO..LSO, per FIPS 197 (AES), so reverse the bytes
      // ALT: Maintain the IV in LSO..MSO order as the Nonce is formed that way.
      MAP_LL_ENC_ReverseBytes( &connPtr->encInfo.IV[LL_ENC_IV_S_OFFSET], LL_ENC_IV_S_LEN );

      // place the IV into the Nonce to be used for this connection
      // Note: If a Pause Encryption control procedure is started, the
      //       old Nonce value will be used until encryption is disabled.
      // Note: The IV is sequenced LSO..MSO within the Nonce.
      // ALT: Maintain the IV in LSO..MSO order as the Nonce is formed that way.
      for (i=0; i<LL_ENC_IV_LEN; i++)
      {
        connPtr->encInfo.nonce[ LL_ENC_NONCE_IV_OFFSET+i ] =
          connPtr->encInfo.IV[ (LL_ENC_IV_LEN-i)-1 ];
      }

      // generate the Session Key (i.e. SK = AES128(LTK, SKD))
      LL_ENC_GenerateSK( connPtr->encInfo.LTK,
                         connPtr->encInfo.SKD,
                         connPtr->encInfo.SK );

      // indicate the SK is valid
      connPtr->encInfo.SKValid = TRUE;

      // set flag to discard all incoming data transmissions
      connPtr->rxDataEnabled = FALSE;

      // Note: Done for now; the peripheral will send LL_CTRL_START_ENC_REQ.

      break;

    // Start Encryption Request
    case LL_CTRL_START_ENC_REQ:
      // set a flag to indicate we've received this packet
      connPtr->encInfo.startEncReqRcved = TRUE;

      break;

    // Start Encryption Response
    case LL_CTRL_START_ENC_RSP:
      // check for re-start encryption
      if ( connPtr->encInfo.encRestart == TRUE )
      {
        // check if the Ping Feature is a supported feature set item
        if ( (connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_PING) )
        {
          // reset the expiration toggle flag
          connPtr->numAptoExp = 0;

          // re-start encryption, so just update timer
          MAP_osal_CbTimerUpdate( connPtr->aptoTimerId,
                                 (connPtr->aptoValue / 2) );
        }

        // notify the Host a key change was requested
        MAP_LL_EncKeyRefreshCback( connPtr->connId,
                                   LL_ENC_KEY_REQ_ACCEPTED );
      }
      else
      {
        // check if the Ping Feature is a supported feature set item
        if ( (connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_PING) )
        {
          // restart the APTO timer
          MAP_osal_CbTimerStart( MAP_llCBTimer_AptoExpiredCback,
                             (uint8 *)connPtr,
                             (connPtr->aptoValue / 2),
                             &connPtr->aptoTimerId );
        }

        // a new encryption was requested
        MAP_LL_EncChangeCback( connPtr->connId,
                               LL_ENC_KEY_REQ_ACCEPTED,
                               LL_ENCRYPTION_ON );
      }

      // set flag to allow outgoing data transmissions
      connPtr->txDataEnabled = TRUE;

      // return any data entries that were stalled on temp queue to the RF queue
      MAP_llMoveTempTxDataEntries( connPtr );

      // okay to receive data again
      connPtr->rxDataEnabled = TRUE;

      // indicate we've received the start encryption response
      connPtr->encInfo.startEncRspRcved = TRUE;

      break;

    // Pause Encryption Response
    case LL_CTRL_PAUSE_ENC_RSP:
      // set a flag to indicate we have received LL_START_ENC_RSP
      connPtr->encInfo.pauseEncRspRcved = TRUE;

      break;

    // Reject Encryption Indication
    case LL_CTRL_REJECT_IND:
      // either the peripheral's Host has failed to provide an LTK, or
      // the encryption feature is not supported by the peripheral, so read
      // the rejection indication error code
      connPtr->encInfo.encRejectErrCode = *pBuf;

      // and end the start encryption procedure
      connPtr->encInfo.rejectIndRcved = TRUE;

      break;

    // Controller Peripheral Feature Setup
    case LL_CTRL_PERIPHERAL_FEATURE_REQ:
#ifdef LL_TEST_MODE
      switch( llTestMode.testCase )
      {
        case LL_TEST_MODE_TP_CON_SLA_BI_06:
          return;

          // Note: Unreachable statement generates compiler warning!
          //break;

        default:
          break;
      }
#endif // LL_TEST_MODE

      // Note: If a feature request arrives between sending the encryption
      //       request and before receiving the encryption response, then there
      //       was a collision and the Controller should respond. But if the
      //       feature request arrived anytime after the encryption response,
      //       then it is considered an illegal control packet and the
      //       connection is disconnected with a MIC error.

      // check if encryption is in progress
      if ( connPtr->rxDataEnabled == FALSE )
      {
        // set termination reason code
        connPtr->termInfo.reason = LL_MIC_FAILURE_TERM;

        // set flag to indicate a termination indication was received
        connPtr->termInfo.termIndRcvd = TRUE;

        // ALT: Halt the radio first, then terminate the connection.
        // MAP_llHaltRadio( CMD_ABORT );
        // MAP_llConnTerminate( connPtr, LL_MIC_FAILURE_TERM );
      }
      // okay to respond, if supported
      else if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_SLV_FEATURES_EXCHANGE) )
      {
        // unknown data PDU control packet received so save the type
        connPtr->unknownCtrlType = opcode;

        // setup/send a Unknown Response
        // Note: This control packet is not queued, but merely sent.
        if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == FALSE )
        {
          // unable to malloc a packet!
          (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
        }
      }
      else // supported feature for this connection
      {
        // Create a feature set for response PDU
        llCreateCommonFeatureSet(connPtr, pBuf);

        // check if we already started an encryption procedure
        if ( connPtr->txDataEnabled == FALSE )
        {
          // yes, so delay the processing of this control procedure
          MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_FEATURE_RSP );
        }
        else // we haven't, so send reply immediately
        {
          // setup/send a Feature Request Response
          // Note: This control packet is not queued, but merely sent.
          // Note: Features to be used will be taken on the next connection
          //       event after the response is successfully transmitted.
          if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_FEATURE_RSP ) == FALSE )
          {
            // unable to malloc a packet!
            (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
          }
        }
      }
      break;

    // Controller Feature Setup
    case LL_CTRL_FEATURE_RSP:
      // Create a feature set for response PDU
      llCreateCommonFeatureSet(connPtr, pBuf);

      // set flag to indicate the response has been received
      connPtr->featureSetInfo.featureRspRcved = LL_FEATURE_RSP_DONE;

      // check if the peer does not support Coded
      if ( !(connPtr->featureSetInfo.featureSet[1] & LL_FEATURE_CODED_PHY) )
      {
        // they do not, so limit max Tx/Rx time to no greater than 2120us
        // Note: Per Vol 6, Part B, Section 5.1.9.
        connPtr->lenInfo.connMaxTxTime = LL_MAX_LINK_DATA_TIME_UNCODED;
        connPtr->lenInfo.connMaxRxTime = LL_MAX_LINK_DATA_TIME_UNCODED;
      }

      if ( deviceFeatureSet.featureSet[0] & LL_FEATURE_DATA_PACKET_LENGTH_EXTENSION )
      {
        // check if the default max number of Tx/Rx bytes is not the min OTA size
        // of 27, or the max Tx/Rx time is not the min OTA time of 328us
        // Note: Will need to adjust these time checks when AE is added.
        if ( (connPtr->lenInfo.connMaxTxOctets != LL_MIN_LINK_DATA_LEN)  ||
             (connPtr->lenInfo.connMaxRxOctets != LL_MIN_LINK_DATA_LEN)  ||
             (connPtr->lenInfo.connMaxTxTime   != LL_MIN_LINK_DATA_TIME) ||
             (connPtr->lenInfo.connMaxRxTime   != LL_MIN_LINK_DATA_TIME) )
        {
          // schedule a data length update control procedure as soon as possible
	      MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_LENGTH_REQ );
        }
      }

      break;

    // Version Information Indication
    case LL_CTRL_VERSION_IND:
#ifdef LL_TEST_MODE
      switch( llTestMode.testCase )
      {
        case LL_TEST_MODE_TP_CON_SLA_BI_05:
          return;
          // Note: Unreachable statement generates compiler warning!
          //break;

        default:
          break;
      }
#endif // LL_TEST_MODE

      // check if the peer's version information has already been obtained
      if ( connPtr->verExchange.peerInfoValid != TRUE )
      {
        // get the peer's version information and save it
        connPtr->verInfo.verNum = *pBuf++;

        // copy compnay ID
        pBuf = MAP_llMemCopySrc( (uint8 *)&connPtr->verInfo.comId, pBuf, 2 );

        // copy subversion number
        pBuf = MAP_llMemCopySrc( (uint8 *)&connPtr->verInfo.subverNum, pBuf, 2 );

        // set a flag to indicate it is now valid
        connPtr->verExchange.peerInfoValid = TRUE;
      }

      // Note: If the version indication arrived between sending the encryption
      //       request and before receiving the encryption response, then there
      //       was a collision and the Controller should respond (if a version
      //       indication exchange hasn't already occurred). But if the version
      //       indication arrived anytime after the encryption response, then
      //       it is considered an illegal control packet and the connection
      //       is disconnected with a MIC error.
      // Note: Vol 6, Part B, Section 5.1.3.1 implies only START_ENC_REQ,
      //       START_ENC_RSP, TERMINATE_IND, and REJECT can be sent by the
      //       Peripheral, but here, the interpretation taken is the Peripheral is
      //       replying.
      // Note: Vol 6, Part B, Section 5.1.3.1, paragraph 8 says the Peripheral
      //       should accept UNKNOWN_RSP after ENC_RSP is sent.

      // check if the ENC_RSP has been received yet
      if ( connPtr->rxDataEnabled == FALSE )
      {
        // set termination reason code
        connPtr->termInfo.reason = LL_MIC_FAILURE_TERM;

        // set flag to indicate a termination indication was received
        connPtr->termInfo.termIndRcvd = TRUE;

        // ALT: Halt the radio first, then terminate the connection.
        // MAP_llHaltRadio( CMD_ABORT );
        // MAP_llConnTerminate( connPtr, LL_MIC_FAILURE_TERM );
      }
      else // ENC_RSP not sent yet, so okay to reply
      {
        // check if a version indication has been sent or if the host not already ask for it
        if ( (connPtr->verExchange.verInfoSent == FALSE) && (connPtr->verExchange.hostRequest == FALSE) )
        {
          // check if we already started an encryption procedure
          if ( connPtr->txDataEnabled == FALSE )
          {
            // yes, so delay the processing of this control procedure
            MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_VERSION_IND );
          }
          else // we haven't, so send reply immediately
          {
            // send peer's request for our version information
            // Note: This control packet is not queued, but merely sent.
            if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_VERSION_IND) == FALSE )
            {
              // unable to malloc a packet!
              (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
              break;
            }
            // version indication has been sent
            connPtr->verExchange.verInfoSent = TRUE;
          }
        }
      }
      break;

    // Ping Request
    case LL_CTRL_PING_REQ:
#ifdef LL_TEST_MODE
      switch( llTestMode.testCase )
      {
        case LL_TEST_MODE_TP_SEC_SLA_BV_08:
          return;

          // Note: Unreachable statement generates compiler warning!
          //break;

        default:
          break;
      }
#endif // LL_TEST_MODE

      // check if the Ping Feature is a supported feature set item
      if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_PING) )
      {
        // unknown data PDU control packet received so save the type
        connPtr->unknownCtrlType = opcode;

        // setup/send an Unknown Response
        // Note: This control packet is not queued, but merely sent.
        if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == FALSE )
        {
          // unable to malloc a packet!
          (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
        }
      }
      else // this feature is supported
      {
        // setup/send a Ping Response
        // Note: This control packet is not queued, but merely sent.
        if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_PING_RSP) == FALSE )
        {
          // unable to malloc a packet!
          (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
        }
      }
      break;

    // Ping Response
    case LL_CTRL_PING_RSP:
      connPtr->pingReqPending = FALSE;
      break;

    // Terminate Indication
    case LL_CTRL_TERMINATE_IND:
#if defined(LL_TEST_MODE)
      switch( llTestMode.testCase )
      {
        case LL_TEST_MODE_TP_CON_SLA_BI_02:
          return;

          //Note: Unreachable statement generates compiler warning!
          //break;

        default:
          break;
      }
#endif

      // read the reason code
      connPtr->termInfo.reason = *pBuf;

      // do not set flag to indicate a termination indication was received
      // Note: When using PM, it is possible this routine could terminate
      //       connection and try to shutdown the RF Core while the radio
      //       is still running (as we are in the context of an ISR). To
      //       prevent this, we wait until the connection ends before
      //       terminating by letting the connection event complete. However,
      //       since the TaskEnd routine checks this flag to terminate, we
      //       must not set it here or we will end prematurely.
      // Note: The flag should already be FALSE, but it is set here to make it
      //       clear that it should not be set to TRUE.
      connPtr->termInfo.termIndRcvd = FALSE;

      // received a terminate from peer host, so terminate after
      // confirming we have sent an ACK
      // Note: For the central, we have to ensure that this control
      //       packet was ACK'ed. For that, the nR has a new flag that
      //       is set when the control packet is received, and cleared
      //       when the control packet received is ACK'ed.
      // Note: This is not an issue as a peripheral because the terminate
      //       packet will re-transmit until the peripheral ACK's.
      MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_TERMINATE_RX_WAIT_FOR_TX_ACK,
                            LL_CTRL_UNDEFINED_PKT );

      break;

    // Connection Parameter Request or Response
    case LL_CTRL_CONNECTION_PARAM_REQ:
      // check if the Connection Parameter Request feature is supported
      if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_CONN_PARAMS_REQ) )
      {
        // control packet received for features that are not supported
        connPtr->unknownCtrlType = LL_CTRL_CONNECTION_PARAM_REQ;

        // setup/send an Unknown Response
        // Note: This control packet is not queued, but merely sent.
        if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == FALSE )
        {
          // unable to malloc a packet!
          (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
        }
      }
      // check if a connection parameter request procedure is already active
      else if ( (connPtr->ctrlPktInfo.ctrlPktActive == TRUE) &&
                (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CONNECTION_PARAM_REQ) )
      {
        // send a Reject Indication
        // Note: Continue to wait for this procedure to complete properly.
        MAP_llSendReject( connPtr,
                          LL_CTRL_CONNECTION_PARAM_REQ,
                          LL_STATUS_ERROR_TRANSACTION_COLLISION );

        MAP_LL_ConnParamUpdateRejectCback( status,
                                           connPtr->connId,
                                           connPtr->connParams.intervalMax,
                                           connPtr->connParams.latency,
                                           connPtr->connParams.timeout );
      }
      // check if an updated parameters control procedure is already pending
      else if ( ((connPtr->ctrlPktInfo.ctrlPktCount > 0) &&
                 (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CONNECTION_UPDATE_IND)) ||
                 (connPtr->pendingParamUpdate == PARAM_UPDATE_PENDING) )
      {
        // check if the Reject Indication Extended feature is supported
        if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_REJECT_EXT_IND) )
        {
          // control packet received for features that are not supported
          connPtr->unknownCtrlType = LL_CTRL_CONNECTION_PARAM_REQ;

          // setup/send an Unknown Response
          // Note: This control packet is not queued, but merely sent.
          if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == FALSE )
          {
            // unable to malloc a packet!
            (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
          }
        }
        else // the Reject Indication Extended is at least supported
        {
          // Collision! Central's update control prcedure overrules, so set
          // rejection opcode and error code and send reject indication.
          // Note: There is no control procedure timeout associated with the
          //       update control procedures. Instead, it only waits for an ACK
          //       while monitoring if the Instant has been passed (in which case
          //       the connection is terminated anyway). So it is okay to queue
          //       this control packet.
          MAP_llSendReject( connPtr,
                            LL_CTRL_CONNECTION_PARAM_REQ,
                            LL_STATUS_ERROR_TRANSACTION_COLLISION );

          MAP_LL_ConnParamUpdateRejectCback( status,
                                             connPtr->connId,
                                             connPtr->connParams.intervalMax,
                                             connPtr->connParams.latency,
                                             connPtr->connParams.timeout );

        }
      }
      // check if an updated channel control procedure is already pending
      else if ( ((connPtr->ctrlPktInfo.ctrlPktCount > 0) &&
                 (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CHANNEL_MAP_IND)) ||
                 (connPtr->pendingChanUpdate == TRUE) )
      {
        // check if the Reject Indication Extended feature is supported
        if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_REJECT_EXT_IND) )
        {
          // control packet received for features that are not supported
          connPtr->unknownCtrlType = LL_CTRL_CONNECTION_PARAM_REQ;

          // setup/send an Unknown Response
          // Note: This control packet is not queued, but merely sent.
          if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == FALSE )
          {
            // unable to malloc a packet!
            (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
          }
        }
        else // the Reject Indication Extended is at least supported
        {
          // Collision! Central's update control prcedure overrules, so set
          // rejection opcode and error code and send a Reject Indication.
          // Note: There is no control procedure timeout associated with the
          //       update control procedures. Instead, it only waits for an ACK
          //       while monitoring if the Instant has been passed (in which case
          //       the connection is terminated anyway). So it is okay to queue
          //       this control packet.
          MAP_llSendReject( connPtr,
                            LL_CTRL_CONNECTION_PARAM_REQ,
                            LL_STATUS_ERROR_DIFFERENT_TRANS_COLLISION );

          MAP_LL_ConnParamUpdateRejectCback( status,
                                             connPtr->connId,
                                             connPtr->connParams.intervalMax,
                                             connPtr->connParams.latency,
                                             connPtr->connParams.timeout );
        }
      }
      else // okay to process parameters
      {
        // save parameters
        connPtr->connParams.intervalMin     = BUILD_UINT16(pBuf[0], pBuf[1]);
        connPtr->connParams.intervalMax     = BUILD_UINT16(pBuf[2], pBuf[3]);
        connPtr->connParams.latency         = BUILD_UINT16(pBuf[4], pBuf[5]);
        connPtr->connParams.timeout         = BUILD_UINT16(pBuf[6], pBuf[7]);
        connPtr->connParams.periodicity     = pBuf[8];
        connPtr->connParams.refConnEvtCount = BUILD_UINT16(pBuf[9], pBuf[10]);
        connPtr->connParams.offset0         = BUILD_UINT16(pBuf[11], pBuf[12]);
        connPtr->connParams.offset1         = BUILD_UINT16(pBuf[13], pBuf[14]);
        connPtr->connParams.offset2         = BUILD_UINT16(pBuf[15], pBuf[16]);
        connPtr->connParams.offset3         = BUILD_UINT16(pBuf[17], pBuf[18]);
        connPtr->connParams.offset4         = BUILD_UINT16(pBuf[19], pBuf[20]);
        connPtr->connParams.offset5         = BUILD_UINT16(pBuf[21], pBuf[22]);

        // check the basic connection parameters are valid
        // check the combination of the basic connection parameters are valid
        // check the parameters for the connection parameters request are valid
        // check the parameters for APTO are valid
        if ( MAP_llValidateConnParams( connPtr,
                                       connPtr->connParams.intervalMin,
                                       connPtr->connParams.intervalMax,
                                       connPtr->connParams.latency,
                                       connPtr->connParams.timeout,
                                       connPtr->currentEvent,
                                       connPtr->connParams.periodicity,
                                       connPtr->connParams.refConnEvtCount,
                                       (uint16 *)&connPtr->connParams.offset0 ) )
        {
          // check if the Reject Indication Extended feature is supported
          if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_REJECT_EXT_IND) )
          {
            // control packet received for features that are not supported
            connPtr->unknownCtrlType = LL_CTRL_CONNECTION_PARAM_REQ;

            // setup/send an Unknown Response
            // Note: This control packet is not queued, but merely sent.
            if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == FALSE )
            {
              // unable to malloc a packet!
              (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
            }
          }
          else // the Reject Indication Extended is at least supported
          {
            // the connection parameter request parameters are invalid
            // so reject them
            MAP_llSendReject( connPtr,
                              LL_CTRL_CONNECTION_PARAM_REQ,
                              LL_STATUS_ERROR_INVALID_PARAMS );

            MAP_LL_ConnParamUpdateRejectCback( status,
                                               connPtr->connId,
                                               connPtr->connParams.intervalMax,
                                               connPtr->connParams.latency,
                                               connPtr->connParams.timeout );
          }
        }
        else // parameters are okay
        {
          // TBD: Extend this feature by comparing/evaluating parameters, and
          //      determine the best connection interval between min/max,
          //      taking periodicity into account.
          // ALT: If the parameters are no good for LL, then send Reject with
          //      LL_STATUS_ERROR_UNSUPPORTED_PARAM_VAL.
          // for now, set the values as before.
          // Note: If indicated to the Host, these values could be changed by
          //       the Host's Reply.
          connPtr->paramUpdate.connInterval = connPtr->connParams.intervalMax; // Interval_Max;
          connPtr->paramUpdate.peripheralLatency = connPtr->connParams.latency;     // Latency;
          connPtr->paramUpdate.connTimeout  = connPtr->connParams.timeout;     // Timeout;

          // TBD: If a valid offset is used, then set window size and window
          //      offset based on this.

          // window size (units of 1.25ms)
          connPtr->paramUpdate.winSize = LL_WINDOW_SIZE;

          // set the window offset (units of 1.25ms)
          connPtr->paramUpdate.winOffset = LL_WINDOW_OFFSET;

#ifdef LL_TEST_MODE
          switch( llTestMode.testCase )
          {
            case LL_TEST_MODE_TP_CON_MAS_BV_31_1:
            case LL_TEST_MODE_TP_CON_MAS_BV_31_2:
            case LL_TEST_MODE_TP_CON_MAS_BV_31_3:
              // override window offset
              if ( connPtr->connParams.offset0 != 0xFFFF )
              {
                // set the window offset (units of 1.25ms)
                connPtr->paramUpdate.winOffset = connPtr->connParams.offset0;
              }
              break;

            case LL_TEST_MODE_TP_CON_MAS_BV_33:
              // override window offset
              if ( connPtr->connParams.offset0 != 0xFFFF )
              {
                // set the window offset (units of 1.25ms)
                connPtr->paramUpdate.winOffset = connPtr->connParams.offset0;
              }

              // DROP THROUGH!

            case LL_TEST_MODE_TP_CON_MAS_BV_32:
              {
                uint16 numMults;
                uint16 intervalMin = connPtr->connParams.intervalMin;
                uint16 intervalMax = connPtr->connParams.intervalMax;
                uint8  periodicity = connPtr->connParams.periodicity;

                numMults  = (intervalMax / periodicity) - (intervalMin / periodicity);
                numMults += ((intervalMin % periodicity) == 0) ? 1 : 0;

                // check if we have at least one CI that's a preferred periodicity multiple
                if ( numMults > 0 )
                {
                  // override the selected connection interval
                  connPtr->paramUpdate.connInterval =
                    ((intervalMin % periodicity) == 0) ?
                    intervalMin                        :
                    (intervalMin + (periodicity - ( intervalMin % periodicity)));
                }
              }
              break;

            case LL_TEST_MODE_TP_CON_SLA_BV_26:
              // pretend there's a collision
              connPtr->connParamReqFlags.connParamReqRcved = TRUE;
              return;

              // Note: Unreachable statement generates compiler warning!
              //break;

            case LL_TEST_MODE_TP_CON_SLA_BV_28:
              {
                uint8 rand[] = { 0x90, 0x78, 0x56, 0x34, 0x12, 0xEF, 0xCD, 0xAB };
                uint8 eDiv[] = { 0x74, 0x24 };
                uint8 ltk[]  = { 0xBF, 0x01, 0xFB, 0x9D, 0x4E, 0xF3, 0xBC, 0x36,
                                 0xD8, 0x74, 0xF5, 0x39, 0x41, 0x38, 0x68, 0x4C };

                // force what looks to the Peripheral what looks like a collision with ENC
                // Note: MAP_ not needed as LL_TEST_MODE is for FlashOnly buidls.
                LL_StartEncrypt( connPtr->connId,
                                 rand,
                                 eDiv,
                                 ltk );
              }
              break;

            // otherwise
            default:
              break;
          }
#endif // LL_TEST_MODE

          // set the relative offset of the number of events for the update to take place
          // Note: The absolute event number will be determined at the time the packet
          //       is placed in the TX FIFO.
          // Note: The central should allow a minimum of 6 connection events that the
          //       peripheral will be listening for before the instant occurs.
          connPtr->paramUpdateEvent = (connPtr->curParam.peripheralLatency+1) + LL_INSTANT_NUMBER_MIN;

          // determine if the LL should indicate this request to the Host
          // Note: If only an anchor point move, then don't notify the Host.
          // Note: If the CI/SL/LSTO has changed, but are still within what is
          //       requested, then don't notify the Host.

          // check for any CI/SL/LSTO change, and if so, pass params to Host
          // TBD: Determine how the current CI is supposed to be compared to a
          //      a min/max CI. Easy if outside the range, but what about when
          //      inside the range?
          // TBD: Determine if LL should first compare/evaluate parameters to
          //      obtain the CI/SL/LSTO values, then check if different from
          //      the current parameters, then indicate to them to the Host
          //      (which could of course change them again).
          if ( (connPtr->paramUpdate.connInterval != (connPtr->curParam.connInterval >> 1)) ||
               (connPtr->paramUpdate.peripheralLatency != connPtr->curParam.peripheralLatency) ||
               (connPtr->paramUpdate.connTimeout  != (connPtr->curParam.connTimeout >> 4)) )
          {
            // but first check if any control procedure is currently active
            if (( connPtr->ctrlPktInfo.ctrlPktCount > 0 ) &&
                ( connPtr->ctrlPktInfo.ctrlPkts[0] != LL_CTRL_CTE_REQ))
            // ALT: Determine if there's a better way here.
            //if ( (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_ENC_REQ)       ||
            //     (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_START_ENC_RSP) ||
            //     (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_PAUSE_ENC_REQ) ||
            //     (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_PAUSE_ENC_RSP))
            {
              // Collision! we can't continue at this point, so save type
              connPtr->ctrlPktInfo.ctrlPktPending = LL_CTRL_CONNECTION_PARAM_REQ;

              // and delay the processing of this control procedure
              MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_DUMMY_PLACE_HOLDER_RECEIVE );
            }
            else // don't need to delay indicating this request to the Host
            {
              // enqueue a place holder
              llManageControlPacketQueue( connPtr, LL_CTRL_DUMMY_PLACE_HOLDER_TRANSMIT );

              // Note: Will either get a Reply or a Negative Reply from Host
              MAP_LL_RemoteConnParamReqCback( connPtr->connId,
                                              connPtr->connParams.intervalMin,
                                              connPtr->connParams.intervalMax,
                                              connPtr->connParams.latency,
                                              connPtr->connParams.timeout );
            }
          }
          else // Host doesn't need to be involved
          {
            // queue control packet for processing based on update params
            llManageControlPacketQueue( connPtr, LL_CTRL_CONNECTION_UPDATE_IND );
          }
        }
      }

      break;

    // Connection Parameter Response
    case LL_CTRL_CONNECTION_PARAM_RSP:
      {
        // save params for validity checks
        uint16 Interval_Min            = BUILD_UINT16(pBuf[0], pBuf[1]);
        uint16 Interval_Max            = BUILD_UINT16(pBuf[2], pBuf[3]);
        uint16 Latency                 = BUILD_UINT16(pBuf[4], pBuf[5]);
        uint16 Timeout                 = BUILD_UINT16(pBuf[6], pBuf[7]);
        uint8  PreferredPeriodicity    = pBuf[8];
        uint16 ReferenceConnEventCount = BUILD_UINT16(pBuf[9], pBuf[10]);
        uint16 *pOffsets               = (uint16 *)&pBuf[11];

        // set connection parameter flag to indicate connection parameter
        // response was received
        // ALT: Can just replace head of queue with Update or Reject.
        //connPtr->connParamReqFlags.connParamRspRcved = TRUE;

        // check the basic connection parameters are valid
        // check the combination of the basic connection parameters are valid
        // check the parameters for the connection parameters request are valid
        // check the parameters for APTO are valid
        if ( MAP_llValidateConnParams( connPtr,
                                       Interval_Min,
                                       Interval_Max,
                                       Latency,
                                       Timeout,
                                       connPtr->currentEvent,
                                       PreferredPeriodicity,
                                       ReferenceConnEventCount,
                                       pOffsets ) )
        {
          // check if the Reject Indication Extended feature is supported
          if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_REJECT_EXT_IND) )
          {
            // control packet received for features that are not supported
            connPtr->unknownCtrlType = LL_CTRL_CONNECTION_PARAM_REQ;

            // setup/send an Unknown Response
            // Note: This control packet is not queued, but merely sent.
            if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == FALSE )
            {
              // unable to malloc a packet!
              (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
            }
          }
          else // the Reject Indication Extended is at least supported
          {
            // remove control packet from processing queue and drop through
            MAP_llDequeueCtrlPkt( connPtr );

            // the connection parameter request parameters are invalid
            // so reject them
            MAP_llSendReject( connPtr,
                              LL_CTRL_CONNECTION_PARAM_REQ,
                              LL_STATUS_ERROR_INVALID_PARAMS );
          }
        }
        else // parameters are okay
        {
          // TBD: Extend this feature by comparing/evaluating parameters, and
          //      determine the best connection interval between min/max,
          //      taking periodicity into account.
          // ALT: If the parameters are no good for LL, then send Reject with
          //      LL_STATUS_ERROR_UNSUPPORTED_PARAM_VAL.
          // for now, assume the peer's parameters

          // update to peer's connection interval, peripheral latency, and LSTO
          connPtr->paramUpdate.connInterval = Interval_Max;
          connPtr->paramUpdate.peripheralLatency = Latency;
          connPtr->paramUpdate.connTimeout  = Timeout;

#ifdef LL_TEST_MODE
          switch( llTestMode.testCase )
          {
            case LL_TEST_MODE_TP_CON_SLA_BV_30_1:
            case LL_TEST_MODE_TP_CON_SLA_BV_30_2:
            case LL_TEST_MODE_TP_CON_SLA_BV_30_3:
              // override window offset
              if ( connPtr->connParams.offset0 != 0xFFFF )
              {
                // set the window offset (units of 1.25ms)
                connPtr->paramUpdate.winOffset = pOffsets[0];
              }
              break;

            case LL_TEST_MODE_TP_CON_SLA_BV_32:
              // override window offset
              if ( connPtr->connParams.offset0 != 0xFFFF )
              {
                // set the window offset (units of 1.25ms)
                connPtr->paramUpdate.winOffset = pOffsets[0];
              }
              // DROP THROUGH!

            case LL_TEST_MODE_TP_CON_SLA_BV_31:
              {
                uint16 numMults;
                uint16 intervalMin = Interval_Min;
                uint16 intervalMax = Interval_Max;
                uint8  periodicity = PreferredPeriodicity;

                numMults  = (intervalMax / periodicity) - (intervalMin / periodicity);
                numMults += ((intervalMin % periodicity) == 0) ? 1 : 0;

                // check if we have at least one CI that's a preferred periodicity multiple
                if ( numMults > 0 )
                {
                  // override the selected connection interval
                  connPtr->paramUpdate.connInterval =
                    ((intervalMin % periodicity) == 0) ?
                    intervalMin                        :
                    (intervalMin + (periodicity - ( intervalMin % periodicity)));
                }
              }
              break;

            case LL_TEST_MODE_JIRA_3478:
              MAP_LL_RemoteConnParamReqNegReply( connPtr->connId,
                                                 HCI_ERROR_CODE_UNACCEPTABLE_CONN_PARAMETERS );
              break;

            // otherwise
            default:
              break;
          }

          // send update indication for all but the following test cases
          if ( llTestMode.testCase != LL_TEST_MODE_JIRA_3478 )
          {
            // queue control packet for processing based on update params
            MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_CONNECTION_UPDATE_IND,
                                  LL_CTRL_UNDEFINED_PKT );
          }
#else // !LL_TEST_MODE

          // queue control packet for processing based on update params
          MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_CONNECTION_UPDATE_IND,
                                LL_CTRL_UNDEFINED_PKT );
#endif // LL_TEST_MODE
        }
      }
      break;

    /*
    ** Reject Indication Extended
    */

    // Reject Indication Extended
    case LL_CTRL_REJECT_EXT_IND:
      // save rejected opcode and error code, and indicate RejectIndExt received
      connPtr->rejectIndExt.rejectOpcode = (uint8)pBuf[0];
      connPtr->rejectIndExt.errorCode    = pBuf[1];
#ifdef RTLS_CTE
      if (connPtr->rejectIndExt.rejectOpcode == LL_CTRL_CTE_REQ)
      {
        llCte[connPtr->connId].initiator.requestEnable = FALSE;
        llCte[connPtr->connId].initiator.sendRequest = FALSE;
        HCI_CteRequestFailedEvent(connPtr->rejectIndExt.errorCode,connPtr->connId);
      }
      else
#endif // RTLS_CTE
      {
        // either the peripheral's Host has failed to provide an LTK, or
        // the encryption feature is not supported by the peripheral, so read
        // the rejection indication error code
        connPtr->encInfo.encRejectErrCode = pBuf[1];

        // and end the start encryption procedure
        connPtr->encInfo.rejectIndRcved = TRUE;

        // set connection parameter flag to indicate rejection was received
        connPtr->connParamReqFlags.rejectIndExtRcved = TRUE;
      }
      break;

    /*
    ** PHY Request
    */

    case LL_CTRL_PHY_REQ:
      // check if the PHY Change feature is supported
      // Note: While this device necessarily supports this feature (as the
      //       code is conditionally included), it is possible the connection
      //       does not support this feature after a feature exchange. Although
      //       the peer shouldn't send a control packet for a feature they
      //       indicated they did not support, it is possible Tester software
      //       such as Codenomics might. In any case, treat as Unknown.
      if ( !(connPtr->featureSetInfo.featureSet[1] & LL_FEATURE_2M_PHY) )
      {
        // control packet received for an unsupported connection feature
        connPtr->unknownCtrlType = LL_CTRL_PHY_REQ;

        // setup/send an Unknown Response
        // Note: This control packet is not queued, but merely sent.
        if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == FALSE )
        {
          // unable to malloc a packet!
          (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
        }
      }
#if defined( LL_TEST_MODE )
      else if ( llTestMode.testCase == LL_TEST_MODE_TP_CON_SLA_BV_45 )
      {
        break;
      }
#endif // LL_TEST_MODE
      // check for the same transaction collisions
      // Note: No check if packet is active as it is possible the command
      //       queued
      else if ( (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_PHY_REQ) ||
                (connPtr->pendingPhyUpdate        == TRUE) )
      {
        // Collision! So reject this request.
        // Note: The collision is from same transaction.
        MAP_llSendReject( connPtr,
                          LL_CTRL_PHY_REQ,
                          HCI_ERROR_CODE_LMP_ERR_TRANSACTION_COLLISION );
      }
      // check for the other transaction collisions
      else if ( (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CONNECTION_UPDATE_IND) ||
                (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CHANNEL_MAP_IND)       ||
                (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CONNECTION_PARAM_REQ)  ||
                (connPtr->pendingParamUpdate      == PARAM_UPDATE_PENDING)          ||
                (connPtr->pendingChanUpdate       == TRUE) )
      {
        // Collision! So reject this request.
        // Note: The collision is from other transaction.
        MAP_llSendReject( connPtr,
                          LL_CTRL_PHY_REQ,
                          HCI_ERROR_CODE_DIFFERENT_TRANSACTION_COLLISION );
      }
      else // okay to process this control packet
      {
        // find common phys of peer device
        // Note: We only support symmetric phys!
        pBuf[0] = (pBuf[0] & pBuf[1]) & LL_PHY_SUPPORTED_PHYS;

        // find common phys between our device and the peer
        connPtr->phyInfo.updatePhy = connPtr->phyInfo.phyPreference & pBuf[0];

        // check if there are more than one PHY common to our device and peer
        // Note: Again, this is needed to ensure our device does not cause an
        //       asymmetric PHY connection. This can result if our device and
        //       the peer device both set the preferred PHYs to 1M and 2M. The
        //       peer device could then do an update to an asymmetric PHY.
        if ( (connPtr->phyInfo.updatePhy != LL_PHY_NONE) &&
             (connPtr->phyInfo.updatePhy & (connPtr->phyInfo.updatePhy-1)) )
        {
          // there is, so need to down select M_TO_S/S_TO_M for Update Phy
          // check if our Host specified Coded in preferences
          // Note: If the Host performs a Set Default Phy or Set Phy with more
          //       than one phy that includes Coded, and given that Coded is
          //       one of the possible choices here, then Coded phy will be selected.
          //       Otherwise, the fast phy will be chosen.
          if ( (connPtr->phyInfo.updatePhy & LL_PHY_CODED) &&
               (connPtr->phyInfo.phyOpts  != LL_PHY_OPT_NONE) )
          {
            // choose coded
            connPtr->phyInfo.updatePhy = LL_PHY_CODED;
          }
          else // find the fastest
          {
            // there is, so need to down select M_TO_S/S_TO_M for Update Phy
            // by finding the fastest PHY
            // Note: If the fastest PHY happens to be the current PHY, then the
            // next statement will check this and set the update to no PHY.
            for (i=LL_PHY_FASTEST_PHY; i>LL_PHY_NONE; i>>=1)
            {
              if ( connPtr->phyInfo.updatePhy & i )
              {
                connPtr->phyInfo.updatePhy = i;
                break;
              }
            }
          }
        }

        // check if the update PHY is the same as the current PHY
        if ( connPtr->phyInfo.updatePhy == connPtr->phyInfo.curPhy )
        {
          // it is, so no PHY update needed
          connPtr->phyInfo.updatePhy = LL_PHY_NONE;
        }

        // check if no phy update will occur
        if ( connPtr->phyInfo.updatePhy == LL_PHY_NONE )
        {
          // instant shall be zero
          // Note: No need to adjust connActualMaxTxOctets since no PHY change
          //       is going to occur.
          connPtr->phyUpdateEvent = 0;
        }
        else // set the phy update event
        {
          // set the relative offset, in events, for the update instant
          // Note: The absolute event number will be determined at the time
          //       the packet is queued for transmission.
          // Note: The central should allow a minimum of 6 connection events that
          //       the peripheral will be listening for before the instant occurs.
          connPtr->phyUpdateEvent = (connPtr->curParam.peripheralLatency+1) +
                                     LL_INSTANT_NUMBER_MIN;

          // check if the update will be Coded
          // Note: If so, we have to update connEffectiveMaxTxTime so we can find
          //       the connActualMaxTxOctets.
          if ( connPtr->phyInfo.updatePhy == LL_PHY_CODED )
          {
            connPtr->lenInfo.connEffectiveMaxTxTime =
              MAP_llSetCodedMaxTxTime( connPtr );
          }

          // set the slowest PHY based on update PHY
          // Note: This variable is used in case a Change Length control
          //       procedure occurs.
          connPtr->lenInfo.connSlowestPhy = connPtr->phyInfo.updatePhy;

          // update the effective Tx buffer size, factoring in Time
          // Note: Per the specification, the max Tx Time between sending a PHY
          //       Update and the Instant must be per the PHY change.
          // Note: The specfication requires that the Controller not transmit
          //       PDUs with payload greater than connEffectiveMaxTxOctets OR
          //       take more than connEffectiveMaxTxTime in us to transmit. By
          //       converting connEffectiveMaxTxTime into octets at the current
          //       PHY data rate, connEffectiveMaxTxOctets can be the lesser of
          //       octets and time, but all in terms of octets.
          connPtr->lenInfo.connActualMaxTxOctets =
            MIN( connPtr->lenInfo.connEffectiveMaxTxOctets,
                 MAP_llTime2Octets( connPtr->lenInfo.connSlowestPhy,
                                    connPtr->phyInfo.phyOpts,
                                    connPtr->lenInfo.connEffectiveMaxTxTime,
                                    MIC_ENABLED ) );
        }

        // queue update phy control packet
        llManageControlPacketQueue( connPtr, LL_CTRL_PHY_UPDATE_REQ );
      }
      break;

    /*
    ** PHY Response
    */

    case LL_CTRL_PHY_RSP:
      // check that there is at least one common phy between tx and rx
      // Note: This device currently only supports symmetric connections.
      //       If there are no common PHYs, then phy will be LL_PHY_NONE.
      pBuf[0] = (pBuf[0] & pBuf[1]) & LL_PHY_SUPPORTED_PHYS;

      // find a common phy between our device and the peer
      connPtr->phyInfo.updatePhy &= pBuf[0];

      // check if there are more than one PHY common to our device and peer
      if ( (connPtr->phyInfo.updatePhy != LL_PHY_NONE) &&
           (connPtr->phyInfo.updatePhy & (connPtr->phyInfo.updatePhy-1)) )
      {
        // there is, so need to down select M_TO_S/S_TO_M for Update Phy
        // check if our Host specified Coded in preferences
        // Note: If the Host performs a Set Default Phy or Set Phy with more
        //       than one phy that includes Coded, and given that Coded is
        //       one of the possible choices here, then the phyOptions
        //       parameter will be used decide which phy will be selected.
        //       Otherwise, the fast phy will be chosen.
        // Note: The value of phyOpts defaults to LL_PHY_OPT_NONE.
        if ( (connPtr->phyInfo.updatePhy & LL_PHY_CODED) &&
             (connPtr->phyInfo.phyOpts  != LL_PHY_OPT_NONE) )
        {
          // choose coded
          connPtr->phyInfo.updatePhy = LL_PHY_CODED;
        }
        else // find the fastest
        {
          // Note: If the fastest PHY happens to be the current PHY, then the
          // next statement will check this and set the update to no PHY.
          for (i=LL_PHY_FASTEST_PHY; i>0; i>>=1)
          {
            // check if we have a PHY that isn't the current PHY
            if ( connPtr->phyInfo.updatePhy & i )
            {
              // we do, so use it
              connPtr->phyInfo.updatePhy = i;
              break;
            }
          }
        }
      }

      // check if the update PHY is the same as the current PHY
      if ( connPtr->phyInfo.updatePhy == connPtr->phyInfo.curPhy )
      {
        // it is, so no PHY update needed
        connPtr->phyInfo.updatePhy = LL_PHY_NONE;
      }

      // check if no phy update will occur
      if (  connPtr->phyInfo.updatePhy == LL_PHY_NONE )
      {
        // instant shall be zero
        connPtr->phyUpdateEvent = 0;

        // set the PHY for connActualMaxTxOctets to the current PHY
        // Note: Necessary as connActualMaxTxOctets was adjusted as the PHY
        //       control procedure progressed. Now that it turned out there
        //       will be no PHY change, we must restore connActualMaxTxOctets
        //       based on our PHY before this control procedure began.
        connPtr->lenInfo.connSlowestPhy = connPtr->phyInfo.curPhy;
      }
      else // set the phy update event
      {
        // set the relative offset, in events, for the update instant
        // Note: The absolute event number will be determined at the time
        //       the packet is queued for transmission.
        // Note: The central should allow a minimum of 6 connection events that
        //       the peripheral will be listening for before the instant occurs.
        connPtr->phyUpdateEvent = (connPtr->curParam.peripheralLatency+1) +
                                  LL_INSTANT_NUMBER_MIN;

        // check if the update will be Coded
        // Note: If so, we have to update connEffectiveMaxTxTime so we can find
        //       the connActualMaxTxOctets.
        if ( connPtr->phyInfo.updatePhy == LL_PHY_CODED )
        {
          connPtr->lenInfo.connEffectiveMaxTxTime =
            MAP_llSetCodedMaxTxTime( connPtr );
        }

        // set the PHY for connActualMaxTxOctets based on the update PHY
        connPtr->lenInfo.connSlowestPhy = connPtr->phyInfo.updatePhy;
      }

      // update the effective Tx buffer size, factoring in Time
      // Note: Per the specification, the max Tx Time between sending a PHY
      //       Update and the Instant must be per the PHY change.
      // Note: The specfication requires that the Controller not transmit PDUs
      //       with payload greater than connEffectiveMaxTxOctets OR take more
      //       than connEffectiveMaxTxTime in us to transmit. By converting
      //       connEffectiveMaxTxTime into octets at the current PHY data rate,
      //       connEffectiveMaxTxOctets can be the lesser of octets and time,
      //       but all in terms of octets.
      connPtr->lenInfo.connActualMaxTxOctets =
        MIN( connPtr->lenInfo.connEffectiveMaxTxOctets,
             MAP_llTime2Octets( connPtr->lenInfo.connSlowestPhy,
                                connPtr->phyInfo.phyOpts,
                                connPtr->lenInfo.connEffectiveMaxTxTime,
                                MIC_ENABLED ) );

      // set flag to indicate we received a response to our request
      // Note: This will cause an Update Phy to be sent to Central.
      SET_FEATURE_FLAG( connPtr->phyInfo.phyFlags, PHY_RSP_RECEIVED );
      break;

    /*
    ** Extended Data Length
    */
    case LL_CTRL_LENGTH_REQ:
    case LL_CTRL_LENGTH_RSP:
    {
      uint16 newEffectiveVal;
      uint8  notifyHost;
      uint8  replaceBuffers;
      uint16 receivedRemoteMaxRxOctets;
      uint16 receivedRemoteMaxRxTime;
      uint16 receivedRemoteMaxTxOctets;
      uint16 receivedRemoteMaxTxTime;

      // last actions based on opcode
      if ( opcode == LL_CTRL_LENGTH_REQ )
      {
        // Note: If a data length request arrives between sending the encryption
        //       request and before receiving the encryption response, then
        //       there was a collision and the Controller should respond. But
        //       But if the data length request arrived anytime after the
        //       encryption response, then it is considered an illegal control
        //       packet, and the connection is disconnected with a MIC error.

        // check if encryption is in progress
        if ( connPtr->rxDataEnabled == FALSE )
        {
          // set termination reason code
          connPtr->termInfo.reason = LL_MIC_FAILURE_TERM;

          // set flag to indicate a termination indication was received
          connPtr->termInfo.termIndRcvd = TRUE;

          // ALT: Halt the radio first, then terminate the connection.
          // MAP_llHaltRadio( CMD_ABORT );
          // MAP_llConnTerminate( connPtr, LL_MIC_FAILURE_TERM );

          break;
        }
      }

      // get the received remote data length parameters to validate the values
      receivedRemoteMaxRxOctets = BUILD_UINT16(pBuf[0], pBuf[1]);
      receivedRemoteMaxRxTime   = BUILD_UINT16(pBuf[2], pBuf[3]);
      receivedRemoteMaxTxOctets = BUILD_UINT16(pBuf[4], pBuf[5]);
      receivedRemoteMaxTxTime   = BUILD_UINT16(pBuf[6], pBuf[7]);

      // last actions based on opcode
      if ( opcode == LL_CTRL_LENGTH_RSP )
      {
        // check if remote data length parameters are in valid range
        // in case a parameter is invalid - replace it with the nearest valid boundry value.
        // once replaced - this alone should not trigger a notification to host.
        // a notification to host should be sent only if a valid remote parameter recived and taking its value into
        // account causes a change of the current supported.
        if (receivedRemoteMaxRxOctets < LL_MIN_LINK_DATA_LEN) { receivedRemoteMaxRxOctets = LL_MIN_LINK_DATA_LEN; receivedRemoteMaxRxTime = LL_MIN_LINK_DATA_TIME;  }
        if (receivedRemoteMaxRxOctets > LL_MAX_LINK_DATA_LEN) { receivedRemoteMaxRxOctets = LL_MAX_LINK_DATA_LEN; receivedRemoteMaxRxTime = LL_MAX_LINK_DATA_TIME;  }

        if (receivedRemoteMaxRxTime < LL_MIN_LINK_DATA_TIME)  { receivedRemoteMaxRxTime = LL_MIN_LINK_DATA_TIME;  receivedRemoteMaxRxOctets = LL_MIN_LINK_DATA_LEN; }
        if (receivedRemoteMaxRxTime > LL_MAX_LINK_DATA_TIME)  { receivedRemoteMaxRxTime = LL_MAX_LINK_DATA_TIME;  receivedRemoteMaxRxOctets = LL_MAX_LINK_DATA_LEN; }

        if (receivedRemoteMaxTxOctets < LL_MIN_LINK_DATA_LEN) { receivedRemoteMaxTxOctets = LL_MIN_LINK_DATA_LEN; receivedRemoteMaxTxTime = LL_MIN_LINK_DATA_TIME;  }
        if (receivedRemoteMaxTxOctets > LL_MAX_LINK_DATA_LEN) { receivedRemoteMaxTxOctets = LL_MAX_LINK_DATA_LEN; receivedRemoteMaxTxTime = LL_MAX_LINK_DATA_TIME;  }

        if (receivedRemoteMaxTxTime < LL_MIN_LINK_DATA_TIME)  { receivedRemoteMaxTxTime = LL_MIN_LINK_DATA_TIME;  receivedRemoteMaxTxOctets = LL_MIN_LINK_DATA_LEN; }
        if (receivedRemoteMaxTxTime > LL_MAX_LINK_DATA_TIME)  { receivedRemoteMaxTxTime = LL_MAX_LINK_DATA_TIME;  receivedRemoteMaxTxOctets = LL_MAX_LINK_DATA_LEN; }
      }

      // save off peer's values
      connPtr->lenInfo.connRemoteMaxRxOctets = receivedRemoteMaxRxOctets;
      connPtr->lenInfo.connRemoteMaxRxTime   = receivedRemoteMaxRxTime;
      connPtr->lenInfo.connRemoteMaxTxOctets = receivedRemoteMaxTxOctets;
      connPtr->lenInfo.connRemoteMaxTxTime   = receivedRemoteMaxTxTime;

      //////////////////////////////////////////////////////////////////////////
      // Effective Maximum Rx Time
      // Note: Needed by Effective Maximum Tx Time!
      //////////////////////////////////////////////////////////////////////////

      // find the effective Rx time
      newEffectiveVal = MIN( connPtr->lenInfo.connMaxRxTime,
                             connPtr->lenInfo.connRemoteMaxTxTime );

      // based on current phy
      if ( connPtr->phyInfo.curPhy == LL_PHY_CODED )
      {
        newEffectiveVal = MAX( 2704, newEffectiveVal );
      }

      // check if length info has changed
      notifyHost = (newEffectiveVal != connPtr->lenInfo.connEffectiveMaxRxTime);

      // update the effective Rx buffer size
      connPtr->lenInfo.connEffectiveMaxRxTime = newEffectiveVal;

      //////////////////////////////////////////////////////////////////////////
      // Effective Maximum Tx Time
      // Note: Needed by Effective Maximum Tx Octets!
      //////////////////////////////////////////////////////////////////////////

      // based on current phy
      if ( connPtr->phyInfo.curPhy == LL_PHY_CODED )
      {
        newEffectiveVal =
          MAP_llSetCodedMaxTxTime( connPtr );

        // check if length info has changed
        notifyHost |= (newEffectiveVal != connPtr->lenInfo.connEffectiveMaxTxTime);

        connPtr->lenInfo.connEffectiveMaxTxTime = newEffectiveVal;
      }
      else // !LL_PHY_CODED
      {
        // find the effective Tx time
        newEffectiveVal = MIN( connPtr->lenInfo.connMaxTxTime,
                               connPtr->lenInfo.connRemoteMaxRxTime );

        // check if length info has changed
        notifyHost |= (newEffectiveVal != connPtr->lenInfo.connEffectiveMaxTxTime);

        // update the effective Rx buffer size
        connPtr->lenInfo.connEffectiveMaxTxTime = newEffectiveVal;
      }

      //////////////////////////////////////////////////////////////////////////
      // Effective Maximum Tx Octets
      //////////////////////////////////////////////////////////////////////////

      // find the effective Tx buffer size
      newEffectiveVal = MIN( connPtr->lenInfo.connMaxTxOctets,
                             connPtr->lenInfo.connRemoteMaxRxOctets );

      // check if length info has changed
      // Note: Sets variable to zero or one.
      notifyHost |= (newEffectiveVal != connPtr->lenInfo.connEffectiveMaxTxOctets);

      // update the effective Tx buffer size
      connPtr->lenInfo.connEffectiveMaxTxOctets = newEffectiveVal;

      // update the effective Tx buffer size, factoring in Time
      // Note: The specfication requires that the Controller not transmit PDUs
      //       with payload greater than connEffectiveMaxTxOctets OR take more
      //       than connEffectiveMaxTxTime in us to transmit. By converting
      //       connEffectiveMaxTxTime into octets at the current PHY data rate,
      //       connEffectiveMaxTxOctets can be the lesser of octets and time,
      //       but all in terms of octets. However, in order not to confuse
      //       the Host, connEffectiveMaxTxOctets has to be properly maintained
      //       (i.e. based only on its comparison to connRemoteMaxRxOctets),
      //       and reported, while a different internally used value can be
      //       used to cap Tx based on PHY and PHY changes (as given by
      //       connSlowestPhy).
      connPtr->lenInfo.connActualMaxTxOctets =
        MIN( connPtr->lenInfo.connEffectiveMaxTxOctets,
             MAP_llTime2Octets( connPtr->phyInfo.curPhy,
                                connPtr->phyInfo.phyOpts,
                                connPtr->lenInfo.connEffectiveMaxTxTime,
                                MIC_ENABLED ) );

      //////////////////////////////////////////////////////////////////////////
      // Effective Maximum Rx Octets
      //////////////////////////////////////////////////////////////////////////

      // find the effective Rx buffer size
      newEffectiveVal = MIN( connPtr->lenInfo.connMaxRxOctets,
                             connPtr->lenInfo.connRemoteMaxTxOctets );

      // check if length info has changed
      notifyHost |= (newEffectiveVal != connPtr->lenInfo.connEffectiveMaxRxOctets);


      // check if the new buffer size is larger than current buffer size
      // Note: If this is the case, the old buffers need to be replaced.
      replaceBuffers = (newEffectiveVal > connPtr->lenInfo.connEffectiveMaxRxOctets);

      // update the effective Rx buffer size
      connPtr->lenInfo.connEffectiveMaxRxOctets = newEffectiveVal;

      //////////////////////////////////////////////////////////////////////////
      // Remaining Action
      //////////////////////////////////////////////////////////////////////////

      // set max Rx packet length allowed on connection in PHY
      // check if the old buffers should be replaced
      if ( replaceBuffers )
      {
        // replace unused Rx buffers
        SET_FEATURE_FLAG( connPtr->lenInfo.lenFlags, REPLACE_RX_BUFFERS );
      }

      // last actions based on opcode
      if ( opcode == LL_CTRL_LENGTH_REQ )
      {
        // check if any length info has changed
        if ( notifyHost )
        {
          // yes, so notify Host now
          MAP_LL_DataLengthChangeEventCback( connPtr->connId,
                                             connPtr->lenInfo.connEffectiveMaxTxOctets,
                                             connPtr->lenInfo.connEffectiveMaxTxTime,
                                             connPtr->lenInfo.connEffectiveMaxRxOctets,
                                             connPtr->lenInfo.connEffectiveMaxRxTime );
        }

        // check if we already started an encryption procedure
        if ( connPtr->txDataEnabled == FALSE )
        {
          // yes, so delay the processing of this control procedure
          llManageControlPacketQueue( connPtr, LL_CTRL_LENGTH_RSP );
        }
        else // we haven't, so send reply immediately
        {
          // setup/send a Data Length Response
          // Note: This control packet is not queued, but merely sent.
          if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_LENGTH_RSP ) == FALSE )
          {
            // unable to malloc a packet!
            (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
          }
        }
      }
      else // opcode == LL_CTRL_LENGTH_RSP
      {
        // check if any length info has changed
        if ( notifyHost )
        {
          // send data length update event to the Host
          SET_FEATURE_FLAG( connPtr->lenInfo.lenFlags, NOTIFY_HOST );
        }

        // set flag to indicate we received a response to our request
        SET_FEATURE_FLAG( connPtr->lenInfo.lenFlags, LEN_RSP_RECEIVED );
      }
    }
    break;

    /*
    ** CTE Request and Response
    */
#ifdef RTLS_CTE
    case LL_CTRL_CTE_REQ:
      {
        uint8 cteLen = pBuf[0] & LL_CTE_INFO_TIME_MASK;
        uint8 cteType = (pBuf[0] & LL_CTE_INFO_TYPE_MASK) >> LL_CTE_INFO_TYPE_OFFSET;

        // check if the CTE Feature is a supported feature set item
        if (( llCte[connPtr->connId].responder.responseEnable == FALSE ) ||
            ((llCte[connPtr->connId].responder.supportedTypes & BV(cteType)) == 0) ||
            (connPtr->phyInfo.curPhy == LL_PHY_CODED))
        {
           MAP_llSendReject( connPtr,LL_CTRL_CTE_REQ, LL_STATUS_ERROR_UNSUPPORTED_PARAM_VAL );
        }
        else // this feature is supported
        {
          llCte[connPtr->connId].responder.len = cteLen;
          llCte[connPtr->connId].responder.type = cteType;
          // setup/send a Response
          if ( MAP_llSetupCte( connPtr,FALSE ) == FALSE )
          {
            // unable to malloc a packet!
            (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
          }
        }
      }
      break;

    case LL_CTRL_CTE_RSP:
      if (llCte[connPtr->connId].initiator.requestInterval == 0)
      {
        llCte[connPtr->connId].initiator.requestEnable = FALSE;
      }
      llCte[connPtr->connId].initiator.sendRequest = FALSE;

      break;
#endif // RTLS_CTE
    /*
    ** Unknown Response
    */

    // Peer Device Received an Unknown Control Type
    case LL_CTRL_UNKNOWN_RSP:
      // check if a BLE feature is not understood by the peer device
      switch( *pBuf )
      {
        case LL_CTRL_PING_REQ:
          connPtr->pingReqPending = FALSE;
          break;

        case LL_CTRL_CONNECTION_PARAM_REQ:
          connPtr->connParamReqFlags.unknownRspRcved = TRUE;
          // Clear remote feature bit
          connPtr->featureSetInfo.featureSet[0] &= ~LL_FEATURE_CONN_PARAMS_REQ;
          break;

        case LL_CTRL_LENGTH_REQ:
          SET_FEATURE_FLAG( connPtr->lenInfo.lenFlags, UNKNOWN_RSP_RECEIVED );
          SET_FEATURE_FLAG( connPtr->lenInfo.lenFlags, DISABLE_LEN_REQUEST );
          break;

        case LL_CTRL_PHY_REQ:
          SET_FEATURE_FLAG( connPtr->phyInfo.phyFlags, UNKNOWN_RSP_RECEIVED );
          SET_FEATURE_FLAG( connPtr->phyInfo.phyFlags, DISABLE_PHY_REQUEST );
          break;
#ifdef RTLS_CTE
        case LL_CTRL_CTE_REQ:
          llCte[connPtr->connId].initiator.requestEnable = FALSE;
          llCte[connPtr->connId].initiator.sendRequest = FALSE;
          // Clear CTE response feature bit
          connPtr->featureSetInfo.featureSet[2] &= ~(LL_FEATURE_CONNECTION_CTE_RESPONSE);
          HCI_CteRequestFailedEvent(LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE,connPtr->connId);
          break;
#endif // RTLS_CTE
        default:
          break;
      }
      break;

    /*
    ** Default
    */

    // Our Device Received an Unknown Control Type
    default:
      // unknown data PDU control packet received so save the type
      connPtr->unknownCtrlType = opcode;

      // send an Unknown Response
      // Note: This control packet is not queued, but merely sent.
      if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == FALSE )
      {
        // unable to malloc a packet!
        (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
      }

      break;
  } // switch control packet

  return;
}
#endif // INIT_CFG


/*******************************************************************************
 * @fn          llAddTxDataEntry API
 *
 * @brief       This function is used to add a Tx data entry to the internal
 *              Tx data entry queue, adjusting the internal number of entries.
 *              If the radio is not on, then the queuing operation is done
 *              manually.
 *
 *              Note: It is assumed the number of allowed entries is managed by
 *                    the calling routine.
 *
 *              Note: It is assumed the data entry queue is really based on a
 *                    data queue.
 *
 *              Note: This command can only fail for one of two reasons:
 *                    either a parameter error (e.g. queue pointer or data
 *                    entry pointer is not in a valid memory address range or
 *                    is not word aligned), or a queue error (i.e. the queue
 *                    can't be appended to; for example, when the pCurrEntry
 *                    is not NULL and pLastEntry is NULL). These conditions
 *                    should never occur, so if we have an error here, it is
 *                    fatal.
 *
 *              Note: We really can not continue from here as we have an
 *                    allocated packet just dangling and if encryption is
 *                    enabled, our packet counter, and thus the nonce, will
 *                    be wrong.
 *
 * input parameters
 *
 * @param       pDataEntryQ   - Pointer to data entry queue.
 * @param       pDataEntry    - Pointer to data entry to add.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llAddTxDataEntry( void *pDataEntryQ,
                       void *pDataEntry )
{
  halIntState_t cs;
  HAL_ENTER_CRITICAL_SECTION(cs);
  // add the buffer to the RF
  RCL_TxBuffer_put(((txDataQ_t *)pDataEntryQ)->rfDataBuffers, pDataEntry);

  // add also the buffer to the LL list so it could relese it after it will be transmitted.
  // we call directly to the List function because the RCL_TxBuffer_put will also add it to the RF fifo.
  List_put(&(((txDataQ_t *)pDataEntryQ)->llDataBuffers), pDataEntry);
  HAL_EXIT_CRITICAL_SECTION(cs);

  return;
}


/*
** RF Hardware Abstraction Layer Application Programming Interface
*/

/*******************************************************************************
 * @fn          llClearRxDataEntry API
 *
 * @brief       This function is used to clear the RX data entry so that the radio
 *              can once again use it. It should be called after the user has processed the
 *              data entry.
 *
 * input parameters
 *
 * @param       pDataEntryQ - Pointer to data entry queue.
 * @param       pBuffers - Pointer to multi buffers that were finished.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llClearRxDataEntry( void *pDataEntryQ, List_List *pBuffers )
{
  RCL_MultiBuffer *multiBuffer;
  halIntState_t cs;

  HAL_ENTER_CRITICAL_SECTION(cs);
  while ((multiBuffer = RCL_MultiBuffer_get(pBuffers)) != NULL)
  {
    RCL_MultiBuffer_clear(multiBuffer);
    RCL_MultiBuffer_put(pDataEntryQ, multiBuffer);
  }

  // Prepare list of RX buffers that are done
  List_clearList(pBuffers);
  HAL_EXIT_CRITICAL_SECTION(cs);
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * @fn          llClearScanDataQueue API
 *
 * @brief       This function is used to clear all the scan data entries so that the radio
 *              can once again use it. It should be called after the scan command finished
 *
 * input parameters
 *
 * @param       clearAll - TRUE or FALSE in case of clear only finished buffer.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llClearScanDataQueue( uint8 clearAll )
{
  RCL_MultiBuffer *multiBuffer;
  // set the Scan receive buffers

  for (uint8 i=0; i<NUM_RX_SCAN_ENTRIES; i++)
  {
    multiBuffer = (RCL_MultiBuffer *)&scanDataEntry[i];

    if ((clearAll == TRUE) || (multiBuffer->state == RCL_BufferStateFinished))
    {
      RCL_MultiBuffer_clear(multiBuffer);
    }
  }
}
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
#ifdef USE_PERIODIC_SCAN
/*******************************************************************************
 * @fn          llClearPeriodicScanDataQueue API
 *
 * @brief       This function is used to clear all the periodic scan data entries so that the radio
 *              can once again use it. It should be called after the scan command finished
 *
 * input parameters
 *
 * @param       clearAll - TRUE or FALSE in case of clear only finished buffer.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llClearPeriodicScanDataQueue( uint8 clearAll )
{
    llClearScanDataQueue(clearAll);
}
#endif // USE_PERIODIC_SCAN
#endif

/*******************************************************************************
 */
