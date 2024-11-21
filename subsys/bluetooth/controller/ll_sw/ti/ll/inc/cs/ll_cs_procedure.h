/******************************************************************************

 @file  ll_cs_procedure.h

 @brief LL CS Procedure contains the APIs that are responsible for Initialziing
        the CS module. Building the CS steps of a CS subevent.
        Manages the CS double buffers.
        Sends CS Step results to the Host.

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
#include "ll_cs_common.h"
#include "ll_csdrbg.h"
#include "ll_cs_rcl.h"

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

#define CS_MAX_CHANNEL_REPETITIONS         3      //! CS Max Channel Map repetitions
#define CS_SUBEVENT_RESULT_OPCODE          0x31   //!< CS Subevent Result Event Opcode
#define CS_CONTINUE_SUBEVENT_RESULT_OPCODE 0x32   //!< CS Continue Subevent Result Event Opcode

/*******************************************************************************
 * EXTERNS
 */

/*******************************************************************************
 * TYPEDEFS
 */
typedef void (*csResultsCb_t)(void);

/*******************************************************************************
 * LOCAL VARIABLES
 */
extern drbgParams_t csDrbgParams;

extern uint8 csNumSteps;

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * @fn          llCsInit
 *
 * @brief       Initialize the CS feature
 * This function calls llCsInit Db which in turn allocates the
 * memory needed for CS, and initializes the structures and their
 * initial settings for usage
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None
 *
 * @return      Status
 *              CS_INSUFFICIENT_MEMORY
 *              CS_SUCCESS
 */
uint8 llCsInit(void);

/*******************************************************************************
 * @fn          llCsClearConnProcedures
 *
 * @brief       Clear Procedures per connection (usually due to termination)
 *
 * input parameters
 *
 * @param       connId - connection Identifier
 *
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsClearConnProcedures(uint16 connId);

/*******************************************************************************
 * @fn          llCsFreeAll
 *
 * @brief       Call llCsDbFree to free all memory allocated for CS.
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsFreeAll(void);

/*******************************************************************************
 * @fn          llCsSetFeatureBit
 *
 * @brief       Set the CS feature bit in deviceFeatureSet.featureSet
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsSetFeatureBit(void);

/*******************************************************************************
 * @fn          llCsInitProcedureStepList
 *
 * @brief       Initializes the Step List, step num and channel arrays
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connId  -  Connection Id
 * @param       configId - Configuration ID of the CS procedure to run
 * @param       isfirstSE - is first SE
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Status
 *              LL_CS_STATUS_NO_PROCEDURE - if no CS procedure is enabled
 *              LL_CS_STATUS_INVALID_CONN_PTR - invalid connection pointer
 *              LL_CS_STATUS_SUCCESS
 */
csStatus_e llCsInitProcedureStepList(uint16 connId, uint8 configId, uint8 isfirstSE);

/*******************************************************************************
 * @fn          llCsSetupStepBuffers
 *
 * @brief       Setup Step Buffers
 * Setup Step Buffers for a new or an ongiong subevent.
 * If a new subevent, the first buffer is built and begins with mode 0 steps.
 * The second buffer is optional, depends on the availability of a second buffer.
 * If it's an ongoing subevent, always opt to use csStepsBuff1, since we don't
 * want it to begin with mode 0 steps.
 *
 * input parameters
 *
 * @param       connId       - Connection Id
 * @param       configId     - CS Config Id
 * @param       isNewSubevent- flag that indicates if this is a new subevent
 * @param       csStepsBuff0 - Pointer to the first buffer. Use when setting up
 *                             a new subevent. Otherwise, set to NULL.
 * @param       csStepsBuff1 - Pointer to the second buffer. When not used, set
 *                             to NULL.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsSetupStepBuffers(uint16 connId, uint8 configId, csNewSubevent_e isNewSubevent,
                          csStepsBuffer_t* csStepsBuff0,
                          csStepsBuffer_t* csStepsBuff1 );

/*******************************************************************************
 * @fn          llCsStartProcedure
 *
 * @brief       Start a CS Procedure
 * This function checks whether all the requirements for starting a CS procedure
 * have been met, and if they are a CS procedure starts by building the RCL
 * command and a CS task.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Status
 */
csStatus_e llCsStartProcedure(llConnState_t* connPtr);

/*******************************************************************************
 * @fn          llCsStartTestProcedure
 *
 * @brief       Start a CS Test Procedure
 * This function is the equivalent of llCsStartProcedure but for test mode.
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
csStatus_e llCsStartTestProcedure(void);

/*******************************************************************************
 * @fn          llCsStartStepListGen
 *
 * @brief       If a CS_START procedure was completed or next procedure should
 *              begin, generate step list for the upcoming procedure.
 *
 * input parameters
 *
 * @param       connId - connection Identifier
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
uint8 llCsStartStepListGen(uint16 connId);

/*******************************************************************************
 * @fn          llCsSetupSubevent
 *
 * @brief       Setup CS RCL command which is equal to a CS subevent
 *
 * input parameters
 *
 * @param       connId - Connection Id
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
uint8 llCsSetupSubEvent(uint16 connId);

/*******************************************************************************
 * @fn          llCsFreeStepsAndResults
 *
 * @brief       Free CS step buffers and CS results buffer
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
void llCsFreeStepsAndResults(void);

/*******************************************************************************
 * @fn          llCsSelectStepChannel
 *
 * @brief       Selects the step's channel based on the step mode
 * The only caller for this function is llCsSetupStep.
 * The CS Channel Index is  a number between 2-78
 * With Expections.
 * See Table in CS SPEC:
 * Channel Sounding physical channels
 * Mapping of CS channel index to RF physical channel
 *
 * input parameters
 *
 * @param       stepMode - step mode (0, 1, 2, 3)
 * @param       connId - connection ID
 * @param       isRepetition - is a repeated main mode
 * @param       csConfig - pointer to CS config
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Selected Channel Index
 */
uint8 llCsSelectStepChannel(uint8 stepMode, uint16 connId, uint8 isRepetition,
                            csConfigurationSet_t* csConfig);

/*******************************************************************************
 * @fn         llCsShuffleIndexArray
 *
 * @brief      Shuffle Channel Index Array
 * This function chooses the channel selection algorithm to shuffle
 * the channel index array of the given cs mode (0 or non-0)
 *
 * input parameters
 *
 * @param       mode - cs step mode (0 or non-0)
 * @param       numchan - number of channels (array size)
 * @param       chanArr - struct with info about the channel array
 * @param       filteredArr - filtered channel array
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
uint8 llCsShuffleIndexArray(uint8 mode, uint8 numChan,
                            modeSpecificChanInfo_t* chanArr,
                            uint8* filteredArr);

/*******************************************************************************
 * @fn          llCsSelectAA
 *
 * @brief       Select Step's Access Address.
 *
 * input parameters
 *
 * @param       csRole - CS role initiator or reflector
 * @param       aaRx - pointer to the first part of the access address
 * @param       aaTx - pointer to the second part of the access address
 *
 * output parameters
 *
 * @param       aaRx
 * @param       aaTx
 *
 * @return      None
 */
void llCsSelectAA(uint8 csRole, uint32_t* aaRx, uint32_t* aaTx);

/*******************************************************************************
 * @fn          llCsGetRandomSequence
 *
 * @brief       Get Random Sequence from DRBG
 *
 * input parameters
 *
 * @param       csRole - CS role
 * @param       pTx - pointer to transmitted Random Sequence
 * @param       pRx - pointer to the recvd Random Sequence
 * @param       payloadLen - payload length
 * output parameters
 *
 * @param       pTx
 * @param       pTx
 *
 * @return      None
 */
void llCsGetRandomSequence(uint8 csRole, uint32_t* pTx, uint32_t* pRx,
                           uint8 payloadLen);

/*******************************************************************************
 * @fn          llCsGetToneExtention
 *
 * @brief       Get Tone Extension bit from DRBG
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None
 *
 * @return      Tone extension bit
 */
uint8 llCsGetToneExtention(void);

/*******************************************************************************
 * @fn          llCsInitChanIdxArr
 *
 * @brief       Initialize Channel Index array
 * This function turns the filtered channel map to a channel index
 * array. It sets the initial channel index info in the struct.
 * It shuffles the mode 0 channel index array which will be used
 * immediately.
 *
 * input parameters
 *
 * @param       configId - CS config ID
 * @param       connId - connection ID
 * @param       csConfig - pointer to CS config
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      status
 */
csStatus_e llCsInitChanIdxArr(uint8 configId, uint16 connId,
                              csConfigurationSet_t* csConfig);
