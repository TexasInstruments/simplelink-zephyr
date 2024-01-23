/******************************************************************************

 @file  ll_scheduler.h

 @brief This file contains the types, constants, API's etc. for the
        Link Layer (LL) task scheduler.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2009 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*********************************************************************
 *
 * WARNING!!!
 *
 * THE API'S FOUND IN THIS FILE ARE FOR INTERNAL STACK USE ONLY!
 * FUNCTIONS SHOULD NOT BE CALLED DIRECTLY FROM APPLICATIONS, AND ANY
 * CALLS TO THESE FUNCTIONS FROM OUTSIDE OF THE STACK MAY RESULT IN
 * UNEXPECTED BEHAVIOR.
 *
 */

#ifndef LL_SCHEDULER_H
#define LL_SCHEDULER_H

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * INCLUDES
 */
#include "bcomdef.h"

/*******************************************************************************
 * MACROS
 */


/*******************************************************************************
 * CONSTANTS
 */

// Scheduler Constants
#define LL_SCHED_START_IMMED        0
#define LL_SCHED_START_EVENT        1
#define LL_SCHED_START_PRIMARY      2
#define LL_SDAA_SCHED_HANDLED       3
#define LL_SCHED_START_CS           4
//
#define LL_SCHED_START_IMMED_PAD    (3 *  RAT_TICKS_IN_625US)
#define LL_SCHED_PRE_CUTOFF         (10 * RAT_TICKS_IN_625US)
#define LL_SCHED_POST_CUTOFF        (4 * RAT_TICKS_IN_625US)
#define LL_SCHED_OVERHEAD           (LL_SCHED_PRE_CUTOFF + LL_SCHED_POST_CUTOFF)

// Task ID
#define LL_TASK_ID_ADVERTISER                    0x0001
#define LL_TASK_ID_SCANNER                       0x0002
#define LL_TASK_ID_INITIATOR                     0x0004
#define LL_TASK_ID_PERIODIC_ADVERTISER           0x0008
#define LL_TASK_ID_PERIODIC_SCANNER              0x0010
#define LL_TASK_ID_RX_WINDOW                     0x20
#define LL_TASK_ID_PERIPHERAL                    0x0040
#define LL_TASK_ID_CENTRAL                       0x0080
#define LL_TASK_ID_CS                            0x0100
#define LL_TASK_ID_STANDARD_BLE                  0xFFFE
#define LL_TASK_ID_NONE                          0xFFFF

// Task ID Masks
#define LL_TASK_ID_SECONDARY_TASKS               (LL_TASK_ID_ADVERTISER      | \
                                                  LL_TASK_ID_SCANNER         | \
                                                  LL_TASK_ID_INITIATOR       | \
                                                  LL_TASK_ID_PERIODIC_ADVERTISER | \
                                                  LL_TASK_ID_PERIODIC_SCANNER)

// Task State
#define LL_TASK_STATE_INACTIVE                   0
#define LL_TASK_STATE_ACTIVE                     1
#define LL_TASK_STATE_UNDEFINED                  0xFF

// Save State Data
#define ACCESS_ADDR_LEN                          4
#define CRC_INIT_LEN                             3

// The number of slots (i.e. 625us timer ticks) is the amount of time
// each central has to execute including post-processing and scheduling.
// It determines the relative offset of each additional connection
// interval, and thus limits the number of allowed concurrent connections.
// For example, if the value is four, then three connections can fit
// without affecting each other or the minimum connection interval
// (i.e. 7.5ms). But if the value is 12, then the minimum connection
// interval for three connections is 22.5ms. For five is 37.5ms. Etc.
#define NUM_SLOTS_PER_CENTRAL                    8

// The number of task blocks needed depends on the build configuration. The
// Non-connectable Adv and Scan would each require a task block if supported.
// If a Peripheral connection is supported, then a task is needed for the connectable
// Adv. This task is then reused when a Peripheral connection is formed. If a Central
// connection is supported, then a task is needed for Init. This task is then
// reused when a Central connection is formed. Thus, the total number of needed
// tasks is at most two plus the number of connections.
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_NCONN_CFG)
#define NUM_TASK_BLOCKS_ADV_NCONN_CFG            2 //Adv + Periodic Adv
#else
#define NUM_TASK_BLOCKS_ADV_NCONN_CFG            0
#endif // CTRL_CONFIG=ADV_NCONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
#define NUM_TASK_BLOCKS_SCAN_CFG                 2 //scanner + periodic scanner
#else
#define NUM_TASK_BLOCKS_SCAN_CFG                 0
#endif // CTRL_CONFIG=SCAN_CFG

// collect the total number of non-connection related task blocks based config
// Note: The number of connections is defined in the llConfigTable.
#define LL_NUM_TASK_BLOCKS                    (NUM_TASK_BLOCKS_ADV_NCONN_CFG + \
                                               NUM_TASK_BLOCKS_SCAN_CFG + 1)

#define HANDLE_INACTIVE    0 // A handle that was not opened or is closed
#define HANDLE_ACTIVE      1 // A handle that is opened

#define NUM_RCL_HANDLES    2 // A Standard BLE handle and a CS handle
#define BLE_RCL_HANDLE     0 // The Standard BLE handle index
#define CS_RCL_HANDLE      1 // The CS handle index

/*******************************************************************************
 * TYPEDEFS
 */

// Task Information Data Type
// Note: Needed for forward reference to taskInfo_t from taskInfo_t.
typedef struct taskInfo_t taskInfo_t;

// Task Information
struct taskInfo_t
{
  uint16 taskID;                 // type of LL task
  uint8  taskState;              // whether task is active or inactive
  uint32 command;                // address to radio operation
  uint32 startTime;              // initial RF command start time; used by PM
  uint32 anchorPoint;            // anchor point
  uint32 lastStartTime;          // previous start time
  void   (*setup)(taskInfo_t *); // function used for specific setup operations
  uint32 rfEvents;               // RF events
};

// Task List
typedef struct
{
  uint8       numTasks;          // number of active tasks
  uint16      activeTasks;       // bits to indicate which tasks are active
  uint16      lastSecTask;       // the last secondary task; used for combo states
  uint16      lastActiveTasks;   // the active tasks checked last time
  taskInfo_t *curTask;           // currently active task
  taskInfo_t *llTasks;           // array of tasks
} taskList_t;

// RCL Handle List
typedef struct rclHandleList
{
  RCL_Handle rclHandle;
  uint8      state:1;
  uint8      reserved:7;
} rclHandleList_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * Functions
 */

extern taskList_t  llTaskList;
extern taskInfo_t *pRXWindowTask;
//
extern void        llSchedulerInit( void );
extern void        llScheduler( void );
extern void        llScheduleTask( taskInfo_t *llTask );
extern void        llScheduleTask_sPatch( taskInfo_t *llTask );
extern uint8       llFindStartType( taskInfo_t *secTask, taskInfo_t *primTask );
extern taskInfo_t *llFindNextSecTask( uint16 secTaskID );
extern taskInfo_t *llAllocTask( uint16 llTaskID );
extern void        llFreeTask( taskInfo_t **llTask );
extern taskInfo_t *llGetCurrentTask( void );
extern RCL_Command *llSchedulerGetCsCmd( void );
extern taskInfo_t *llGetTask( uint16 llTaskID );
extern uint8       llGetTaskState( uint16 llTaskID );
extern uint8       llActiveTask( uint16 llTaskID );
extern uint8       llGetActiveTasks( void );
extern uint8       llGetNumTasks( void );
extern void        llSetupRatCompare( taskInfo_t *llTask );
extern void        llClearRatCompare( void );
//
extern void        llSetupAdv( void );
extern void        llSetupDirectedAdvEvt( void );
extern void        llSetupUndirectedAdvEvt( void );
extern void        llSetupNonConnectableAdvEvt( void );
extern void        llSetupDiscoverableAdvEvt( void );
extern void        llSetupScan( void );
extern void        llSetupInit( uint8 );
extern void        llSetupConn( uint8 );
//
extern void        llExtAdvSchedSetup( taskInfo_t *llTask );
extern void        llExtScanSchedSetup( taskInfo_t *llTask );
extern void        llExtInitSchedSetup( taskInfo_t *llTask );
extern void        llLinkSchedSetup( taskInfo_t *llTask );
extern void        llPeriodicScanSchedSetup( taskInfo_t *llTask );
extern void        llPeriodicAdvSchedSetup( taskInfo_t *llTask );
extern RCL_Handle  llScheduler_getBleHandle( void );
extern RCL_Handle  llScheduler_getHandle( uint16 taskID );
extern uint32      llScheduler_getSwitchTime(uint16 taskID);

#ifdef __cplusplus
}
#endif

#endif /* LL_SCHEDULER_H */
