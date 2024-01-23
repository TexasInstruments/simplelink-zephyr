/******************************************************************************

 @file  ll_cs_db.h

 @brief This file contains the Data Structures and APIs for the Channel Souding
        Feature in the Bluetooth Low Energy (BLE) Controller.

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
#include "bcomdef.h"
#include "ll_cs_common.h"

/*******************************************************************************
 * EXTERNS
 */
extern uint8 stepCalcInd;
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
 * @fn          llCsInitDb
 *
 * @brief       Initializes the Database
 * Allocating the memory required the CS Database.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Status
 *              CS_INSUFFICIENT_MEMORY
 *              CS_SUCCESS
 */
uint8 llCsInitDb(void);

/*******************************************************************************
 * @fn          llCsDbClearCsData
 *
 * @brief       Initializes and resets the CS connection data
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection identifier.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbClearCsConnData(uint16 connId);

/*******************************************************************************
 * @fn          llCsDbFree
 *
 * @brief       Free the database
 *
 * @design      BLE_LOKI-506
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
void llCsDbFree(void);

/*******************************************************************************
 * @fn          llCsDbMarkProcedureCompleted
 *
 * @brief       Marks the bit of the provided cs procedure ID as completed (on)
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection identifier.
 * @param       csProcedureId - the procedure identifier to be marked as
 * completed
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbMarkProcedureCompleted(uint16 connId, uint8 csProcedureId);

/*******************************************************************************
 * @fn          llCsDbClearProcedureCompleted
 *
 * @brief       Clear Procedure Completed Flag
 *
 * input parameters
 *
 * @param       connId - connection ID
 * @param       csProcedureID - prcoedure ID
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbClearProcedureCompleted(uint16 connId, uint8 csProcedureId);

/*******************************************************************************
 * @fn          llCsDbIsProcdureCompleted
 *
 * @brief       Check if a CS procedure is completed
 * This function checks if a CS procedure is completed by checking
 * if the CS procedure's bit is active in the completedProcedure's bitmap.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Handle
 * @param       procedureId - Procedure bit, use the macros defined
 *                          in CS Ctrl Procedures
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE if the proecure is completed, FALSE otherwise.
 */
uint8 llCsDbIsProcdureCompleted(uint16 connId, uint8 procedureId);

/*******************************************************************************
 * @fn          llCsDbResetProcedureCompletedFlag
 *
 * @brief       Reset Procedure COmpleted Flag
 * This function clears the Complete Procedure flag of CS procedures that can
 * be run more than once.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Handle
 * @param       procedureId - Procedure bit, use the macros defined
 *                          in CS Ctrl Procedures
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbResetProcedureCompletedFlag(uint16 connId, uint8 procedureId);

/*******************************************************************************
 * @fn          llCsDbSetActiveProcedure
 *
 * @brief       Set Active CS Procedure.
 * Ensure no other CS procedure is already active
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Handle
 * @param       procedureId - Procedure bit, use the macros defined
 *                          in CS Ctrl Procedures
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      CS_STATUS_SUCCESS is procedure is activated successfully
 *              CS_STATUS_PROCEDURE_IN_PROGRESS cannot activate this procedure
 *              since another procedure is progress
 */
uint8 llCsDbSetActiveProcedure(uint16 connId, uint8 procedureId);

/*******************************************************************************
 * @fn          llCsDbSetSecurityVectors
 *
 * @brief       Sets Security Vectors in CS Database.
 * This API is used by the CS Security Module after it generates the CS
 * Security Vectors. The security vectors come in two parts, a central and a
 * peripheral. When both parts of the vectors are available locally after the
 * security procedure is complete, the vectors are combined such
 * that:
 *     The central's part starts from: 0
 *     central part starts from: VECTOR_SIZE/2
 *
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       pSecurityVectors - Pointer to Security Vectors.
 * @param       offset - the offset to save the security vectors
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbSetSecurityVectors(uint16 connId, csSecVectors_t* pSecurityVectors,
                              uint8 offset);

/*******************************************************************************
 * @fn          llCsDbGetSecurityVectors
 *
 * @brief       Gets Security Vectors in CS Database.
 * The CS DRBG uses this API to get the security vectors and use them.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       pSecurityVectors - Pointer to Security Vectors
 *
 * output parameters
 *
 * @param       pSecurityVectors
 *
 * @return      None
 */
void llCsDbGetSecurityVectors(uint16 connId, csSecVectors_t* pSecurityVectors);

/*******************************************************************************
 * @fn          llCsDbGetLocalCapabilities
 *
 * @brief       Gets the local CS capabilities.
 * This API is called once a Read Local CS capabilities HCI command is received.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       pLocalCapabilities - pointer to local Capabilities.
 *
 * output parameters
 *
 * @param       pLocalCapabilities
 *
 * @return      None
 */
void llCsDbGetLocalCapabilities(csCapabilities_t* pLocalCapabilities);

/*******************************************************************************
 * @fn          llCsDbSetPeerCapabilities
 *
 * @brief       Sets the CS Peer Capabilities of the connection
 * Note: in the future it should also be stored in NV.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       pRemoteCapabilities - pointer to remote Capabilities.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsDbSetPeerCapabilities(uint16 connId,
                               csCapabilities_t* pRemoteCapabilities);

/*******************************************************************************
 * @fn          llCsDbGetPeerCapabilities
 *
 * @brief       Gets the CS Peer Capabilities of the connection
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       pRemoteCapabilities - pointer to remote Capabilities.
 * output parameters
 *
 * @param       pLocalCapabilities
 *
 * @return      None
 */
void llCsDbGetPeerCapabilities(uint16 connId,
                               csCapabilities_t* pRemoteCapabilities);

/*******************************************************************************
 * @fn          llCsDbGetLocalFaeTbl
 *
 * @brief       Get Local FAE table
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       pFae - pointer to local FAE table
 *
 * output parameters
 *
 * @param       pFae - pointer to FAE table.
 *
 * @return      Status
 *              CS_STATUS_UNSUPPORTED_FEATURE if NO FAE
 *              CS_STATUS_SUCCESS if FAE
 */
csStatus_e llCsDbGetLocalFaeTbl(csFaeTbl_t* pFae);

/*******************************************************************************
 * @fn          llCsDbSetRemoteFaeTbl
 *
 * @brief       Set Remote FAE table
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       pFae - pointer to FAE table to set.
 * @param       connId - connection handle
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      status
 */
csStatus_e llCsDbSetRemoteFaeTbl(uint16 connId, csFaeTbl_t* pFae);

/*******************************************************************************
 * @fn          llCsDbGetRemoteFaeTbl
 *
 * @brief       Get Remote Fae Table
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       pFae - pointer to FAE table to set.
 * @param       connId - connection handle
 *
 * output parameters
 *
 * @param       pFae.
 *
 * @return      None
 */
void llCsDbGetRemoteFaeTbl(uint16 connId, csFaeTbl_t* pFae);

/*******************************************************************************
 * @fn          llCsDbSetConfiguration
 *
 * @brief       Set CS configuration by configuration Id.
 * CS configuration memory is allocated, then the data is set.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       pConfig - pointer to configuration set
 * output parameters
 *
 * @param       None
 *
 * @return      Status
 *              CS_STATUS_INSUFFICIENT_MEMORY if mem alloc failed
 */
csStatus_e llCsDbSetConfiguration(uint16 connId, csConfigurationSet_t* pConfig);

/*******************************************************************************
 * @fn          llCsDbRemoveConfiguration
 *
 * @brief       Disable configuration
 * Triggered by Remove configuration HCI command. This frees the memory
 * allocated for this configuration Then sets it as NULL.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection handle
 * @param       configId - configuration Id to disable
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbRemoveConfiguration(uint16 connId, uint8 configId);

/*******************************************************************************
 * @fn          llCsDbGetConfigState
 *
 * @brief       Get CS configuration State (enabled or disabled)
 *
 * input parameters
 *
 * @param       connId - connection ID
 * @param       configID - cs config ID
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      CS_DISABLED - if config is disabled
 *              CS_ENABLED  - if config is enabled
 */
uint8 llCsDbGetConfigState(uint16 connId, uint8 configId);

/*******************************************************************************
 * @fn          llCsDBGetConfigRole
 *
 * @brief       Get CS Role from CS config
 *
 * input parameters
 *
 * @param       connId - connection ID
 * @param       configID - cs config ID
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      CS_ROLE_INITIATOR or CS_ROLE_REFLECTOR
 */
uint8 llCsDbGetConfigRole(uint16 connId, uint8 configId);

/*******************************************************************************
 * @fn          llCsDbGetConfiguration
 *
 * @brief       Gets CS configuration by configuration Id
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       configId - CS configuration Id
 * @param       pConfig - pointer to configuration set
 * output parameters
 *
 * @param       pConfig
 *
 * @return      status
 */
csStatus_e llCsDbGetConfiguration(uint16 connId, uint8 configId,
                                csConfigurationSet_t* pConfig);

/*******************************************************************************
 * @fn          llCsDbSetCurrentConfigId
 *
 * @brief       Set current configuration.
 * This is needed because there is an array of configurations
 * And when setting up we may lose the current index
 * This is used to store it
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       configId - CS configuration Id
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsDbSetCurrentConfigId(uint16 connId, uint8 configId);

/*******************************************************************************
 * @fn          llCsDbRemobeCurrentConfigId
 *
 * @brief       Remove current config ID
 *
 * input parameters
 *
 * @param       connId - connection ID
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbRemoveCurrentConfigId(uint16 connId);

/*******************************************************************************
 * @fn          llCsDbGetCurrentConfigId
 *
 * @brief       Get current configuration.
 * This is needed because there is an array of configurations
 * And when setting up we may lose the current index
 * This is used to get it
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * output parameters
 *
 * @param       None
 *
 * @return      current config Id
 */
uint8 llCsDbGetCurrentConfigId(uint16 connId);

/*******************************************************************************
 * @fn          llCsDbSetProcedureParams
 *
 * @brief       Sets CS Procedure Parameters by configuration Id
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       configId - CS configuration Id
 * @param       pProcParams - pointer to procedure parameters
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsDbSetProcedureParams(uint16 connId, uint8 configId,
                              csProcedureParams_t* pProcParams);

/*******************************************************************************
 * @fn          llCsDbGetProcedureParams
 *
 * @brief       Gets CS Procedure Parameters by configuration Id
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       configId - CS configuration Id
 * @param       pProcParams - pointer to procedure parameters
 * output parameters
 *
 * @param       pProcParams
 *
 * @return      None
 */
void llCsDbGetProcedureParams(uint16 connId, uint8 configId,
                              csProcedureParams_t* pProcParams);

/*******************************************************************************
 * @fn          llCsDbEnableProcedureParams
 *
 * @brief       Enables or disables the procedure parameter set
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId   - connection Id
 * @param       configId - CS configuration Id
 * @param       enable   - enable or disable procedure params
 * output parameters
 *
 * @param       pProcParams
 *
 * @return      None
 */
void llCsDbEnableProcedureParams(uint16 connId, uint8 configId, uint8 enable);

/*******************************************************************************
 * @fn          llCsDbIsProcedureEnabled
 *
 * @brief       Is CS procedure enabled
 *
 * input parameters
 *
 * @param       connId - connection ID
 * @param       configID - cs config ID
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
uint8 llCsDbIsProcedureEnabled(uint16 connId, uint8 configId);

/*******************************************************************************
 * @fn          llCsDbGetProcedureTerminateState
 *
 * @brief       Get current terminateState.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId   - connection Id
 * @param       configId - CS configuration Id
 *
 * output parameters
 *
 * @param       None
 *
 * @return      terminateState
 */
uint8 llCsDbGetProcedureTerminateState(uint16 connId, uint8 configId);

/*******************************************************************************
 * @fn          llCsDbSetProcedureTerminateState
 *
 * @brief       Set terminateState parameter
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId         - connection Id
 * @param       configId       - CS configuration Id
 * @param       terminateState - indicate if the procedure state should be
 *                    updated to CS_DISABLE. The following values are optional:
 *                    CS_TERMINATE_RECEIVED and CS_TERMINATE_DISABLE
 *
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsDbSetProcedureTerminateState(uint16 connId, uint8 configId,
                                      uint8 terminateState);

/*******************************************************************************
 * @fn          llCsDbSetEnableProcedureDuration
 *
 * @brief       Set the procedure enable duration parameter
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId   - connection Id
 * @param       configId - CS configuration Id
 * @param       duration - Procedure duration
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsDbSetEnableProcedureDuration(uint16 connId, uint8 configId,
                                      uint16 duration);

/*******************************************************************************
 * @fn          llCsDbSetEnableProcedureCount
 *
 * @brief       Set the procedure enable procedure count parameter
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId    - connection Id
 * @param       configId  - CS configuration Id
 * @param       count     - The number of the procedure repetitions
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsDbSetEnableProcedureCount(uint16 connId, uint8 configId, uint16 count);

/*******************************************************************************
 * @fn          llCsDbSetEnableProcedureInterval
 *
 * @brief       Set the procedure enable procedure event interval parameter
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId            - connection Id
 * @param       configId          - CS configuration Id
 * @param       procedureInterval - The number of connection interval between
 *                                  two consecutive procedures
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsDbSetEnableProcedureInterval(uint16 connId, uint8 configId,
                                      uint16 procedureInterval);

/*******************************************************************************
 * @fn          llCsDbSetDefaultSettings
 *
 * @brief       Sets the default settings in the CS DB
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       defaultSettings - default settings: role enable and
 *                                csSyncAntennaSelection
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsDbSetDefaultSettings(uint16 connId,
                              csDefaultSettings_t* defaultSettings);

/*******************************************************************************
 * @fn          llCsDbGetDefaultSettings
 *
 * @brief       Gets the default settings from the CS DB
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       defaultSettings - default settings: role enable and
 *                                csSyncAntennaSelection
 * output parameters
 *
 * @param       None
 *
 * @return      defaultSettings
 */
void llCsDbGetDefaultSettings(uint16 connId,
                              csDefaultSettings_t* defaultSettings);

/*******************************************************************************
 * @fn          llCsDbSetProcedureEnableData
 *
 * @brief       Sets the procedure enable data that is built when sending
 * LL_CS_REQ packet. Used to update the enable data once LL_CS_RSP or LL_CS_IND
 * is received
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId   - connection Id
 * @param       configId - configuration Id
 * @param       enData   - enable data
 *
 * output parameters
 *
 * @param       None
 *
 * @return      void
 */
void llCsDbSetProcedureEnableData(uint16 connId, uint8 configId,
                                  csProcedureEnable_t* enData);

/*******************************************************************************
 * @fn          llCsDbGetProcedureEnableData
 *
 * @brief       Gets the procedure enable data that was set when LL_CS_REQ pkt
 *              was built.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId   - connection Id
 * @param       configId - configuration Id
 * @param       enData   - enable data
 *
 * output parameters
 *
 * @param       enData
 *
 * @return      none
 */
void llCsDbGetProcedureEnableData(uint16 connId, uint8 configId,
                                  csProcedureEnable_t* enData);

/*******************************************************************************
 * @fn          llCsDbCompareProcedureData
 *
 * @brief       Compare procedure data saved in the csdb, to what is provided
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       configId - config Id
 * @param       procData - procedure enable data
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
uint8 llCsDbCompareProcedureData(uint16 connId, uint8 configId,
                                 csProcedureEnable_t* procData);

/*******************************************************************************
 * @fn          llCsDbGetAclCounter
 *
 * @brief       Get the ACL counter that the CS is pending
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId   - connection Id
 * @param       configId - configuration Id
 *
 * output parameters
 *
 * @param       None
 *
 * @return      ACL Counter
 */
uint8 llCsDbGetAclCounter(uint16 connId, uint8 configId);

/*******************************************************************************
 * @fn          llCsDbSetNextProcedureConnEvent
 *
 * @brief       Set the connEvent that the next CS Event or Procedure should
 *              begin from
 *
 * input parameters
 *
 * @param       connId    - connectionId
 * @param       configId  - config Id
 * @param       connEvent - connection event
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbSetNextProcedureConnEvent(uint16 connId, uint8 configId,
                                     uint8 connEvtCount);

/*******************************************************************************
 * @fn          llCsDbSetProcedureEnableIndData
 *
 * @brief       Sets the procedure enable data received over the LL_CS_IND pkt
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId   - connection Id
 * @param       configId - configuration Id
 * @param       enData   - enable data
 *
 * output parameters
 *
 * @param       enData
 *
 * @return      none
 */
void llCsDbSetProcedureEnableIndData(uint16 connId, uint8 configId,
                                     csProcedureEnable_t* enData);

/*******************************************************************************
 * @fn          llCsDbGetNextProcedureFlag
 *
 * @brief       Get the next procedure flag
 *
 * input parameters
 *
 * @param       connId   - connection Id
 * @param       configId - configuration Id
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
uint8 llCsDbGetNextProcedureFlag(uint16 connId, uint8 configId);

/*******************************************************************************
 * @fn          llCsDbSetNextProcedureFlag
 *
 * @brief       Set the next procedure flag
 *
 * input parameters
 *
 * @param       connId   - connection Id
 * @param       next     - flag to set
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbSetNextProcedureFlag(uint16 connId, uint8 next);

/*******************************************************************************
 * @fn          llCsDbUpdateChannelMap
 *
 * @brief       Update channel map
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       pChM - pointer to channel map update
 * @param       currentTime - current RAT time
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Status
 *              CS_STATUS_INVALID_CHM If the channel map combination results in
 * zero
 */
csStatus_e llCsDbUpdateChannelMap(uint8* pChM, uint32 currentTime);

/*******************************************************************************
 * @fn          llCsDbGetLastChmUpdateTime
 *
 * @brief       Get Last Channel Map Update time
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Last Channel Map Update Time
 */
uint32 llCsDbGetLastChmUpdateTime(void);

/*******************************************************************************
 * @fn          llCsDbGetChannelMap
 *
 * @brief       Get the Channel Map classification
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       pChM - pointer to channel map
 *
 * output parameters
 *
 * @param       pChm.
 *
 * @return      None
 */
void llCsDbGetChannelMap(uint8* pChm);

/*******************************************************************************
 * @fn          llCsDbFilterChannelMap
 *
 * @brief       Get the Channel Map classification
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       pFilteredChm - pointer to filterd channel map
 *
 * output parameters
 *
 * @param       pFilteredChm.
 *
 * @return      None
 */
void llCsDbFilterChannelMap(uint8* pFilteredChM);

/*******************************************************************************
 * @fn          llCsDbSetActiveConnId
 *
 * @brief      Set Active Conn Id
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection identifier
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbSetActiveConnId(uint16 connId);

/*******************************************************************************
 * @fn          llCsDbGetActiveConnId
 *
 * @brief      Get active connection ID
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       none
 *
 * output parameters
 *
 * @param       ConnId.
 *
 * @return      None
 */
uint16 llCsDbGetActiveConnId(void);

/*******************************************************************************
 * @fn          llCsDbSetBleRole
 *
 * @brief       Set BLE role in the CS DB
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       role - BLE role, central or peripheral
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbSetBleRole(csBleRole role);

/*******************************************************************************
 * @fn          llCsDbGetBleRole
 *
 * @brief       Get BLE role: Central or Peripheral.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       none
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      BLE Role
 */
csBleRole llCsDbGetBleRole(void);

/*******************************************************************************
 * @fn          llCsDbGetAciTable
 *
 * @brief       Get ACI table info for given ACI
 *
 * input parameters
 *
 * @param       ACI - Antenna Config Index
 *
 * output parameters
 *
 * @param       None.
 *
 * @return     array with 2 elemnts, the initior's N_AP and the reflector's
 *             N_AP for the given ACI.
 */
uint8* llCsDbGetAciTable(csACI_e ACI);

/*******************************************************************************
 * @fn          llCsDbGetACI
 *
 * @brief       Get the ACI for given initiator N_AP and reflector N_AP
 *
 * input parameters
 *
 * @param       initNAP - initiator Num Antenna Path
 * @param       refNAP - reflector's Num Antenna Path
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
csACI_e llCsDbGetACI(uint8 initNAP, uint8 refNAP);

/*******************************************************************************
 * @fn          llCsDbInitChanIndexInfo
 *
 * @brief       Initialize the channel Index info struct.
 * There are two channel Index arrays, mode0 and nonMode0.
 * They are allocated according to the size numChan.
 * And initialized to the filtered channel map (chanArray)
 * Later it will be shuffled.
 *
 * @note        The allocated memory is freed in llCsDbFreeChannelIndexArray
 *              If the config is removed. And when all CS is freed.
 *              Also, when need to a allocate a different size.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param      connId    - connection Id
 * @param      configId  - CS config Id
 * @param      numCha    - number of channels to allocate
 * @param      chanArray - filtered channel index array to use
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      status: SUCCESS or INFSUFFICIENT_MEMORY
 */
csStatus_e llCsDbInitChanIndexInfo(uint16 connId, uint8 configId, uint8 numChan,
                                 uint8* chanIdxArr);

/*******************************************************************************
 * @fn          llCsDbGetChanInfo
 *
 * @brief       Get the channel info of given connId, configId
 *
 * input parameters
 *
 * @param       connId - connection handle
 * @param       configId - cs config Id
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      pointer to channel info
 */
csChanInfo_t* llCsDbGetChanInfo(uint16 connId, uint8 configId);

/*******************************************************************************
 * @fn          llCsDbGetNumChan
 *
 * @brief       Get the number of channels for the given connId and configId.
 *
 * input parameters
 *
 * @param       connId - connection Handle
 * @param       configId - CS config Identifier
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      numChans
 */
uint8 llCsDbGetNumChan(uint16 connId, uint8 configId);

/*******************************************************************************
 * @fn          llCsDbUpdateChanIndexArray
 *
 * @brief       Update Channel Index Array of a specific connId and config Id
 *              In the DB.
 *              Main use: update after shuffling the array.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param      mode      - CS stepMode
 * @param      connId    - connection Id
 * @param      configId  - CS config Id
 * @param      chanArray - channel Index Array Info to update in the DB
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbUpdateChanIndexArray(uint8 mode, uint16 connId, uint8 configId,
                                modeSpecificChanInfo_t* chanArr);

/*******************************************************************************
 * @fn          llCsDbSetRemainingMmSteps
 *
 * @brief       Set number of remaining main mode steps.
 * Number of main mode steps for the procedure is calculated when
 * The first subevent is setup. Some (if not all) the main mode
 * steps will be done in the subevent, however, the leftover steps
 * will be saved here.
 *
 * input parameters
 *
 * @param       connId - connection handle
 * @param       configId - cs config identifier
 * @param       mmSteps - number of main mode steps
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbSetRemainingMmSteps(uint16 connId, uint8 configId, uint16 mmSteps);

/*******************************************************************************
 * @fn          llCsDbGetRemainingMmSteps
 *
 * @brief       Get the number of steps remaining for the procedure.
 *
 * input parameters
 *
 * @param       connId - connection handle
 * @param       configId - cs config identifier
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
uint16 llCsDbGetRemainingMmSteps(uint16 connId, uint8 configId);

/*******************************************************************************
 * @fn          llCsDbGetSubeventInfo
 *
 * @brief       Get Subevent info from the DB
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       type   - info type:
 *                       CS_SE_INFO_NUM_STEPS to get the num of steps
 *                       CS_SE_INFO_STEP_COUNT to get the step count
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      numSteps or stepCount per type
 */
uint8 llCsDbGetSubeventInfo(uint16 connId, csSubeventInfo_e type);

/*******************************************************************************
 * @fn          llCsDbGetSubeventInfo
 *
 * @brief       Get Subevent info from the DB
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       type   - info type per csSubeventInfo_e
 * @param       count  - count of given type
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llCsDbSetSubeventCount(uint16 connId, csSubeventInfo_e type, uint8 count);

/*******************************************************************************
 * @fn          llCsDbResetSubeventInfo
 *
 * @brief       Reset Subevent Info.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llCsDbResetSubeventInfo(uint16 connId);

/*******************************************************************************
 * @fn          llCsDbIncrementSubeventInfoCounter
 *
 * @brief       Increment a subeventInfo counter by incVal
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       type   - info type
 * @param       incVal - incremention value
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llCsDbIncrementSubeventInfoCounter(uint16 connId, csSubeventInfo_e type,
                                        uint8 incVal);

/*******************************************************************************
 * @fn          llCsDbIncrementProcCounter
 *
 * @brief       Increment procedure counter
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       counter - the type of procedure counter
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
uint16 llCsDbIncrementProcCounter(uint16 connId, csProcedureCounter_e counter);

/*******************************************************************************
 * @fn          llCsDbResetProcCounter
 *
 * @brief       Reset Procedure Counter
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       counter - the type of procedure counter
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbResetProcCounter(uint16 connId, csProcedureCounter_e counter);

/*******************************************************************************
 * @fn          llCsDbGetProcCounter
 *
 * @brief       Get procedure countern
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       counter - the type of procedure counter
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
uint16 llCsDbGetProcCounter(uint16 connId, csProcedureCounter_e counter);

/*******************************************************************************
 * @fn          llCsDbSetEventsPerProcedure
 *
 * @brief       Set events per procedure
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       eventsPerProcedure - events per procedure
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbSetEventsPerProcedure(uint16 connId, uint16 eventsPerProcedure);

/*******************************************************************************
 * @fn          llCsDbGetEventsPerProcedure
 *
 * @brief       Get Events Per Procedure
 *
 * input parameters
 *
 * @param       connId - connection Id
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
uint16 llCsDbGetEventsPerProcedure(uint16 connId);

/*******************************************************************************
 * @fn          llCsDbSetEventAnchorPoint
 *
 * @brief       Set event anchor point
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       anchorPoint - anchor point
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsDbSetEventAnchorPoint(uint16 connId, uint32_t anchorPoint);

/*******************************************************************************
 * @fn          llCsDbGetEventAnchorPoint
 *
 * @brief       Get event anchor point
 *
 * input parameters
 *
 * @param       connId - connection Id
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
uint32_t llCsDbGetEventAnchorPoint(uint16 connId);

/*******************************************************************************
 * @fn          llCsDbInitDRBGCache
 *
 * @brief       Allocate and initialize the DRBG cache
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Status
 */
csStatus_e llCsDbInitDRBGCache(void);

/*******************************************************************************
 * @fn          llCsDbResetDRBGCache
 *
 * @brief       Reset the DRBG cache
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      status
 */
csStatus_e llCsDbResetDRBGCache(void);

/*******************************************************************************
 * @fn          llCsDbFreeDRBGCache
 *
 * @brief       Free DRBG cache structs
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
void llCsDbFreeDRBGCache(void);

/*******************************************************************************
 * @fn          llCsDbSetRandomBitsCache
 *
 * @brief       Sets random bits to cache by transaction Id
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       transactionId - CS transaction Id
 * @param       pRandomBits - pointer to random bits
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsDbSetRandomBitsCache(uint8 transactionId, uint8* pRandomBits);

/*******************************************************************************
 * @fn          llCsDbRandomBitsAvailable
 *
 * @brief       Check if enough bits are available in cache.
 * This is to know if we need to generate more bits or we can use
 * the cashed buffer.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       transactionId - CS transaction Id
 * @param       numBitsRequired - number of required bits
 *
 * output parameters
 *
 * @param       pRandomBits
 *
 * @return      None
 */
uint8 llCsDbRandomBitsAvailable(uint8 transactionId, uint8 numBitsRequired);

/*******************************************************************************
 * @fn          llCsDbGetRandomBitsFromCache
 *
 * @brief       Gets random bits from cache by transaction Id
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       transactionId - CS transaction Id
 * @param       numBitsRequired - number of required bits
 * @param       pRandomBits - pointer to random bits
 * output parameters
 *
 * @param       pRandomBits
 *
 * @return      None
 */
void llCsDbGetRandomBitsFromCache(uint8 transactionId, uint8 numBitsRequired,
                                  uint8* pRandomBits);

/*******************************************************************************
 * @fn          llCsDbGetChannelIdxArray
 *
 * @brief       Get Channel Index Array from the DB
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       mode      - CS mode
 * @param       connId    - connection Id
 * @param       configId  - config Id
 * @param       chanData  - chan Data
 * @param       numChan   - numChan
 *
 * output parameters
 *
 * @param       chanData   - channel Index Array struct
 * @param       numChan    - num of channels in the array
 *
 * @return      none
 */
void llCsDbGetChannelIdxArray(uint8 mode, uint16 connId, uint8 configId,
                              modeSpecificChanInfo_t* chanData, uint8* numChan);

/*******************************************************************************
 * @fn          llCsDbGetChannelIndex
 *
 * @brief       Get Channel Index from the channel Index Array
 *              increment the channels used counter
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection ID
 * @param       configId - CS config Id
 * @param       mode - CS mode
 * output parameters
 *
 * @param       None
 *
 * @return      Channel Index
 */
uint8 llCsDbGetChannelIndex(uint16 connId, uint8 configId, uint8 mode);

/*******************************************************************************
 * @fn          llCsDbFreeChannelIndexArray
 *
 * @brief       Free channel Index array
 * The channel index array is dynamically allocated
 * Hence we should make sure to use this API to free it when done
 * There are two scenarios in which we would want to free:
 * * CS is done
 * * Channel Index array size is changed (if the CHM classification
 *   changed) then we need to free and allocate according to the
 *   new size
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       configId - cs config Id
 *
 * output parameters
 *
 * @param  None
 *
 * @return None
 */
void llCsDbFreeChannelIndexArray(uint16 connId, uint8 configId);

/*******************************************************************************
 * @fn          getRoleModeSpecificDataType
 *
 * @brief       Role Mode Spefici Data type
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param      role - CS role: initiator or reflector
 * @param      mode - CS mode: 0,1,2,3
 *
 * output parameters
 *
 * @param       RoleModeSpecificDataType
 *
 * @return      None
 */
csModeRole_e getRoleModeSpecificDataType(uint8 role, uint8 mode);

/*******************************************************************************
 * @fn          llCsDbGetBits
 *
 * @brief       The function returns an integer containing the bits starting
 *              from the specified index.
 *
 * @design      BLE_LOKI-506
 * input parameters
 *
 * @param       num      - The number you want to extract bits from.
 * @param       numBits - The number of bits you want to extract.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      integer containing the bits starting from the specified index.
 */
uint8 llCsDbGetBits(uint8 num, uint8 startIdx, uint8 numBits);
