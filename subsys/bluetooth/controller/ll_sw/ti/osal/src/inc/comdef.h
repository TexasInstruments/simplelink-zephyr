/******************************************************************************

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2004 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/**
 *  @file       comdef.h
 *  @brief      Common Defines
 */

#ifndef COMDEF_H
#define COMDEF_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef CONFIG_SOC_CC2340R5
#define USE_RCL
#define CC23X0
#define FREERTOS

/* hci_test.opt */
#define Display_DISABLE_ALL
#define ICALL_EVENTS
#define ICALL_JT
#define ICALL_LITE
#define ICALL_MAX_NUM_ENTITIES 6
#define ICALL_MAX_NUM_TASKS 3
#define ICALL_STACK0_ADDR
#define MAX_NUM_BLE_CONNS 8
#define MAX_PDU_SIZE 255
#define NPI_SPI_CONFIG CONFIG_SPI_0
#define xNPI_USE_SPI
#define NPI_USE_UART
#define xPOWER_SAVING
#define STACK_LIBRARY
#define USE_ICALL
#define OSAL_CBTIMER_NUM_TASKS 1
#define xUSE_RCOSC
#define ONE_BLE_LIB_SIZE_OPTIMIZATION
//#define USE_AE

/* build_components.opt*/


#define BROADCASTER_CFG 0x01
#define OBSERVER_CFG 0x02
#define PERIPHERAL_CFG 0x04
#define CENTRAL_CFG 0x08

#define ADV_NCONN_CFG 0x01
#define ADV_CONN_CFG 0x02
#define SCAN_CFG 0x04
#define INIT_CFG 0x08

#define ADV_CFG ADV_NCONN_CFG+ADV_CONN_CFG
#define LINK_CFG ADV_CONN_CFG+INIT_CFG
#define FULL_CFG INIT_CFG+SCAN_CFG+ADV_NCONN_CFG+ADV_CONN_CFG

#define L2CAP_COC_CFG 0x80
#define HOST_V41_MASK 0x80

#define CTRL_V41_MASK 0x7F

#define SCAN_REQ_RPT_CFG 0x02

#define PHY_2MBPS_CFG 0x01
#define PHY_LR_CFG 0x02
#define HDC_NC_ADV_CFG 0x04
#define AE_CFG 0x08
#define PERIODIC_ADV_CFG 0x10
#define AOA_AOD_CFG 0x20
#define CHAN_ALGO2_CFG 0x40

#define EXTENDED_STACK_SETTINGS_DEFAULT 0x00
#define CENTRAL_GUARD_TIME_ENABLE 0x01

/* build_config_src */
#define HCI_TL_FULL
#define CTRL_CONFIG ADV_NCONN_CFG+ADV_CONN_CFG+SCAN_CFG+INIT_CFG
#define BLE_VS_FEATURES SCAN_REQ_RPT_CFG
#define BLE_V50_FEATURES PHY_2MBPS_CFG+PHY_LR_CFG+HDC_NC_ADV_CFG+CHAN_ALGO2_CFG+AE_CFG
#define ICALL_LITE_12_PARAMS
#define NO_OSAL_SNV

#endif

/*********************************************************************
 * INCLUDES
 */

/* HAL */
#include "hal_types.h"
#include "hal_defs.h"
#ifdef CONFIG_SOC_CC2340R5
#include "hal_mcu.h"
#endif
/// @cond NODOC

/*********************************************************************
 * Lint Keywords
 */
#ifdef CC33xx
#define VOID void
#else
#define VOID (void)
#endif // CC33xx

#define NULL_OK
#define INP
#define OUTP
#define ONLY
#define READONLY
#define SHARED
#define KEEP
#define RELAX
#ifndef UNUSED
  #define UNUSED
#endif

/*********************************************************************
 * CONSTANTS
 */

#ifndef false
  #define false 0
#endif

#ifndef true
  #define true 1
#endif

#ifndef CONST
  #define CONST const
#endif

#ifndef GENERIC
  #define GENERIC
#endif

/// @endcond // NODOC

/*** Generic Status Return Values ***/
#define SUCCESS                   0x00 //!< SUCCESS
#ifndef CC33xx
#define FAILURE                   0x01 //!< Failure
#else
#define FAILURE_CC33XX            0x01 //!< Failure CC33xx to avoid redefinition with Osprey
#endif // CC33xx
#define INVALIDPARAMETER          0x02 //!< Invalid Parameter
#define INVALID_TASK              0x03 //!< Invalid Task
#define MSG_BUFFER_NOT_AVAIL      0x04 //!< No HCI Buffer is Available
#define INVALID_MSG_POINTER       0x05 //!< Invalid Message Pointer
#define INVALID_EVENT_ID          0x06 //!< Invalid Event ID
#define INVALID_INTERRUPT_ID      0x07 //!< Invalid Interupt ID
#define NO_TIMER_AVAIL            0x08 //!< No Timer Available
#define NV_ITEM_UNINIT            0x09 //!< NV Item Uninitialized
#define NV_OPER_FAILED            0x0A //!< NV Operation Failed
#define INVALID_MEM_SIZE          0x0B //!< Invalid Memory Size
#define NV_BAD_ITEM_LEN           0x0C //!< NV Bad Item Length

/*********************************************************************
 * TYPEDEFS
 */

/// @cond NODOC

// Generic Status return
typedef uint8 Status_t;

// Data types
typedef int32   int24;
typedef uint32  uint24;

/// @endcond // NODOC

/*********************************************************************
 * Global System Events
 */

#define SYS_EVENT_MSG               0x8000  //!< A message is waiting event

/*********************************************************************
 * Global Generic System Messages
 */

#define KEY_CHANGE                0xC0    //!< Key Events

// OSAL System Message IDs/Events Reserved for applications (user applications)
// 0xE0 - 0xFC

/*********************************************************************
 * MACROS
 */

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

#endif /* COMDEF_H */
