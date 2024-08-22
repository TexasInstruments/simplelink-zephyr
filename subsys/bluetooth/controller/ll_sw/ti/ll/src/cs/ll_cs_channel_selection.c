/******************************************************************************

 @file  ll_cs_channel_selection.c

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
#include "cs/ll_cs_db.h"
#include "cs/ll_cs_channel_selection.h"
#include "cs/ll_cs_common.h"
#include "cs/ll_cs_sec.h"
#include "cs/ll_cs_logs.h"

#include "rom_jt.h"

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
* Public function defined in ll_cs_channel_selection.h.
*/
csStatus_e llCsChSel3aAnd3b(uint8* pShuffledChannelArray, uint8* pFilteredArr,
                          uint8 nChannels, csTransactionId_e trId)
{
    // vaidate the channel array and bit map array is intialized and
    // there is at least 15 channels
    if ((pShuffledChannelArray == NULL) || (nChannels < CS_MIN_NUM_OF_CHN))
    {
        return CS_STATUS_INVALID_CHM;
    }

    // Shuffle the array and return pointer to the shuffled array
    cr1(pShuffledChannelArray,pFilteredArr, nChannels, trId);

    llCsLogChannelIndexArray(pShuffledChannelArray, nChannels);

    return CS_STATUS_SUCCESS;
}
