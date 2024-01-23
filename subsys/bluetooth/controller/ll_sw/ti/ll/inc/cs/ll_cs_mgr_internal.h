/******************************************************************************

 @file  ll_cs_mgr_internal.h

 @brief This file includes the internal functions used by the module ll_Cs_mgr

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
#include "bcomdef.h"
#include "ll_common.h"
#include "cs/ll_cs_common.h"

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
 * @fn          llCsSelectTimeConfig
 *
 * @brief       Select the best time for CS Config
 *              This function will modify the fields tIp1, tIp2, tFcs, tPm
 *              in the recieved pointer struct.
 *
 * input parameters
 *
 * @param       connId - connection Id
 * @param       pConfig - pointer to CS config struct
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsSelectTimeConfig(uint16 connId, csConfigurationSet_t* pConfig);

/*******************************************************************************
 * @fn          llCsGetBestTime
 *
 * @brief       Choose the best CS timings config.
 *              This function chooses TIP1,2, T_FCS etc
 *
 * input parameters
 *
 * @param       timeCapability - timing capabilties
 * @param       type - timing type (TIP1,2, T_FCS, etc...)
 *
 * output parameters
 *
 * @param       None
 *
 * @return      Best Time Config
 */
uint8 llCsGetBestTime(uint8 timeCapability, uint8 type);