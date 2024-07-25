
/******************************************************************************

 @file  ll_cs_ctrl_pkt_internal.h

 @brief This file includes the internal functions used by ll_cs_ctrl_pkt mgr

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
struct csReq_t
{
    uint8   configId:6;
    uint8   rfu:2;
    uint16  connEventCount;
    uint32  offsetMin:24;
    uint32  offsetMax:24;
    uint16  maxProcedureDur;
    uint16  eventInterval;           /* units of connInt */
    uint8   subEventsPerEvent;       /* num of CS SubEvents in a CS Event */
    uint16  subEventInterval;        /* units 625 us*/
    uint32  subEventLen:24;          /* tune ub 1250 ms */
    uint16  procedureInterval;
    uint16  procedureCount;
    csACI_e ACI;
    uint8   preferredPeerAntenna;
    uint8   phy;
    uint8   pwrDelta;
 } __attribute__((packed));
 // Typedef separated from packed struct due to MisraC
 typedef struct csReq_t csReq_t;

struct csRsp_t
{
    uint8   configId:6;
    uint8   rfu:2;
    uint16  connEventCount;
    uint32  offsetMin:24;
    uint32  offsetMax:24;
    uint16  eventInterval;           /* units of connInt */
    uint8   subEventsPerEvent;       /* num of CS SubEvents in a CS Event */
    uint16  subEventInterval;        /* units 625 us*/
    uint32  subEventLen:24;          /* tune ub 1250 ms */
    csACI_e ACI;
    uint8   phy;
    uint8   pwrDelta;
} __attribute__((packed));
// Typedef separated from packed struct due to MisraC
typedef struct csRsp_t csRsp_t;

struct csInd_t
{
    uint8   configId:6;
    uint8   rfu:2;
    uint16  connEventCount;
    uint32  offset:24;
    uint16  eventInterval;     /* units of connInt */
    uint8   subEventsPerEvent; /* num of CS SubEvents in a CS Event */
    uint16  subEventInterval;  /* units 625 us*/
    uint32  subEventLen:24;       /* tune ub 1250 ms */
    csACI_e ACI;
    uint8   phy;
    uint8   pwrDelta;
}__attribute__((packed));
// Typedef separated from packed struct due to MisraC
typedef struct csInd_t csInd_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * @fn          llCsProcessCsCtrlProcedure
 *
 * @brief       Process a CS Control Packet Procedure.
 * It either sets up and transmits the CS control packet, or dequeues it when
 * it's done. This API is called by llProcessMaster/SlaveControlProcedures
 * upon Last Command Done interrupt if a CS Control Packet is queued.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connPtr - connection information pointer
 * @param       ctrlPkt - control packet opcode
 * @param       procedure - procedure type
 * @param       payloadLen - Length of payload
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
uint8 llCsProcessCsCtrlProcedure(llConnState_t* connPtr, uint8 ctrlPkt,
                                 uint8 procedure, uint8 payloadLen);

/*******************************************************************************
 * @fn          llCsNegotiateCsReq
 *
 * @brief       Negotiate CS Request
 * This function checks if the recieved cs req packet complies
 * with the cs configuration and the capabilities.
 * this function may modify csreq when needed.
 * the modified csreq will be sent as a cs_rsp or cs_ind packet
 *
 * input parameters
 *
 * @param       connPtr - connection Poitner
 * @param       csReq - pointer to CS Request
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      csStatus_e
 */
csStatus_e llCsNegotiateCsReq(llConnState_t* connPtr, csProcedureEnable_t* csReq);

/*******************************************************************************
 * @fn          llCsNegotiateCsRsp
 *
 * @brief       Negotiate CS Rsp
 * This function checks if the recieved cs rsp packet complies
 * with the cs configuration and the capabilities.
 * this function may modify csrsp when needed.
 * the modified csRsp will be sent as cs_ind packet
 *
 * input parameters
 *
 * @param       connPtr - connection pointer
 * @param       csRsp - pointer to csRsp
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
csStatus_e llCsNegotiateCsRsp(llConnState_t* connPtr, csProcedureEnable_t* csRsp);

/*******************************************************************************
 * @fn          llCsConfirmInd
 *
 * @brief       The final stage of the CS procedure negotiaion.
 * This function confirms that the CS IND that was received is
 * acceptable by the local device based on the CS_REQ/RSP and
 * the configuration and capabilities.
 *
 * input parameters
 *
 * @param       connPtr - connection pointer
 * @param       csInd - point to cs Ind packet data
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
csStatus_e llCsConfirmInd(llConnState_t* connPtr, csProcedureEnable_t* csInd);

/*******************************************************************************
 * @fn          llCsReq2Data
 *
 * @brief       Convert csReq struct into raw data that will be sent OTA
 *
 * input parameters
 *
 * @param       data - pointer to raw data as it will be sent
 * @param       csReq - pointer to the structurized data
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsReq2Data(uint8* data, csProcedureEnable_t* csReq);

/*******************************************************************************
 * @fn          llCsRsp2Data
 *
 * @brief       Convert csRsp struct into raw data that will be sent OTA
 *
 * input parameters
 *
 * @param       data - pointer to raw data
 * @param       csRsp - pointer to cs Resp
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsRsp2Data(uint8* data, csProcedureEnable_t* csRsp);

/*******************************************************************************
 * @fn          llCsInd2Data
 *
 * @brief       Convert csInd struct into raw data to send OTA
 *
 * input parameters
 *
 * @param       data - pointer to raw data
 * @param       csInd - pointer to struct to convert
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsInd2Data(uint8* data, csProcedureEnable_t* csInd);

/*******************************************************************************
 * @fn          llCsGetSubeventLen
 *
 * @brief       This function calculates the best possible subevent len
 * The calculation is based on MaxSubeventLen, procedure Len, offset and
 * connection interval
 *
 * input parameters
 *
 * @param       maxSubeventLen - max subevent length
 * @param       offsetMin - minimum offset
 * @param       procLen = procedure length
 * @param       connInterval - connection interval
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      subevent len
 */
uint32 llCsGetSubeventLen(uint32 maxSubeventLen, uint32 offsetMin, uint16 procLen,
                      uint16 connInterval);

/*******************************************************************************
 * @fn          llCsSelectACI
 *
 * @brief       This function selecs the ACI configuration based on each
 *              device's capabilities.
 *
 * input parameters
 *
 * @param       configId - CS configuration ID
 * @param       connId - connection ID
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      ACI
 */
csACI_e llCsSelectACI(uint8 configId, uint16 connId);

/*******************************************************************************
 * @fn          llCsParseCsIndData
 *
 * @brief       Parse the data from the LL_CS_IND PKT
 *
 * input parameters
 *
 * @param       csInd - csInd data
 * @param       pBuf - packet data buffer
 *
 * @design      BLE_LOKI-506
 *
 * output parameters
 *
 * @param       csInd
 *
 * @return      None
 */
void llCsParseCsIndData(csProcedureEnable_t* csInd, uint8* pBuf);

/*******************************************************************************
 * @fn          llCsParseCsRspData
 *
 * @brief       Parse the data from the LL_CS_RSP PKT
 *
 * input parameters
 *
 * @param       csRsp - csRsp data
 * @param       pBuf - packet data buffer
 *
 * @design      BLE_LOKI-506
 *
 * output parameters
 *
 * @param       csRsp
 *
 * @return      None
 */
void llCsParseCsRspData(csProcedureEnable_t* csRsp, uint8* pBuf);

/*******************************************************************************
 * @fn          llCsParseCsReqData
 *
 * @brief       Parse the data from the LL_CS_REQ PKT
 *
 * input parameters
 *
 * @param       csReq - csReq data
 * @param       pBuf - packet data buffer
 *
 * @design      BLE_LOKI-506
 *
 * output parameters
 *
 * @param       csReq
 *
 * @return      None
 */
void llCsParseCsReqData(csProcedureEnable_t* csReq, uint8* pBuf);

/*******************************************************************************
 * @fn          llCsSetupCtrlPkt
 *
 * @brief       Build CS Control Packet
 * This function is used to build the different CS control packets
 * for sending over the air. The control packets are build
 * according to the Sepcification. We assume that the caller of
 * this function sends only ctrlType that belong to CS. The packet
 * that was build is added to the Tx Queue for the RF to transmit.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connPtr - connection information pointer
 * @param       ctrlPkt - control packet opcode
 * @param       ctrlLen - length of the packet
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Status
 *              CS_STATUS_INVALID_CONN_PTR - if connPts is NULL
 *              CS_STATUS_CONNECTION_TERMINATED - if connection got terminated
 *              CS_STATUS_INVALID_PKT_LEN - if ctrlLen is invalid
 *              CS_STATUS_SUCCESS - if packet was processed successfully
 * @note it is assumed that this API is used only when ctrlType is a CS ctrl
 * opcode.
 */
uint8 llCsSetupCtrlPkt(llConnState_t* connPtr, uint8 ctrlType, uint8 ctrlLen);

/*******************************************************************************
 * @fn          llCsSetupCapabilities
 *
 * @brief       Build the CS Capabilities packet data
 *
 * input parameters
 *
 * @param       data - data pointer
 *
 * @design      BLE_LOKI-506
 * output parameters
 *
 * @param       data - updated with the CS capabilities
 *
 * @return      None
 */
void llCsSetupCapabilities(uint8* data);

/*******************************************************************************
 * @fn          llCsSetupConfigData
 *
 * @brief       Build the CS Config packet data
 *
 * input parameters
 *
 * @param       data - data pointer
 * @param       connId - connection identifier
 * @param       configId - cs config Id
 *
 * @design      BLE_LOKI-506
 *
 * output parameters
 *
 * @param       data - updated with the CS config packet data
 *
 * @return      status
 */
uint8 llCsSetupConfigData(uint8* data, uint16 connId, uint8 configId);

/*******************************************************************************
 * @fn          llCsSetupSecurityData
 *
 * @brief       Build the CS Security packet data
 *
 * input parameters
 *
 * @param       data - data pointer
 * @param       connId - connection identifier
 * @param       offset - data offset
 *
 * @design      BLE_LOKI-506
 *
 * output parameters
 *
 * @param       data - updated with the CS config packet data
 *
 * @return      None
 */
uint8 llCsSetupSecurityData(uint8* data, uint16 connId, uint8 offset);

/*******************************************************************************
 * @fn          llCsParseSecurityData
 *
 * @brief       Parses security data that is recieved from the ctrl packet
 *              LL_CS_SEC_REQ/RSP and sets it in the ll_cs_db
 *
 * input parameters
 *
 * @param       data - data pointer
 * @param       connId - connection identifier
 * @param       offset - data offset
 *
 * @design      BLE_LOKI-506
 *
 * output parameters
 *
 * @param       data - updated with the CS config packet data
 *
 * @return      None
 */
void llCsParseSecurityData(uint8* data, uint16 connId, uint8 offset);

/*******************************************************************************
 * @fn          llCsSetupFaeTblData
 *
 * @brief       Setup the data for the LL_CS_FAE_RSP PKT
 *
 * input parameters
 *
 * @param       data - data pointer
 *
 * @design      BLE_LOKI-506
 *
 * output parameters
 *
 * @param       data - updated with the FAE table
 *
 * @return      None
 */
void llCsSetupFaeTblData(uint8* data);

/*******************************************************************************
 * @fn          llCsSetupCSReq
 *
 * @brief       Setup the data for the LL_CS_REQ PKT
 *
 * input parameters
 *
 * @param       data - data pointer
 * @param       connId - connection identifier
 *
 * @design      BLE_LOKI-506
 *
 * output parameters
 *
 * @param       data
 *
 * @return      None
 */
void llCsSetupCsReq(uint8* data, uint16 connId);

/*******************************************************************************
 * @fn          llCsSetupCsRsp
 *
 * @brief       Setup the data for the LL_CS_RSP PKT
 *
 * input parameters
 *
 * @param       data - data pointer
 * @param       connId - connection identifier
 *
 * @design      BLE_LOKI-506
 *
 * output parameters
 *
 * @param       data
 *
 * @return      None
 */
void llCsSetupCsRsp(uint8* data, uint16 connId);

/*******************************************************************************
 * @fn          llCsSetupCsInd
 *
 * @brief       Setup the data for the LL_CS_IND PKT
 *
 * input parameters
 *
 * @param       data - data pointer
 * @param       connId - connection identifier
 *
 * @design      BLE_LOKI-506
 *
 * output parameters
 *
 * @param       data
 *
 * @return      None
 */
void llCsSetupCsInd(uint8* data, uint16 connId);

/*******************************************************************************
 * @fn          llCsSetupTerminateInd
 *
 * @brief       Setup the data for the LL_CS_TERMINATE_IND PKT
 *
 * input parameters
 *
 * @param       data - data pointer
 * @param       connId - connection identifier
 *
 * @design      BLE_LOKI-506
 *
 * output parameters
 *
 * @param       data
 *
 * @return      None
 */
void llCsSetupTerminateInd(uint8* data, uint16 connId);

/*******************************************************************************
 * @fn          llCsSetupTerminateInd
 *
 * @brief       Setup the data for the LL_CS_CHANNEL_IND PKT
 *
 * input parameters
 *
 * @param       data - data pointer
 * @param       connId - connection identifier
 *
 * @design      BLE_LOKI-506
 *
 * output parameters
 *
 * @param       data
 *
 * @return      None
 */
void llCsSetupChmInd(uint8* data, uint16 connId);

/*******************************************************************************
 * @fn          llCsSubeventsPerEvent
 *
 * @brief       Calculate How many subevent will fit into 1 event
 *
 * input parameters
 *
 * @param       procedureLen - CS Procedure Length
 * @param       connInterval - connection interval
 * @param       subeventInterval - CS subevent Interval
 *
 * output parameters
 *
 * @param       None
 *
 * @return      num subevents per event
 */
uint8 llCsSubeventsPerEvent(uint16 procedureLen, uint16 connInterval,
                            uint16 subEventInterval);
