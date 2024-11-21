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
 * The selected start time depends on the CS Role and whether the
 * SubeventCount is 0 or more.
 *
 * input parameters
 *
 * @param       connId - Connection Id
 * @param       role - CS Role, initiator or reflector
 * @param       subeventCount - CS subevent counter
 * @param       rclCmd - CS RCL command
 *
 * output parameters
 *
 * @param       None.
 *
 * @return     cmdStartTime
 */
uint32_t llCsSetupCmdStartTime( uint16 connId, uint8 role, uint8 subEventCount,
                                RCL_CmdBleCs rclCmd );

/*******************************************************************************
 * @fn          llCsGetRxWidening
 *
 * @brief       Get RCL Rx Window Widening
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Window Widening
 *                  * 0 (wait forever) if Test Mode is enabled
 *                  * 250us otherwise
 */
uint16_t llCsGetRxWidening(void);

/*******************************************************************************
 * @fn          llCsRclScheduleNextSubevent
 *
 * @brief       Schedule Next Subevent
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
void llCsRclScheduleNextSubevent(void);

/*******************************************************************************
 * @fn          llCsRclGetTxPower
 *
 * @brief       Get Tx Power for the RCL command
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      txPower
 */
RCL_Command_TxPower llCsRclGetTxPower(int8 maxTxPower);

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
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsRClBufferSetup(csRclCmdData_t csRclDataInt);

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
void llCsClearRclBuffers( void );
