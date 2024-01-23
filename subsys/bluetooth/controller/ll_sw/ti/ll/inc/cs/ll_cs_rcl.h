/******************************************************************************

 @file  ll_cs_rcl.h

 @brief CS RF funcs

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2023 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifndef LL_CS_RCL_H
#define LL_CS_RCL_H
/*******************************************************************************
 * INCLUDES
 */
#include <ti/drivers/rcl/commands/ble_cs.h>
#include <ti/drivers/rcl/handlers/ble_cs.h>
#include "ll_cs_common.h"

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * MACROS
 */
#ifdef CS_TEST
#define SIZE_OF_BUFFER_DATA(x) (x * sizeof(RCL_CmdBleCs_Step))
#define BLE_CS_CREATE_BASIC_STEP(m, tx, rx)                                    \
    {                                                                          \
        .channelIdx = 0, .mode = m, .toneExtension = 0, .antennaPermIdx = 0,   \
        .payloadLen = 0, .aaTx = tx, .aaRx = rx,                               \
        .payloadTx = {0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA},         \
        .payloadRx = {                                                         \
            0xBBBBBBBB,                                                        \
            0xBBBBBBBB,                                                        \
            0xBBBBBBBB,                                                        \
            0xBBBBBBBB                                                         \
        }                                                                      \
    }
#endif

/*******************************************************************************
 * TYPEDEFS
 */
typedef struct
{
    struct
    {
        List_Elem __elem__;
        RCL_BufferState state; /*!< Buffer state */
        uint16_t length;       /*!< Number of bytes in the data field */
        uint16_t headIndex;    /*!< Number of bytes consumed */
        uint16_t tailIndex;    /*!< Number of bytes written */
    } header;
    RCL_CmdBleCs_Step steps[];
} ble_cs_steps_buffer_t;

typedef struct
{
    ble_cs_steps_buffer_t *csStepsBuff0;
    ble_cs_steps_buffer_t *csStepsBuff1;
    uint8_t               *csStepResultsBuff0;
    RCL_CmdBleCs_Stats    *csOutput;
    uint8                 numSteps;
    uint8                 buffsAllocated; /* flag that indicates if the buffers are allocated */
 } csRclCmdData_t;

typedef struct
{
    const LRF_Config* lrfConfigPtr;
} csLrfConfig_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */
extern csLrfConfig_t csLrfConfig;

/*******************************************************************************
 * EXTERNS
 */
extern uint16 ble_cs_steps_buffer_size;
extern uint16 ble_cs_step_results_buffer_size;

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * @fn          llCsSubevent_PostProcess
 *
 * @brief       A subevent was completed
 * Process subevent results, prepare for next subevent, event
 * or procedure if needed.
 * End things if procedure is done (no more steps needed)
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
extern void llCsSubevent_PostProcess(void);

/*******************************************************************************
 * @fn          llCsSteps_PostProcess
 *
 * @brief       Post Process when a steps buffer was consumed
 * Get a pointer the consumed step buffer, clear it.
 * If more steps are needed, generate more steps, and init the
 * buffer again.
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
extern void llCsSteps_PostProcess(void);

/*******************************************************************************
 * @fn          llCsResults_PostProcess
 *
 * @brief       Post Process when CS results are available.
 *
 * input parameters
 *
 * @param       procedureDone - flag indicates if a procedure is done
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
extern void llCsResults_PostProcess(uint8 procedureDone);

/*******************************************************************************
 * @fn          llCsProcessResults
 *
 * @brief       Process CS results and notify Host
 *
 * input parameters
 *
 * @param       resBuf - pointer to results buffer
 * @param       isProcedureDone - is procedure done
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsProcessResults(RCL_CmdBleCs_SubeventResults* resBuf,
                        uint8 isProcedureDone);

/*******************************************************************************
 * @fn          llCsSetupRcl
 *
 * @brief       This function sets up the device for CS
 * Setup CS RCL command, parameters and output registers.
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       csRclDataInt - pointer to CS RCL cmd
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      status
 *              success
 */
csStatus_e llCsSetupRcl(uint16 connId, csRclCmdData_t csRclDataInt);

/*******************************************************************************
 * @fn          llCsRclCallback
 *
 * @brief       CS RCL callback
 *
 * input parameters
 *
 * @param       cmd - pointer to rcl command
 * @param       lrfEvents - lrfEvents
 * @param       rclEvents - rclEvents
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsRclCallback(RCL_Command* cmd, LRF_Events lrfEvents,
                     RCL_Events rclEvents);

/*******************************************************************************
 * @fn          llCsProcessResultsCb
 *
 * @brief       Process results callback
 *
 * input parameters
 *
 * @param       procedureDoneSt - procedure done status
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsProcessResultsCb(csProcDoneStat_e procedureDoneSt);

/*******************************************************************************
 * @fn          llCsGenerateMoreSteps
 *
 * @brief       Generate More CS Steps
 * Used when need to switch Step Buffers.
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       numSteps - number of steps to generate
 * @param       stepListBuf - pointer to the stepList
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsGenerateMoreSteps(uint16 connId, uint8 numSteps,
                           ble_cs_steps_buffer_t* stepListBuf);

/*******************************************************************************
 * @fn          llCsRclFreeTask
 *
 * @brief       Free CS Task
 * Free Steps and Steps results buffers.
 * Clear procedure flags.
 * Free the CS Task
 *
 * input parameters
 *
 * @param       connHandle - connection handle aka id
 * @param       configId - cs config ID
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsRclFreeTask(uint16 connHandle, uint8 configId);

#endif
