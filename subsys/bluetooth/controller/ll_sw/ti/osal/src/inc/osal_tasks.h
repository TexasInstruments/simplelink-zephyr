/******************************************************************************

 @file  osal_tasks.h

 @brief This file contains the OSAL Task definition and manipulation functions.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2004 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifndef OSAL_TASKS_H
#define OSAL_TASKS_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */
#define TASK_NO_TASK      ICALL_UNDEF_DEST_ID

/*********************************************************************
 * TYPEDEFS
 */

/*
 * Event handler function prototype
 */
typedef uint32 (*pTaskEventHandlerFn)( unsigned char task_id, uint32 event );

/*********************************************************************
 * GLOBAL VARIABLES
 */

extern const pTaskEventHandlerFn tasksArr[];
extern const uint8 tasksCnt;
/*********************************************************************
 * FUNCTIONS
 */

/*
 * Call each of the tasks initialization functions.
 */
extern void osalInitTasks( void );

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* OSAL_TASKS_H */
