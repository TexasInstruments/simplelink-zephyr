/******************************************************************************

 @file  npi_task_osprey.c

 @brief NPI is an Application Thread that provides a common
        Network Processor Interface framework.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2015 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/


// ****************************************************************************
// includes
// ****************************************************************************
#include "npi_task.h"
#include "npi_ble.h"
#include "npi_frame.h"
#include "hci.h"
#include "hci_tl.h"
#include "osi.h"
#include "uart.h"
#include "sdio.h"
#include "npi_export_osprey.h"
#include "ble_autonomous_hif.h"

// ****************************************************************************
// typedefs
// ****************************************************************************
typedef struct
{
    OsiMsgQ_t*          pMessageQueue;
    uint8*              pMessageQueueDB;
    OsiThread_t         threadTcb;
    OsiSyncObj_t        syncObj;
    EventGroupObj_t     eventGrpObj;
} NPI_ThreadParams_t;

typedef struct
{
    hciPacket_t *pNpiHciPkt;
} npiRxQueueRec;

typedef struct
{
    npiPkt_t *pNpiPkt;
} npiTxQueueRec;

typedef struct
{
    npiIncomingEventCBack_t incomingRXEventAppCBFunc;
    NPIFrameHCI_Params_t *pCurrentFrameHciRaw;
    uint8 *pLastQueuedRxMsg;
    npiPkt_t *pLastQueuedTxMsg;
}NPI_ControlBlock_t;

typedef struct
{
    uint32 rxqTotalDiscarded;
    uint32 rxqMaxCount;
    uint32 txqTotalDiscarded;
    uint32 txqMaxCount;
} NPI_Stats_t;

typedef enum
{
    NPI_TASK_RX_PACKET_RECEIVED_EVENT         = BIT_0,
    NPI_TASK_RX_PACKET_UART_WAKEUP_EVENT      = BIT_1,
    NPI_TASK_RX_SDIO_PACKET_START_EVENT       = BIT_2
} NPI_Task_RX_Events_e;

typedef enum
{
    NPI_TASK_TX_PACKET_READY_EVENT            = BIT_0,
    NPI_TASK_TX_PACKET_SDIO_ACK_EVENT         = BIT_1
} NPI_Task_TX_Events_e;

// ****************************************************************************
// Defines
// ****************************************************************************
#define NPI_MSGQ_RX_MSG_SIZE               (sizeof(npiRxQueueRec))   // size of messages
#define NPI_MSGQ_RX_MAX_NUM_MSGS           (20)                      // maximal number of NPI RX messages the queue can hold

#define NPI_MSGQ_TX_MSG_SIZE               (sizeof(npiTxQueueRec))   // size of messages
#define NPI_MSGQ_TX_MAX_NUM_MSGS           (20)                      // maximal number of NPI TX messages the queue can hold

#define NPI_TASK_RX_PACKET_EVENTS          (NPI_TASK_RX_PACKET_RECEIVED_EVENT | NPI_TASK_RX_PACKET_UART_WAKEUP_EVENT | NPI_TASK_RX_SDIO_PACKET_START_EVENT)
#define NPI_TASK_TX_PACKET_EVENTS          (NPI_TASK_TX_PACKET_READY_EVENT | NPI_TASK_TX_PACKET_SDIO_ACK_EVENT)


//*****************************************************************************
// internal variables
//*****************************************************************************
NPI_ThreadParams_t      NPI_RxPacketThreadParams;
NPI_ThreadParams_t      NPI_RxQueueThreadParams;
NPI_ThreadParams_t      NPI_TxThreadParams;

NPI_ControlBlock_t      NPI_CB;
NPI_Stats_t             NPI_Stats;


//*****************************************************************************
// globals
//*****************************************************************************
UINT8   bleNpiRxPacketThreadStack    [ BLE_NPI_RX_PACKET_TASK_STACK_SIZE ];
UINT8   bleNpiRxQueueThreadStack     [ BLE_NPI_RX_QUEUE_TASK_STACK_SIZE  ];
UINT8   bleNpiTxThreadStack          [ BLE_NPI_TX_TASK_STACK_SIZE        ];

// Entity ID globally used to check for source and/or destination of messages
static ICall_EntityID   rx_q_entity;

// Event globally used to post local events and pend on system and local events
static ICall_SyncHandle rx_q_syncEvent;


//*****************************************************************************
// function prototypes
//*****************************************************************************
static void NPITask_initTransport(void);

static void NPITaskRXPacket_entry(void* pParam);
static void NPITaskRXPacket_initializeTask(void);
static void NPITaskRXPacket_process(void);
static void NPITaskRXPacket_incomingFrameCB(NPIFrameHCI_Params_t *pFrame);
hciPacket_t* NPITaskRXPacket_BuildHCIframe(NPIFrameHCI_Params_t *pFrame);

static void NPITaskRXQueue_entry(void* pParam);
static void NPITaskRXQueue_initializeTask(void);
static void NPITaskRXQueue_process(void);

static void NPITaskTX_entry(void* pParam);
static void NPITaskTX_initializeTask(void);
static void NPITaskTX_process(void);
static void NPITaskTX_UartWriteIsrCB(void);


// -----------------------------------------------------------------------------
//! \brief      NPI Task SDIO HCI ACK ISR CB
//! \context    SDIO IRQ
//! \return     void
// -----------------------------------------------------------------------------
static void NPITask_SdioHciACKIsrCB(void)
{
    //Signal NPI TX process that ACK received
    osi_EventGroupSet(&NPI_TxThreadParams.eventGrpObj, NPI_TASK_TX_PACKET_SDIO_ACK_EVENT);

    //Signal after ACK received
    osi_SyncObjSignal(&NPI_TxThreadParams.syncObj);
}

// -----------------------------------------------------------------------------
//! \brief      NPI Task SDIO HCI NACK ISR CB
//! \context    SDIO IRQ
//! \return     void
// -----------------------------------------------------------------------------
static void NPITask_SdioHciNACKIsrCB(void)
{
    //Signal NPI TX process that NACK received
    osi_EventGroupSet(&NPI_TxThreadParams.eventGrpObj, NPI_TASK_TX_PACKET_SDIO_ACK_EVENT);

    //Signal after NACK received
    osi_SyncObjSignal(&NPI_TxThreadParams.syncObj);
}

// -----------------------------------------------------------------------------
//! \brief      NPI Task Init the Transport
//! \context    BLE Main Thread
//! \return     void
// -----------------------------------------------------------------------------
static void NPITask_initTransport(void)
{
    if (ReadFn1_En())
    {
        GTRACE(GRP_BLE_MAIN, "Enable SDIO BLE transport");
        Sdio_SetCallbacks(NPITask_SdioHciACKIsrCB, NPITask_SdioHciNACKIsrCB);
        Sdio_HCIInit();
        Config_CONDMA_BLE_SDIO_nUART(1);
        Sdio_SetTransportMode(TRUE);
    }
    else
    {
        GTRACE(GRP_BLE_MAIN, "Enable UART BLE transport");
        Uart_HCIInit();
        Sdio_SetTransportMode(FALSE);
    }
}

// -----------------------------------------------------------------------------
//! \brief      NPI RX Packet incoming frame CB
//! \context    UART IRQ/DMA IRQ (depends on packet data length)
//! \return     void
// -----------------------------------------------------------------------------
static void NPITaskRXPacket_incomingFrameCB(NPIFrameHCI_Params_t *pFrame)
{
    OsiReturnVal_e  rc;

    GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG,"NPITaskRXPacket_incomingFrameCB: Enter");

    //Signal RX Completed event to autonomous UART
    autohif_RxCompleted_Event();

    //Pickup the current RX HCI RAW packet pointer
    NPI_CB.pCurrentFrameHciRaw = pFrame;

    //Signal NPI RX process that a raw HCI packet is ready
    rc = osi_EventGroupSet(&NPI_RxPacketThreadParams.eventGrpObj, NPI_TASK_RX_PACKET_RECEIVED_EVENT);
    if (OSI_OK != rc)
    {
        GTRACE(GRP_GENERAL_ERROR, "ERROR: osi_EventGroupSet failed, rc = %d", rc);
        ASSERT_GENERAL(0);
    }
}

// -----------------------------------------------------------------------------
//! \brief      NPI RX Packet Build HCI frame
//! \context    RX Packet Queue Thread
//! \return     hciPacket_t
// -----------------------------------------------------------------------------
//--------------------------------------------------
// NPI RX Packet Structure (pFrame)
//--------------------------------------------------
// malloc head is here with all the data
// |
// |
// V
// ----------------------------------------------------------------------------------------------------
//! HCI command packet format:
//!      hdr       |                     pData                               |
//! event | status | Packet Type | Command opcode | length | command payload |
//!  1B   |   1B   |     1B      |      2         |   1    |      n          |
//!
//!
//! HCI data packet format:
//!      hdr       | pktType | connHandle | pbFalg  | pktLen |                  pData                 |
//! event | status |         |            |         |        |                                        |
//!  1B   |   1B   |   1B    |     2B     |   1B    |   2B   |                    n                   |
//!                                                          | pktType | connHandle | pktLen | length |
//!                                                          |   1B    |     2B     |   2B   |   n    |
// ----------------------------------------------------------------------------------------------------
hciPacket_t* NPITaskRXPacket_BuildHCIframe(NPIFrameHCI_Params_t *pFrame)
{
    hciPacket_t     *pCmdMsg = NULL;
    hciDataPacket_t *pDataMsg = NULL;
    uint8           rxqMsgCount = osi_MsgQCount(NPI_RxQueueThreadParams.pMessageQueue);
    uint32          dataLen = 0;

    GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG,"NPITaskRXPacket_BuildHCIframe: Enter. Current RX massages count %d",rxqMsgCount);

    switch (pFrame->PKT_Token)
    {
        case HCI_CMD_PACKET:
        case HCI_EXTENDED_CMD_PACKET:
            /* Try to allocate memory for the command massage */
            if (rxqMsgCount < NPI_MSGQ_RX_MAX_NUM_MSGS)
            {
                pCmdMsg = (hciPacket_t *)ICall_allocMsg(sizeof(hciPacket_t));
            }

            if (pCmdMsg)
            {
                // Try to allocate memory for the pData
                dataLen = pFrame->HeaderLength + pFrame->LEN_Token;
                pCmdMsg->pData = ICall_mallocLimited(dataLen);

                if(!pCmdMsg->pData)
                {
                    GTRACE(GRP_BLE_ERROR,"NPITaskRXPacket_BuildHCIframe: Failed to allocate pData with size %d", dataLen);
                    ICall_freeMsg(pCmdMsg);
                    pCmdMsg = NULL;
                }
                else
                {
                    // Set pData to the first byte after the hciPacket bytes
                    os_memcpy(&pCmdMsg->pData[0], &pFrame->RxBuffer[0], (pFrame->HeaderLength + pFrame->LEN_Token));
                }

                // set header specific fields
                pCmdMsg->hdr.status = 0xFF;

                // check if a Controller Link Layer VS command
                if (((pFrame->OPCODE_Token >> 10) == VENDOR_SPECIFIC_OGF) &&
                    (((pFrame->OPCODE_Token >> 7) & 0x07) != HCI_OPCODE_CSG_LINK_LAYER))
                {
                    GTRACE(GRP_BLE_DBG,"NPITaskRXPacket_BuildHCIframe: Received Controller Link Layer VS command");

                    // this is a vendor specific command
                    pCmdMsg->hdr.event = HCI_EXT_CMD_EVENT;

                    // so strip the OGF (i.e. the most significant 6
                                            // bits of the opcode)
                    pCmdMsg->pData[2] &= 0x03;
                }
                else // specification specific command
                {
                    // this is a normal host-to-controller event
                    pCmdMsg->hdr.event = HCI_HOST_TO_CTRL_CMD_EVENT;
                }
            }
            break;

        case HCI_ACL_DATA_PACKET:
            /* Try to allocate memory for the data massage */
            if (rxqMsgCount < NPI_MSGQ_RX_MAX_NUM_MSGS)
            {
                pDataMsg = (hciDataPacket_t *)ICall_allocMsg(sizeof(hciDataPacket_t));
            }

            if (pDataMsg)
            {
                pDataMsg->hdr.event = HCI_HOST_TO_CTRL_DATA_EVENT;
                pDataMsg->hdr.status = 0xFF;
                pDataMsg->pktType = pFrame->PKT_Token;
                // mask out PB and BC Flags
                pDataMsg->connHandle = pFrame->HANDLE_Token & 0x0FFF;
                //isolate PB Flag
                pDataMsg->pbFlag = (pFrame->HANDLE_Token & 0x3000) >> 12;
                pDataMsg->pktLen = pFrame->LEN_Token;

                // Try to allocate memory for the pData (only in case LEN_Token > 0)
                if (pFrame->LEN_Token > 0)
                {
                    dataLen = pFrame->LEN_Token;
                    pDataMsg->pData = ICall_mallocLimited(dataLen);

                    if(!pDataMsg->pData)
                    {
                        GTRACE(GRP_BLE_ERROR,"NPITaskRXPacket_BuildHCIframe: Failed to allocate pData with size %d", dataLen);
                        ICall_freeMsg(pDataMsg);
                        pDataMsg = NULL;
                    }
                    else
                    {
                        // Copy the payload portion to pData (without the header)
                        os_memcpy(&pDataMsg->pData[0], &pFrame->RxBuffer[pFrame->HeaderLength], pFrame->LEN_Token);
                    }
                }
                else
                {
                    // No need to allocate in case LEN_Token is 0
                    pDataMsg->pData = NULL;
                }
            }
            break;
        default:
            GTRACE(GRP_BLE_ERROR, "Received unsupported packet type: %d", pFrame->PKT_Token);
            ASSERT_GENERAL(0);
    }

    if(pCmdMsg)
    {
        return (hciPacket_t*)pCmdMsg;
    }
    else if(pDataMsg)
    {
        return (hciPacket_t*)pDataMsg;
    }
    else
    {
        return NULL;
    }
}

// -----------------------------------------------------------------------------
//! \brief      NPI TX UART Write CB
//! \context    DMA IRQ
//! \return     void
// -----------------------------------------------------------------------------
static void NPITaskTX_UartSdioWriteIsrCB(void)
{
    // this function is common to UART & SDIO
    GTRACE(GRP_BLE_NPI_TX_DBG, "NPITaskTX_UartSdioWriteIsrCB: free lastTxQMsg=0x%8.8x, pktLen=%d",
           (uint32)(NPI_CB.pLastQueuedTxMsg),
           NPI_CB.pLastQueuedTxMsg->pktLen);

    //Free last Queued TX massage
    ICall_freeMsg(NPI_CB.pLastQueuedTxMsg);

    //Initialize the last Queued TX massage
    NPI_CB.pLastQueuedTxMsg = NULL;

    //Unlock the UART Write for future calls
    if (Sdio_IsTrasportEnabled() == FALSE)
    {
        osi_SyncObjSignal(&NPI_TxThreadParams.syncObj);
    }
}

// -----------------------------------------------------------------------------
//! \brief      Initialization for the NPI Thread
//! \context    Create RX Packet Thread
//! \return     void
// -----------------------------------------------------------------------------
static void NPITaskRXPacket_initializeTask(void)
{
    OsiReturnVal_e  rc;

    // Disable the UART/SDIO threshold irq
    if (Sdio_IsTrasportEnabled() == TRUE)
    {
        Sdio_SetThreshold(0, NULL);
    }
    else
    {
        Uart_SetThreshold(0, NULL);
    }

    //Create NPI RX Event group Object
    rc = osi_EventGroupCreate(&NPI_RxPacketThreadParams.eventGrpObj, "npiTaskRxPacketEvents");
    if (rc != OSI_OK)
    {
        GTRACE_NVIC(GRP_BLE_ERROR, "ERROR: fail to create RX event group object (rc = %d)", rc);
        ASSERT_GENERAL(0);
    }
    rc = osi_EventGroupClear(&NPI_RxPacketThreadParams.eventGrpObj, 0xFFFFFFFF);
    if (rc != OSI_OK)
    {
        GTRACE_NVIC(GRP_BLE_ERROR, "ERROR: fail to clear RX Event Group Task Events (rc = %d)", rc);
        ASSERT_GENERAL(0);
    }

    //Initialize Frame and reset HCI packet for the first run
    NPIFrame_initialize(&NPITaskRXPacket_incomingFrameCB);
}

// -----------------------------------------------------------------------------
//! \brief      Initialization for the NPI Thread
//! \context    Create RX Queue Thread
//! \return     void
// -----------------------------------------------------------------------------
static void NPITaskRXQueue_initializeTask(void)
{
    OsiReturnVal_e  rc;

    /* Enroll the service that this stack represents */
    ICall_enrollService ( ICALL_SERVICE_CLASS_NPI, NULL, &rx_q_entity,
                          &rx_q_syncEvent );

    //allocated the NPI RX message queue
    NPI_RxQueueThreadParams.pMessageQueue = mem_Malloc(sizeof(OsiMsgQ_t));

    //allocated the NPI RX message queue database
    NPI_RxQueueThreadParams.pMessageQueueDB = mem_Malloc(NPI_MSGQ_RX_MSG_SIZE * NPI_MSGQ_RX_MAX_NUM_MSGS);

    //Initialize the last Queued RX massage
    NPI_CB.pLastQueuedRxMsg = NULL;

    // Create NPI RX message queue
    rc = osi_MsgQCreate(NPI_RxQueueThreadParams.pMessageQueue,
                       "NPI_RX_MSGQ",
                       NPI_MSGQ_RX_MSG_SIZE,
                       NPI_MSGQ_RX_MAX_NUM_MSGS,
                       NPI_RxQueueThreadParams.pMessageQueueDB);
    if (OSI_OK != rc)
    {
        GTRACE_NVIC(GRP_BLE_ERROR, "ERROR: Command queue: osi_MsgQCreate failed, rc = %d, line = %d",
                rc,__LINE__);
        ASSERT_GENERAL(0);
    }
}


// -----------------------------------------------------------------------------
//! \brief      Initialization for the NPI Thread
//! \context    Create TX Thread
//! \return     void
// -----------------------------------------------------------------------------
static void NPITaskTX_initializeTask(void)
{
    OsiReturnVal_e  rc;

    //allocated the NPI TX message queue
    NPI_TxThreadParams.pMessageQueue = mem_Malloc(sizeof(OsiMsgQ_t));

    //allocated the NPI TX message queue database
    NPI_TxThreadParams.pMessageQueueDB = mem_Malloc(NPI_MSGQ_TX_MSG_SIZE * NPI_MSGQ_TX_MAX_NUM_MSGS);

    //Initialize the last Queued TX massage
    NPI_CB.pLastQueuedTxMsg = NULL;

    //Create NPI TX message queue
    rc = osi_MsgQCreate(NPI_TxThreadParams.pMessageQueue,
                       "NPI_TX_MSGQ",
                       NPI_MSGQ_TX_MSG_SIZE,
                       NPI_MSGQ_TX_MAX_NUM_MSGS,
                       NPI_TxThreadParams.pMessageQueueDB);
    if (OSI_OK != rc)
    {
        GTRACE_NVIC(GRP_BLE_ERROR, "ERROR: Command queue: osi_MsgQCreate failed, rc = %d, line = %d",
                rc,__LINE__);
        ASSERT_GENERAL(0);
    }

    //Create NPI TX Sync Object
    rc = osi_SyncObjCreate(&NPI_TxThreadParams.syncObj);
    if (OSI_OK != rc)
    {
        GTRACE_NVIC(GRP_BLE_ERROR, "ERROR: NpiTxsemObj create: osi_semObjCreate failed, rc = %d, line = %d",
                rc,__LINE__);
        ASSERT_GENERAL(0);
    }

    //Create NPI TX Event group Object
    rc = osi_EventGroupCreate(&NPI_TxThreadParams.eventGrpObj, "npiTaskTxPacketEvents");
    if (rc != OSI_OK)
    {
        GTRACE_NVIC(GRP_BLE_ERROR, "ERROR: fail to create TX event group object (rc = %d)", rc);
        ASSERT_GENERAL(0);
    }
    rc = osi_EventGroupClear(&NPI_TxThreadParams.eventGrpObj, 0xFFFFFFFF);
    if (rc != OSI_OK)
    {
        GTRACE_NVIC(GRP_BLE_ERROR, "ERROR: fail to clear TX Event Group Task Events (rc = %d)", rc);
        ASSERT_GENERAL(0);
    }
}

// -----------------------------------------------------------------------------
//! \brief      NPI main event RX processing loop.
//! \context    NPI RX Packet Thread
//! \return     void
// -----------------------------------------------------------------------------
static void NPITaskRXPacket_process(void)
{
    OsiReturnVal_e      rc;
    npiRxQueueRec       npiRxQRec;
    UINT32              eventReceived = 0;
    Bool_e              keepRunning = TRUE;

    /* Forever loop */
    while (keepRunning)
    {
        //Wait on event group object
        rc = osi_EventGroupWait(&NPI_RxPacketThreadParams.eventGrpObj,
                                NPI_TASK_RX_PACKET_EVENTS,
                                &eventReceived,
                                OSI_WAIT_FOREVER);

        ASSERT_GENERAL(rc == OSI_OK);

        if (eventReceived & NPI_TASK_RX_PACKET_RECEIVED_EVENT)
        {
            //Build the frame as the application expects it
            npiRxQRec.pNpiHciPkt = NPITaskRXPacket_BuildHCIframe(NPI_CB.pCurrentFrameHciRaw);

            //Check if frame was allocated successfully
            if (npiRxQRec.pNpiHciPkt != NULL)
            {
                //Add message to NPI RX Queue
                rc = osi_MsgQWrite(NPI_RxQueueThreadParams.pMessageQueue, &npiRxQRec, OSI_NO_WAIT);

                if (OSI_OK != rc)
                {
                    GTRACE_NVIC(GRP_BLE_ERROR,"NPITaskRXPacket_process ERROR: failed to add message to NPI RX Queue");
                    ASSERT_GENERAL(0);
                }

                if (osi_MsgQCount(NPI_RxQueueThreadParams.pMessageQueue) > NPI_Stats.rxqMaxCount)
                {
                    NPI_Stats.rxqMaxCount++;
                }

                //Signal RX transaction was completed
                if (Sdio_IsTrasportEnabled() == FALSE)
                {
                    autohif_uartRxPacketReceivedHandler();
                }
                else
                {
                    autohif_sdioRxPacketReceivedHandler();
                }
            }
            else
            {
                NPI_Stats.rxqTotalDiscarded++;
                GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG,"NPITaskRXPacket_process: Failed to add or allocate packet. Total RX discarded massage count %d",
                            NPI_Stats.rxqTotalDiscarded);
            }

            //Initialize Frame and reset HCI packet for the next run
            NPIFrame_initialize(&NPITaskRXPacket_incomingFrameCB);
        }

        if (eventReceived & NPI_TASK_RX_PACKET_UART_WAKEUP_EVENT)
        {
            autohif_uartRxPacketWakeupHandler();
        }

        if (eventReceived & NPI_TASK_RX_SDIO_PACKET_START_EVENT)
        {
            autohif_sdioRxPacketStartHandler();
        }
    }
}


// -----------------------------------------------------------------------------
//! \brief      NPI main event RX Queue processing loop.
//! \context    NPI RX Queue Thread
//! \return     void
// -----------------------------------------------------------------------------
static void NPITaskRXQueue_process(void)
{
    OsiReturnVal_e          osi_rc;
    npiRxQueueRec           rxQRec;
    Bool_e                  keepRunning = TRUE;

    /* Forever loop */
    while (keepRunning)
    {
        //Pull massage and process it
        osi_rc = osi_MsgQRead(NPI_RxQueueThreadParams.pMessageQueue, &rxQRec, OSI_WAIT_FOREVER);

        if(OSI_OK == osi_rc)
        {
            GTRACE(GRP_BLE_NPI_RX_Q_DBG,"NPITaskRXQueue_process: massage read from queue successfully");
            if (NPI_CB.incomingRXEventAppCBFunc != NULL)
            {
                //Store the last RX massage pData pointer (it might be adjusted later on in the stack)
                if (rxQRec.pNpiHciPkt->hdr.event == HCI_HOST_TO_CTRL_DATA_EVENT)
                {
                    NPI_CB.pLastQueuedRxMsg = ((hciDataPacket_t *)(rxQRec.pNpiHciPkt))->pData;

                    GTRACE(GRP_BLE_AUTO_HIF_DBG,"NPITaskRXQueue_process: DATA Packet Header: 0x%x 0x%x 0x%x",
                           ((hciDataPacket_t *)(rxQRec.pNpiHciPkt))->pktType,
                           ((hciDataPacket_t *)(rxQRec.pNpiHciPkt))->connHandle,
                           ((hciDataPacket_t *)(rxQRec.pNpiHciPkt))->pktLen);
                }
                else
                {
                    NPI_CB.pLastQueuedRxMsg = rxQRec.pNpiHciPkt->pData;

                    GTRACE(GRP_BLE_AUTO_HIF_DBG,"NPITaskRXQueue_process: CMD Packet Header: 0x%x 0x%x 0x%x 0x%x",
                           rxQRec.pNpiHciPkt->pData[0],
                           rxQRec.pNpiHciPkt->pData[1],
                           rxQRec.pNpiHciPkt->pData[2],
                           rxQRec.pNpiHciPkt->pData[3]);
                }

                //Trigger the application callback
                NPI_CB.incomingRXEventAppCBFunc((uint8_t *)rxQRec.pNpiHciPkt);

                //Free the allocated packet and its pData
                ICall_free(NPI_CB.pLastQueuedRxMsg);
                ICall_freeMsg(rxQRec.pNpiHciPkt);

                //Initialize the last Queued RX massage
                NPI_CB.pLastQueuedRxMsg = NULL;
            }
            else
            {
                GTRACE(GRP_BLE_ERROR,"NPITaskRXQueue_process: No callback was set");
                ASSERT_GENERAL(0);
            }
        }
        else
        {
            GTRACE(GRP_BLE_ERROR,"NPITaskRXQueue_process: osi_MsgQRead failed, osi_rc = %d", osi_rc);
            ASSERT_GENERAL(0);
        }
    }
}


// -----------------------------------------------------------------------------
//! \brief      NPI main event TX processing loop.
//! \context    NPI TX Thread
//! \return     void
// -----------------------------------------------------------------------------
static void NPITaskTX_process(void)
{
    OsiReturnVal_e          osi_rc;
    OsiReturnVal_e          queue_rc;
    int8_t                  uart_rc;
    int8_t                  sdio_rc;
    npiTxQueueRec           txQRec;
    UINT32                  eventReceived = 0;
    Bool_e                  keepRunning = TRUE;

    /* Forever loop */
    while (keepRunning)
    {
        //Wait on event group object
        osi_rc = osi_EventGroupWait(&NPI_TxThreadParams.eventGrpObj,
                                    NPI_TASK_TX_PACKET_EVENTS,
                                    &eventReceived,
                                    OSI_WAIT_FOREVER);

        ASSERT_GENERAL(osi_rc == OSI_OK);

        if (eventReceived & NPI_TASK_TX_PACKET_READY_EVENT)
        {
            while (TRUE)
            {
                //Pull all pending massages and process
                queue_rc = osi_MsgQRead(NPI_TxThreadParams.pMessageQueue, &txQRec, OSI_NO_WAIT);

                if (OSI_TIMEOUT == queue_rc)
                {
                    //Finished reading all pending packets. Queue is empty.
                    break;
                }

                ASSERT_GENERAL(OSI_OK == queue_rc);

                //Save last Queued Tx Msg in order to free it
                NPI_CB.pLastQueuedTxMsg = (npiPkt_t *)txQRec.pNpiPkt;
                GTRACE(GRP_BLE_NPI_TX_DBG,"pNpiPkt=0x%8.8x, lastTxQMsg=0x%8.8x, pktLen=%d",
                        (uint32)txQRec.pNpiPkt     ,
                        (uint32)(NPI_CB.pLastQueuedTxMsg) ,
                        NPI_CB.pLastQueuedTxMsg->pktLen   );

                if (Sdio_IsTrasportEnabled() == TRUE)
                {
                    autohif_sdioTxTransactionStartHandler();
                    sdio_rc = Sdio_WriteBytesDMA(NPI_CB.pLastQueuedTxMsg->pData, NPI_CB.pLastQueuedTxMsg->pktLen, NPITaskTX_UartSdioWriteIsrCB);

                    if (sdio_rc != SDIO_HAL_SUCCESS)
                    {
                        GTRACE(GRP_BLE_ERROR,"NPITaskTX_process: Sdio_WriteBytesDMA failed, sdio_rc=%d, msg=0x%8.8x Data(bytes 0-3) %8.8x, pktLen=%d",
                                uart_rc                                ,
                                (uint32)NPI_CB.pLastQueuedTxMsg        ,
                                (uint32)NPI_CB.pLastQueuedTxMsg->pData ,
                                NPI_CB.pLastQueuedTxMsg->pktLen        );

                        ASSERT_GENERAL(0);
                    }
                }
                else // UART Enabled
                {
                    uart_rc = Uart_WriteBytesDMA(NPI_CB.pLastQueuedTxMsg->pData, NPI_CB.pLastQueuedTxMsg->pktLen, NPITaskTX_UartSdioWriteIsrCB);

                    if (uart_rc != UART_HAL_SUCCESS)
                    {
                        GTRACE(GRP_BLE_ERROR,"NPITaskTX_process: Uart_WriteBytesDMA failed, uart_rc=%d, msg=0x%8.8x Data(bytes 0-3) %8.8x, pktLen=%d",
                                uart_rc                                ,
                                (uint32)NPI_CB.pLastQueuedTxMsg        ,
                                (uint32)NPI_CB.pLastQueuedTxMsg->pData ,
                                NPI_CB.pLastQueuedTxMsg->pktLen        );

                        ASSERT_GENERAL(0);
                    }

                    // Check if CTS is Off (It means Host might have entered into sleep)
                    if (Uart_IsCtsOn() == FALSE)
                    {
                        // Protect from being preempted by a Uart_ToggleRts which can also run from IRQ context
                        DISABLE_MED_PRIORITY_INTERRUPTS_NO_RT
                        // Wakeup Host by toggling the RTS
                        Uart_ToggleRts();
                        ENABLE_MED_PRIORITY_INTERRUPTS_NO_RT
                    }
                }            

                osi_rc = osi_SyncObjWait(&NPI_TxThreadParams.syncObj, OSI_WAIT_FOREVER);
                ASSERT_GENERAL(OSI_OK == osi_rc);
            }
        }

        if (eventReceived & NPI_TASK_TX_PACKET_SDIO_ACK_EVENT)
        {
            autohif_sdioTxTransactionCompletedHandler();
        }
    }
}

// -----------------------------------------------------------------------------
//! \brief      NPI RX Task entry function
//! \context    Create RX Packet Thread
//! \return     void
// -----------------------------------------------------------------------------
static void NPITaskRXPacket_entry(void* pParam)
{
    GTRACE(GRP_BLE_NPI_RX_PKT_DBG,"NPITaskRXPacket_entry: Enter");

    // Initialize application
    NPITaskRXPacket_initializeTask();

    // No return from process
    NPITaskRXPacket_process();
}


// -----------------------------------------------------------------------------
//! \brief      NPI RX Queue Task entry function
//! \context    Create RX Queue Thread
//! \return     void
// -----------------------------------------------------------------------------
static void NPITaskRXQueue_entry(void* pParam)
{
    GTRACE(GRP_BLE_NPI_RX_Q_DBG,"NPITaskRXQueue_entry: Enter");

    // Initialize application
    NPITaskRXQueue_initializeTask();

    // No return from process
    NPITaskRXQueue_process();
}


// -----------------------------------------------------------------------------
//! \brief      NPI TX Task entry function
//! \context    Create TX Thread
//! \return     void
// -----------------------------------------------------------------------------
static void NPITaskTX_entry(void* pParam)
{
    GTRACE(GRP_BLE_NPI_TX_DBG,"NPITaskTX_entry: Enter");

    // Initialize application
    NPITaskTX_initializeTask();

    // No return from process
    NPITaskTX_process();
}


// -----------------------------------------------------------------------------
// Exported Functions


// -----------------------------------------------------------------------------
//! \brief      Task creation function for NPI
//! \context    Create NPI Threads
//! \return     void
// -----------------------------------------------------------------------------
void NPITask_createTask(uint32_t stackID)
{
    OsiReturnVal_e rc;

    GTRACE_NVIC(GRP_BLE_DBG, "BLE NPI task init");

    //Init the transport layer
    NPITask_initTransport();

    //Init Control Block and Statistics
    os_memset(&NPI_CB,0,sizeof(NPI_CB));
    os_memset(&NPI_Stats,0,sizeof(NPI_Stats));

    //Create The NPI RX thread
    rc = osi_ThreadCreate(
            &NPI_RxPacketThreadParams.threadTcb   ,  //  OsiThread_t*                    pThread,
            BLE_NPI_RX_PACKET_TASK_THR_IDX        ,  //  UINT                            thread_idx,
            _BLE_NPI_RX_PACKET_TASK_THR_NAME      ,  //  char*                           pThreadName,
            BLE_NPI_RX_PACKET_TASK_STACK_SIZE     ,  //  UINT32                          StackSize,
            BLE_NPI_RX_PACKET_TASK_PRIORITY       ,  //  UINT32                          Priority,
            NPITaskRXPacket_entry                 ,  //  P_THREAD_ENTRY_FUNCTION         pEntryFunc,
            NULL                                  ,  //  void*                           pParam,
            bleNpiRxPacketThreadStack             ,  //  void*                           pStack,
            TRUE                                  ,  //  BOOLEAN                         AutoStart,
            TRUE                                  ); //  BOOLEAN                         ProtectStack


    if (OSI_OK != rc)
    {
        GTRACE(GRP_BLE_ERROR,"BLE NPI RX task: osi_ThreadCreate failed, rc = %d", rc);
        ASSERT_GENERAL(0);
    }

    GTRACE_NVIC(GRP_BLE_NPI_RX_Q_DBG, "BLE NPI RX Queue task init");

    //Create The NPI RX thread
    rc = osi_ThreadCreate(
            &NPI_RxQueueThreadParams.threadTcb    ,  //  OsiThread_t*                    pThread,
            BLE_NPI_RX_QUEUE_TASK_THR_IDX         ,  //  UINT                            thread_idx,
            _BLE_NPI_RX_QUEUE_TASK_THR_NAME       ,  //  char*                           pThreadName,
            BLE_NPI_RX_QUEUE_TASK_STACK_SIZE      ,  //  UINT32                          StackSize,
            BLE_NPI_RX_QUEUE_TASK_PRIORITY        ,  //  UINT32                          Priority,
            NPITaskRXQueue_entry                  ,  //  P_THREAD_ENTRY_FUNCTION         pEntryFunc,
            NULL                                  ,  //  void*                           pParam,
            bleNpiRxQueueThreadStack              ,  //  void*                           pStack,
            TRUE                                  ,  //  BOOLEAN                         AutoStart,
            TRUE                                  ); //  BOOLEAN                         ProtectStack


    if (OSI_OK != rc)
    {
        GTRACE(GRP_BLE_ERROR,"BLE NPI RX Queue task: osi_ThreadCreate failed, rc = %d", rc);
        ASSERT_GENERAL(0);
    }

    GTRACE_NVIC(GRP_BLE_NPI_TX_DBG, "NPI TX task init");

    //Create The NPI TX thread
    rc = osi_ThreadCreate(
            &NPI_TxThreadParams.threadTcb         ,  //  OsiThread_t*                    pThread,
            BLE_NPI_TX_TASK_THR_IDX               ,  //  UINT                            thread_idx,
            _BLE_NPI_TX_TASK_THR_NAME             ,  //  char*                           pThreadName,
            BLE_NPI_TX_TASK_STACK_SIZE            ,  //  UINT32                          StackSize,
            BLE_NPI_TX_TASK_PRIORITY              ,  //  UINT32                          Priority,
            NPITaskTX_entry                       ,  //  P_THREAD_ENTRY_FUNCTION         pEntryFunc,
            NULL                                  ,  //  void*                           pParam,
            bleNpiTxThreadStack                   ,  //  void*                           pStack,
            TRUE                                  ,  //  BOOLEAN                         AutoStart,
            TRUE                                  ); //  BOOLEAN                         ProtectStack


    if (OSI_OK != rc)
    {
        GTRACE(GRP_BLE_ERROR,"BLE NPI TX task: osi_ThreadCreate failed, rc = %d", rc);
        ASSERT_GENERAL(0);
    }
}


// -----------------------------------------------------------------------------
//! \brief      Register callback function to reroute incoming (from host)
//!             NPI messages.
//!
//! \param[in]  appRxCB   Callback function.
//! \param[in]  reRouteType Type of re-routing requested
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPITask_registerIncomingRXEventAppCB(npiIncomingEventCBack_t appRxCB,
                                          NPI_IncomingNPIEventRerouteType reRouteType)
{
    NPI_CB.incomingRXEventAppCBFunc = appRxCB;
}


// -----------------------------------------------------------------------------
//! \brief      API for application task to send a message to the Host.
//!
//! \param[in]  pMsg    Pointer to message buffer.
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPITask_sendToHost(uint8_t *pMsg)
{
    OsiReturnVal_e      rc;
    npiTxQueueRec       npiTxQRec;

    //--------------------------------------------------
    // NPI TX Packet Structure (pMsg)
    //--------------------------------------------------
    // malloc head is here with all the data
    // |
    // |
    // V
    //--------------------------------------------------
    // event | status | pktLen | pData
    // 1B    | 1B     | 2B     | 4B
    //                            ^
    //                            |
    //                            D[0] D[1] D[2]....
    //                            HCI Data ....
    //--------------------------------------------------
    //Store the pointer to the NPI packet and write to queue
    npiTxQRec.pNpiPkt = (npiPkt_t *)pMsg;

    GTRACE(GRP_BLE_NPI_TX_DBG,"NPITask_sendToHost: pMsg=0x%8.8x pNpiPkt=0x%8.8x, pktLen=%d, pData=0x%8.8x",
           (uint32)pMsg              ,
           (uint32)npiTxQRec.pNpiPkt ,
           (uint32)npiTxQRec.pNpiPkt->pktLen,
           *((uint32*)npiTxQRec.pNpiPkt->pData));

    GTRACE(GRP_BLE_AUTO_HIF_DBG,"NPITask_sendToHost: Packet Header: 0x%x 0x%x 0x%x 0x%x",
           npiTxQRec.pNpiPkt->pData[0],
           npiTxQRec.pNpiPkt->pData[1],
           npiTxQRec.pNpiPkt->pData[2],
           npiTxQRec.pNpiPkt->pData[3]);

    rc = osi_MsgQWrite(NPI_TxThreadParams.pMessageQueue, &npiTxQRec, OSI_NO_WAIT);
    if (OSI_OK != rc)
    {
        NPI_Stats.txqTotalDiscarded++;
        GTRACE_NVIC(GRP_BLE_AUTO_HIF_DBG,"NPITask_sendToHost: failed to add message to NPI TX Queue. Total TX discarded massage count %d",
                    NPI_Stats.txqTotalDiscarded);
        ICall_freeMsg(npiTxQRec.pNpiPkt);
    }

    if (osi_MsgQCount(NPI_TxThreadParams.pMessageQueue) > NPI_Stats.txqMaxCount)
    {
        NPI_Stats.txqMaxCount++;
    }

    //Signal NPI TX process that a new HCI packet is ready
    rc = osi_EventGroupSet(&NPI_TxThreadParams.eventGrpObj, NPI_TASK_TX_PACKET_READY_EVENT);
    if (OSI_OK != rc)
    {
        GTRACE_NVIC(GRP_BLE_ERROR,"ERROR: failed to signal event to NPI TX process");
        ASSERT_GENERAL(0);
    }
}

//*****************************************************************************
// exported function
//*****************************************************************************

// -----------------------------------------------------------------------------
//! \brief      NPI Task function that sends one time message in order to host know when he can start send hci packets
//! \context    BLE main thread
//! \return     void
// -----------------------------------------------------------------------------
void NPITask_SendBleReadyMessagetoHost(void)
{
    npiPkt_t     *msg = NULL;
    uint8_t totalLength = sizeof(npiPkt_t) + HCI_EVENT_MIN_LENGTH + 4;
    msg = (npiPkt_t *)ICall_allocMsg(totalLength);
    if(msg)
    {
        GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG, "NPITask_SendBleReadyMessagetoHost totalLength = %d bytes",totalLength);

        //Complete the packet and send it
        // Icall message event, status, and pointer to packet
        msg->hdr.event  = 0xFF;
        msg->hdr.status = 0xFF;

        // fill in length and data pointer
        msg->pktLen = HCI_EVENT_MIN_LENGTH + 4;
        msg->pData  = (uint8*)(msg+1);
        // fill in BLE Complete Event data
        msg->pData[0] = HCI_VE_EVENT_CODE;
        msg->pData[1] = HCI_VE_EVENT_CODE;
        msg->pData[2] = 0x2;

        // We keep all the information the same across report, only the data type will change.
        msg->pData[3]  = HI_UINT16(0x042A);
        msg->pData[4]  = LO_UINT16(0x042A);
        msg->pData[5]  = 0x0; // To be aligned to SDIO
        msg->pData[6]  = 0x0; // To be aligned to SDIO

        NPITask_sendToHost((uint8_t *)msg);
    }
}

// -----------------------------------------------------------------------------
//! \brief      NPI Task RX Packet after wakeup set event
//! \context    UART IRQ
//! \return     void
// -----------------------------------------------------------------------------
void NPITask_UartRxPacketWakeupSetEvent()
{
    OsiReturnVal_e      rc;

    rc = osi_EventGroupSet(&NPI_RxPacketThreadParams.eventGrpObj, NPI_TASK_RX_PACKET_UART_WAKEUP_EVENT);
    if (OSI_OK != rc)
    {
        GTRACE(GRP_GENERAL_ERROR, "ERROR: osi_EventGroupSet failed, rc = %d", rc);
        ASSERT_GENERAL(0);
    }
}

// -----------------------------------------------------------------------------
//! \brief      NPI Task RX Packet for SDIO transaction set event
//! \context    UART IRQ
//! \return     void
// -----------------------------------------------------------------------------
void NPITask_SdioRxPacketNotifySetEvent()
{
    OsiReturnVal_e      rc;

    rc = osi_EventGroupSet(&NPI_RxPacketThreadParams.eventGrpObj, NPI_TASK_RX_SDIO_PACKET_START_EVENT);
    if (OSI_OK != rc)
    {
        GTRACE(GRP_GENERAL_ERROR, "ERROR: osi_EventGroupSet failed, rc = %d", rc);
        ASSERT_GENERAL(0);
    }
}
