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
 * @fn          llCsSetupStepList
 *
 * @brief       This API builds the Step List
 * Double Buffer for the RF. The function selects the Step Modes to
 * build, then calls the internal function llCsSetupStep that
 * builds the step. This function allocates the Channel Index
 * Arrays Mode0ChIdxArray and NonMode0ChIdxArray.
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       connPtr -  Pointer to the current connection
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
csStatus_e llCsSetupStepList(llConnState_t* connPtr, uint8 configId,
                           uint8 isfirstSE);

/*******************************************************************************
 * @fn          llCsStartProcedure
 *
 * @brief       This function is used to trigger the CS module
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
uint8 llCsStartProcedure(llConnState_t* connPtr);

/*******************************************************************************
 * @fn          llCsStartStepListGen
 *
 * @brief       If a CS_START procedure was completed or next procedure should
 *              begin, generate step list for the upcoming procedure.
 *
 * input parameters
 *
 * @param       connPtr - connection Pointer
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
uint8 llCsStartStepListGen(llConnState_t* connPtr);

/*******************************************************************************
 * @fn          llCsSetupSubevent
 *
 * @brief       Setup CS RCL command which is equal to a CS subevent
 *
 * input parameters
 *
 * @param       connPtr - connection pointer
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
uint8 llCsSetupSubEvent(llConnState_t* connPtr);

/*******************************************************************************
 * @fn          freeCsStepsAndResults
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
void freeCsStepsAndResults(void);

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
 * @return      Status
 */
uint8 llCsSelectAA(uint8 csRole, uint32_t* aaRx, uint32_t* aaTx);
