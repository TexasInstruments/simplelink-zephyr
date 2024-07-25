/******************************************************************************

 @file  ll_cs_rcl_internal.h

 @brief Internal functions used by ll_cs_rcl module

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2022 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*******************************************************************************
 * INCLUDES
 */
#include <ti/drivers/rcl/commands/ble_cs.h>
#include <ti/drivers/rcl/handlers/ble_cs.h>
#include "ll_cs_common.h"
#include "ll_common.h"

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * EXTERNS
 */

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
 * @fn          llCsSetupCmdStartTime
 *
 * @brief       Setup the CS Command start time
 * The selected start time depends on the CS Roke and whether the
 * SubeventCount is 0 or more.
 *
 * input parameters
 *
 * @param       rclCmd - cs RCL command
 * @param       connPtr - pointer to connection Info
 * @param       role - CS Role, initiator or reflector
 * @param       subeventCount - CS subevent counter
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsSetupCmdStartTime(RCL_CmdBleCs rclCmd, llConnState_t* connPtr,
                           uint8 role, uint8 subEventCount);

/*******************************************************************************
 * @fn          llCsRClBufferSetup
 *
 * @brief       Setup the CS RCL double buffers.
 * Clear results buffers, init the steps and results buffers.
 * Put buffers in queue.
 *
 * input parameters
 *
 * @param       csRclDataInt - RCL command data
 * @param       init - flag, whether to init the buffers
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsRClBufferSetup(csRclCmdData_t csRclDataInt, bool init);

/*******************************************************************************
 * @fn          llCsClearRclBuffers
 *
 * @brief       Clear Rcl command buffers when done
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
void llCsClearRclBuffers();
