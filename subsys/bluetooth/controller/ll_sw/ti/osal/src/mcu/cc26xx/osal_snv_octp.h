/******************************************************************************

 @file  osal_snv_octp.h

 @brief This module defines the OSAL simple non-volatile memory functions.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2015 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifndef OSAL_SNV_OCTP_H
#define OSAL_SNV_OCTP_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */

#include "hal_types.h"
#include "nvintf.h"

/*********************************************************************
 * CONSTANTS
 */
#define OSAL_NV_MIN_COMPACT_THRESHOLD   70 // Minimum compaction threshold
#define OSAL_NV_MAX_COMPACT_THRESHOLD   95 // Maximum compaction threshold

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * FUNCTIONS
 */

/*********************************************************************
 * @fn      osal_snv_register
 *
 * @brief   Set up function pointers to NV services.
 *
 * @return  NV_INTF_SUCCESS if registration succeeds. NV_INTF_FAIL, otherwise.
 */
extern uint8_t osal_snv_register( NVINTF_nvFuncts_t *pFptrs, uint8_t systemId );

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* OSAL_SNV_OCTP.H */
