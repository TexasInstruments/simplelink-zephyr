/******************************************************************************

 @file  mac_comdef.h

 @brief Type definitions and macros.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2014 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifndef MACCOMDEF_H
#define MACCOMDEF_H

#ifdef __cplusplus
extern "C"
{
#endif


/*********************************************************************
 * INCLUDES
 */
#include "comdef.h"
#include "saddr.h"


/*********************************************************************
 * CONSTANTS
 */

 /*** Return Values ***/
#define ZSUCCESS                          SUCCESS


#define MAC_NV_EX_LEGACY                   0x0000


// OSAL NV item IDs
#define MAC_NV_EXTADDR                     0x0001

#define MAC_NV_INITIALIZED                 0x0010

#define MAC_NV_COORD_EXTENDED_ADDRESS      0x0021
#define MAC_NV_COORD_SHORT_ADDRESS         0x0022
#define MAC_NV_PAN_ID                      0x0023
#define MAC_NV_RX_ON_WHEN_IDLE             0x0024
#define MAC_NV_SHORT_ADDRESS               0x0025
#define MAC_NV_SECURITYENABLED             0x0025
#define MAC_NV_LOGICALCHANNEL              0x0026
#define MAC_NV_EXTENDED_ADDRESS            0x0027
#define MAC_NV_LOGICAL_CHANNEL             0x0028
#define MAC_NV_ASSOCIATION_PERMIT          0x0029
#define MAC_NV_DEVICE_RECORD               0x002A
#define MAC_NV_DEVICE_RECORD_NUM           0x002B
#define MAC_NV_SRC_MATCH_ENABLE            0x002C

#define MAC_NV_PAN_COORD_SHORT_ADDRESS     0x0030
#define MAC_NV_PAN_COORD_EXTENDED_ADDRESS  0x0031
#define MAC_NV_DEVICE_TABLE_ENTRIES        0x0032
#define MAC_NV_KEY_TABLE_ENTRIES           0x0033

#define MAC_NV_DEVICE_TABLE                0x0040 //50 allocated for the same
#define MAC_NV_KEY_TABLE                   0x0050



/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * FUNCTIONS
 */

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* MACCOMDEF_H */
