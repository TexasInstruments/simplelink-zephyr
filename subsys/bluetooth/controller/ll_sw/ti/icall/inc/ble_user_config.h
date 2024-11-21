/******************************************************************************
 @file:       ble_user_config.h

 @brief:    This file contains user configurable variables for the BLE
            Controller and Host. This file is also used for defining the
            type of RF Front End used with the TI device, such as using
            a Differential front end with External Bias, etc. Please see
            below for more detail.

            To change the default values of configurable variables:
              - Include the followings in your application main.c file:
                #ifndef USE_DEFAULT_USER_CFG

                #include "ble_user_config.h"

                // BLE user defined configuration
                bleUserCfg_t user0Cfg = BLE_USER_CFG;

                #endif // USE_DEFAULT_USER_CFG
              - Set the preprocessor symbol MAX_NUM_BLE_CONNS, MAX_NUM_PDU,
                MAX_PDU_SIZE, L2CAP_NUM_PSM or L2CAP_NUM_CO_CHANNELS
                to a desired value in your application project.
              - Include "ble_user_config.h" in your stack OSAL_ICallBle.c
                file.
              - Call setBleUserConfig at the start of stack_main. Actually,
                it is okay to set the variables anywhere in stack_main as
                long as it is BEFORE osal_init_system, but best to set at
                the very start of stack_main.

            Note: User configurable variables are only used during the
                  initialization of the Controller and Host. Changing
                  the values of these variables after this will have no
                  effect.

            Note: To use the default user configurable variables, define
                  the preprocessor symbol USE_DEFAULT_USER_CFG in your
                  application project.

            For example:
              - In your application main.c, include:
                #ifndef USE_DEFAULT_USER_CFG

                #include "bleUserConfig.h"

                // BLE user defined configuration
                bleUserCfg_t user0Cfg = BLE_USER_CFG;
                #endif // USE_DEFAULT_USER_CFG
              - In your application project, set the preprocessor symbol
                MAX_NUM_BLE_CONNS to 1 to change the maximum number of BLE
                connections to 1 from the default value of 3.
              - In your stack OSAL_ICallBle.c file, call setBleUserCfg to
                update the user configuration variables:
                #include "bleUserConfig.h"
                :
                int stack_main(void *arg)
                {
                  setBleUserConfig((bleUserCfg_t *)arg);
                  :
                }

            Default values:
              maxNumConns       : 1
              maxNumPDUs        : 5
              maxPduSize        : 27 (or 69 if Secure Connection enabled)
              maxNumPSM         : 3
              maxNumCoChannels  : 3
              maxAcceptListElems: 16
              maxResolvListElems: 10

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2014 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef BLE_USER_CONFIG_H
#define BLE_USER_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * INCLUDES
 */

#include "icall_user_config.h"

#include "ble_dispatch.h"
#include "hal_assert.h"
#include "bcomdef.h"

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

// Defines only required by Application.
// Note: ICALL_STACK0_ADDR is assigned the entry point of the stack image and
//       is only defined for the Application project
#if defined(ICALL_STACK0_ADDR)

#ifndef CONFIG_ZEPHYR
#include <ti_drivers_config.h>
#endif // CONFIG_ZEPHYR

// RF Front End Settings
// Note: The use of these values completely depends on how the PCB is laid out.
//       Please see Device Package and Evaluation Module (EM) Board below.
#define RF_FE_DIFFERENTIAL              0
#define RF_FE_SINGLE_ENDED_RFP          1
#define RF_FE_SINGLE_ENDED_RFN          2
#define RF_FE_ANT_DIVERSITY_RFP_FIRST   3
#define RF_FE_ANT_DIVERSITY_RFN_FIRST   4
#define RF_FE_SINGLE_ENDED_RFP_EXT_PINS 5
#define RF_FE_SINGLE_ENDED_RFN_EXT_PINS 6
//
#define RF_FE_INT_BIAS                  (0<<3)
#define RF_FE_EXT_BIAS                  (1<<3)

// This is a common file for the legacy and sysconfig examples,
// the parameters under ifndef SYSCFG are defined in this file for
// the legacy examples and generated using the sysconfig tool for
// the sysconfig examples
#ifndef SYSCFG
// Maximum number of BLE connections. It should be set based on the
// device GAP role. Here're some recommended values:
//      * Central:     3
//      * Peripheral:  1
//      * Observer:    0
//      * Broadcaster: 0
// Note: When the GAP role includes Peripheral and no v4.1 Controller features
//       are configured, MAX_NUM_BLE_CONNS must not be greater than 1
#ifndef MAX_NUM_BLE_CONNS
  #define MAX_NUM_BLE_CONNS             1
#endif

/************************************/

// Specifies whether LFOSC (RCOSC) was configured in CCFG module by the user
#define SRC_CLK_IS_LFOSC                0

// User configurable extra PPM for peripheral RX window widening
// This only applies if SRC_CLK_IS_LFOSC is set to 1, default value will be 1500PPM
#define USER_CFG_LFOSC_EXTRA_PPM        1500

// bitmask of extended stack settings
#ifndef EXTENDED_STACK_SETTINGS
#define EXTENDED_STACK_SETTINGS         0x00
#endif

// bitmask which enables the offset of the overrides for BAW
#define CC2652RB_OVERRIDE_USED          0x02

// Maximum number of BLE HCI PDUs. If the maximum number connections (above)
// is set to 0 then this number should also be set to 0.
#ifndef MAX_NUM_PDU
  #define MAX_NUM_PDU                   5
#endif

// Maximum size in bytes of the BLE HCI PDU. Valid range: 27 to 255
// The maximum ATT_MTU is MAX_PDU_SIZE - 4.
#ifndef MAX_PDU_SIZE
  #define MAX_PDU_SIZE                  69
#endif
#endif //SYSCFG

// Maximum number of L2CAP Protocol/Service Multiplexers (PSM)
#ifndef L2CAP_NUM_PSM
  #define L2CAP_NUM_PSM                 3
#endif

// Maximum number of L2CAP Connection Oriented Channels
#ifndef L2CAP_NUM_CO_CHANNELS
  #define L2CAP_NUM_CO_CHANNELS         8
#endif

// Use dynamic filter list when the device role is advertiser only and number of bond is greater than 5
#ifndef USE_DFL
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG)) && !(CTRL_CONFIG & (SCAN_CFG | INIT_CFG)) // (If the device role is advertiser only)
#if defined(GAP_BOND_MGR) && (GAP_BONDINGS_MAX > 5) // If number of bondings greater than 5
  #define USE_DFL
#endif // (advertiser only)
#endif // (number of bondings greater than 5)
#endif // !USE_DFL

#ifndef MAX_NUM_AL_ENTRIES // MAX_NUM_AL_ENTRIES
#if defined(USE_DFL) && defined(GAP_BOND_MGR) // (Radio core using dynamic filter list)
#define MAX_NUM_AL_ENTRIES             GAP_BONDINGS_MAX
#else // !(Radio core using dynamic filter list)
#define MAX_NUM_AL_ENTRIES             5
#endif // (Radio core using dynamic filter list)
#endif// MAX_NUM_AL_ENTRIES

#ifndef MAX_NUM_RL_ENTRIES
#define MAX_NUM_RL_ENTRIES              10  // at 60 bytes per RL entry
#endif // (MAX_NUM_RL_ENTRIES undefined)

// Redefine MAX_NUM_AL_ENTRIES and MAX_NUM_RL_ENTRIES to GAP_BONDINGS_MAX
// when using dynamic filter list.
#ifdef USE_DFL // (Radio core using dynamic filter list)
#ifdef MAX_NUM_AL_ENTRIES
#undef MAX_NUM_AL_ENTRIES
#endif // MAX_NUM_AL_ENTRIES
#ifdef MAX_NUM_RL_ENTRIES
#undef MAX_NUM_RL_ENTRIES
#endif // MAX_NUM_RL_ENTRIES
#define MAX_NUM_AL_ENTRIES              GAP_BONDINGS_MAX  // at 8 bytes per AL entry
#define MAX_NUM_RL_ENTRIES              GAP_BONDINGS_MAX  // at 60 bytes per RL entry
#endif // (Radio core using dynamic filter list)

#ifndef CFG_MAX_NUM_RL_ENTRIES
#ifdef GAP_BOND_MGR
#define CFG_MAX_NUM_RL_ENTRIES             GAP_BONDINGS_MAX // at 8 bytes per AL entry
#else
#define CFG_MAX_NUM_RL_ENTRIES             10  // at 8 bytes per AL entry
#endif
#endif

#ifndef ADV_RPT_INC_CHANNEL
#define ADV_RPT_INC_CHANNEL            0
#endif

//
// Device Package and Evaluation Module (EM) Board
//

#ifndef PM_STARTUP_MARGIN
  #define PM_STARTUP_MARGIN             300
#endif

#define BLE_USER_CFG                    { &bleStackConfig,         \
                                          &bleAppServiceInfoTable }

// Make sure there's enough heap needed for BLE connection Tx buffers, which
// is based on MAX_PDU_SIZE and MAX_NUM_PDU configured by the application.
// The heap memory needed for BLE connection Tx buffers should not be more
// that 1/3 of the total ICall heap size (HEAPMGR_SIZE).
//
//  Notes: Over the Air (OTA) PDU Size = 27, and LL Header Size = 14
//         If HEAPMGR_SIZE = 0 then auto-size heap is being used
//
#if (MAX_NUM_BLE_CONNS > 0) && !defined(NO_HEAPSIZE_VALIDATE) && (HEAPMGR_SIZE != 0)
  #if  (((((MAX_PDU_SIZE / 27) + 1) * MAX_NUM_PDU) * (27 + 14)) > (HEAPMGR_SIZE / 3))
    #warning Not enough heap for configured MAX_NUM_PDU and MAX_PDU_SIZE! Adjust HEAPMGR_SIZE.
  #endif
#endif

#endif // !(CTRL_CONFIG | HOST_CONFIG)

/*******************************************************************************
 * TYPEDEFS
 */
typedef void (*pfnFastStateUpdate_t)(uint32_t stackType, uint32_t stackState);

typedef struct
{
  uint8_t                     maxNumConns;
  uint8_t                     maxNumPDUs;
  uint8_t                     maxPduSize;
  uint8_t                     maxNumPSM;
  uint8_t                     maxNumCoChannels;
  uint8_t                     maxAcceptListElems;
  uint8_t                     maxResolvListElems;
  pfnBMAlloc_t                *pfnBMAlloc;
  pfnBMFree_t                 *pfnBMFree;
  ECCParams_CurveParams       *eccParams;
  pfnFastStateUpdate_t        fastStateUpdateCb;
  uint32_t                    bleStackType;
  uint32_t                    extStackSettings; // | reserved | use CC2652RB | MasterGuard |
                                                // |   31..2  |       1      |      0      |
  uint8                       advReportIncChannel;
  uint8                       useDFL;
} stackSpecific_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */
// The Tx Power value
extern int8 defaultTxPowerDbm;

extern pfnBMAlloc_t pfnBMAlloc;
extern pfnBMFree_t  pfnBMFree;

extern uint16_t bleUserCfg_maxPduSize;
extern uint16_t llUserConfig_maxPduSize;

extern uint8  userCfgClockType;
extern uint16 userCfgAdditionalPPM;

extern const stackSpecific_t       bleStackConfig;
extern applicationService_t        bleAppServiceInfoTable;

/*********************************************************************
 * FUNCTIONS
 */
extern void setBleUserConfig(icall_userCfg_t *userCfg);
extern void RegisterAssertCback(assertCback_t appAssertHandler);
extern void DefaultAssertCback(uint8 assertCause, uint8 assertSubCause);
extern assertCback_t appAssertCback; // only App's ble_user_config.c
extern assertCback_t halAssertCback; // only Stack's ble_user_config.c

#ifdef __cplusplus
}
#endif

#endif /* BLE_USER_CONFIG_H */
