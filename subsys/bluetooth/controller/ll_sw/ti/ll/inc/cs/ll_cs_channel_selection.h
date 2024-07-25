/******************************************************************************

 @file  ll_cs_channel_selection.h

 @brief CS channel selection. Handels all algorithem of the channel selection
        and supported functions.

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
 * LOCAL FUNCTION DECLARATIONS
 */

/*******************************************************************************
 * @fn          llCsChSel3aAnd3b
 *
 * @brief       Channel selection algorithem explained in CS spec
 * section 3.12.35: Channel selection algorithm #3a for mode-0 steps Channel
 * selection algorithm #3b for non-mode-0 steps The fucntion is used to generate
 * a randomized channel map with uniform distribution
 *
 * @design      BLE_LOKI-506
 *
 * input parameters
 *
 * @param       pShuffledChannelArray  - shuffled channel array
 * @param       pFilteredArr           - Filtered (non shuffled channel array)
 * @param       nChannels              - num channels (array size)
 * @param       trId                   - CS transaction ID.
 * output parameters
 *
 * @param       pShuffledChannelArray  - Pointer to array of channels
 *                                       that had been shuffled with
 *                                       algorithem 3a or 3b
 *
 * @return      status
 *              CS_STATUS_INVALID_CHM  - not enough valid channels
 *                                       in pBitMapArrayOfChannels
 *              CS_STATUS_SUCCESS
 */
csStatus_e llCsChSel3aAnd3b(uint8* pShuffledChannelArray, uint8* pFilteredArr,
                          uint8 nChannels, csTransactionId_e trId);
