/******************************************************************************

 @file  ll_scheduler.c

 @brief This file contains the Link Layer (LL) Task Scheduler routines routines
        routines.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2009 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*******************************************************************************
 * INCLUDES
 */
#include "ll_common.h"
#include "ll_scheduler.h"
#include "hal_mcu.h"
#include "ll_privacy.h"
#include "ll_rat.h"
#include "ll_ae.h"
#ifdef BLE_HEALTH
#include <health_toolkit/inc/debugInfo_errno.h>
#endif //BLE_HEALTH
#include <ti/drivers/rcl/RCL.h>
//
#include "rom_jt.h"

// SW Tracer
#ifdef DEBUG_SW_TRACE
#define DBG_ENABLE
#include "dbgid_sys_mst.h"
#endif // DEBUG_SW_TRACE
#include "cs/ll_cs_rcl.h"

/*******************************************************************************
 * MACROS
 */

// Returns the state of a handle whether it is open or not
#define IS_HANDLE_ACTIVE(handleType) rclHandles[handleType].state

/*******************************************************************************
 * CONSTANTS
 */

#define LL_SCHED_START_UNDEF        0xFF

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * EXTERNS
 */

extern void LL_rclRescheduleCommand(RCL_Command *cmd);

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

// BLE Tasks
taskList_t llTaskList;
// sdaa task
taskInfo_t *pRXWindowTask = NULL;

// pointer to next AE set to be scheduled
extern sortedAdv_t *pNextAdvSet;

// number of enabled adv sets
extern uint8 numActiveAdvSets;

// handle to radio driver for BLE client
extern RCL_Handle    rfHandle;
extern RCL_Client    rfClient;

// rcl handles list {0: standard BLE, 1: CS }
rclHandleList_t rclHandles[NUM_RCL_HANDLES] = {0};


/*******************************************************************************
 * Internal Functions
 */
void llScheduler_rclClose(uint8 handleType);

/*******************************************************************************
 * Functions
 */

/*******************************************************************************
 * @fn          llSchedulerInit
 *
 * @brief       This function is used to initialize the LL Task Scheduler.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSchedulerInit( void )
{
  uint8 i;

  // clear all tasks
  for (i=0; i<(maxNumConns+LL_NUM_TASK_BLOCKS); i++)
  {
    llTaskList.llTasks[i].taskState     = LL_TASK_STATE_INACTIVE;
    llTaskList.llTasks[i].taskID        = LL_TASK_ID_NONE;
    llTaskList.llTasks[i].setup         = NULL;
    llTaskList.llTasks[i].rfEvents      = 0;
    llTaskList.llTasks[i].command       = 0;
    llTaskList.llTasks[i].startTime     = 0;
    llTaskList.llTasks[i].anchorPoint   = 0;
    llTaskList.llTasks[i].lastStartTime = 0;
  }

  // clear last secondary task ID
  llTaskList.lastSecTask = LL_TASK_ID_NONE;

  // clear active task list
  llTaskList.activeTasks = llTaskList.lastActiveTasks = 0;

  // clear the task count
  llTaskList.numTasks = 0;

  // clear the current task
  llTaskList.curTask = NULL;

  return;
}

/*******************************************************************************
 * @fn          llScheduler
 *
 * @brief       This function is the BLE Link Layer task scheduler. It
 *              is used to decide which task starts next.
 *
 *              Assumptions:
 *              If the current task is not NULL when the scheduler is called,
 *              then there must be at least one active task in the system.
 *              However, the active task(s) need not be the current task, as
 *              it may have ended. For example, if Peripheral+Adv, and we have a
 *              disconnection, then when the scheduler is called, the current
 *              task is Peripheral, but it is no longer active.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llScheduler( void )
{
  uint8      startTypeSDAA;
  taskInfo_t *curTask = llTaskList.curTask;

  // check if there's nothing left to do
  if ( curTask == NULL )
  {
    // check if any post-radio operations were scheduled, but somehow missed
    // Note: This really should not happen, but doesn't cost much to ensure
    //       a schedule post-RF operation isn't missed.
    MAP_llProcessPostRfOps();

    return;
  }

  // base scheduling on the current task that just finished
  switch( curTask->taskID )
  {
    // generic handling of secondary tasks
    case LL_TASK_ID_ADVERTISER:
    case LL_TASK_ID_SCANNER:
    case LL_TASK_ID_INITIATOR:
    case LL_TASK_ID_PERIODIC_ADVERTISER:
    case LL_TASK_ID_PERIODIC_SCANNER:
    case LL_TASK_ID_RX_WINDOW:
    {
      // Sanity Check:
      // Fatal error - Not possible to have no active tasks at this point.
      if ( MAP_llGetNumTasks() == 0 )
      {
        LL_ASSERT( FALSE );
        return;
      }
      if ( MAP_llActiveTask(LL_TASK_ID_ADVERTISER) )
      {
        // select the next advertising set to be scheduled
        // will be use in case the advertising task will be selected
        MAP_llFindNextSecCmd(llGetTask( LL_TASK_ID_ADVERTISER ));
      }
#ifdef USE_PERIODIC_ADV
      if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_ADVERTISER) )
      {
        // select the next periodic advertising set to be scheduled
        // will be use in case the periodic advertising task will be selected
        MAP_llFindNextSecCmd(llGetTask( LL_TASK_ID_PERIODIC_ADVERTISER ));
      }
#endif
#ifdef USE_PERIODIC_SCAN
      if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_SCANNER) )
      {
        // select the next periodic scan set to be scheduled
        // will be use in case the periodic scan task will be selected
        MAP_llFindNextSecCmd(llGetTask( LL_TASK_ID_PERIODIC_SCANNER ));
      }
#endif
      // find next secondary task, if any
      // Note: If there are no active secondary tasks, then NULL is
      //       returned. It is assumed here that in this case, there
      //       has to be at least one active connection since curTask
      //       is not NULL and llGetNumTasks is not zero!
      taskInfo_t *nextSecTask = MAP_llFindNextSecTask( curTask->taskID );
      // ALT? ble5OpCmd_t *nextSecCmd  = (ble5OpCmd_t *)(((rfOpCmd_NOP_t *)(nextSecTask->command))->nopCmd.pNextRfOp);
      void *nextSecCmd = NULL;
      if (nextSecTask != NULL)
      {
        if ((nextSecTask->taskID == LL_TASK_ID_ADVERTISER) ||
            (nextSecTask->taskID == LL_TASK_ID_PERIODIC_ADVERTISER) ||
            (nextSecTask->taskID == LL_TASK_ID_PERIODIC_SCANNER))
        {
          nextSecCmd = (void *)nextSecTask->command;
        }
        else
        {
          nextSecCmd = MAP_llFindNextSecCmd( nextSecTask );
        }
      }
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (INIT_CFG | ADV_CONN_CFG))
      // check if there are any active connections (Central or Peripheral)
      if ( llConns.numActiveConns != 0 )
      {
        uint8          startType = LL_SCHED_START_PRIMARY;
        llConnState_t *nextConnPtr  = MAP_llDataGetConnPtr( MAP_llGetNextConn() );
        taskInfo_t    *nextConnTask = nextConnPtr->llTask;
        void          *nextConnCmd  = ((void *)nextConnTask->command);
        taskInfo_t    *csTask       = MAP_llGetTask(LL_TASK_ID_CS);
        RCL_Command    *csCmd = (RCL_Command *)llSchedulerGetCsCmd();

        // Set the next connection variable
        llConns.nextConn = nextConnPtr->connId;

        // check if there is a command
        if ( nextSecCmd == NULL )
        {
          if ( csCmd != NULL )
          {
            uint32 connMinTime = RAT_TICKS_IN_500US +
                                 MAP_llScheduler_getSwitchTime(nextConnPtr->taskID) +
                                 LL_MARGIN_TIME_FOR_TIMER_HANDLING_RAT_TICKS;

            // If the CS command is active and the secondary isn't the decision should
            // be done between the CS and the connection
            if ( csCmd != NULL )
            {
              // If the connection is the CS connection and it scheduled before
              // the CS commands schedule the connection else the CS command
              if( (MAP_llTimeCompare(((RCL_Command *)nextConnCmd)->timing.absStartTime, csCmd->timing.absStartTime) == FALSE) &&
                  (MAP_llTimeDelta(((RCL_Command *)nextConnCmd)->timing.absStartTime, csCmd->timing.absStartTime) > connMinTime) )
              {
                startType = LL_SCHED_START_PRIMARY;
              }
              else
              {
                startType = LL_SCHED_START_CS;
              }
            }
          }
        }
        else
        {
          // check if there's enough time for secondary task (if any)
          // Note: The next secondary task pointer may be NULL, in which case
          //       a primary start type is returned.
          startType = MAP_llFindStartType( nextSecTask, nextConnTask );
        }

        // This function return LL_SDAA_SCHED_HANDLED when RX window is scheduled,
                // therefore llScheduler() will finish here. The function will return the
                // new start type of next task which can be change when the next channel
                // is blocked or there isn't sufficient time to schedule RX window for
                // overloaded channel.
                // This function can split the TX queue of connection if there
                // is no time for RX window. In this case the next task type will be
                // priary task.
                // This function may do nothing when RX window isn't neccesary or sdaa
                // module is disable. In this case the function return start type task
                // equal to the start type that inserted as input.
                startTypeSDAA = MAP_llHandleSDAAControlTX(nextConnPtr, nextSecTask, startType);
                if(startTypeSDAA != startType)
                {
                    if (startTypeSDAA == LL_SDAA_SCHED_HANDLED)
                    {
                        return;
                    }
                    startType = startTypeSDAA;
                }

        // check if the secondary task can start
        if ( (startType != LL_SCHED_START_PRIMARY) && (startType != LL_SCHED_START_CS) )
        {
          if ( nextSecTask == NULL )
          {
             // Sanity Check:
             // Fatal Error: Next Secondary task must be valid!
             LL_ASSERT( FALSE );
             return;
          }

          // what we do and how we do it depends on secondary task
          switch( nextSecTask->taskID )
          {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
            case LL_TASK_ID_ADVERTISER:
              MAP_llSetTaskAdv(startType,nextSecCmd);
              break;

#ifdef USE_PERIODIC_ADV
            case LL_TASK_ID_PERIODIC_ADVERTISER:
              MAP_llSetTaskPeriodicAdv();
              break;
#endif //USE_PERIODIC_ADV
#endif // ADV_NCONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
            case LL_TASK_ID_SCANNER:
              MAP_llSetTaskScan(startType,nextSecTask,nextSecCmd,nextConnCmd);
              break;

#ifdef USE_PERIODIC_SCAN
            case LL_TASK_ID_PERIODIC_SCANNER:
              MAP_llSetTaskPeriodicScan();
              break;
#endif //USE_PERIODIC_SCAN
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
            case LL_TASK_ID_INITIATOR:
              MAP_llSetTaskInit(startType,nextSecTask,nextSecCmd,nextConnCmd);
              break;
#endif // INIT_CFG

            default:
              // Sanity Check:
              // Fatal Error: Next Secondary task must be valid!
              LL_ASSERT( FALSE );

              break;
          }  // switch on next secondary task ID
        }
        else if (startType == LL_SCHED_START_PRIMARY)// start type is PRIMARY (a connection)
        {
          // which LL state based on role
          if ( nextConnTask->taskID == LL_TASK_ID_CENTRAL )
          {
            MAP_llSetTaskCentral(nextConnPtr->connId, nextConnCmd);
          }
          else // assume taskId == LL_TASK_ID_PERIPHERAL
          {
            if (MAP_llCheckPeripheralTerminate(nextConnPtr->connId) == TRUE)
            {
              return;
            }
            MAP_llSetTaskPeripheral(nextConnPtr->connId, nextConnCmd);
          }

          // start connection
          // Note: Sets curTask.
          MAP_llScheduleTask( nextConnTask );
        }  // if start type is not PRIMARY
        else // The start type is CS
        {
          uint16 csConnTaskID = llGetCsConnTaskID();
          if ( csConnTaskID == LL_TASK_ID_CENTRAL )
          {
            llState = LL_STATE_CONN_CENTRAL;
          }
          else
          {
            llState = LL_STATE_CONN_PERIPHERAL;
          }
        }
      }
      else // there are no active connections
#endif  // INIT_CFG | ADV_CONN_CFG
      {
        if ( nextSecTask == NULL)
        {
          // Sanity Check:
          // Next secondary task should not be NULL.
          LL_ASSERT( nextSecTask != NULL );
          return;
        }

        // This function return LL_SDAA_SCHED_HANDLED when RX window is scheduled,
        // therefore llScheduler() will finish here. The function will return the
		// new start type of next task which can be change when the next channel
		// is blocked or there isn't sufficient time to schedule RX window for
		// overloaded channel.
		// This function can split the TX queue of connection if there
		// is no time for RX window. In this case the next task type will be
		// priary task.
		// This function may do nothing when RX window isn't neccesary or sdaa
		// module is disable. In this case the function return start type task
		// equal to the start type that inserted as input.
		startTypeSDAA = MAP_llHandleSDAAControlTX(NULL, nextSecTask, LL_SCHED_START_EVENT);
		if (startTypeSDAA == LL_SDAA_SCHED_HANDLED)
		{
			return;
		}

        // check if the next secondary task is the current task
        if ( curTask == nextSecTask )
        {
          // what we do and how we do it depends on secondary task
          switch( nextSecTask->taskID )
          {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
            case LL_TASK_ID_ADVERTISER:
            case LL_TASK_ID_PERIODIC_ADVERTISER:
              // check if we have a valid secondary command
              if ( nextSecCmd == NULL )
              {
                // possible there's valid Adv task, but none are enabled
                return;
              }

              // just resume Adv
              break;
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
            case LL_TASK_ID_SCANNER:
              MAP_llSetTaskScan(LL_SCHED_START_UNDEF,nextSecTask,nextSecCmd,NULL);
              return;
            case LL_TASK_ID_PERIODIC_SCANNER:
              // just resume Scan
              break;
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
            case LL_TASK_ID_INITIATOR:
              // setup the connection start time
              MAP_llSetTaskInit(LL_SCHED_START_UNDEF,nextSecTask,nextSecCmd,NULL);
              return;
#endif // INIT_CFG

            default:
              // Sanity Check:
              // Fatal Error: Next Secondary task must be valid!
              LL_ASSERT( FALSE );

              break;
          }  // switch on next secondary task ID

          // it is, so just schedule
          MAP_llScheduleTask( nextSecTask );
        }
        else // next secondary task is different from previous secondary task
        {
          // what we do and how we do it depends on the next secondary task
          switch( nextSecTask->taskID )
          {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
            case LL_TASK_ID_ADVERTISER:
              // check if we have a valid secondary command
              if ( nextSecCmd == NULL )
              {
                // possible there's valid Adv task, but none are enabled
                return;
              }
              MAP_llSetTaskAdv(LL_SCHED_START_UNDEF,nextSecCmd);
              break;

#ifdef USE_PERIODIC_ADV
            case LL_TASK_ID_PERIODIC_ADVERTISER:
              // check if we have a valid secondary command
              if ( nextSecCmd == NULL )
              {
                // possible there's valid Adv task, but none are enabled
                return;
              }
              MAP_llSetTaskPeriodicAdv();
              break;
#endif
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
            case LL_TASK_ID_SCANNER:
              MAP_llSetTaskScan(LL_SCHED_START_UNDEF,nextSecTask,nextSecCmd,NULL);
              break;

#ifdef USE_PERIODIC_SCAN
            case LL_TASK_ID_PERIODIC_SCANNER:
              // check if we have a valid secondary command
              if ( nextSecCmd == NULL )
              {
                // possible there's valid Adv task, but none are enabled
                return;
              }
              MAP_llSetTaskPeriodicScan();
              break;
#endif
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
            case LL_TASK_ID_INITIATOR:
              MAP_llSetTaskInit(LL_SCHED_START_UNDEF,nextSecTask,nextSecCmd,NULL);
              break;
#endif // INIT_CFG

            default:
              // Sanity Check:
              // Fatal Error: Next Secondary task must be valid!
              LL_ASSERT( FALSE );

              break;
          }  // switch on next secondary task ID
        }  // if curTask == nextSecTask (no active connections)
      }  // if there is an active connection
      break;
    }  // case secondary task

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (INIT_CFG | ADV_CONN_CFG))
    case LL_TASK_ID_PERIPHERAL:
    case LL_TASK_ID_CENTRAL:
    case LL_TASK_ID_CS:
      // check if there are any active connections
      if ( llConns.numActiveConns != 0 )
      {
        uint8          startType = LL_SCHED_START_PRIMARY;
        llConnState_t *nextConnPtr  = MAP_llDataGetConnPtr( MAP_llGetNextConn() );
        taskInfo_t    *nextConnTask = nextConnPtr->llTask;
        taskInfo_t    *nextSecTask  = MAP_llFindNextSecTask( llTaskList.lastSecTask );
        void          *nextConnCmd  = ((void *)nextConnTask->command);
        llConnState_t *curConnPtr   = MAP_llDataGetConnPtr( llConns.currentConn );
        taskInfo_t    *csTask       = MAP_llGetTask(LL_TASK_ID_CS);
        RCL_Command    *csCmd        = (RCL_Command *)llSchedulerGetCsCmd();

        // Set the next connection variable
        llConns.nextConn = nextConnPtr->connId;

        // This function return LL_SDAA_SCHED_HANDLED when RX window is scheduled,
		// therefore llScheduler() will finish here. The function will return the
		// new start type of next task which can be change when the next channel
		// is blocked or there isn't sufficient time to schedule RX window for
		// overloaded channel.
		// This function can split the TX queue of connection if there
		// is no time for RX window. In this case the next task type will be
		// priary task.
		// This function may do nothing when RX window isn't neccesary or sdaa
		// module is disable. In this case the function return start type task
		// equal to the start type that inserted as input.
		startTypeSDAA = MAP_llHandleSDAAControlTX(nextConnPtr, NULL, LL_SCHED_START_PRIMARY);
		if(startTypeSDAA != LL_SCHED_START_PRIMARY)
		{
			if (startTypeSDAA == LL_SDAA_SCHED_HANDLED)
			{
				return;
			}
			startType = startTypeSDAA;
		}
        if (curTask->taskID == LL_TASK_ID_PERIPHERAL)
        {
          if (MAP_llCheckPeripheralTerminate(nextConnPtr->connId) == TRUE)
          {
            return;
          }
        }

        // check if there is a command
        {
          if ( csCmd != NULL )
          {
            uint32 connMinTime = RAT_TICKS_IN_500US +
                                 MAP_llScheduler_getSwitchTime(nextConnPtr->taskID) +
                                 LL_MARGIN_TIME_FOR_TIMER_HANDLING_RAT_TICKS;

            // If the CS command is active and the secondary isn't the decision should
            // be done between the CS and the connection
            if ( csCmd != NULL )
            {
              // If the connection is the CS connection and it scheduled before
              // the CS commands schedule the connection else the CS command
              if( (MAP_llTimeCompare(((RCL_Command *)nextConnCmd)->timing.absStartTime, csCmd->timing.absStartTime) == FALSE) &&
                  (MAP_llTimeDelta(((RCL_Command *)nextConnCmd)->timing.absStartTime, csCmd->timing.absStartTime) > connMinTime) )
              {
                startType = LL_SCHED_START_PRIMARY;
              }
              else
              {
                startType = LL_SCHED_START_CS;
              }
            }
          }
        }

        // check if there are any active secondary tasks
        if ( MAP_llGetActiveTasks() & LL_TASK_ID_SECONDARY_TASKS )
        {
          if ( MAP_llActiveTask(LL_TASK_ID_ADVERTISER) )
          {
            MAP_llFindNextSecCmd(llGetTask( LL_TASK_ID_ADVERTISER ));
          }
#ifdef USE_PERIODIC_ADV
          if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_ADVERTISER) )
          {
            MAP_llFindNextSecCmd(llGetTask( LL_TASK_ID_PERIODIC_ADVERTISER ));
          }
#endif
#ifdef USE_PERIODIC_SCAN
          if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_SCANNER) )
          {
            MAP_llFindNextSecCmd(llGetTask( LL_TASK_ID_PERIODIC_SCANNER ));
          }
#endif
          // there's at least one secondary task, so find next secondary
          // task based based on the last secondary task
          taskInfo_t *nextSecTask = MAP_llFindNextSecTask( llTaskList.lastSecTask );

          // ALT? ble5OpCmd_t *nextSecCmd  = (ble5OpCmd_t *)(((rfOpCmd_NOP_t *)(nextSecTask->command))->nopCmd.pNextRfOp);
          void *nextSecCmd = NULL;
          if (nextSecTask != NULL)
          {
            if ((nextSecTask->taskID == LL_TASK_ID_ADVERTISER) ||
                (nextSecTask->taskID == LL_TASK_ID_PERIODIC_ADVERTISER) ||
                (nextSecTask->taskID == LL_TASK_ID_PERIODIC_SCANNER))
            {
              nextSecCmd = (void *)nextSecTask->command;
            }
            else
            {
              nextSecCmd = MAP_llFindNextSecCmd( nextSecTask );
            }
          }
          // check if there is a command
          if ( nextSecCmd == NULL )
          {
              if ( csCmd != NULL )
              {
                uint32 connMinTime = RAT_TICKS_IN_500US +
                                     MAP_llScheduler_getSwitchTime(nextConnPtr->taskID) +
                                     LL_MARGIN_TIME_FOR_TIMER_HANDLING_RAT_TICKS;

                // If the CS command is active and the secondary isn't the decision should
                // be done between the CS and the connection
                if ( csCmd != NULL )
                {
                  // If the connection is the CS connection and it scheduled before
                  // the CS commands schedule the connection else the CS command
                  if( (MAP_llTimeCompare(((RCL_Command *)nextConnCmd)->timing.absStartTime, csCmd->timing.absStartTime) == FALSE) &&
                       (MAP_llTimeDelta(((RCL_Command *)nextConnCmd)->timing.absStartTime, csCmd->timing.absStartTime) > connMinTime) )
                  {
                    startType = LL_SCHED_START_PRIMARY;
                  }
                  else
                  {
                    startType = LL_SCHED_START_CS;
                  }
                }
              }
          }
          else
          {
            // check if there's enough time for secondary task (if any)
            // Note: The next secondary task pointer may be NULL, in which case
            //       a primary start type is returned.
            startType = MAP_llFindStartType( nextSecTask, nextConnTask );
          }

          // This function return LL_SDAA_SCHED_HANDLED when RX window is scheduled,
          // therefore llScheduler() will finish here. The function will return the
          // new start type of next task which can be change when the next channel
          // is blocked or there isn't sufficient time to schedule RX window for
          // overloaded channel.
          // This function can split the TX queue of connection if there
          // is no time for RX window. In this case the next task type will be
          // priary task.
          // This function may do nothing when RX window isn't neccesary or sdaa
          // module is disable. In this case the function return start type task
          // equal to the start type that inserted as input.
          startTypeSDAA = MAP_llHandleSDAAControlTX(nextConnPtr, NULL, LL_SCHED_START_PRIMARY);
          if(startTypeSDAA != LL_SCHED_START_PRIMARY)
          {
              if (startTypeSDAA == LL_SDAA_SCHED_HANDLED)
              {
                  return;
              }
              startType = startTypeSDAA;
          }

          // check if the secondary task can start
          if ( (startType != LL_SCHED_START_PRIMARY) && (startType != LL_SCHED_START_CS) )
          {
            if ( nextSecTask == NULL )
            {
               // Sanity Check:
               // Fatal Error: Next Secondary task must be valid!
               LL_ASSERT( FALSE );
               return;
            }

            // what we do and how we do it depends on secondary task
            switch( nextSecTask->taskID )
            {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_NCONN_CFG)
              case LL_TASK_ID_ADVERTISER:
                MAP_llSetTaskAdv(startType,nextSecCmd);
                return;

                // Note: Unreachable statement generates compiler warning!
                //break;
#ifdef USE_PERIODIC_ADV
              case LL_TASK_ID_PERIODIC_ADVERTISER:
                MAP_llSetTaskPeriodicAdv();
                return;
                //break;
#endif
#endif // ADV_NCONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
              case LL_TASK_ID_SCANNER:
                MAP_llSetTaskScan(startType,nextSecTask,nextSecCmd,nextConnCmd);
                return;

                // Note: Unreachable statement generates compiler warning!
                //break;
#ifdef USE_PERIODIC_SCAN
              case LL_TASK_ID_PERIODIC_SCANNER:
                MAP_llSetTaskPeriodicScan();
                return;
                //break;
#endif
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
              case LL_TASK_ID_INITIATOR:
                MAP_llSetTaskInit(startType,nextSecTask,nextSecCmd,nextConnCmd);
                return;

                // Note: Unreachable statement generates compiler warning!
                //break;
#endif // INIT_CFG

              default:
                // Sanity Check:
                // Fatal Error: Next Secondary task must be valid!
                LL_ASSERT( FALSE );

                break;
            }  // switch on next secondary task
          }  // if start type is not PRIMARY
        }  // if any secondary tasks

        if (startType == LL_SCHED_START_PRIMARY)
        {
          // either there is a next secondary task but it can't be started
          // before the next connection, or there are no secondary tasks;
          // either way, start next connection
          // check if the next connection is different from the current one
          // Note: This is only possible witht he Central connection.
          if ( curConnPtr != nextConnPtr )
          {
            // which LL state based on role
            // Note: Currently, Central+Peripheral combo isn't permitted, but will be
            //       in the future, so leave this here.
            if ( nextConnTask->taskID == LL_TASK_ID_CENTRAL )
            {
              MAP_llSetTaskCentral(nextConnPtr->connId, nextConnCmd);
            }
            else // assume taskId == LL_TASK_ID_PERIPHERAL
            {
              MAP_llSetTaskPeripheral(nextConnPtr->connId, nextConnCmd);
            }
          }

          // either way, nextConnPtr is the connection to schedule; if the current
          // connection isn't the next connection, then its context was saved and
          // the next task is nextConnTask; if the current connection is the next
          // connection, then it's still nextConnTask that is the next task
          // Note: Sets curTask.
          MAP_llScheduleTask( nextConnTask );
        }
        else // The start type is CS
        {
          // the way of choosing the llState is still correct
          uint16 csConnTaskID = llGetCsConnTaskID();
          if ( csConnTaskID == LL_TASK_ID_CENTRAL )
          {
            llState = LL_STATE_CONN_CENTRAL;
          }
          else
          {
            llState = LL_STATE_CONN_PERIPHERAL;
          }

          MAP_llScheduleTask( csTask );
        }
      }
      else // there are no active connections
      {
        // check if there are any active secondary tasks
        if ( MAP_llGetActiveTasks() & LL_TASK_ID_SECONDARY_TASKS )
        {
          if ( MAP_llActiveTask(LL_TASK_ID_ADVERTISER) )
          {
            MAP_llFindNextSecCmd(llGetTask( LL_TASK_ID_ADVERTISER ));
          }
#ifdef USE_PERIODIC_ADV
          if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_ADVERTISER) )
          {
            MAP_llFindNextSecCmd(llGetTask( LL_TASK_ID_PERIODIC_ADVERTISER ));
          }
#endif
#ifdef USE_PERIODIC_SCAN
          if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_SCANNER) )
          {
            MAP_llFindNextSecCmd(llGetTask( LL_TASK_ID_PERIODIC_SCANNER ));
          }
#endif
          // there's at least one secondary task, so find next secondary
          // task based based on the last secondary task
          taskInfo_t *nextSecTask = MAP_llFindNextSecTask( llTaskList.lastSecTask );
          // ALT? ble5OpCmd_t *nextSecCmd  = (ble5OpCmd_t *)(((rfOpCmd_NOP_t *)(nextSecTask->command))->nopCmd.pNextRfOp);
          void *nextSecCmd = NULL;
          if (nextSecTask != NULL)
          {
            if ((nextSecTask->taskID == LL_TASK_ID_ADVERTISER) ||
                (nextSecTask->taskID == LL_TASK_ID_PERIODIC_ADVERTISER) ||
                (nextSecTask->taskID == LL_TASK_ID_PERIODIC_SCANNER))
            {
              nextSecCmd = (void *)nextSecTask->command;
            }
            else
            {
              nextSecCmd = MAP_llFindNextSecCmd( nextSecTask );
            }

            // what we do and how we do it depends on secondary task
            switch( nextSecTask->taskID )
            {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
              case LL_TASK_ID_ADVERTISER:
                // check if we have a valid secondary command
                if ( nextSecCmd == NULL )
                {
                  // possible there's valid Adv task, but none are enabled
                  return;
                }
                MAP_llSetTaskAdv(LL_SCHED_START_UNDEF,nextSecCmd);
                break;
  #ifdef USE_PERIODIC_ADV
              case LL_TASK_ID_PERIODIC_ADVERTISER:
                // check if we have a valid secondary command
                if ( nextSecCmd == NULL )
                {
                  // possible there's valid Adv task, but none are enabled
                  return;
                }
                MAP_llSetTaskPeriodicAdv();
                break;
#endif
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
              case LL_TASK_ID_SCANNER:
                MAP_llSetTaskScan(LL_SCHED_START_UNDEF,nextSecTask,nextSecCmd,NULL);
                break;
#ifdef USE_PERIODIC_SCAN
              case LL_TASK_ID_PERIODIC_SCANNER:
                // check if we have a valid secondary command
                if ( nextSecCmd == NULL )
                {
                  // possible there's valid Adv task, but none are enabled
                  return;
                }
                MAP_llSetTaskPeriodicScan();
                break;
#endif
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
              case LL_TASK_ID_INITIATOR:
                MAP_llSetTaskInit(LL_SCHED_START_UNDEF,nextSecTask,nextSecCmd,NULL);
                break;
#endif // INIT_CFG

              default:
                break;
            }  // switch on next secondary task
          }  // if secondary task != NULL
        }  // if active secondary task

        // Note: No active connection or active secondary task

      }  // if active connection
      break;
#endif  // INIT_CFG | ADV_CONN_CFG

    case LL_TASK_ID_NONE:
      // fatal error - not possible
      LL_ASSERT( FALSE );

      break;

    default:
      // Sanity Check:
      // Fatal Error: Invalid Task ID
      LL_ASSERT( FALSE );

      break;
  }  // switch on curTask taskID

  return;
}

/*******************************************************************************
 * @fn          llSetTask
 *
 * @brief       This function is used to active a specified task
 *
 * input parameters
 *
 * @param       llTaskID - The active task block based on this task ID.
 * @param       state - LL state type.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetTask( uint8 llTaskID, uint8 state )
{
  // save last secondary task
  llTaskList.lastSecTask = llTaskID;

  // set LL state
  llState = state;
  // start next secondary task
  // Note: Sets curTask.
  MAP_llScheduleTask( MAP_llGetTask( llTaskID ) );
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llSetTaskAdv
 *
 * @brief       This function is used to schedule the Advertising task
 *
 * input parameters
 *
 * @param       startType - immediate or not (relevant when exist primary command).
 * @param       nextSecCmd - next adv command.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetTaskAdv( uint8 startType, void *nextSecCmd )
{
  // check if the task should start immediately:
  // - because of task start time relative to connection time
  // Note: If not immediate, then task will start at Start Time.
  if ((startType == LL_SCHED_START_IMMED) ||
     ((startType == LL_SCHED_START_UNDEF) &&
     (MAP_llTimeCompare( ((RCL_Command *)nextSecCmd)->timing.absStartTime,
      MAP_llGetCurrentTime() + LL_SCHED_PRE_CUTOFF ) == FALSE )))
  {
    // it is, so base it on the current time
    ((RCL_Command *)nextSecCmd)->timing.absStartTime =
      MAP_llGetCurrentTime() + LL_SCHED_START_IMMED_PAD;
  }

  // start adv task
  llSetTask(LL_TASK_ID_ADVERTISER, LL_STATE_EXT_ADV);
}

#ifdef USE_PERIODIC_ADV
/*******************************************************************************
 * @fn          llSetTaskPeriodicAdv
 *
 * @brief       This function is used to schedule the periodic Advertising task
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetTaskPeriodicAdv( void )
{
  // start the task
  llSetTask(LL_TASK_ID_PERIODIC_ADVERTISER, LL_STATE_PERIODIC_ADV);
}
#endif // USE_PERIODIC_ADV
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * @fn          llSetTaskScan
 *
 * @brief       This function is used to schedule the scan task
 *
 * input parameters
 *
 * @param       startType    - immediate or not (relevant when exist primary command).
 * @param       nextSecTask  - next scan task.
 * @param       nextSecCmd   - next scan command.
 * @param       nextConnCmd  - next primary command (NULL if not exist).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetTaskScan( uint8 startType, taskInfo_t *nextSecTask, void *nextSecCommand, void *nextConnCmd )
{
  RCL_Command *nextSecCmd = (RCL_Command *)nextSecCommand;
  // The secondary task end time calculation varaible.
  uint32 secTaskEndTimeCalc = 0;

  // Get the info for the next connection to be scheduled.
  llConnState_t    *nextConnPtr = MAP_llDataGetConnPtr( llConns.nextConn );

  // check if the task should start immediately:
  // - because of task start time relative to connection time
  // - because the Scan is Continuous (already set in scanCmd)
  // Note: If not immediate, then task will start at Start Time (if
  //       not Continuous), or Now (if Continuous).
  if (((nextConnCmd != NULL) && (startType == LL_SCHED_START_IMMED)) ||
      ((nextConnCmd == NULL) && (MAP_llTimeCompare(nextSecCmd->timing.absStartTime,
                                 MAP_llGetCurrentTime() + LL_SCHED_PRE_CUTOFF) == FALSE)))
  {
    // it is, so base it on the current time
    nextSecCmd->timing.absStartTime = MAP_llGetCurrentTime() + LL_SCHED_START_IMMED_PAD;

    // to make sure it is not run over
    // set window
       nextSecCmd->timing.relGracefulStopTime =
         (extScanInfo->pScanParam->extScanParam[extScanIndex].scanWindow * RAT_TICKS_IN_625US);

    // update event start time
    extScanInfo->scanStartTime = nextSecCmd->timing.absStartTime;
  }

  // In case there is a connection.
  if (nextConnCmd != NULL)
  {
   // Calculate the secondary's task end time according to the state of the connection.
   // In case the secondary task has a higher priority than the primary task, it's end time would be
   // adjusted in order for the connection to be kept alive.
   secTaskEndTimeCalc = llGetSecondaryTaskEndTime(nextSecTask,                                                      // Current secondary task chosen.
                                                  extScanInfo->pScanParam->extScanParam[extScanIndex].scanWindow,   // Length of secondary task.
                                                  nextConnPtr);                                                     // Current primary task chosen.
    // adjust Scan end time
    // Note: The Timeout trigger is set to the Scan Window, relative
    //       to the start of the Scan. If the Scan Window ends before
    //       the next connection, the Scan will end per usual. If the
    //       Scan Window ends after the next connection is supposed
    //       to start, the Scan will end due to the End Trigger. In
    //       either case, just set the End Trigger to the next
    //       connection's cutoff.
    // Note: Post processing will always disable the End Trigger.
    nextSecCmd->timing.relHardStopTime = (secTaskEndTimeCalc == 0) ? 0 : MAP_llTimeDelta( secTaskEndTimeCalc, nextSecCmd->timing.absStartTime );
  }
  else
  {
    nextSecCmd->timing.relHardStopTime = 0;
  }

  // start Scan
  llSetTask(LL_TASK_ID_SCANNER, LL_STATE_SCAN);
}

#ifdef USE_PERIODIC_SCAN
/*******************************************************************************
 * @fn          llSetTaskPeriodicScan
 *
 * @brief       This function is used to schedule the periodic Scan task
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetTaskPeriodicScan( void )
{
  // start the task
  llSetTask(LL_TASK_ID_PERIODIC_SCANNER, LL_STATE_PERIODIC_SCAN);
}
#endif // USE_PERIODIC_SCAN
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * @fn          llSetTaskInit
 *
 * @brief       This function is used to schedule the init task
 *
 * input parameters
 *
 * @param       startType   - immediate or not (relevant when exist primary command).
 * @param       nextSecTask - next init task.
 * @param       nextSecCmd  - next init command.
 * @param       nextConnCmd - next primary command (NULL if not exist).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetTaskInit( uint8 startType, taskInfo_t *nextSecTask, void *nextSecCommand, void *nextConnCmd )
{
  RCL_Command *nextSecCmd = (RCL_Command *)nextSecCommand;

  // The secondary task end time calculation varaible.
  uint32 secTaskEndTimeCalc = 0;

  // Get the info for the next connection to be scheduled.
  llConnState_t    *nextConnPtr = MAP_llDataGetConnPtr( llConns.nextConn );

  // check if the task should start immediately:
  // - because of task start time relative to connection time
  // - because the Scan is Continuous (already set in initCmd)
  // Note: If not immediate, then task will start at Start Time (if
  //       not Continuous), or Now (if Continuous).
  if ((( nextConnCmd != NULL ) && ( startType == LL_SCHED_START_IMMED )) ||
      (( nextConnCmd == NULL ) && ( MAP_llTimeCompare( nextSecCmd->timing.absStartTime,
                                    MAP_llGetCurrentTime() + LL_SCHED_PRE_CUTOFF ) == FALSE )))
  {
    // it is, so base it on the current time
    nextSecCmd->timing.absStartTime = MAP_llGetCurrentTime() + LL_SCHED_START_IMMED_PAD;

    // set window
        nextSecCmd->timing.relGracefulStopTime =
          (extInitInfo->pCreateConn->extInitParam[extInitIndex].scanWindow * RAT_TICKS_IN_625US);


    // update event start time
    extInitInfo->initStartTime = nextSecCmd->timing.absStartTime;
  }

  // setup the connection start time
  MAP_llSetupConn( extInitInfo->connId );

  // In case there is a connection.
  if ( nextConnCmd != NULL )
  {
    // Calculate the secondary's task end time according to the state of the connection.
    // In case the secondary task has a higher priority than the primary task, it's end time would be
    // adjusted in order for the connection to be kept alive.
    secTaskEndTimeCalc = llGetSecondaryTaskEndTime(nextSecTask,                                                       // Current secondary task chosen.
                                                   extInitInfo->pCreateConn->extInitParam[extInitIndex].scanWindow,   // Length of secondary task.
                                                   nextConnPtr);                                                      // Current primary task chosen.
    // adjust Init end time
    // Note: The Timeout trigger is set to the Scan Window, relative
    //       to the start of the Scan. If the Scan Window ends before
    //       the next connection, the Scan will end per usual. If the
    //       Scan Window ends after the next connection is supposed
    //       to start, the Scan will end due to the End Trigger. In
    //       either case, just set the End Trigger to the next
    //       connection's cutoff.
    // Note: Post processing will always disable the End Trigger.
    nextSecCmd->timing.relHardStopTime = (secTaskEndTimeCalc == 0) ? 0 : MAP_llTimeDelta( secTaskEndTimeCalc, ((RCL_Command *)nextSecCmd)->timing.absStartTime );
  }
  else
  {
    // Change the end trig type from absolute time to never and set the end trig time to 0
    nextSecCmd->timing.relHardStopTime = 0;
  }

  // start Init
  llSetTask(LL_TASK_ID_INITIATOR, LL_STATE_INIT);
}
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * @fn          llSetTaskCentral
 *
 * @brief       This function is used to schedule the Central task
 *
 * input parameters
 *
 * @param       connId - connection ID.
 * @param       nextConnCmd - next primary command.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetTaskCentral( uint8 connId, void *nextConnCmd )
{
  llState = LL_STATE_CONN_CENTRAL;

#ifndef CC23X0
  // check if one packet per event is enabled
  if (onePktPerEvt == TRUE)
  {
    // set limit for the number of packets to transmit before it ends
    ((centralParam_t *)((ble5OpCmd_t *)nextConnCmd)->pParams)->maxTxPkt = ONE_PKT_PER_EVENT;
  }
  else // one packet per event is disabled and number of connections is one
  {
    // so restore configured max number of packets
    ((centralParam_t *)((ble5OpCmd_t *)nextConnCmd)->pParams)->maxTxPkt =
      llConfigTable.maxPktsPerEvtPtr->maxMstPktsPerEvt;
  }
#endif
  // set this connection as the current connection
  llConns.currentConn = connId;
}
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
/*******************************************************************************
 * @fn          llSetTaskPeripheral
 *
 * @brief       This function is used to schedule the Peripheral task
 *
 * input parameters
 *
 * @param       connId - connection ID.
 * @param       nextConnCmd - next primary command.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetTaskPeripheral( uint8 connId, void *nextConnCmd )
{
  llState = LL_STATE_CONN_PERIPHERAL;

#ifndef CC23X0
  // check if one packet per event is enabled
  // Note: Only one Peripheral connection allowed.
  if ( onePktPerEvt == TRUE )
  {
    // set limit for the number of packets to transmit before it ends
    ((peripheralParam_t *)((ble5OpCmd_t *)nextConnCmd)->pParams)->maxTxPkt = ONE_PKT_PER_EVENT;
  }
  else // one packet per event is disabled
  {
    // so restore configured max number of packets
    ((peripheralParam_t *)((ble5OpCmd_t *)nextConnCmd)->pParams)->maxTxPkt =
      llConfigTable.maxPktsPerEvtPtr->maxSlvPktsPerEvt;
  }
#endif
  // set this connection as the current connection
  llConns.currentConn = connId;
}
#endif

/*******************************************************************************
 * @fn          llCheckIsSecTaskCollideWithPrimTaskInLsto
 *
 * @brief       This function is used to check if a certain secondary task
 *              collides in time with a primary connection event in:
 *              - LSTO.
 *              - Connection Establishment Mode.
 *
 * @design      /ref did_408769671
 *
 * input parameters
 *
 * @param       secTask         - Pointer to the secondary task information structure.
 * @param       timeGap         - Time consumption for the secondary task.
 * @param       selectedConnId  - The first connection id to check with secondary task.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      TRUE -  If Secondary task's Collides with a primary task in LSTO
 *                      or Connection Establishment Mode.
 *              FALSE - If Secondary task's Does not Collide with a primary
 *                      task in LSTO or Connection Establishment Mode.
 *              Note: In case TRUE is returned the Primary Task would be scheduled.
 */
uint8 llCheckIsSecTaskCollideWithPrimTaskInLsto( taskInfo_t    *secTask,
                                                 uint32         timeGap,
                                                 uint16         selectedConnId)
{
  llConnState_t *currConnPtr = MAP_llDataGetConnPtr( selectedConnId );
  llConnState_t *nextConnPtr = NULL;
  uint32 curTime = MAP_llGetCurrentTime() + RAT_TICKS_IN_625US;
  uint16 nextConnId = selectedConnId;

  // Check valid input
  if ((currConnPtr == NULL) || (secTask == NULL) || (!(currConnPtr->activeConn)))
  {
    return FALSE;
  }

  uint32 secStartTime  = ((RCL_Command *)(secTask->command))->timing.absStartTime;
  uint32 primStartTime = ((RCL_Command *)(currConnPtr->llTask->command))->timing.absStartTime;
  /********************************************/
  /********* Check Single Connection **********/
  /********************************************/
  // Check if the entered connection id collides with the secondary task and in LSTO / Connection Establishment Mode.
  /** Check Collision **/
  if ((MAP_llTimeDelta( primStartTime, curTime ) < timeGap) ||
      (MAP_llTimeCompare( secStartTime + timeGap, primStartTime) == TRUE))
  {
    // Check that the input connection is not in LSTO / Connection Establishment Mode / Starvation Mode
    if ((currConnPtr->numEventsLeft <= (currConnPtr->expirationValue / 2)) || (currConnPtr->firstPacket) || (currConnPtr->StarvationMode))
    {
      // The starvation mode was started already under the "llGetNextConn" function in case it reached half of it's LSTO.
      return TRUE;
    }
  }

  // Get next active connection after the current start connection id.
  nextConnId = llFindNextActiveConnId(selectedConnId);

  /************************************************************************/
  /******** Find other LSTO / Connection Establishment Connection *********/
  /************************************************************************/
  // Go over the active connections
  while (nextConnId != LL_INACTIVE_CONNECTIONS)
  {
    // Get the info ptr for the next connection.
    nextConnPtr = MAP_llDataGetConnPtr( nextConnId );

    primStartTime = ((RCL_Command *)(currConnPtr->llTask->command))->timing.absStartTime;

    /** Check Collision **/
    // Check for a collision between the secondary task and the connection id.
    if ((MAP_llTimeDelta( primStartTime, curTime ) < timeGap) ||
        (MAP_llTimeCompare( secStartTime + timeGap, primStartTime) == TRUE))
    {
      // Check that the input connection is not in LSTO / Connection Establishment Mode / Starvation Mode
      if ((nextConnPtr->numEventsLeft <= (nextConnPtr->expirationValue / 2)) || (nextConnPtr->firstPacket) || (nextConnPtr->StarvationMode))
      {
        // In case we would schedule a connection because another connection is in LSTO - Set the Starvation Mode ON for the current connection scheduled.
        // Note that the current scheduled connection is currConnPtr that would set the starvation mode on in case it is close to LSTO by a 10% margin.
        if ((currConnPtr->StarvationMode == FALSE) && ((currConnPtr->numEventsLeft - llGetLstoNumOfEventsLeftMargin(currConnPtr->connId)) <= (currConnPtr->expirationValue / 2)))
        {
          MAP_llSetStarvationMode(currConnPtr->connId, LL_SET_STARVATION_MODE_ON);
        }
        // Schedule the connection instead of the secondary task.
        return TRUE;
      }
    }
    // No collision with this connection.
    // This means no collisions would be with next connections.
    else
    {
      return FALSE;
    }

    // Get next active connection after last one.
    nextConnId =  llFindNextActiveConnId(nextConnId);
  }

  return FALSE;
}

/*******************************************************************************
 * @fn          llFindStartType
 *
 * @brief       This function is used to determine if there is enough time for
 *              the secondary task to start, and if so, whether to start
 *              immediately or at its scheduled task start time. If the
 *              secondary task is Scan, and it is scanning continuously, then
 *              its scheduled task start time is set to the current time so
 *              that the rest of the algorithm will schedule it immediately
 *              if there's enough time. If there is not enough time for the
 *              secondary task, then the primary task is scheduled.
 *
 * input parameters
 *
 * @param       secTask  - Pointer to a secondary task, or NULL if none.
 * @param       primTask - Pointer to a primary task, such as Central.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_SCHED_START_IMMED - Secondary task starts ASAP.
 *              LL_SCHED_START_EVENT - Secondary task starts at its T2E1 time.
 *              LL_SCHED_START_PRIMARY - Start primary task instead.
 */
uint8 llFindStartType( taskInfo_t *secTask,
                       taskInfo_t *primTask )
{
  uint32      timeGap = LL_SCHED_OVERHEAD;
  uint32      curTime;
  RCL_Command *primCmd = NULL;
  RCL_Command *secCmd = NULL;
  RCL_Command  *csCmd = NULL;
  RCL_Command  *tempCmd = NULL;

  // Check Valid input
  if ( primTask == NULL )
  {
    return( LL_STATUS_ERROR_INVALID_PARAMS );
  }

  // Check if there is a secondary task
  if ( secTask == NULL )
  {
    // then assume primary task
    return( LL_SCHED_START_PRIMARY );
  }

  primCmd = (RCL_Command *)primTask->command;
  secCmd = (RCL_Command *)secTask->command;
  csCmd = (RCL_Command *)llSchedulerGetCsCmd();

  // take a snapshot of the current time
  // Note: Add one tick of pad.
  curTime = MAP_llGetCurrentTime() + RAT_TICKS_IN_625US;

  // Get the info for the next connection to be scheduled.
  llConnState_t    *nextConnPtr = MAP_llDataGetConnPtr( llConns.nextConn );

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  // check if the secondary task is the Scanner, and that it's scan window is larger than the
  // default LL_SCHED_OVERHEAD. This is relevent only in case the secondary task has a higher
  // priority than the primary task.
  if ((secTask->taskID == LL_TASK_ID_SCANNER) && (llCompareSecondaryPrimaryTasksQoSParam(LL_QOS_TYPE_PRIORITY, secTask, nextConnPtr) == TRUE) &&
      (extScanInfo->pScanParam->extScanParam[extScanIndex].scanWindow * RAT_TICKS_IN_625US > LL_SCHED_OVERHEAD))
  {
    // Update the timeGap of the scan to be the scan window instead of default LL_SCHED_OVERHEAD.
    timeGap = extScanInfo->pScanParam->extScanParam[extScanIndex].scanWindow * RAT_TICKS_IN_625US;
  }
  // check if the secondary task is the Scanner, and scanning is continuous
  if ( ((secTask->taskID == LL_TASK_ID_SCANNER) &&
        (extScanInfo->pScanParam->extScanParam[extScanIndex].scanInterval ==
         extScanInfo->pScanParam->extScanParam[extScanIndex].scanWindow)) )
  {
    // set secondary task start time to current time
    // Note: This is done to ensure the rest of the algorithm will flag this
    //       secondary task to start immediately, assuming it is able to start
    //       at all.
    secCmd->timing.absStartTime = curTime;
  }
#endif  // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  // check if the secondary task is the Initiator, and that it's scan window is larger than the
  // default LL_SCHED_OVERHEAD. This is relevent only in case the secondary task has a higher
  // priority than the primary task.
  if ((secTask->taskID == LL_TASK_ID_INITIATOR) && (llCompareSecondaryPrimaryTasksQoSParam(LL_QOS_TYPE_PRIORITY, secTask, nextConnPtr) == TRUE) &&
      (extInitInfo->pCreateConn->extInitParam[extInitIndex].scanWindow * RAT_TICKS_IN_625US > LL_SCHED_OVERHEAD))
  {
    // Update the timeGap of the init to be the scan window instead of default LL_SCHED_OVERHEAD.
    timeGap = extInitInfo->pCreateConn->extInitParam[extInitIndex].scanWindow * RAT_TICKS_IN_625US;
  }
  // check if the secondary task is the Initiator, and scanning is continuous
  if ( ((secTask->taskID == LL_TASK_ID_INITIATOR) &&
        (extInitInfo->pCreateConn->extInitParam[extInitIndex].scanInterval ==
         extInitInfo->pCreateConn->extInitParam[extInitIndex].scanWindow)) )
  {
    // set secondary task start time to current time
    // Note: This is done to ensure the rest of the algorithm will flag this
    //       secondary task to start immediately, assuming it is able to start
    //       at all.
    secCmd->timing.absStartTime = curTime;
  }
#endif  // INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))

  // if secondary task is advertise , the time which will be consumed is calculated dynamicly
  if (secTask->taskID == LL_TASK_ID_ADVERTISER)
  {
    timeGap = US_TO_RAT_TICKS(pNextAdvSet->timeConsume);
  }
#ifdef USE_PERIODIC_ADV
  // check for periodic advertising
  else if (secTask->taskID == LL_TASK_ID_PERIODIC_ADVERTISER)
  {
    llPeriodicAdvSet_t *pPeriodicAdv = MAP_llGetCurrentPeriodicAdv();
    if (pPeriodicAdv != NULL)
    {
      timeGap = pPeriodicAdv->totalOtaTime;
    }
  }
#endif
#endif // ADV_NCONN_CFG || ADV_CONN_CFG
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
#ifdef USE_PERIODIC_SCAN
  // check for periodic scan
  if (secTask->taskID == LL_TASK_ID_PERIODIC_SCANNER)
  {
    llPeriodicScanSet_t *pPeriodicScan = MAP_llGetCurrentPeriodicScan(PERIODIC_SCAN_STATE_SYNCED);
    if (pPeriodicScan != NULL)
    {
      timeGap = pPeriodicScan->totalOtaTime;
    }
  }
#endif
#endif  // SCAN_CFG

  if( csCmd != NULL)
  {
    uint32 connMinTime = RAT_TICKS_IN_500US +
                         MAP_llScheduler_getSwitchTime(primTask->taskID) +
                         LL_MARGIN_TIME_FOR_TIMER_HANDLING_RAT_TICKS;

    // If the connection is the CS connection and it scheduled before
    // the CS commands schedule the connection else the CS command
    if((MAP_llTimeCompare(primCmd->timing.absStartTime, csCmd->timing.absStartTime) == FALSE) &&
		(MAP_llTimeDelta(primCmd->timing.absStartTime, csCmd->timing.absStartTime) > connMinTime) )
    {
      // Go to check Conn VS secondary
	  tempCmd = primCmd;
    }
	  else
	  {
	    tempCmd = csCmd;
	  }
    }
    else
    {
      tempCmd = primCmd;
    }

  /*******************************************************************************/
  /* check if there's enough time for the secondary task to run                  */
  /* Note: While it is assumed the primary task's start time is before the       */
  /*       current time (otherwise the task would hang), we still have to handle */
  /*       counter wrap.                                                         */
  /*******************************************************************************/
  if ( ((MAP_llTimeDelta( tempCmd->timing.absStartTime, curTime ) > timeGap) &&
       (MAP_llTimeCompare( secCmd->timing.absStartTime, tempCmd->timing.absStartTime - timeGap ) == FALSE)) )
  {
    // the secondary task has enough time to start relative to the primary
    // task's cutoff, but check if there's enough time relative to current time
    // Note: This is needed so we don't try to start the secondary task at a
    //       time that may have already expired. That is, it is possible that
    //       the secondary task may have not been scheduled because of a
    //       conflict with the primary task. In this case, the secondary task's
    //       start time is long since expired.
    if ( MAP_llTimeCompare( secCmd->timing.absStartTime,
                            curTime + LL_SCHED_PRE_CUTOFF - LL_SCHED_START_IMMED_PAD ) == FALSE )
    {
      // the secondary task is either in the past, or not far enough into the
      // future, so start it immediately
      return( LL_SCHED_START_IMMED );
    }
    else // the secondary task's start time is far enough into the future
    {
      // so there's enough time to start it based on its own interval
      return( LL_SCHED_START_EVENT );
    }
  }
  else // not enough time to start secondary task at all
  {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))

    /****************************************/
    /************ Priority Check ************/
    /****************************************/
    // Check if the secondary task has a higher priority than the primary task.
    // Schedule the adv set only in case it does not collide with connection events
    // that are in LSTO or in connection establishment
    if (MAP_llCompareSecondaryPrimaryTasksQoSParam(LL_QOS_TYPE_PRIORITY, secTask, nextConnPtr) == TRUE)
    {
      // Check that the secondary task does not collide with connection events
      // that are in LSTO or in connection establishment mode.
      if (MAP_llCheckIsSecTaskCollideWithPrimTaskInLsto(secTask, timeGap, llConns.nextConn) == FALSE)
      {
        // the secondary task has enough time to start relative to the primary
        // task's cutoff, but check if there's enough time relative to current time.
        if ( MAP_llTimeCompare( secCmd->timing.absStartTime,
                               curTime + LL_SCHED_PRE_CUTOFF - LL_SCHED_START_IMMED_PAD ) == FALSE )
        {
          // Update that we need to schedule the secondary task (immediately) instead of the
          // the primary task.
          return ( LL_SCHED_START_IMMED );
        }
        else
        {
          // so there's enough time to start it based on its own interval.
          return( LL_SCHED_START_EVENT );
        }
      }
    }
#endif
    // so resume the primary task
    if ( tempCmd == csCmd )
    {
      return ( LL_SCHED_START_CS );
    }
    return( LL_SCHED_START_PRIMARY );
  }
}

/*******************************************************************************
 * @fn          llFindNextSecTask
 *
 * @brief       This function is used to find the next secondary task, if any,
 *              based on the previous secondary task that executed (if any).
 *              It uses a strict round-robin algorithm based on the currently
 *              active secondary tasks. That is:
 *
 *              Adv  -> Scan -> Init -> Adv
 *              Scan -> Init -> Adv  -> Scan
 *              Init -> Adv  -> Scan -> Init
 *
 * input parameters
 *
 * @param       secTaskID - Task ID of the current secondary task (if a
 *                          secondary task just finished) or previous secondary
 *                          task (if a primary task just finished), or
 *                          LL_TASK_ID_NONE if neither or none are active.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to next secondary task, or NULL if none.
 */
taskInfo_t *llFindNextSecTask( uint16 secTaskID )
{
  uint32 timeGap = LL_SCHED_PRE_CUTOFF;

  // check if there wasn't a previous secondary task or the previous
  // task was RX window task
  if ( (secTaskID == LL_TASK_ID_NONE) || (secTaskID == LL_TASK_ID_RX_WINDOW))
  {
    // check if there are any active secondary tasks
    // Note: Either the current task that ended is a secondary task, in which case
    //       secTask is not NULL, or a primary task ended and a check that some
    //       secondary task is active was true. So it should be safe to remove
    //       this if statement.
    if ( MAP_llGetActiveTasks() & LL_TASK_ID_SECONDARY_TASKS )
    {
      if ( MAP_llActiveTask(LL_TASK_ID_INITIATOR) )
      {
        return( MAP_llGetTask(LL_TASK_ID_INITIATOR) );
      }
      else if ( MAP_llActiveTask(LL_TASK_ID_ADVERTISER) )
      {
        return( MAP_llGetTask(LL_TASK_ID_ADVERTISER) );
      }
      else if ( MAP_llActiveTask(LL_TASK_ID_SCANNER) )
      {
        return( MAP_llGetTask(LL_TASK_ID_SCANNER) );
      }
      else if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_SCANNER) )
      {
        return( MAP_llGetTask(LL_TASK_ID_PERIODIC_SCANNER) );
      }
      else if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_ADVERTISER) )
      {
        return( MAP_llGetTask(LL_TASK_ID_PERIODIC_ADVERTISER) );
      }
      else
      {
        // Sanity Check
        // Not possible!
        LL_ASSERT( FALSE );
      }
    }
    else // there are no secondary tasks
    {
      return( NULL );
    }
  }
  else
  {
    // base decision on current task
    switch( secTaskID )
    {
      case LL_TASK_ID_ADVERTISER:
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
        if (( MAP_llActiveTask(LL_TASK_ID_ADVERTISER) ) && (pNextAdvSet != NULL))
        {
          timeGap = US_TO_RAT_TICKS(pNextAdvSet->timeConsume);
        }
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
#ifdef USE_PERIODIC_SCAN
        if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_SCANNER) )
        {
          return MAP_llSelectTaskPeriodicScan(secTaskID,timeGap);
        }
        else
#endif // USE_PERIODIC_SCAN
        // check if the Scan is active
        if ( MAP_llActiveTask(LL_TASK_ID_SCANNER) )
        {
          return MAP_llSelectTaskScan(secTaskID,timeGap);
        }
        // check if the Init is active
        else if ( MAP_llActiveTask(LL_TASK_ID_INITIATOR) )
        {
          return MAP_llSelectTaskInit(secTaskID,timeGap);
        }
#ifdef USE_PERIODIC_ADV
        else if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_ADVERTISER) )
        {
          return MAP_llSelectTaskPeriodicAdv(secTaskID,timeGap);
        }
#endif // USE_PERIODIC_ADV
        else // there is no other secondary task
        {
          // so check if even the current task is still active
          return( (MAP_llActiveTask(secTaskID))?MAP_llGetTask(secTaskID):NULL );
        }

        break;

      case LL_TASK_ID_SCANNER:
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
        if ( MAP_llActiveTask(LL_TASK_ID_SCANNER) )
        {
          timeGap = (extScanInfo->pScanParam->extScanParam[extScanIndex].scanWindow * RAT_TICKS_IN_625US) +
                     EXT_SCAN_MARGIN_TIME_RAT_TICKS;
        }
#endif // SCAN_CFG

        // check if the Init is active
        if ( MAP_llActiveTask(LL_TASK_ID_INITIATOR) )
        {
          return MAP_llSelectTaskInit(secTaskID,timeGap);
        }
#ifdef USE_PERIODIC_ADV
        else if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_ADVERTISER) )
        {
          return MAP_llSelectTaskPeriodicAdv(secTaskID,timeGap);
        }
#endif // USE_PERIODIC_ADV
        // check if the Adv is active
        else if ( MAP_llActiveTask(LL_TASK_ID_ADVERTISER) )
        {
          return MAP_llSelectTaskAdv(secTaskID,timeGap);
        }
#ifdef USE_PERIODIC_SCAN
        else if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_SCANNER) )
        {
          return MAP_llSelectTaskPeriodicScan(secTaskID,timeGap);
        }
#endif // USE_PERIODIC_SCAN
        else // there is no other secondary task
        {
          // so check if even the current task is still active
          return( (MAP_llActiveTask(secTaskID))?MAP_llGetTask(secTaskID):NULL );
        }

        break;

      case LL_TASK_ID_INITIATOR:
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
        if ( MAP_llActiveTask(LL_TASK_ID_INITIATOR) )
        {
          timeGap = (extInitInfo->pCreateConn->extInitParam[extInitIndex].scanWindow * RAT_TICKS_IN_625US) +
                     EXT_INIT_MARGIN_TIME_RAT_TICKS;
        }
#endif // INIT_CFG
#ifdef USE_PERIODIC_ADV
        if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_ADVERTISER) )
        {
          return MAP_llSelectTaskPeriodicAdv(secTaskID,timeGap);
        }
        else
#endif // USE_PERIODIC_ADV
        // check if the Adv is active
        if ( MAP_llActiveTask(LL_TASK_ID_ADVERTISER) )
        {
          return MAP_llSelectTaskAdv(secTaskID,timeGap);
        }
#ifdef USE_PERIODIC_SCAN
        // check if the Periodic Scan is active
        else if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_SCANNER) )
        {
          return MAP_llSelectTaskPeriodicScan(secTaskID,timeGap);
        }
#endif //USE_PERIODIC_SCAN
        else if ( MAP_llActiveTask(LL_TASK_ID_SCANNER) )
        {
          return MAP_llSelectTaskScan(secTaskID,timeGap);
        }
        else // there is no other secondary task
        {
          // so check if even the current task is still active
          return( (MAP_llActiveTask(secTaskID))?MAP_llGetTask(secTaskID):NULL );
        }

        break;

#ifdef USE_PERIODIC_ADV
      case LL_TASK_ID_PERIODIC_ADVERTISER:
      {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
        llPeriodicAdvSet_t *pPeriodicAdv = MAP_llGetCurrentPeriodicAdv();
        if (pPeriodicAdv != NULL)
        {
          timeGap = pPeriodicAdv->totalOtaTime;
        }
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
        // check if the Init is active
        if ( MAP_llActiveTask(LL_TASK_ID_INITIATOR) )
        {
          return MAP_llSelectTaskInit(secTaskID,timeGap);
        }
        // check if the Adv is active
        else if ( MAP_llActiveTask(LL_TASK_ID_ADVERTISER) )
        {
          return MAP_llSelectTaskAdv(secTaskID,timeGap);
        }
#ifdef USE_PERIODIC_SCAN
        else if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_SCANNER) )
        {
          return MAP_llSelectTaskPeriodicScan(secTaskID,timeGap);
        }
#endif // USE_PERIODIC_SCAN
        // check if the Scan is active
        else if ( MAP_llActiveTask(LL_TASK_ID_SCANNER) )
        {
          return MAP_llSelectTaskScan(secTaskID,timeGap);
        }
        else // there is no other secondary task
        {
          // so check if even the current task is still active
          return( (MAP_llActiveTask(secTaskID))?MAP_llGetTask(secTaskID):NULL );
        }
      }
      break;
#endif // USE_PERIODIC_ADV

#ifdef USE_PERIODIC_SCAN
      case LL_TASK_ID_PERIODIC_SCANNER:
      {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
        llPeriodicScanSet_t *pPeriodicScan = MAP_llGetCurrentPeriodicScan(PERIODIC_SCAN_STATE_SYNCED);
        if (pPeriodicScan != NULL)
        {
          timeGap = pPeriodicScan->totalOtaTime;
        }
#endif //SCAN_CFG
        // check if the Scan is active
        if ( MAP_llActiveTask(LL_TASK_ID_SCANNER) )
        {
          return MAP_llSelectTaskScan(secTaskID,timeGap);
        }
        // check if the Init is active
        else if ( MAP_llActiveTask(LL_TASK_ID_INITIATOR) )
        {
          return MAP_llSelectTaskInit(secTaskID,timeGap);
        }
#ifdef USE_PERIODIC_ADV
        else if ( MAP_llActiveTask(LL_TASK_ID_PERIODIC_ADVERTISER) )
        {
          return MAP_llSelectTaskPeriodicAdv(secTaskID,timeGap);
        }
#endif // USE_PERIODIC_ADV
        // check if the Adv is active
        else if ( MAP_llActiveTask(LL_TASK_ID_ADVERTISER) )
        {
          return MAP_llSelectTaskAdv(secTaskID,timeGap);
        }
        else // there is no other secondary task
        {
          // so check if even the current task is still active
          return( (MAP_llActiveTask(secTaskID))?MAP_llGetTask(secTaskID):NULL );
        }
      }
      break;
#endif // USE_PERIODIC_SCAN
      default:
        // current task is not a secondary task
        break;
    }
  }

  return( NULL );
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llSelectTaskAdv
 *
 * @brief       This function is used to select the adv llTask
 *              as the next secondary task
 *
 * input parameters
 *
 * @param       secTaskID  - The previous secondary task ID.
 * @param       timeGap  - previous secondary command consume time.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to selected llTask block.
 */
taskInfo_t *llSelectTaskAdv( uint8 secTaskID, uint32 timeGap )
{
  taskInfo_t *curSecTask = MAP_llGetTask( secTaskID );
  taskInfo_t *nextSecTask = MAP_llGetTask( LL_TASK_ID_ADVERTISER );

  RCL_Command *curSecCmd  = (curSecTask != NULL)?(RCL_Command *)curSecTask->command:NULL;
  RCL_Command *nextSecCmd = (nextSecTask != NULL)?(RCL_Command *)nextSecTask->command:NULL;

  // make sure current task is still active
  if ((curSecCmd != NULL) && (MAP_llActiveTask(secTaskID)))
  {
     // check if current task has enough time before next Scan
    if ( (nextSecCmd != NULL) &&
         (MAP_llTimeCompare( curSecCmd->timing.absStartTime,
                             nextSecCmd->timing.absStartTime-timeGap ) == FALSE ) )
    {
      // there is, so current task is next
      return( curSecTask );
    }
  }

  // either the curTask is not active, or it is active and there's
  // enough time before the next secondary task
  return( nextSecTask );
}
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * @fn          llSelectTaskInit
 *
 * @brief       This function is used to select the init llTask
 *              as the next secondary task
 *
 * input parameters
 *
 * @param       secTaskID  - The previous secondary task ID.
 * @param       timeGap  - previous secondary command consume time.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to selected llTask block.
 */
taskInfo_t *llSelectTaskInit( uint8 secTaskID, uint32 timeGap )
{
  taskInfo_t *curSecTask = MAP_llGetTask( secTaskID );
  taskInfo_t *nextSecTask = MAP_llGetTask( LL_TASK_ID_INITIATOR );

  RCL_Command *curSecCmd  = (curSecTask != NULL)?(RCL_Command *)curSecTask->command:NULL;
  RCL_Command *nextSecCmd = (nextSecTask != NULL)?(RCL_Command *)nextSecTask->command:NULL;

  // make sure current task is still active
  if ((curSecCmd != NULL) && (MAP_llActiveTask(secTaskID)))
  {
    // check if current task has enough time before next Scan
    if ( (nextSecCmd != NULL) &&
         (MAP_llTimeCompare( curSecCmd->timing.absStartTime,
                             nextSecCmd->timing.absStartTime-timeGap ) == FALSE ) )
    {
      // there is, so current task is next
      return( curSecTask );
    }
  }

  // either the curTask is not active, or it is active and there's
  // enough time before the next secondary task
  return( nextSecTask );
}
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * @fn          llSelectTaskScan
 *
 * @brief       This function is used to select the scan llTask
 *              as the next secondary task
 *
 * input parameters
 *
 * @param       secTaskID  - The previous secondary task ID.
 * @param       timeGap  - previous secondary command consume time.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to selected llTask block.
 */
taskInfo_t *llSelectTaskScan( uint8 secTaskID, uint32 timeGap )
{
  taskInfo_t *curSecTask = MAP_llGetTask( secTaskID );
  taskInfo_t *nextSecTask = MAP_llGetTask( LL_TASK_ID_SCANNER );

  RCL_Command *curSecCmd  = (curSecTask != NULL)?(RCL_Command *)curSecTask->command:NULL;
  RCL_Command *nextSecCmd = (nextSecTask != NULL)?(RCL_Command *)nextSecTask->command:NULL;

  // make sure current task is still active
  if ((curSecCmd != NULL) && (MAP_llActiveTask(secTaskID)))
  {
    if ( (nextSecCmd != NULL) &&
         (MAP_llTimeCompare( curSecCmd->timing.absStartTime,
                             nextSecCmd->timing.absStartTime- timeGap ) == FALSE ) )
    {
      // there is, so current task is next
      return( curSecTask );
    }
  }

  // either the curTask is not active, or it is active and there's
  // enough time before the next secondary task
  return( nextSecTask );
}
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
#ifdef USE_PERIODIC_SCAN
/*******************************************************************************
 * @fn          llSelectTaskPeriodicScan
 *
 * @brief       This function is used to select the periodic scan llTask
 *              as the next secondary task
 *
 * input parameters
 *
 * @param       secTaskID  - The previous secondary task ID.
 * @param       timeGap  - previous secondary command consume time.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to selected llTask block.
 */
taskInfo_t *llSelectTaskPeriodicScan( uint8 secTaskID, uint32 timeGap )
{
  taskInfo_t *curSecTask = MAP_llGetTask( secTaskID );
  RCL_CmdBle5PeriodicScanner *curSecCmd  = (curSecTask != NULL)?(RCL_CmdBle5PeriodicScanner *)curSecTask->command:NULL;
  taskInfo_t *nextSecTask = MAP_llGetTask( LL_TASK_ID_PERIODIC_SCANNER );
  RCL_CmdBle5PeriodicScanner *nextSecCmd  = (RCL_CmdBle5PeriodicScanner *)nextSecTask->command;

  // check valid periodic scan command
  // the command will be null in case the selection procedure (llFindNextPeriodicScan)
  // selected the scan over the periodic scan
  if ((nextSecCmd == NULL) && (MAP_llActiveTask(LL_TASK_ID_SCANNER)))
  {
    if (secTaskID == LL_TASK_ID_SCANNER)
    {
      return( curSecTask );
    }
    nextSecTask = MAP_llGetTask( LL_TASK_ID_SCANNER );
    nextSecCmd  = (RCL_CmdBle5PeriodicScanner *)nextSecTask->command;
  }

  // make sure current task is still active
  if ( MAP_llActiveTask(secTaskID) && (curSecCmd != NULL))
  {
    // check if current task has enough time to run again before next periodic adv
    // TRUE when first parameter is GT the second parameter
    if ( MAP_llTimeCompare( curSecCmd->common.timing.absStartTime,
                            nextSecCmd->common.timing.absStartTime - timeGap ) == FALSE )
    {
      // there is, so current task is next
      return( curSecTask );
    }
  }

  // either the curTask is not active, or it is active and there's
  // enough time before the next secondary task
  return( nextSecTask );
}
#endif // USE_PERIODIC_SCAN
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
#ifdef USE_PERIODIC_ADV
/*******************************************************************************
 * @fn          llSelectTaskPeriodicAdv
 *
 * @brief       This function is used to select the periodic adv llTask
 *              as the next secondary task
 *
 * input parameters
 *
 * @param       secTaskID  - The previous secondary task ID.
 * @param       timeGap  - previous secondary command consume time.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to selected llTask block.
 */
taskInfo_t *llSelectTaskPeriodicAdv( uint8 secTaskID, uint32 timeGap )
{
  taskInfo_t *curSecTask = MAP_llGetTask( secTaskID );
  taskInfo_t *nextSecTask = MAP_llGetTask( LL_TASK_ID_PERIODIC_ADVERTISER );

  RCL_Command *curSecCmd  = (curSecTask != NULL)?(RCL_Command *)curSecTask->command:NULL;
  RCL_Command *nextSecCmd  = (RCL_Command *)nextSecTask->command;

  // Check valid periodic adv command
  // The command will be null in case the selection procedure (llFindNextPeriodicAdv)
  // Selected the ext adv over the periodic adv
  if ((nextSecCmd == NULL) && (MAP_llActiveTask(LL_TASK_ID_ADVERTISER)))
  {
    if (secTaskID == LL_TASK_ID_ADVERTISER)
    {
      return( curSecTask );
    }
    nextSecTask = MAP_llGetTask( LL_TASK_ID_ADVERTISER );
    nextSecCmd  = (RCL_Command *)nextSecTask->command;
  }

  // Make sure current task is still active
  if ((curSecCmd != NULL) && (MAP_llActiveTask(secTaskID)))
  {
     // Check if current task has enough time before next Scan
    if ( (nextSecCmd != NULL) &&
         (MAP_llTimeCompare( curSecCmd->timing.absStartTime,
                             nextSecCmd->timing.absStartTime-timeGap ) == FALSE ) )
    {
      // There is, so current task is next
      return( curSecTask );
    }
  }

  // Either the curTask is not active, or it is active and there's
  // enough time before the next secondary task
  return( nextSecTask );
}
#endif // USE_PERIODIC_ADV
#endif

/*******************************************************************************
 * @fn          llAllocTask
 *
 * @brief       This function is used to assign a free llTask block to a BLE
 *              role. Note that the llTask blocks are assigned to task state
 *              in a hard-coded fashion.
 *
 * input parameters
 *
 * @param       llTaskID  - The BLE task requesting this task block.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to llTask block, or NULL.
 */
taskInfo_t *llAllocTask( uint16 llTaskID )
{
  uint8 i;

  LL_ASSERT( llTaskList.numTasks < (maxNumConns+LL_NUM_TASK_BLOCKS) );

  // check if Advertising has already been allocated
  // Note: For AE, the same Advertiser task block can be used for all Adv Sets.
  //       So only allocate if not allocated before.
  // Note: This is temporary as lltask will be replaced with dynamic allocation.
  if (( llTaskID == LL_TASK_ID_ADVERTISER ) ||
      ( llTaskID == LL_TASK_ID_PERIODIC_ADVERTISER ) ||
      ( llTaskID == LL_TASK_ID_PERIODIC_SCANNER ))
  {
    for (i=0; i<(maxNumConns+LL_NUM_TASK_BLOCKS); i++)
    {
      // check if active, and task ID is Advertiser
      if ( (llTaskList.llTasks[i].taskState == LL_TASK_STATE_ACTIVE) &&
           (llTaskList.llTasks[i].taskID    == llTaskID) )
      {
        // already allocated, so nothing more to do here
        return( &llTaskList.llTasks[i] );
      }
    }
  }

  // find the first free block
  for (i=0; i<(maxNumConns+LL_NUM_TASK_BLOCKS); i++)
  {
    // check if free
    if ( llTaskList.llTasks[i].taskState == LL_TASK_STATE_INACTIVE )
    {
      taskInfo_t *llTask = &llTaskList.llTasks[i];

      // first, make sure this task block has either never been assigned
      // before, or is not the current task
      // Note: The plan was to make the allocation/free of taskInfo_t blocks be
      //       from heap, but never got there, so ended up in this half-ass
      //       state where the blocks are "allocated" and "freed", but the
      //       memory is still static. The TaskDone and Scheduler still index
      //       these blocks, checking if active based on taskID. That is,
      //       even if say Scan is stopped, curTask might still point to the
      //       block even when freed. However, this routine could replace
      //       the taskID, and that could mess up the Scheduler (e.g. Init
      //       replaces Scan, and Scheduler doesn't call llSetupInit because
      //       curTask == nexSecTask). To avoid that, an inactive task block
      //       will only be allocated if its taskID is invalid or it is not
      //       the current task (e.g. Scan is cancelled, then restarted).
      //       Sadly, this requires one extra task block to be allocated.
      if ( (llTask->taskID != LL_TASK_ID_NONE) &&
           (llTask == llTaskList.curTask) )
      {
        continue;
      }

      // set the task ID
      llTask->taskID  = llTaskID;

      // initialize the rest of the block
      llTask->command = 0;
      llTask->setup   = NULL;

      // set this task as active
      llTask->taskState = LL_TASK_STATE_ACTIVE;

      // mark task as active
      llTaskList.activeTasks |= llTaskID;

      // increase the number of active tasks
      llTaskList.numTasks++;

      // check if this is the first task or if we just formed a connection
      // Note: When creating a connection on the Central/Peripheral, the Init/Adv
      //       task is active until a connection is formed. When this happens,
      //       the Init/Adv task is effectively replaced by the Central/Peripheral
      //       task.
      if ( (llTaskList.numTasks == 1)      ||
           (llTaskID == LL_TASK_ID_CENTRAL) ||
           (llTaskID == LL_TASK_ID_PERIPHERAL) )
      {
        // yes, so make the current task be this task
        llTaskList.curTask = llTask;
      }

      // if there is any change in active tasks indicator,
      // notify through fast state update callback
      if ( llTaskList.activeTasks != llTaskList.lastActiveTasks )
      {
        llTaskList.lastActiveTasks = llTaskList.activeTasks;
        if ( llUserConfig.fastStateUpdateCb != NULL )
        {
          llUserConfig.fastStateUpdateCb( llUserConfig.bleStackType,
                                          (uint32) llTaskList.activeTasks );
        }
      }

      // done
      return( llTask );
    }
  }

  // not possible as we had at least one task free, but none was found!
  LL_ASSERT( FALSE );

  // nothing to return
  return( NULL );
}


/*******************************************************************************
 * @fn          llFreeTask
 *
 * @brief       This function is used to free a llTask block. It removes the
 *              pointer to the task block from the owner's structure, removes
 *              its pointer to the owner's structure, sets the task to inactive,
 *              and reduces the number of tasks by one.
 *
 *              Note: Although the task pointer is NULL'ed, the data structure
 *                    still exists, and except for when there are no more tasks,
 *                    current task will still point to this structure. This is
 *                    done to allow the scheduler to finish processing the
 *                    current task, even if it is no longer a valid task.
 *                    For this reason, the taskID is not changed here. As long
 *                    as a FreeTask isn't followed by a AllocateTask, this is
 *                    not an issue. This happens in two places: when Init
 *                    trannsitions to Central and when Adv transitions to Central.
 *                    But in these cases, the the connection task effectively
 *                    replaces the discovery task, and the current task becomes
 *                    the Peripheral or Central, so that the schedule processes the
 *                    connection task as the current task.
 *
 * input parameters
 *
 * @param       llTask - Pointer to task block to be freed.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llFreeTask( taskInfo_t **llTask )
{
  taskInfo_t *taskBlock;

  // check if this llTask has already been freed
  if ( (llTask  == NULL)  ||
       (*llTask == NULL)  ||
       ((*llTask)->taskID == LL_TASK_ID_NONE) )
  {
    // there's nothing more to do here
    return;
  }

  // save pointer to task block
  taskBlock = *llTask;

  // check if this is the last Adv Set
  // Note: For AE, the same Advertiser task block can be used for all Adv Sets.
  //       So only free when there's only no enabled Adv Sets left.
  // Note: This is temporary as lltask will be replaced with dynamic allocation.
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  if ( (taskBlock->taskID == LL_TASK_ID_NONE) ||
       ((taskBlock->taskID == LL_TASK_ID_ADVERTISER) && (MAP_LL_CountAdvSets(LE_COUNT_ENABLED_ADV_SETS) != 0)))
#else
  if (taskBlock->taskID == LL_TASK_ID_NONE)
#endif // ADV_NCONN_CFG || ADV_CONN_CFG
  {
    // there's nothing more to do here
    return;
  }

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))

  if (taskBlock->taskID == LL_TASK_ID_ADVERTISER)
  {
    if (numActiveAdvSets == 1)
    {
      MAP_osal_mem_free(pNextAdvSet);
      pNextAdvSet = NULL;
      numActiveAdvSets = 0;
    }
  }
#endif // ADV_NCONN_CFG || ADV_CONN_CFG

  // null the pointer to the task block in the caller's data structure
  *llTask = NULL;

  // clear the llTask role so it can be used again
  taskBlock->taskState = LL_TASK_STATE_INACTIVE;

  if ( taskBlock->taskID == LL_TASK_ID_PERIPHERAL ||
       taskBlock->taskID == LL_TASK_ID_CENTRAL )
  {
    // if peripheral or central, clear the task ID only when there is no more
    // the same kind of active tasks
    if ( MAP_llGetTask( taskBlock->taskID ) == NULL)
    {
      llTaskList.activeTasks &= ~taskBlock->taskID;
    }
  }
  else
  {
    // if neither peripheral nor central, clear the task ID anyway
    llTaskList.activeTasks &= ~taskBlock->taskID;
  }

  // reduce the number of active tasks only in case it is above zero
  if (llTaskList.numTasks > 0 )
  {
    llTaskList.numTasks--;
  }

  // check if this was the last task
  if ( llTaskList.numTasks == 0 )
  {
    // yes, so clear the current task
    llTaskList.curTask = NULL;

    // set state to IDLE/INACTIVE
    llState = LL_STATE_IDLE;
  }

  // if there is any change in active tasks indicator,
  // notify through fast state update callback
  if ( llTaskList.activeTasks != llTaskList.lastActiveTasks )
  {
    llTaskList.lastActiveTasks = llTaskList.activeTasks;

    if ( llUserConfig.fastStateUpdateCb != NULL )
    {
      llUserConfig.fastStateUpdateCb( llUserConfig.bleStackType,
                                      (uint32) llTaskList.activeTasks );
    }
  }

  return;
}


/*******************************************************************************
 * @fn          llGetCurrentTask
 *
 * @brief       This function is used to return a pointer to the current active
 *              task.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      Pointer to the currently active task block, or NULL.
 */
taskInfo_t *llGetCurrentTask( void )
{
  if ( llTaskList.numTasks > 0 )
  {
    return( llTaskList.curTask );
  }

  // no active task
  return( NULL );
}

/*******************************************************************************
 * @fn          llSchedulerGetCsCmd
 *
 * @brief       This function will search in the system task list if the CS task
 *              is active and will return the pointer to the CS task info
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      Pointer to the CS task in the task list or NULL
 */
RCL_Command *llSchedulerGetCsCmd( void )
{
  taskInfo_t *csTask = NULL;

  csTask = llGetTask(LL_TASK_ID_CS);

  if (csTask != NULL )
  {
    return (RCL_Command *)csTask->command;
  }

  // No active CS task
  return NULL;
}

/*******************************************************************************
 * @fn          llGetTaskState
 *
 * @brief       This function is used to return the tasks's state. If the task
 *              exists, then it is active, by definition.
 *
 * input parameters
 *
 * @param       llTaskID - The task ID.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      LL_TASK_STATE_ACTIVE or LL_TASK_STATE_INACTIVE
 */
uint8 llGetTaskState( uint16 llTaskID )
{
  uint8 i;

  // find the task given by task ID
  for (i=0; i<(maxNumConns+LL_NUM_TASK_BLOCKS); i++)
  {
    if ( llTaskList.llTasks[i].taskID == llTaskID )
    {
      return( llTaskList.llTasks[i].taskState );
    }
  }

  return( LL_TASK_STATE_INACTIVE );
}


/*******************************************************************************
 * @fn          llActiveTask
 *
 * @brief       This function is used to check if a tasks's state is active.
 *
 * input parameters
 *
 * @param       llTaskID - The task ID.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      TRUE:  Task is active.
 *              FALSE: Task is inactive.
 */
uint8 llActiveTask( uint16 llTaskID )
{
  uint8 i;

  // find the task given by task ID
  for (i=0; i<(maxNumConns+LL_NUM_TASK_BLOCKS); i++)
  {
    if ( (llTaskList.llTasks[i].taskID    == llTaskID) &&
         (llTaskList.llTasks[i].taskState == LL_TASK_STATE_ACTIVE) )
    {
      return( TRUE );
    }
  }

  return( FALSE );
}


/*******************************************************************************
 * @fn          llGetActiveTasks
 *
 * @brief       This function is used to return the Active Tasks status byte.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      The one byte Active Tasks status.
 */
uint8 llGetActiveTasks( void )
{
  return( llTaskList.activeTasks );
}


/*******************************************************************************
 * @fn          llGetTask
 *
 * @brief       This function is used to return a pointer to the specified task
 *              if it is active.
 *
 * input parameters
 *
 * @param       llTaskID - The active task block based on this task ID.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      Pointer to the specified task, or NULL if inactive.
 */
taskInfo_t *llGetTask( uint16 llTaskID )
{
  uint8 i;

  // find the task given by task ID
  for (i=0; i<(maxNumConns+LL_NUM_TASK_BLOCKS); i++)
  {
    if ( (llTaskList.llTasks[i].taskID == llTaskID) &&
         (llTaskList.llTasks[i].taskState == LL_TASK_STATE_ACTIVE) )
    {
      return( &llTaskList.llTasks[i] );
    }
  }

  return( NULL );
}


/*******************************************************************************
 * @fn          llGetNumTasks
 *
 * @brief       This function is used to return the number of active tasks.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      Number of active tasks.
 */
uint8 llGetNumTasks( void )
{
  return( llTaskList.numTasks );
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llExtAdvSchedSetup
 *
 * @brief       This function is used to perform any scheduler setup just before
 *              posting the command.
 *
 * input parameters
 *
 * @param       llTask - Pointer to task information structure.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llExtAdvSchedSetup( taskInfo_t *llTask )
{
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );

  // Sanity Check
  LL_ASSERT( (llTask->taskID == LL_TASK_ID_ADVERTISER) );

  // Sanity Check
  LL_ASSERT( pAdvSet != NULL );

  if (( pAdvSet != NULL ) && ( llTask->taskID == LL_TASK_ID_ADVERTISER ))
  {
    if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
    {
      // check if the start time is already in the past
      // TRUE when first param is greater than second.
      if ( MAP_llTimeCompare( MAP_llGetCurrentTime()+LL_SCHED_START_IMMED_PAD,
                            ((RCL_Command *)pAdvSet->pRfCmds)->timing.absStartTime ) )
      {
        ((RCL_Command *)pAdvSet->pRfCmds)->timing.absStartTime = MAP_llGetCurrentTime() + LL_SCHED_START_IMMED_PAD;
        pAdvSet->advStartTime = ((RCL_Command *)pAdvSet->pRfCmds)->timing.absStartTime;
      }

      if ( pAdvSet->pAdvParam->txPower == AE_TX_POWER_NO_PREFERENCE )
      {
        pAdvSet->txPowerIndex = curTxPowerVal;
      }
      aeRf_t *pRf = (aeRf_t *)pAdvSet->pRfCmds;
      pRf->advCmd.txPower = pAdvSet->txPowerIndex;
    }

    // pointer to first radio operation command
    pAdvSet->llTask->command = (uint32)pAdvSet->pRfCmds;

    // set RF events
    ((RCL_Command *)pAdvSet->pRfCmds)->runtime.lrfCallbackMask.value = LRF_EventOpError.value;
    ((RCL_Command *)pAdvSet->pRfCmds)->runtime.rclCallbackMask.value |= RCL_EventLastCmdDone.value;

    // check if this is a Scannable advertisement
    if ( TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) )
    {
      ((RCL_Command *)pAdvSet->pRfCmds)->runtime.lrfCallbackMask.value |= LRF_EventRxEmpty.value;
    }

    // enable RxEntryDone for directed adv
    if ( TST_AE_PROPS_DIR(pAdvSet->pAdvParam->eventProps) )
    {
      ((RCL_Command *)pAdvSet->pRfCmds)->runtime.lrfCallbackMask.value |= LRF_EventRxOk.value;
    }

    // check if any start event is requried
    if ( (pAdvSet->pAdvParam->notifyEnableFlags & AE_NOTIFY_ENABLE_ADV_SET_START) ||
         (pAdvSet->pAdvParam->notifyEnableFlags & AE_NOTIFY_ENABLE_ADV_START) )
    {
      // enable the command started event
      ((RCL_Command *)pAdvSet->pRfCmds)->runtime.rclCallbackMask.value |= RCL_EventCmdStarted.value;
    }

    // enable the RCL_EventRxEntryAvail event - this will be used to check for the SCAN_REQ packet
    ((RCL_Command *)pAdvSet->pRfCmds)->runtime.rclCallbackMask.value |= RCL_EventRxEntryAvail.value;
    ((RCL_Command *)pAdvSet->pRfCmds)->runtime.lrfCallbackMask.value |= LRF_EventRxOk.value;

    // only if address resolution is enabled
    if ( privInfo.addrResolution )
    {
      // check type of advertisement
      // Note: No Scan/Init response when advertising non-connectable.
      if ( TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) ||
           TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) )
      {
        // Enable RCL_EventRxEntryAvail event - to be able to receive privIgn CBs
	    ((RCL_Command *)pAdvSet->pRfCmds)->runtime.rclCallbackMask.value |= RCL_EventRxEntryAvail.value;
	    ((RCL_Command *)pAdvSet->pRfCmds)->runtime.lrfCallbackMask.value |= LRF_EventRxOk.value;
      }
    }

    // save the the last schedule time for calculate number of missed packets
    pNextAdvSet->timeScheduled = pAdvSet->advStartTime;
    // update duration expiration time
    if ((pAdvSet->pEnable->duration) && (pAdvSet->durationExpireTime == 0))
    {
      pAdvSet->durationExpireTime = pAdvSet->advStartTime + (pAdvSet->pEnable->duration * RAT_TICKS_IN_10MS);
    }
  }
  return;
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
#ifdef USE_PERIODIC_ADV
/*******************************************************************************
 * @fn          llPeriodicAdvSchedSetup
 *
 * @brief       This function is used to perform any scheduler setup just before
 *              posting the command.
 *
 * input parameters
 *
 * @param       llTask - Pointer to task information structure.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llPeriodicAdvSchedSetup( taskInfo_t *llTask )
{
  llPeriodicAdvSet_t *pPeriodicAdv = MAP_llGetCurrentPeriodicAdv();

  // decrease the priority
  if ((pPeriodicAdv != NULL) && (pPeriodicAdv->intPriority > LL_QOS_LOW_PRIORITY))
  {
    pPeriodicAdv->intPriority = LL_QOS_LOW_PRIORITY;
  }
#ifdef RTLS_CTE
  // check that the CTE sampling is enable
  if (llCteSamples.pAutoCopyBuffers != NULL)
  {
    // disable the antenna switch
    llRfOverrideCteValue(0,RFC_FWPAR_CTE_ANT_SWITCH,RFC_CTE_ANT_SWITCH_OFFSET);
    // disable the auto copy
    llRfOverrideCteValue(0,RFC_FWPAR_CTE_AUTO_COPY,RFC_CTE_AUTO_COPY_OFFSET);
  }
#endif
}
#endif // USE_PERIODIC_ADV
#endif // ADV_NCONN_CFG | ADV_CONN_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * @fn          llExtScanSchedSetup
 *
 * @brief       This function is used to perform any scheduler setup just before
 *              posting the command.
 *
 * input parameters
 *
 * @param       llTask - Pointer to task information structure.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llExtScanSchedSetup( taskInfo_t *llTask )
{
  // Sanity Check
  LL_ASSERT( (llTask->taskID == LL_TASK_ID_SCANNER) );

  // only if address resolution is enabled
  if ( privInfo.addrResolution )
  {
      // check if RPA has changed, and if so, update Scan address
      // Note: Assumes if the local IRK is valid, then the local RPA exists.
      if ( LL_IS_ADDR_TYPE_RPA(extScanInfo->ownAddrType) &&
           !MAP_LL_PRIV_IsZeroIRK( resolvingList[LOCAL_RL_INDEX].IRK ) )

      {
        // update the RPA (whether it has changed or not)
        // Note: scanParam.pDeviceAddr points to scanInfo->ownAddr.
        // Note: It would take just as long (longer actually) to first compare
        //       the address to see if it has changed. Faster to just copy.
        // Note: Sadly, we can't just point scanParam.pDeviceAddr to the RL RPA
        //       (if valid) as we could end up changing it while the radio is
        //       using it.
        MAP_osal_memcpy( extScanInfo->ownAddr,
                         resolvingList[LOCAL_RL_INDEX].RPA,
                         B_ADDR_LEN );

        // In devices using RF Driver (rflib instead of rcl), the command
        // itself will point to extScanInfo->ownAddr which naturally updates
        // the RPA.
        // When using RCL, the RF Command needs to be directly updated
        // Copy the address into the command.
        MAP_osal_memcpy( extScanCmd.ctx->ownA,
                         resolvingList[LOCAL_RL_INDEX].RPA,
                         B_ADDR_LEN );
      }
  }

#if defined(CC13X2P)
  // setup RF Setup and Radio Command for Tx Power PA based on current Tx Power
  MAP_llTxPwrSwitchPA( curTxPowerVal, (uint32 *)&extScanCmd );
#endif // CC13X2P

#ifdef RTLS_CTE
  // check that the CTE sampling is enable
  if (llCteSamples.pAutoCopyBuffers != NULL)
  {
    // disable the antenna switch
    llRfOverrideCteValue(0,RFC_FWPAR_CTE_ANT_SWITCH,RFC_CTE_ANT_SWITCH_OFFSET);
    // disable the auto copy
    llRfOverrideCteValue(0,RFC_FWPAR_CTE_AUTO_COPY,RFC_CTE_AUTO_COPY_OFFSET);
  }
#endif // RTLS_CTE

#ifdef USE_PERIODIC_SCAN
  // reset the number of missed scan windows -
  // used with periodic scan in schedule selection procedure
  extScanNumMissed = 0;
  // set the accept sync info bit
  MAP_llUpdateExtScanAcceptSyncInfo();
#endif // USE_PERIODIC_SCAN
  return;
}

#ifdef USE_PERIODIC_SCAN
/*******************************************************************************
 * @fn          llPeriodicScanSchedSetup
 *
 * @brief       This function is used to perform any scheduler setup just before
 *              posting the command.
 *
 * input parameters
 *
 * @param       llTask - Pointer to task information structure.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llPeriodicScanSchedSetup( taskInfo_t *llTask )
{
  llPeriodicScanSet_t *pPeriodicScan = MAP_llGetCurrentPeriodicScan(PERIODIC_SCAN_STATE_SYNCED);

#ifdef RTLS_CTE
  // check that the CTE sampling is enable
  if (llCteSamples.pAutoCopyBuffers != NULL)
  {
    // in case the IQ sampling is enable
    if (pPeriodicScan->cteInfo.enable == LL_CTE_SAMPLING_ENABLE)
    {
      // Enable the Packet received with CRC error interrupt and copy samples interrupt
      llTask->rfEvents |= (RF_EventRxNOk | RF_EventSamplesEntryDone);
      // Set the antenna switch
      llRfOverrideCteValue((uint32)(pPeriodicScan->cteInfo.pAntenna),RFC_FWPAR_CTE_ANT_SWITCH,RFC_CTE_ANT_SWITCH_OFFSET);

      // set the auto copy struct pointer in RF memory
      llRfOverrideCteValue((uint32)(&llCteSamples.autoCopy),RFC_FWPAR_CTE_AUTO_COPY,RFC_CTE_AUTO_COPY_OFFSET);
      // reset the auto copy counter
      llCteSamples.autoCopyCompleted = 0;
      // set the CTE max count
      llCteSamples.autoCopy.cteCopyLimitCount = pPeriodicScan->cteInfo.count;
    }
    else
    {
      // Disable the Packet received with CRC error interrupt and copy samples interrupt
      llTask->rfEvents &= ~(RF_EventRxNOk | RF_EventSamplesEntryDone);
      // disable the antenna switch
      llRfOverrideCteValue(0,RFC_FWPAR_CTE_ANT_SWITCH,RFC_CTE_ANT_SWITCH_OFFSET);
      // disable the auto copy
      llRfOverrideCteValue(0,RFC_FWPAR_CTE_AUTO_COPY,RFC_CTE_AUTO_COPY_OFFSET);
    }
  }
#endif // RTLS_CTE

  return;
}
#endif // USE_PERIODIC_SCAN
#endif // SCAN_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * @fn          llExtInitSchedSetup
 *
 * @brief       This function is used to perform any scheduler setup just before
 *              posting the command.
 *
 * input parameters
 *
 * @param       llTask - Pointer to task information structure.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llExtInitSchedSetup( taskInfo_t *llTask )
{
  // only if address resolution is enabled
  if ( privInfo.addrResolution )
  {
    if ( !MAP_LL_PRIV_IsZeroIRK( resolvingList[LOCAL_RL_INDEX].IRK ) )
    {
      // Update the own address in the command - needed if the own address might have changed
      MAP_osal_memcpy(extInitParam.ownA, resolvingList[LOCAL_RL_INDEX].RPA, B_ADDR_LEN);
    }
  }


#if defined(CC13X2P)
  // setup RF Setup and Radio Command for Tx Power PA based on current Tx Power
  MAP_llTxPwrSwitchPA( curTxPowerVal, (uint32 *)&extInitCmd );
#endif // CC13X2P

#ifdef RTLS_CTE
  // check that the CTE sampling is enable
  if (llCteSamples.pAutoCopyBuffers != NULL)
  {
    // disable the antenna switch
    llRfOverrideCteValue(0,RFC_FWPAR_CTE_ANT_SWITCH,RFC_CTE_ANT_SWITCH_OFFSET);
    // disable the auto copy
    llRfOverrideCteValue(0,RFC_FWPAR_CTE_AUTO_COPY,RFC_CTE_AUTO_COPY_OFFSET);
  }
#endif // RTLS_CTE

  return;
}
#endif // INIT_CFG
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llLinkSchedSetup
 *
 * @brief       This function is used to perform any scheduler setup just before
 *              posting the command.
 *
 * input parameters
 *
 * @param       llTask - Pointer to task information structure.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llLinkSchedSetup( taskInfo_t *llTask )
{
  // get connection information
  llConnState_t *connPtr        = MAP_llDataGetConnPtr( llConns.currentConn );
  uint32 startTime = 0;
  RCL_Command *connCmd = (RCL_Command *)connPtr->llTask->command;

  // clear the output parameters
  connOutput = RCL_StatsConnection_DefaultRuntime();

#if defined(CC13X2P)
  // setup RF Setup and Radio Command for Tx Power PA based on current Tx Power
  MAP_llTxPwrSwitchPA( curTxPowerVal, (uint32 *)llTask->command );
#endif // CC13X2P

  /***********************************************************/
  /**** Abort Conn Event According to Max Conn Length ********/
  /***********************************************************/

  // Activate the connection event timer only in case
  // we have more than 1 connection.
  if (llConns.numActiveConns > 1)
  {
    // Adjust the max connection time length -  Reduce margin for timer handling (LL_MARGIN_TIME_FOR_TIMER_HANDLING_RAT_TICKS).
    // Add the start time of the scheduled connection to it's max connection length to receive the time the timer expired.
    // *** Please Notice *** the timer would get an absolute time of the rat ticks to expire.
    connPtr->connMaxTimeLength = startTime + connPtr->connMaxTimeLength - LL_MARGIN_TIME_FOR_TIMER_HANDLING_RAT_TICKS;

    // Check the connection id is valid before activating the timer.
    if (connPtr->connId < llConns.numActiveConns)
    {
      // calculate connPtr->connMaxTimeLength while start time is 0
      // in order to set the stop time relative to the start time
      connCmd->timing.relHardStopTime = connPtr->connMaxTimeLength;
    }
  }

#ifdef RTLS_CTE
  if ((connPtr != NULL) && (llCteSamples.pAutoCopyBuffers != NULL))
  {
    // in case the sampling is enable
    if (llCte[connPtr->connId].initiator.samplingEnable == LL_CTE_SAMPLING_ENABLE)
    {
      // Enable the Packet received with CRC error interrupt and copy samples interrupt
      llTask->rfEvents |= (RF_EventRxNOk | RF_EventSamplesEntryDone);
      // Set the antenna switch
      llRfOverrideCteValue((uint32)(llCte[connPtr->connId].initiator.pAntenna),RFC_FWPAR_CTE_ANT_SWITCH,RFC_CTE_ANT_SWITCH_OFFSET);
      // set the auto copy struct pointer in RF memory
      llRfOverrideCteValue((uint32)(&llCteSamples.autoCopy),RFC_FWPAR_CTE_AUTO_COPY,RFC_CTE_AUTO_COPY_OFFSET);
      // reset the auto copy counter
      llCteSamples.autoCopyCompleted = 0;
      // set the CTE max count as 1 CTE (relevant only to periodic scan)
      llCteSamples.autoCopy.cteCopyLimitCount = 1;
    }
    else
    {
      // Disable the Packet received with CRC error interrupt and copy samples interrupt
      llTask->rfEvents &= ~(RF_EventRxNOk | RF_EventSamplesEntryDone);
      // disable the antenna switch
      llRfOverrideCteValue(0,RFC_FWPAR_CTE_ANT_SWITCH,RFC_CTE_ANT_SWITCH_OFFSET);
      // disable the auto copy
      llRfOverrideCteValue(0,RFC_FWPAR_CTE_AUTO_COPY,RFC_CTE_AUTO_COPY_OFFSET);
    }
  }
#endif // RTLS_CTE
  return;
}
#endif // ADV_CONN_CFG | INIT_CFG

////////////////////////////////////////////////////////////////////////////////
// RAT Compare Callback
// Note: This properly belongs in ll_isr.c
////////////////////////////////////////////////////////////////////////////////
void llCmdStartedEventHandle( void )
{
  // check if Advertiser or Scanner
  switch( llState )
  {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
    case LL_STATE_EXT_ADV:
    {
      advSet_t *pAdvSet;

#ifdef DEBUG_GPIO_CONN
  GPIO_writeDio(HAL_GPIO_1, 1);
#endif // DEBUG_GPIO_CONN

#ifdef DEBUG_GPIO_ADV_SCAN
  GPIO_writeDio(HAL_GPIO_1, 1);
#endif // DEBUG_GPIO_ADV_SCAN

      // get current Adv Set
      pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );

      // got pointer
      if ( pAdvSet && pAdvSet->pAdvParam )
      {
        // check if this is the first advertisement event
        if ( pAdvSet->firstAdvEvt == 0 )
        {
          // check if this callback has been enabled
          if ( pAdvSet->pAdvParam->notifyEnableFlags & AE_NOTIFY_ENABLE_ADV_SET_START )
          {
            MAP_llExtAdvCBack( LL_CBACK_ADV_START_AFTER_ENABLE, &aeCurHandle );
          }
          pAdvSet->firstAdvEvt = 1;
        }
        else // not the first advertisement
        {
          // check if this callback has been enabled
          if ( pAdvSet->pAdvParam->notifyEnableFlags & AE_NOTIFY_ENABLE_ADV_START )
          {
            MAP_llExtAdvCBack( LL_CBACK_ADV_START, &aeCurHandle );
          }
        }
      }
    }
    break;
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
    case LL_STATE_SCAN:

#ifdef DEBUG_GPIO_ADV_SCAN
  GPIO_writeDio(HAL_GPIO_2, 1);
#endif // DEBUG_GPIO_ADV_SCAN

      // check if this is the first time
      switch( extScanInfo->scanStartState )
      {
        case AE_SCAN_START_STATE_FIRST:
          // setup duration and period, if applicable
          // Note: Duration is disabled, or 10ms .. 655.35ms.
          // Note: Duration refers to time spent scanning on both the primary and
          //       secondary channels.
          // Note: Period is disabled, or 1.28s to 83,884s.
          (void)MAP_llStartDurationTimer( LL_EVT_EXT_SCAN_TIMEOUT,
                                          (extScanInfo->pEnable->duration * 10) );

          extScanInfo->scanStartState = AE_SCAN_START_STATE_SECOND;

          // first, so indicate Scan started
          MAP_llExtAdvCBack( LL_CBACK_EXT_SCAN_START, NULL );
          break;

        case AE_SCAN_START_STATE_SECOND:
          // invoke end of Scan Interval event
          // Note: If the start of Scan, then end of previous scan interval.
          // TODO: THIS IS NOT NECESSARILY TRUE FOR COMBO ROLES.
          MAP_llExtAdvCBack( LL_CBACK_EXT_SCAN_INTERVAL_END, NULL );
          break;

        case AE_SCAN_START_STATE_NEXT:
          // this state is when we restart at next period
          // Note: If we don't do this, we'll up sending a scan interval end
          //       when we should be sending a scan window end.
          extScanInfo->scanStartState = AE_SCAN_START_STATE_SECOND;
          break;

        default:
          break;
      }
      break;
#endif // SCAN_CFG

#ifdef DEBUG_GPIO_CONN
    case LL_STATE_INIT:
      GPIO_writeDio(HAL_GPIO_2, 1);
      break;

    case LL_STATE_CONN_CENTRAL:
      GPIO_writeDio(HAL_GPIO_3, 1);
      break;

    case LL_STATE_CONN_PERIPHERAL:
      GPIO_writeDio(HAL_GPIO_4, 1);
      break;
#endif // DEBUG_GPIO_CONN

    case LL_STATE_SDAA_RX_WINDOW:
      // when SDAA module is enabled channels are scaned for noise level.
      MAP_LL_SDAA_SampleRXWindow();
      break;

    default:
      break;
  }
  return;
}

/*******************************************************************************
 * @fn          llScheduleTask
 *
 * @brief       This routine sets up the next task.
 *
 *              Note: The use of the Timer 2 Delta counter will cause a time
 *                    delay in the system that can range from 0 to 624.96875us
 *                    before every active connection interval.
 *
 * input parameters
 *
 * @param       llTask - Pointer to task information.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llScheduleTask( taskInfo_t *llTask )
{
  RCL_CommandStatus status;

  LL_ASSERT( llTask != NULL );

  if ( llTask == NULL ) return;

  // set this task as the current task
  llTaskList.curTask = llTask;
  // check if the start trigger is based on an absolute start time
  if ( ((RCL_Command *)llTask->command)->scheduling != RCL_Schedule_Now)
  {
    // set the tasks initial start time
    // Note: This information is needed for PM.
    llTask->startTime = ((RCL_Command *)llTask->command)->timing.absStartTime;
  }
  else // either TRIGTYPE_NOW or TRIGTYPE_REL_PREV_CMD_START
  {
    // base start time relative to current time (doesn't really matter)
    llTask->startTime = MAP_llGetCurrentTime();
  }

  // task start function used to execute task specific operations
  // Note: This must be executed before any functions that use values that
  //       could be changed (i.e. overrode) by the setup function.
  if ( llTask->setup )
  {
    (*llTask->setup)(llTask);
  }

  BLE_LOG_INT_INT(0, BLE_LOG_MODULE_RF_CMD, "RF  : schedule cmd=0x%x, status=%d\n", ((RCL_Command *)(llTask->command))->cmdId, 0);

  // set the start time for the preemption mechanism
  llSetRfCmdPreemptionParams(((RCL_Command *)(llTask->command))->timing.absStartTime);
  // Set the status to idle in case preemption feature stop the command
  if (((RCL_Command *)(llTask->command))->status == RCL_CommandStatus_DescheduledScheduling)
  {
    ((RCL_Command *)(llTask->command))->status = RCL_CommandStatus_Idle;
  }

  // post the command
  status = RCL_Command_submit(MAP_llScheduler_getHandle(llTask->taskID), (RCL_Command_Handle)llTask->command);

  if ((status >= RCL_CommandStatus_Error)||
      (status >= RCL_CommandStatus_Finished) ||
      (status == RCL_CommandStatus_Idle))
  {
    LL_rclRescheduleCommand((RCL_Command *)llTask->command);

    /**** UPDATE DEBUG INFO MODULE ****/
    (void)MAP_DbgInf_addErrorRec(DBGINF_ERROR_SCHED_SUBMIT_FAILS);
  }
  else
  {
    /**** UPDATE DEBUG INFO MODULE ****/
    (void)MAP_llDbgInf_addSchedRec(llTask);
  }
  return;
}

/*******************************************************************************
 * @fn          llScheduler_getBleHandle
 *
 * @brief       This function adds the BLE Handle to the rclHandles list,
 *              Opens it and sets its status to Active if needed.
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None
 *
 * @return      BLE handle
 */
RCL_Handle llScheduler_getBleHandle( void )
{
  // Check if BLE handle is inactive
  if ( !IS_HANDLE_ACTIVE(BLE_RCL_HANDLE) )
  {
    // Open the BLE handle
    rclHandles[BLE_RCL_HANDLE].rclHandle = RCL_open(&rfClient, llUserConfig.lrfConfigPtr);
    // Set status to active
    rclHandles[BLE_RCL_HANDLE].state = HANDLE_ACTIVE;
  }
  // Return BLE handle
  return rclHandles[BLE_RCL_HANDLE].rclHandle;
}

/*******************************************************************************
 * @fn          llScheduler_getHandle
 *
 * @brief       This function returns the RCL handle based on the given taskID.
 *              It also opens the needed RCL handle and closes the other, and
 *              adds it to the RCL handles list.
 *
 * input parameters
 *
 * @param       taskID - can either be the CS task, or all other BLE tasks.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      Standard BLE Handle, or the CS Handle
 */
RCL_Handle llScheduler_getHandle( uint16 taskID )
{
  // Check if taskID is a standard BLE task (not CS)
  if ( (taskID < LL_TASK_ID_CS) || (taskID == LL_TASK_ID_STANDARD_BLE) )
  {
    // Check if the CS handle is open
    if ( IS_HANDLE_ACTIVE(CS_RCL_HANDLE) )
    {
      // Close the CS handle since this is a standard BLE task
      llScheduler_rclClose(CS_RCL_HANDLE);
    }

    // Check if BLE handle is closed
    if ( !IS_HANDLE_ACTIVE(BLE_RCL_HANDLE) )
    {
      // Open the BKE handle
      rclHandles[BLE_RCL_HANDLE].rclHandle = RCL_open(&rfClient, llUserConfig.lrfConfigPtr);
      // Set status to Active
      rclHandles[BLE_RCL_HANDLE].state = HANDLE_ACTIVE;
    }
    // Return the BLE handle
    return rclHandles[BLE_RCL_HANDLE].rclHandle;
  }
  else
  {
    // Check if BLE handle is open
    if ( IS_HANDLE_ACTIVE(BLE_RCL_HANDLE) )
    {
      // Close it since we need the CS handle
      llScheduler_rclClose(BLE_RCL_HANDLE);
    }

    // Check if the CS handle is closed
    if ( !IS_HANDLE_ACTIVE(CS_RCL_HANDLE) )
    {
      // Open the CS handle
      rclHandles[CS_RCL_HANDLE].rclHandle = RCL_open(&rfClient, llUserConfig.lrfConfigCsPtr);
      // Set status to Active
      rclHandles[CS_RCL_HANDLE].state = HANDLE_ACTIVE;
    }
    // return CS handle
    return rclHandles[CS_RCL_HANDLE].rclHandle;
  }
}

/*******************************************************************************
 * @fn          llScheduler_getSwitchTime
 *
 * @brief       This function returns the switch time of a given task.
 *              A task could be a CS task or Standard BLE Task (all others)
 *              If the current task handle is active, the switch time is 0.
 *              Otherwise it is 120us.
 *
 * input parameters
 *
 * @param       taskID - the task ID to get switch time for
 *
 * output parameters
 *
 * @param       None
 *
 * @return      switch time
 */
uint32 llScheduler_getSwitchTime(uint16 taskID)
{
  // If this is a standard BLE task
  if ( (taskID < LL_TASK_ID_CS) || (taskID == LL_TASK_ID_STANDARD_BLE) )
  {
    // If BLE handle is open
    if ( IS_HANDLE_ACTIVE(BLE_RCL_HANDLE) )
    {
      // Standard BLE task and BLE handle open
      // No switch time
      return 0;
    }
    else
    {
      // Standarb BLE task but handle not open
      // Switch time 120us
      return RAT_TICKS_IN_120US;
    }
  }

  // If this is a CS task
  if ( taskID == LL_TASK_ID_CS )
  {
    // If the CS handle is open
    if ( IS_HANDLE_ACTIVE(CS_RCL_HANDLE) )
    {
      // CS task and CS handle open
      // No switch time
      return 0;
    }
    else
    {
      // CS task and CS handle closed
      // Switch time 120us
      return RAT_TICKS_IN_120US;
    }
  }
  return 0;
}

/*******************************************************************************
 * @fn          llScheduler_rclClose
 *
 * @brief       Close the RCL given RCL handle and set status to inactive.
 *
 * input parameters
 *
 * @param       handleType - BLE_RCL_HANDLE or CS_RCL_HANDLE
 *
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llScheduler_rclClose(uint8 handleType)
{
  // CS Handle should be closed
  RCL_close(rclHandles[handleType].rclHandle);
  rclHandles[handleType].state = HANDLE_INACTIVE;
}
/*******************************************************************************
 */
