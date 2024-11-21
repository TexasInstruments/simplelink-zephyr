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
 *  @file       osal.h
 *  @brief      This API allows the software components in the Z-Stack to be written
 *  independently of the specifics of the operating system, kernel, or
 *  tasking environment (including control loops or connect-to-interrupt
 *  systems).
 */

#ifndef OSAL_H
#define OSAL_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */

#include <limits.h>

#include "comdef.h"
#include "osal_memory.h"
#include "osal_timers.h"

#include <icall.h>

/*********************************************************************
 * MACROS
 */
/// @cond NODOC
#if ( UINT_MAX == 65535 ) /* 8-bit and 16-bit devices */
  #define osal_offsetof(type, member) ((uint16) &(((type *) 0)->member))
#else /* 32-bit devices */
  #define osal_offsetof(type, member) ((uint32) &(((type *) 0)->member))
#endif

#define OSAL_MSG_NEXT(msg_ptr)      ((osal_msg_hdr_t *) (msg_ptr) - 1)->next

#define OSAL_MSG_Q_INIT(q_ptr)      *(q_ptr) = NULL

#define OSAL_MSG_Q_EMPTY(q_ptr)     (*(q_ptr) == NULL)

#define OSAL_MSG_Q_HEAD(q_ptr)      (*(q_ptr))

#define OSAL_MSG_LEN(msg_ptr)      ((osal_msg_hdr_t *) (msg_ptr) - 1)->len

#define OSAL_MSG_ID(msg_ptr)      ((osal_msg_hdr_t *) (msg_ptr) - 1)->dest_id
/// @endcond // NODOC
/*********************************************************************
 * CONSTANTS
 */


// LL Events
#define LL_EVT_NONE                                    0x00000000u
#define LL_EVT_POST_PROCESS_RF                         0x00000001u
#define LL_EVT_INIT_LAST_CMD_DONE                      0x00000002u
#define LL_EVT_INIT_LAST_CMD_DONE_RX_ERR               0x00000004u
#define LL_EVT_CENTRAL_CONN_CREATED                    0x00000008u
#define LL_EVT_CENTRAL_CONN_CANCELLED                  0x00000010u
#define LL_EVT_EXT_SCAN_TIMEOUT                        0x00000020u
#define LL_EVT_EXT_ADV_TIMEOUT                         0x00000040u
#define LL_EVT_INIT_LAST_CMD_DONE_CONNECT              0x00000080u
#define LL_EVT_PERIODIC_SCAN_CANCELLED                 0x00000100u
#define LL_EVT_RESET_SYSTEM_HARD                       0x00000200u
#define LL_EVT_RESET_SYSTEM_SOFT                       0x00000400u
#define LL_EVT_CONN_DISCONNECTED_IMMED                 0x00000800u
#define LL_EVT_ADDRESS_RESOLUTION_TIMEOUT              0x00001000u
#define LL_EVT_INIT_DONE                               0x00002000u
#define LL_EVT_OUT_OF_MEMORY                           0x00004000u
#define LL_EVT_CONN_RX_AVAIL                           0x00008000u
#define LL_EVT_ADV_LAST_CMD_DONE                       0x00010000u
#define LL_EVT_SCAN_LAST_CMD_DONE                      0x00020000u
#define LL_EVT_CENTRAL_LAST_CMD_DONE                   0x00040000u
#define LL_EVT_PERIPHERAL_LAST_CMD_DONE                0x00080000u
#define LL_EVT_SCAN_RX_AVAIL                           0x00100000u
#define LL_EVT_PERIODIC_SCAN_LAST_CMD_DONE             0x00200000u
#define LL_EVT_PERIODIC_SCAN_RX_AVAIL                  0x00400000u
#define LL_EVT_RESCHEDULE                              0x00800000u
#define LL_EVT_CMD_STARTED                             0x01000000u
#define LL_EVT_PERIODIC_ADV_LAST_CMD_DONE              0x02000000u
#define LL_EVT_PERIODIC_ADV_TX_BUFF_FINISHED           0x04000000u
#define LL_EVT_CONN_TX_BUFF_FINISHED                   0x08000000u
#define LL_EVT_ADV_RX_AVAIL                            0x10000000u
#define LL_EVT_ADV_TX_BUFF_FINISHED                    0x20000000u
#define LL_EVT_INIT_RX_ENTRY_DONE                      0x40000000u

#define SYS_EVENT_MSG                                  0x80000000u  //!< A message is waiting event

#define SYS_RESERVED                                   SYS_EVENT_MSG

/*** Interrupts ***/
#define INTS_ALL    0xFF      //!< All interrupts

/*********************************************************************
 * TYPEDEFS
 */
typedef ICall_MsgHdr osal_msg_hdr_t;    //!< ICall Message header

/// @brief OSAL Event Header
typedef struct
{
  uint8  event;           //!< OSAL event
  uint8  status;          //!< event status
} osal_event_hdr_t;

typedef void * osal_msg_q_t;      //!< osal message queue

/** @brief High resolution timer callback function type */
typedef void (*osal_highres_timer_cback_t)(void *arg);

#ifdef ICALL_LITE
/** @brief ICall Message hook*/
typedef void (*osal_icallMsg_hook_t)(void * param);
#endif /* ICALL_LITE */

/*********************************************************************
 * GLOBAL VARIABLES
 */
#ifdef ICALL_EVENTS
extern ICall_SyncHandle osal_syncHandle;      //!< OSAL synchronization handle
#else /* !ICALL_EVENTS */
extern ICall_Semaphore osal_semaphore;        //!< OSAL semaphore
#endif /* ICALL_EVENTS */
extern ICall_EntityID osal_entity;            //!< OSAL entity
extern uint_least32_t osal_tickperiod;        //!< OSAL ticket period
extern void (*osal_eventloop_hook)(void);     //!< OSAL event loop hook


/*********************************************************************
 * FUNCTIONS
 */

/// @cond NODOC
/*** Message Management ***/

/**
 * @brief Allocate message
 *
 * This function is called by a task to allocate a message buffer
 * into which the task will encode the particular message it wishes
 * to send.  This common buffer scheme is used to strictly limit the
 * creation of message buffers within the system due to RAM size
 * limitations on the microprocessor.   Note that all message buffers
 * are a fixed size (at least initially).  The parameter len is kept
 * in case a message pool with varying fixed message sizes is later
 * created (for example, a pool of message buffers of size LARGE,
 * MEDIUM and SMALL could be maintained and allocated based on request
 * from the tasks).
 *
 * @param uint8 len wanted buffer length
 *
 * @return pointer to allocated buffer
 * @return NULL if allocation failed.
 */
  extern uint8 * osal_msg_allocate(uint16 len );

/**
* @brief
*
* This function is called by a task to allocate a message buffer
* into which the task will encode the particular message it wishes
* to send.  This common buffer scheme is used to strictly limit the
* creation of message buffers within the system due to RAM size
* limitations on the microprocessor.   Note that all message buffers
* are a fixed size (at least initially).  The parameter len is kept
* in case a message pool with varying fixed message sizes is later
* created (for example, a pool of message buffers of size LARGE,
* MEDIUM and SMALL could be maintained and allocated based on request
* from the tasks).
* Note that this function will first check if there is enough heap
* memory left after the allocation.
*
* @param   uint8 len  - wanted buffer length
*
* @return  pointer to allocated buffer
* @return  NULL if allocation failed.
*/
  extern uint8 * osal_msg_allocateLimited(uint16 len );

/**
 * @brief  Deallocate Message
 *
 * This function is used to deallocate a message buffer. This function
 * is called by a task (or processing element) after it has finished
 * processing a received message.
 *
 * @param uint8 *msg_ptr pointer to new message buffer
 *
 * @return @ref SUCCESS
 * @return @ref INVALID_MSG_POINTER
 */
  extern uint8 osal_msg_deallocate( uint8 *msg_ptr );

/**
 * @brief Send Osal Message
 *
 * This function is called by a task to send a command message to
 * another task or processing element.  The sending_task field must
 * refer to a valid task, since the task ID will be used
 * for the response message.  This function will also set a message
 * ready event in the destination tasks event list.
 *
 *
 * @param uint8 destination_task Send msg to Task ID
 * @param uint8 *msg_ptr pointer to new message buffer
 *
 * @return @ref SUCCESS
 * @return @ref INVALID_TASK
 * @return @ref INVALID_MSG_POINTER
 */
  extern uint8 osal_msg_send( uint8 destination_task, uint8 *msg_ptr );

/**
 * @brief Send Service Complete message
 *
 * This function is called by icall lite translation when it is done
 * servicing the direct API requested.  The sending_task field
 * refer the initial calling task (the proxy task calling the direct API):
 * It is always consider as valid.
 * This function will send a message to the proxy task.
 * ready event in the destination tasks event list.
 *
 *
 * @param uint8 destination_task Send msg to Task ID
 * @param uint8 *msg_ptr pointer to new message buffer
 *
 * @return @ref SUCCESS
 * @return @ref INVALID_TASK
 * @return @ref INVALID_MSG_POINTER
 */
  extern uint8 osal_service_complete( uint8 destination_task, uint8 *msg_ptr );

/**
 * @brief Push a command message to head of OSAL queue.
 *
 * This function is called by a task to push a command message
 * to the head of the OSAL queue. The destination_task field
 * must refer to a valid task, since the task ID will be used to
 * send the message to. This function will also set a message
 * ready event in the destination task's event list.
 *
 * @param uint8 destination_task Send msg to Task ID
 * @param uint8 *msg_ptr pointer to message buffer
 *
 * @return @ref SUCCESS
 * @return @ref INVALID_TASK
 * @return @ref INVALID_MSG_POINTER
 */
  extern uint8 osal_msg_push_front( uint8 destination_task, uint8 *msg_ptr );

/**
 * @brief Receive command message
 *
 * This function is called by a task to retrieve a received command
 * message. The calling task must deallocate the message buffer after
 * processing the message using the osal_msg_deallocate() call.
 *
 * @param uint8 task_id receiving tasks ID
 *
 * @return message information
 * @return NULL if no message
 */
  extern uint8 *osal_msg_receive( uint8 task_id );

/**
 * @brief Finds in place an OSAL message matching the task_id and event parameters.
 *
 * @param task_id The OSAL task id that the enqueued OSAL message must match.
 * @param event The OSAL event id that the enqueued OSAL message must match.
 *
 * @return NULL if no match
 * @return otherwise an in place pointer to the matching OSAL message
 */
  extern osal_event_hdr_t *osal_msg_find(uint8 task_id, uint8 event);

/**
 * @brief Counts the number of messages in the OSAL message queue with a
 *              a given task ID and message event type.
 *
 * @param task_id The OSAL task id that the enqueued OSAL message must match.
 * @param event The OSAL event id that the enqueued OSAL message must match. 0xFF for
 *              all events.
 *
 * @return The number of OSAL messages that match the task ID and Event.
 */
  extern uint8 osal_msg_count(uint8 task_id, uint8 event);

/**
 * @brief Equeues an OSAL message into an OSAL queue.
 *
 * @param q_ptr OSAL queue
 * @param msg_ptr OSAL message
 */
  extern void osal_msg_enqueue( osal_msg_q_t *q_ptr, void *msg_ptr );

/**
 * @brief Enqueues an OSAL message into an OSAL queue if
 *    the length of the queue is less than max.
 *
 * @param q_ptr OSAL queue
 * @param msg_ptr OSAL message
 * @param max maximum length of queue
 *
 * @return TRUE if message was enqueued
 * @return FALSE otherwise
 */
  extern uint8 osal_msg_enqueue_max( osal_msg_q_t *q_ptr, void *msg_ptr, uint8 max );

/**
 * @brief Dequeues an OSAL message from an OSAL queue.
 *
 * @param q_ptr OSAL queue
 *
 * @return NULL of queue is empty
 * @return otherwise, pointer to OSAL message
 */
  extern void *osal_msg_dequeue( osal_msg_q_t *q_ptr );

/**
 * @brief Pushes an OSAL message to the head of an OSAL queue.
 *
 * @param q_ptr OSAL queue
 * @param msg_ptr OSAL message
 */
  extern void osal_msg_push( osal_msg_q_t *q_ptr, void *msg_ptr );

/**
 * @brief Extracts and removes an OSAL message from the
 *    middle of an OSAL queue.
 *
 * @param q_ptr OSAL queue
 * @param msg_ptr OSAL message to be extracted
 * @param prev_ptr OSAL message before msg_ptr in queue
 */
  extern void osal_msg_extract( osal_msg_q_t *q_ptr, void *msg_ptr, void *prev_ptr );

/**
 * @brief Service function for messaging service
 *
 * @param args  arguments.
 *
 * @return  ICall error code
 */
  extern ICall_Errno osal_service_entry(ICall_FuncArgsHdr *args);


/*** Task Synchronization  ***/

/**
 * @brief Set the event flags for a task.
 *
 * The event passed in is OR'd into the task's event variable.
 *
 * @param task_id receiving tasks ID
 * @param event_flag what event to set
 *
 * @return @ref SUCCESS
 * @return @ref MSG_BUFFER_NOT_AVAIL
 * @return @ref FAILURE
 * @return @ref INVALID_TASK
 */
  extern uint8 osal_set_event( uint8 task_id, uint32 event_flag );


/**
 * @brief Clear the event flags for a task.
 *
 * The event passed in is masked out of the task's event variable.
 *
 * @param task_id receiving tasks ID
 * @param event_flag what event to clear
 *
 * @return @ref SUCCESS
 * @return @ref INVALID_TASK
 */
  extern uint8 osal_clear_event( uint8 task_id, uint32 event_flag );

/*** Task Management  ***/

/**
 * @brief  Map a task id to an ICall entity id for messaging in
 *          both directions (sending and receiving).
 *
 * @param taskid OSAL task id
 * @param dispatchid ICall entity id
 */
  extern void osal_enroll_dispatchid(uint8 taskid,
                                     ICall_EntityID dispatchid);

/**
 * @brief   Map a task id to an ICall entity id
 *
 * This taks id shall be used
 * just for sending a message from an OSAL task.
 * Note that osal_enroll_dispatchid() must never be called
 * with the same OSAL task id used in this function call.
 * However, it is OK to call osal_enroll_dispatchid()
 * with the same ICall entity id and a different OSAL task id.
 *
 * @param taskid OSAL task id
 * @param dispatchid ICall entity id
 */
  extern void osal_enroll_senderid(uint8 taskid, ICall_EntityID dispatchid);

/**
 * @brief   Map no task id to an ICall entity id
 *
 * This task id shall be used
 * just for sending a message from non-OSAL task.
 *
 * @param dispatchid ICall entity id
 */
  extern void osal_enroll_notasksender(ICall_EntityID dispatchid);

/**
 * @brief Initialize global OSAL timer variable.
 *
 * @param tickPeriod period of a RTOS tick in us
 * @param osalMaxMsecs max possible timer duration
 */
  void osal_timer_init(uint_least32_t tickPeriod, uint_least32_t osalMaxMsecs);

/**
 * @brief Initializes the "task" system by creating the
 *   tasks defined in the task table.
 *
 * @return  SUCCESS
 */
  extern uint8 osal_init_system( void );

  /*
   * System Processing Loop
   */
/**
 * @brief This function is the main loop function of the task system.
 *
 * @note This function does not return.
 */
  extern void osal_start_system( void );

/**
 * @brief OSAL main loop
 *
 * This function will make one pass through the OSAL taskEvents table
 * and call the task_event_processor() function for the first task that
 * is found with at least one event pending. If there are no pending
 * events (all tasks), this function puts the processor into Sleep.
 */
  extern void osal_run_system( void );

/**
 * @brief This function returns the task ID of the current (active) task.
 *
 * @return active task ID
 * @return TASK_NO_TASK if no task is active
 */
  extern uint8 osal_self( void );

/// @endcond // NODOC

/*** Helper Functions ***/

/**
 * @brief Calculates the length of a string.
 *
 * @note The string must be null terminated.
 *
 * @param   *pString pointer to text string
 *
 * @return  number of characters
 */
  extern int osal_strlen( char *pString );

/**
 * @brief Generic memory copy.
 *
 * @note This function differs from the standard memcpy(), since
 * it returns the pointer to the next destination uint8. The
 * standard memcpy() returns the original destination address.
 *
 * @param dst destination address
 * @param src source address
 * @param len number of bytes to copy
 *
 * @return pointer to end of destination buffer
 */
  extern void *osal_memcpy( void *dst, const void GENERIC *src, unsigned int len);

/**
 * @brief   Allocates a buffer and copies
 *          the src buffer into the newly allocated space.
 *
 * Allocation is done with with @ref osal_mem_alloc
 *
 * @param src source address
 * @param len number of bytes to copy
 *
 * @return pointer to the new allocated buffer
 * @return NULL if allocation fails
 */
  extern void *osal_memdup( const void GENERIC *src, unsigned int len );

/**
 * @brief   Generic reverse memory copy.
 *
 * Starts at the end of the
 * source buffer, by taking the source address pointer and moving
 * pointer ahead "len" bytes, then decrementing the pointer.
 *
 * @note This function differs from the standard memcpy(), since
 * it returns the pointer to the next destination uint8. The
 * standard memcpy() returns the original destination address.
 *
 * @param dst destination address
 * @param src source address
 * @param len number of bytes to copy
 *
 * @return  pointer to end of destination buffer
 */
  extern void *osal_revmemcpy( void *dst, const void GENERIC *src, unsigned int len);

/**
 * @brief Generic memory compare.
 *
 * @param src1 source 1 address
 * @param src2 source 2 address
 * @param len number of bytes to compare
 *
 * @return TRUE if same
 * @return FALSE if different
 */
  extern uint8 osal_memcmp( const void GENERIC *src1, const void GENERIC *src2, unsigned int len );

/**
 * @brief Set memory buffer to value.
 *
 * @param dest pointer to buffer
 * @param value what to set each uint8 of the message
 * @param size how big
 *
 * @return pointer to destination buffer
 */
  extern void *osal_memset( void *dest, uint8 value, int size );

/**
 * @brief Build a uint16 out of 2 bytes (0 then 1).
 *
 * @param swapped 0 then 1
 *
 * @return uint16
 */
  extern uint16 osal_build_uint16( uint8 *swapped );

/**
 * @brief Build a uint32 out of sequential bytes.
 *
 * @param swapped sequential bytes
 * @param len number of bytes in the uint8 array
 *
 * @return  uint32
 */
  extern uint32 osal_build_uint32( uint8 *swapped, uint8 len );

/**
 * @brief  Random number generator
 *
 * @return  new random number
 */
  extern uint16 osal_rand( void );

/**
 * @brief Buffer an uint32 value - LSB first.
 *
 * @param buf buffer
 * @param val uint32 value
 *
 * @return pointer to end of destination buffer
 */
  extern uint8* osal_buffer_uint32( uint8 *buf, uint32 val );

/**
 * @brief Check if all of the array elements are set to a value
 *
 * @param buf buffer to check
 * @param val value to check each array element for
 * @param len length to check
 *
 * @return  TRUE if all "val"
 * @return  FALSE otherwise
 */
  extern uint8 osal_isbufset( uint8 *buf, uint8 val, uint8 len );

#ifdef ICALL_LITE
/**
 * @brief Set the hook used to parse icall message.
 *
 * @param param pointer to a function that will be called if a icall message
 *          is found.
 */
  extern void osal_set_icall_hook( osal_icallMsg_hook_t param );

/**
 * @brief Assign or retrieve a proxy OSAL task id for an external ICall entity.
 *
 * @param entity ICall entity id
 *
 * @return proxy OSAL task id
 */
  extern uint8 osal_alien2proxy(ICall_EntityID entity);

#endif /* ICALL_LITE */

#define BLE_LOG_INT_INT(handle, type, format, param1, param2)
#define BLE_LOG_INT_STR(handle, type, format, param1, param2)
#define BLE_LOG_INT_TIME(handle, type, start_str, param1)


/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* OSAL_H */
