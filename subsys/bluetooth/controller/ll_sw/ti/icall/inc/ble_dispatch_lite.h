/******************************************************************************

 @file  ble_dispatch_lite.h

 @brief ICall BLE Dispatcher LITE Definitions.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2016 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifndef BLE_DISPATCH_LITE_H
#define BLE_DISPATCH_LITE_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "icall_ble_apimsg.h"
#ifndef CONTROLLER_ONLY
#include "sm.h"
#include "gap.h"
#include "gatt.h"
#include "gatt_uuid.h"
#include "gattservapp.h"
#include "gapbondmgr.h"
#endif // CONTROLLER_ONLY

/*********************************************************************
 * CONSTANTS
 */
extern const uint32_t bleAPItable[];

/*******************************************************************************
 * TYPEDEFS
 */


/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * FUNCTIONS
 */

/*
 * Task Initialization for the task
 */
extern void ble_dispatch_liteInit(uint8_t taskId);

/*
 * Task Event Processor for the task
 */
extern uint32 ble_dispatch_liteProcess(uint8_t taskId, uint32 events);

/*
 * BLE direct API message parser
 */
extern void icall_liteMsgParser(void * msg);

/*
 * Error handler
 */
extern void icall_liteErrorFunction(void);

/*
 * Return revision of the stack
*/
extern uint8 buildRevision(ICall_BuildRevision *pBuildRev);


/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* !BLE_DISPATCH_LITE_H */
