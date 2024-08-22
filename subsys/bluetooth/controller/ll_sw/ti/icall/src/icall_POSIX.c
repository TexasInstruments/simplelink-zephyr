/******************************************************************************
 @file  icall_POSIX.c

 @brief Indirect function Call dispatcher implementation on top of OS.

        This implementation uses heapmgr.h to implement a simple heap with low
        memory overhead but large processing overhead.<br>
        The size of the heap is determined with HEAPMGR_SIZE macro, which can
        be overridden with a compile option.
        Note: The ICall layer (e.g. this file) is using TI internal implementation of POSIX.
        For now, the ICall layer is not supports using outer POSIX on the application layer.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2024 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifdef FREERTOS
#include <FreeRTOS.h>
#endif

#include <ti/drivers/dpl/HwiP.h>
#include <ti/drivers/dpl/SwiP.h>
#include <ti/drivers/dpl/ClockP.h>
#include <ti/drivers/dpl/EventP.h>
#include <ti/drivers/dpl/TaskP.h>
#include <ti/drivers/dpl/MessageQueueP.h>

#define Hwi_disable HwiP_disable
#define Hwi_restore HwiP_restore
#define Hwi_disableinterrupt HwiP_disableInterrupt
#define Hwi_enableinterrupt HwiP_enableInterrupt
#define Swi_restore SwiP_restore
#define Swi_disable SwiP_disable
#define BIOS_WAIT_FOREVER (~(0U))
#define BIOS_NO_WAIT (0U)

#include "osal.h"
#include <stdint.h>
#include <stdio.h>
#include "icall.h"
#include "icall_platform.h"
#include <stdarg.h>
#include <string.h>

#ifndef ICALL_FEATURE_SEPARATE_IMGINFO
#include <icall_addrs.h>
#endif /* ICALL_FEATURE_SEPARATE_IMGINFO */

#define ICALL_THREAD_NAME "BLE_Stack"
#define ICALL_WORKER_THREAD_NAME "Icall_Worker"

#ifndef ICALL_MAX_NUM_ENTITIES
/**
 * Maximum number of entities that use ICall, including service entities
 * and application entities.
 * The value may be overridden by a compile option.
 * Note that there are at least,
 * Primitive service, Stack services along with potentially generic
 * framework service for the stack thread.
 */
#define ICALL_MAX_NUM_ENTITIES     6
#endif

#ifndef ICALL_MAX_NUM_TASKS
/**
 * Maximum number of threads which include entities.
 * The value may be overridden by a compile option.
 */
#define ICALL_MAX_NUM_TASKS        2
#endif

/**
 * @internal
 * Service class value used to indicate an invalid (unused) entry
 */
#define ICALL_SERVICE_CLASS_INVALID_ENTRY  0x0000

/**
 * @internal
 * Service class value used to indicate an entry for an application entity
 */
#define ICALL_SERVICE_CLASS_APPLICATION    ICALL_SERVICE_CLASS_MASK

/**
 * @internal
 * Primitive service entity ID
 */
#define ICALL_PRIMITIVE_ENTITY_ID          0

/**
 * @internal
 * Accessor macro to get a header field (next) from a message pointer
 */
#define ICALL_MSG_NEXT(_p) (((ICall_MsgHdr *)(_p) - 1)->next)

/**
 * @internal
 * Accessor macro to get a header field (dest_id) from a message pointer
 */
#define ICALL_MSG_DEST_ID(_p) (((ICall_MsgHdr *)(_p) - 1)->dest_id)


#ifndef ICALL_TIMER_TASK_STACK_SIZE
/**
 * @internal
 * Timer thread stack size
 */
#define ICALL_TIMER_TASK_STACK_SIZE (512)
#endif //ICALL_TIMER_TASK_STACK_SIZE

/**
 * @internal
 * Creation of the synchronous object between application and service
 */

#define ICALL_POSIX_HEAP_THRESHOLD_U                             1000U


#ifdef ICALL_EVENTS
#define ICALL_POSIX_WAITMATCH_EVENT_ID                          0x00001000
#define ICALL_POSIX_MSG_EVENT_ID                                0x00010000
#define ICALL_SYNC_HANDLE_CREATE() (Event_create(NULL, NULL))
#else /* ICALL_EVENTS */
#define ICALL_SYNC_HANDLE_CREATE() (Semaphore_create(0, NULL, NULL))
#endif /* ICALL_EVENTS */


/**
 * @internal
   * post the synchronous object between application and service
 */
#ifdef ICALL_EVENTS
#define ICALL_SYNC_HANDLE_POST(x)     (Event_post(x, ICALL_POSIX_MSG_EVENT_ID))
#define ICALL_SYNC_HANDLE_POST_WM(x)  (Event_post(x, ICALL_POSIX_WAITMATCH_EVENT_ID))
#else /* ICALL_EVENTS */
#define ICALL_SYNC_HANDLE_POST(x)     (Semaphore_post(x))
#define ICALL_SYNC_HANDLE_POST_WM(x)  (Semaphore_post(x))  /* Semaphore does not have event ID */
#endif /* ICALL_EVENTS */


/**
 * @internal
   * pend for the synchronous object between application and service
 */
#ifdef ICALL_EVENTS
#define ICALL_SYNC_HANDLE_PEND(x, t)    (Event_pend(x, 0, ICALL_POSIX_MSG_EVENT_ID ,t))
#define ICALL_SYNC_HANDLE_PEND_WM(x, t) (Event_pend(x, 0, ICALL_POSIX_WAITMATCH_EVENT_ID, t))
#else /* ICALL_EVENTS */
#define ICALL_SYNC_HANDLE_PEND(x, t)    (Semaphore_pend(x, t))
#define ICALL_SYNC_HANDLE_PEND_WM(x, t) (Semaphore_pend(x, t))  /* Semaphore does not have event ID */
#endif  /* ICALL_EVENTS */

/**
 * @internal
 * Data structure used to access critical section
 * state variable.
 * Without this data structure, C code will violate
 * C89 or C99 strict aliasing rule.
 */
typedef union _icall_cs_state_union_t
{
  /** critical section variable as declared in the interface */
  ICall_CSState  state;
  /** @internal field used to access internal data */
  struct _icall_cs_state_aggr_t
  {
    /** field to store Swi_disable() return value */
    uint_least16_t swikey;
    /** field to store Hwi_disable() return value */
    uint_least16_t hwikey;
  } each;
} ICall_CSStateUnion;

/**
 * @internal Primitive service handler function type
 */
typedef ICall_Errno (*ICall_PrimSvcFunc)(ICall_FuncArgsHdr *);

#ifdef ICALL_FEATURE_SEPARATE_IMGINFO
/* Image information shall be in separate module */

/**
 * Array of entry function addresses of external images.
 *
 * Note that function address must be odd number for Thumb mode functions.
 */
extern const ICall_RemoteTaskEntry ICall_imgEntries[];
/**
 * Array of task priorities of external images.
 * One task is created per image to start off the image entry function.
 * Each element of this array correspond to the task priority of
 * each entry function defined in @ref ICall_imgEntries.
 */
extern const int ICall_imgTaskPriorities[];

/**
 * Array of task stack sizes of external images.
 * One task is created per image to start off the image entry function.
 * Each element of this array correspond to the task stack size of
 * each entry function defined in @ref ICall_imgEntries.
 */
extern const size_t ICall_imgTaskStackSizes[];

/**
 * Array of custom initialization parameters (pointers).
 * Each initialization parameter (pointer) is passed to each corresponding
 * image entry function defined in @ref ICall_imgEntries;
 */
extern const void *ICall_imgInitParams[];

/**
 * Number of external images.
 */
extern const uint_least8_t ICall_numImages;

#define icall_threadEntries ICall_imgEntries
#define ICall_threadPriorities ICall_imgTaskPriorities
#define ICall_threadStackSizes ICall_imgTaskStackSizes
#define ICall_getInitParams(_i) (ICall_imgInitParams[i])
#define ICALL_REMOTE_THREAD_COUNT ICall_numImages
#else /* ICALL_FEATURE_SEPARATE_IMGINFO */
/**
 * @internal
 * Array of entry function of external images.
 */
static const ICall_RemoteTaskEntry icall_threadEntries[] = ICALL_ADDR_MAPS;

/** @internal external image count */
#define ICALL_REMOTE_THREAD_COUNT \
  (sizeof(icall_threadEntries)/sizeof(icall_threadEntries[0]))


/** @internal thread priorities to be assigned to each remote thread */
static const int ICall_threadPriorities[] = ICALL_TASK_PRIORITIES;


/** @internal thread stack max depth for each remote thread */
static const size_t ICall_threadStackSizes[] = ICALL_TASK_STACK_SIZES;

/** @internal initialization parameter (pointer) for each remote thread */
#ifdef ICALL_CUSTOM_INIT_PARAMS
static const void *ICall_initParams[] = ICALL_CUSTOM_INIT_PARAMS;
#define ICall_getInitParams(_i) (ICall_initParams[i])
#else /* ICALL_CUSTOM_INIT_PARAMS */
#define ICall_getInitParams(_i) NULL
#endif /* ICALL_CUSTOM_INIT_PARAMS */

#endif /* ICALL_FEATURE_SEPARATE_IMGINFO */

/** @internal message queue */
typedef void *ICall_MsgQueue;

/** @internal data structure about a task using ICall module */
typedef struct _icall_task_entry_t
{
  TaskP_Handle task;
  ICall_SyncHandle syncHandle;
  ICall_MsgQueue queue;
} ICall_TaskEntry;

/** @internal data structure about an entity using ICall module */
typedef struct _icall_entity_entry_t
{
  ICall_ServiceEnum service;
  ICall_TaskEntry *task;
  ICall_ServiceFunc fn;
#ifdef ICALL_NO_APP_EVENTS
  appCallback_t appCallback;
#endif
} ICall_entityEntry;

/** @internal storage to track all tasks using ICall module */
static ICall_TaskEntry ICall_tasks[ICALL_MAX_NUM_TASKS];

/** @internal storage to track all entities using ICall module */
static ICall_entityEntry ICall_entities[ICALL_MAX_NUM_ENTITIES];

typedef struct _icall_schedule_t
{
  ClockP_Handle clockP;
  ICall_TimerCback cback;
  void *arg;
} ICall_ScheduleEntry;


/* For now critical sections completely disable hardware interrupts
 * because they are used from ISRs in MAC layer implementation.
 * If MAC layer implementation changes, critical section
 * implementation may change to reduce overall interrupt latency.
 */
/* Enter critical section implementation. See header file for comment. */
ICall_CSState ICall_enterCSImpl(void)
{

  ICall_CSStateUnion cu;
  cu.each.hwikey = (uint_least16_t) Hwi_disable();
  cu.each.swikey = (uint_least16_t) Swi_disable();
  return cu.state;
}

TaskP_Handle ICall_taskSelf(void)
{
  return TaskP_getCurrentTask();
}

/* See header file for comment */
ICall_EnterCS ICall_enterCriticalSection = ICall_enterCSImpl;

/* leave critical section implementation. See header file for comment */
void ICall_leaveCSImpl(ICall_CSState key)
{
  ICall_CSStateUnion *cu = (ICall_CSStateUnion *) &key;
  Swi_restore((uint32_t) cu->each.swikey);
  Hwi_restore((uint32_t) cu->each.hwikey);
}

/* See header file for comment */
ICall_LeaveCS ICall_leaveCriticalSection = ICall_leaveCSImpl;

/* Implementing a simple heap using heapmgr.h template.
 * This simple heap depends on critical section implementation
 * and hence the template is used after critical section definition. */
void *ICall_heapMalloc(uint32_t size);
void *ICall_heapRealloc(void *blk, uint32_t size);
void ICall_heapFree(void *blk);

#define HEAPMGR_INIT       ICall_heapInit
#define HEAPMGR_MALLOC     ICall_heapMalloc
#define HEAPMGR_FREE       ICall_heapFree
#define HEAPMGR_REALLOC    ICall_heapRealloc
#define HEAPMGR_GETSTATS   ICall_heapGetStats
#define HEAPMGR_MALLOC_LIMITED ICall_heapMallocLimited

void ICall_heapMgrGetMetrics(uint32_t *pBlkMax,
                             uint32_t *pBlkCnt,
                             uint32_t *pBlkFree,
                             uint32_t *pMemAlo,
                             uint32_t *pMemMax,
                             uint32_t *pMemUB);
#ifdef HEAPMGR_METRICS
#define HEAPMGR_GETMETRICS    ICall_heapMgrGetMetrics
#endif

#define HEAPMGR_LOCK()                                       \
  do { ICall_heapCSState = ICall_enterCSImpl(); } while (0)
#define HEAPMGR_UNLOCK()                                     \
  do { ICall_leaveCSImpl(ICall_heapCSState); } while (0)
#define HEAPMGR_IMPL_INIT()
/* Note that a static variable can be used to contain critical section
 * state since heapmgr.h template ensures that there is no nested
 * lock call. */

/**
 * Worker Thread entity definition
 *
*/
#ifdef CC23X0
/* keep the worker thread priority lower than the icall task (5)
   and higher than the app task (1) */
#define ICALL_WORKER_THREAD_PRIORITY    2
/* worker thread stack size was selected after the ECC SW operation
   usage was determined (~800 bytes) */
#define ICALL_WORKER_THREAD_STACKSIZE   1024

#define ICALL_WORKER_THREAD_QUEUE_SIZE    8

/** @internal data structure for the Worker Thread entity */
typedef struct
{
  TaskP_Handle             threadId;
  TaskP_Params             taskParams;
  MessageQueueP_Handle     queueHandle;
} ICall_workerThreadEntity_t;

ICall_workerThreadEntity_t workerThreadEntity;

/** @internal data structure for the Worker Thread queue message */
typedef struct
{
  void                  *func;
  void                  *arg;
} ICall_WorkerThreadMsg_t;

#endif

/**
 * @internal Searches for a task entry within @ref ICall_tasks.
 * @param taskhandle  OS task handle
 * @return Pointer to task entry when found, or NULL.
 */
static ICall_TaskEntry *ICall_searchTask(TaskP_Handle taskhandle)
{
  size_t i;
  ICall_CSState key;

  key = ICall_enterCSImpl();
  for (i = 0; i < ICALL_MAX_NUM_TASKS; i++)
  {
    if (!ICall_tasks[i].task)
    {
      /* Empty slot */
      break;
    }
    if (taskhandle == ICall_tasks[i].task)
    {
      ICall_leaveCSImpl(key);
      return &ICall_tasks[i];
    }
  }
  ICall_leaveCSImpl(key);
  return NULL;
}

/**
 * @internal Searches for a task entry within @ref ICall_tasks or
 *           build an entry if the entry table is empty.
 * @param  taskhandle  OS task handle
 * @return Pointer to task entry when found, or NULL.
 */

static ICall_TaskEntry *ICall_newTask(TaskP_Handle taskhandle)
{
  size_t i;
  ICall_CSState key;

  key = ICall_enterCSImpl();
  for (i = 0; i < ICALL_MAX_NUM_TASKS; i++)
  {
    if (!ICall_tasks[i].task)
    {
      /* Empty slot */
      ICall_TaskEntry *taskentry = &ICall_tasks[i];
      taskentry->task = taskhandle;
      taskentry->queue = NULL;
      // Create event handle to the osal events
      taskentry->syncHandle = EventP_create();
      if (taskentry->syncHandle == NULL)
      {
        /* abort */
        ICALL_HOOK_ABORT_FUNC();
      }
      ICall_leaveCSImpl(key);
      return taskentry;
    }
    if (taskhandle == ICall_tasks[i].task)
    {
      ICall_leaveCSImpl(key);
      return &ICall_tasks[i];
    }
  }
  ICall_leaveCSImpl(key);
  return NULL;
}

/* See header file for comments. */
ICall_EntityID ICall_searchServiceEntity(ICall_ServiceEnum service)
{
  size_t i;
  ICall_CSState key;

  key = ICall_enterCSImpl();
  for (i = 0; i < ICALL_MAX_NUM_ENTITIES; i++)
  {
    if (ICall_entities[i].service == ICALL_SERVICE_CLASS_INVALID_ENTRY)
    {
      /* Empty slot */
      break;
    }
    if (service == ICall_entities[i].service)
    {
      ICall_leaveCSImpl(key);
      return (ICall_EntityID) i;
    }
  }
  ICall_leaveCSImpl(key);
  return ICALL_INVALID_ENTITY_ID;
}

/**
 * @internal Searches for a service entity entry.
 * @param service  service id
 * @return Pointer to entity entry of the service or
 *         NULL when none found.
 */
static ICall_entityEntry *
ICall_searchService(ICall_ServiceEnum service)
{
  ICall_EntityID entity = ICall_searchServiceEntity(service);
  if (entity == ICALL_INVALID_ENTITY_ID)
  {
    return NULL;
  }
  return &ICall_entities[entity];
}

/* Dispatcher implementation. See ICall_dispatcher declaration
 * for comment. */
static ICall_Errno ICall_dispatch(ICall_FuncArgsHdr *args)
{
  ICall_entityEntry *entity;

  entity =  ICall_searchService(args->service);
  if (!entity)
  {
    return ICALL_ERRNO_INVALID_SERVICE;
  }
  if (!entity->fn)
  {
    return ICALL_ERRNO_INVALID_FUNCTION;
  }

  return entity->fn(args);
}

/* See header file for comments */
ICall_Dispatcher ICall_dispatcher = ICall_dispatch;

/* Static instance of ICall_RemoteTaskArg to pass to
 * remote task entry function.
 * See header file for comments */
static const ICall_RemoteTaskArg ICall_taskEntryFuncs =
{
  ICall_dispatch,
  ICall_enterCSImpl,
  ICall_leaveCSImpl
};

/**
 * @internal Thread entry function wrapper that complies with
 *           OS.
 * @param arg0  actual entry function
 * @param arg1  ignored
 */

TaskP_Handle RemoteTask;
TaskP_Params remoteTaskParams;

struct argsForPosixTaskStart
{
    void *arg0;
    void *arg1;
};
struct argsForPosixTaskStart POSIX_args;

static void ICall_taskEntry(void *arg)

{
    void * arg0 = ((struct argsForPosixTaskStart *)(arg))->arg0;
    void * arg1 = ((struct argsForPosixTaskStart *)(arg))->arg1;

    ICall_CSState key;
    key = ICall_enterCSImpl();
    if (ICall_newTask(ICall_taskSelf()) == NULL)
    {
        /* abort */
        ICALL_HOOK_ABORT_FUNC();
    }
    ICall_leaveCSImpl(key);

    ICall_RemoteTaskEntry entryfn = (ICall_RemoteTaskEntry) arg0;

    entryfn(&ICall_taskEntryFuncs, (void *) arg1);

    return;
}

/* See header file for comments. */
void ICall_init(void)
{
  size_t i;

  for (i = 0; i < ICALL_MAX_NUM_TASKS; i++)
  {
    ICall_tasks[i].task = NULL;
    ICall_tasks[i].queue = NULL;
  }
  for (i = 0; i < ICALL_MAX_NUM_ENTITIES; i++)
  {
    ICall_entities[i].service = ICALL_SERVICE_CLASS_INVALID_ENTRY;
  }
}

#ifdef CC23X0
/**
 * @brief   The worker thread entry function.
 *          This task waits forever on queue message
 *          and executes a requested function call.
 */
void ICall_workerThreadEntry(void *arg)
{
  ICall_WorkerThreadMsg_t msg;

  typedef void (*ICall_workerThreadFuncArg)(void *arg);
  typedef void (*ICall_workerThreadFunc)(void);

  workerThreadEntity.queueHandle = MessageQueueP_create(sizeof(ICall_WorkerThreadMsg_t),
                                                        ICALL_WORKER_THREAD_QUEUE_SIZE);

  if (NULL != workerThreadEntity.queueHandle)
  {
    for(;;)  // Forever Loop
    {
      // wait until receive queue message
      if (MessageQueueP_pend(workerThreadEntity.queueHandle, (void*)&msg, MessageQueueP_WAIT_FOREVER) == MessageQueueP_OK)
      {
        if (msg.arg != NULL)
        {
          // execute the function call
          ((ICall_workerThreadFuncArg)(msg.func))((void *) msg.arg);
          // free the function call argument
          ICall_free( msg.arg );
        }
        else
        {
          // execute the function call without argument
          ((ICall_workerThreadFunc)(msg.func))();
        }
      }
    }
  }
  return;
}

/**
 * @brief   Create the worker thread which is a task that waits on queue
 *          message and executes a requested function call.
 *
 * @param void
 * @return     On success return zero; on error,-1 is returned
 */
int ICall_createWorkerThread(void)
{
  TaskP_Params_init(&workerThreadEntity.taskParams);
  workerThreadEntity.taskParams.priority      = ICALL_WORKER_THREAD_PRIORITY;
  workerThreadEntity.taskParams.stackSize     = ICALL_WORKER_THREAD_STACKSIZE;
  workerThreadEntity.taskParams.name          = ICALL_WORKER_THREAD_NAME;

  workerThreadEntity.threadId = TaskP_create(ICall_workerThreadEntry, &workerThreadEntity.taskParams);

  if (NULL == workerThreadEntity.threadId)
  {
    /* Initialization Thread through DPL didn't succeed */
    ICall_abort();
  }

  return 0;
}

/* See header file for comments. */
int ICall_workerThreadSendMsg(void *func, void *arg, uint16 size)
{
  ICall_WorkerThreadMsg_t msg;
  int status = -1;

  // set the function call
  msg.func = func;
  msg.arg = NULL;
  // check if there is an argument to the function call
  if ((arg != NULL) && (size > 0))
  {
    // allocate and set the function call argument
    msg.arg = (void *)ICall_malloc( size );
    if (msg.arg != NULL)
    {
      memcpy( msg.arg, arg, size);
    }
    else
    {
      return status;
    }
  }
  // check if the worker thread queue is valid
  if (NULL != workerThreadEntity.queueHandle)
  {
    // send the msg to the worker thread queue
    status = MessageQueueP_post(workerThreadEntity.queueHandle , (void *)&msg, MessageQueueP_NO_WAIT);
  }
  if ((status != MessageQueueP_OK) && (msg.arg != NULL))
  {
    // free the function argument in case the msg was not sent
    ICall_free( msg.arg );
  }
  return status;
}
#endif

/* See header file for comments */
void ICall_createRemoteTasksAtRuntime(ICall_RemoteTask_t *remoteTaskTable, uint8_t nbElems)
{
  size_t i;
  /* ICALL_Task_disable is a cheap locking mechanism to lock tasks
   * which may attempt to access the service call dispatcher
   * till all services are registered.
   */
  for (i = 0; i < nbElems; i++)
  {
    TaskP_Params_init(&remoteTaskParams);
    remoteTaskParams.priority      = remoteTaskTable[i].imgTaskPriority;
    remoteTaskParams.stackSize     = remoteTaskTable[i].imgTaskStackSize;
    /* Pass the args via external sturct (POSIX use only single arg) */
    POSIX_args.arg0                = (void*)remoteTaskTable[i].startupEntry;
    POSIX_args.arg1                = (void*)remoteTaskTable[i].ICall_imgInitParam;
    remoteTaskParams.arg           = &POSIX_args;
    remoteTaskParams.name          = ICALL_THREAD_NAME;

    RemoteTask = TaskP_create(ICall_taskEntry, &remoteTaskParams);

    if (NULL == RemoteTask)
    {
      /* Initialization Thread through DPL didn't succeed */
      ICall_abort();
    }
  }
}

/* See header file for comments */
void ICall_createRemoteTasks(void)
{
  size_t i;
  ICall_RemoteTask_t remoteTaskTable[ICALL_REMOTE_THREAD_COUNT];

  for (i = 0; i < ICALL_REMOTE_THREAD_COUNT; i++)
  {
    remoteTaskTable[i].imgTaskPriority = ICall_threadPriorities[i];
    remoteTaskTable[i].imgTaskStackSize = ICall_threadStackSizes[i];
    remoteTaskTable[i].startupEntry = icall_threadEntries[i];
    remoteTaskTable[i].ICall_imgInitParam = (void *) ICall_getInitParams(i);
  }
  ICall_createRemoteTasksAtRuntime(remoteTaskTable, ICALL_REMOTE_THREAD_COUNT);
#ifdef CC23X0
  // create the worker thread
  ICall_createWorkerThread();
#endif
}

/**
 * @internal Queues a message to a message queue.
 * @param q_ptr    message queue
 * @param msg_ptr  message pointer
 */
static void ICall_msgEnqueue( ICall_MsgQueue *q_ptr, void *msg_ptr )
{
  void *list;
  ICall_CSState key;

  // Hold off interrupts
  key = ICall_enterCSImpl();

  ICALL_MSG_NEXT( msg_ptr ) = NULL;
  // If first message in queue
  if ( *q_ptr == NULL )
  {
    *q_ptr = msg_ptr;
  }
  else
  {
    // Find end of queue
    for ( list = *q_ptr; ICALL_MSG_NEXT( list ) != NULL;
          list = ICALL_MSG_NEXT( list ) );

    // Add message to end of queue
    ICALL_MSG_NEXT( list ) = msg_ptr;
  }

  // Re-enable interrupts
  ICall_leaveCSImpl(key);
}

/**
 * @internal Dequeues a message from a message queue
 * @param q_ptr  message queue pointer
 * @return Dequeued message pointer or NULL if none.
 */
static void *ICall_msgDequeue( ICall_MsgQueue *q_ptr )
{
  void *msg_ptr = NULL;
  ICall_CSState key;

  // Hold off interrupts
  key = ICall_enterCSImpl();

  if ( *q_ptr != NULL )
  {
    // Dequeue message
    msg_ptr = *q_ptr;
    *q_ptr = ICALL_MSG_NEXT( msg_ptr );
    ICALL_MSG_NEXT( msg_ptr ) = NULL;
    ICALL_MSG_DEST_ID( msg_ptr ) = ICALL_UNDEF_DEST_ID;
  }

  // Re-enable interrupts
  ICall_leaveCSImpl(key);

  return msg_ptr;
}

/**
 * @internal Prepends a list of messages to a message queue
 * @param q_ptr  message queue pointer
 * @param head   message list to prepend
 */
static void ICall_msgPrepend( ICall_MsgQueue *q_ptr, ICall_MsgQueue head )
{
  void *msg_ptr = NULL;
  ICall_CSState key;

  // Hold off interrupts
  key = ICall_enterCSImpl();

  if ( head != NULL )
  {
    /* Find the end of the queue */
    msg_ptr = head;
    while (ICALL_MSG_NEXT( msg_ptr ) != NULL)
    {
      msg_ptr = ICALL_MSG_NEXT( msg_ptr );
    }
    ICALL_MSG_NEXT(msg_ptr) = *q_ptr;
    *q_ptr = head;
  }

  // Re-enable interrupts
  ICall_leaveCSImpl(key);
}

/**
 * @internal
 * Transforms and entityId into a serviceId.
 * @param entityId   entity id
 * @param servId     pointer to a variable to store
 *                   the resultant service id
 * @return @ICALL_ERRNO_SUCCESS if the transformation was successful.
 *         @ICALL_ERRNO_INVALID_SERVICE if no matching service
 *         is found for the entity id.
 */
static ICall_Errno ICall_primEntityId2ServiceId(ICall_EntityID entityId,
                                                ICall_ServiceEnum *servId)
{
  if (entityId >= ICALL_MAX_NUM_ENTITIES ||
      ICall_entities[entityId].service ==
          ICALL_SERVICE_CLASS_INVALID_ENTRY ||
      ICall_entities[entityId].service ==
          ICALL_SERVICE_CLASS_APPLICATION)
  {
    return ICALL_ERRNO_INVALID_SERVICE;
  }
  *servId = ICall_entities[entityId].service;
  return ICALL_ERRNO_SUCCESS;
}

/**
 * @internal
 * Converts milliseconds to number of ticks.
 * @param msecs milliseconds
 * @param ticks pointer to a variable to store the resultant number of ticks
 * @return @ref ICALL_ERRNO_SUCCESS when successful<br>
 *         @ref ICALL_ERRNO_INVALID_PARAMETER when conversion failed
 *         as the input goes out of range for the output data type.
 */
static ICall_Errno ICall_msecs2Ticks(uint_fast32_t msecs, uint32_t *ticks)
{
  uint_fast64_t intermediate = msecs;

  /*convert to microSec*/
  intermediate *= 1000;
  /*The ClockP tick period is 1us on Loki and 10us on Agama*/
  /*divide with the ticks period*/
  intermediate /= ICall_getTickPeriod();
  if (intermediate >= ((uint_fast64_t) 1 << (sizeof(uint32_t)*8 - 1)))
  {
    /* Out of range.
     * Note that we use only half of the range so that client can
     * determine whether the time has passed or time has yet to come.
     */
    return ICALL_ERRNO_INVALID_PARAMETER;
  }
  *ticks = (uint32_t) intermediate;
  return ICALL_ERRNO_SUCCESS;
}


/**
 * @internal
 * Clock event handler function.
 * This function is used to implement the wakeup scheduler.
 *
 * @param arg  an @ref ICall_ScheduleEntry
 */

static void ICall_clockFunc(uintptr_t arg)
{
  ICall_ScheduleEntry *entry = (ICall_ScheduleEntry *) arg;

  entry->cback(entry->arg);
}


#ifdef COVERAGE_TEST
/**
 * @internal
 * Verification function for ICall implementation
 */
void ICall_verify(void)
{
  size_t i;
  for (i = 0; i < sizeof(ICall_primSvcFuncs)/sizeof(ICall_primSvcFuncs[0]); i++)
  {
    if (i != ICall_primSvcFuncs[i].id)
    {
      ICall_abort();
    }
  }
}
#endif /* COVERAGE_TEST */


#ifdef ICALL_JT
/**
 * Registers an application.
 * Note that this function must be called from the thread
 * from which ICall_wait() function will be called.
 *
 * @param entity  pointer to a variable to store entity id assigned
 *                to the application.
 * @param msgsem  pointer to a variable to store the synchronous object handle
 *                associated with the calling thread.
 * @return @ref ICALL_ERRNO_SUCCESS when successful.<br>
 *         @ref ICALL_ERRNO_NO_RESOURCE when ran out of resource.
 */
ICall_Errno ICall_registerApp(ICall_EntityID *entity,
                                            ICall_SyncHandle *msgSyncHdl)
{

  size_t i;
  ICall_TaskEntry *taskentry = ICall_newTask(ICall_taskSelf());
  ICall_CSState key;

  if (!taskentry)
  {
    /* abort */
    ICALL_HOOK_ABORT_FUNC();
    return ICALL_ERRNO_NO_RESOURCE;
  }

  key = ICall_enterCSImpl();
  for (i = 0; i < ICALL_MAX_NUM_ENTITIES; i++)
  {
    if (ICall_entities[i].service == ICALL_SERVICE_CLASS_INVALID_ENTRY)
    {
      /* Use this entry */
      ICall_entities[i].service = ICALL_SERVICE_CLASS_APPLICATION;
      ICall_entities[i].task = taskentry;
      ICall_entities[i].fn = NULL;
      *entity = (ICall_EntityID) i;
      *msgSyncHdl = taskentry->syncHandle;
      ICall_leaveCSImpl(key);
      return ICALL_ERRNO_SUCCESS;
    }
  }
  /* abort */
  ICALL_HOOK_ABORT_FUNC();
  ICall_leaveCSImpl(key);
  return (ICALL_ERRNO_NO_RESOURCE);
}

/**
 * Allocates memory block for a message.
 * @param size   size of the message body in bytes.
 * @return pointer to the start of the message body of the newly
 *         allocated memory block, or NULL if the allocation
 *         failed.
 */
void *ICall_allocMsg(size_t size)
{
  void * msg = NULL;
  ICall_MsgHdr *hdr = NULL;

  hdr = (ICall_MsgHdr *) ICall_heapMalloc(sizeof(ICall_MsgHdr) + size);

  if (!hdr)
  {
    return NULL;
  }
  hdr->len = size;
  hdr->next = NULL;
  hdr->dest_id = ICALL_UNDEF_DEST_ID;

  // Point to the start of the msg
  msg = ((void *) (hdr + 1));

  return msg;
}

/**
 * Allocates memory block for only if heap left is higher than threshold
 * @param size   size of the message body in bytes.
 * @return pointer to the start of the message body of the newly
 *         allocated memory block, or NULL if the allocation
 *         failed.
 */
void *ICall_allocMsgLimited(size_t size)
{
  void * msg;
  ICall_heapStats_t pStats;
  ICall_getHeapStats(&pStats);
  if(pStats.totalFreeSize - (uint32_t)size > ICALL_POSIX_HEAP_THRESHOLD_U)
  {
    msg = ICall_allocMsg(size);
  }
  else
  {
    msg = NULL;
  }

  return msg;
}

/**
 * Frees the memory block allocated for a message.
 * @param msg   pointer to the start of the message body
 *              which was returned from ICall_allocMsg().
 */
void ICall_freeMsg(void *msg)
{
  ICall_MsgHdr *hdr = (ICall_MsgHdr *) msg - 1;
  ICall_heapFree(hdr);
}

/**
 * Sends a message to a registered server.
 * @param src    Entity id of the sender of the message
 * @param dest   Service id
 * @param format Message format:
 *               @ref ICALL_MSG_FORMAT_KEEP,
 *               @ref ICALL_MSG_FORMAT_1ST_CHAR_TASK_ID or
 *               @ref ICALL_MSG_FORMAT_3RD_CHAR_TASK_ID.
 *               Message format indicates whether and which
 *               field of the message must be transformed
 *               into a implementation specific sender
 *               identity for an external image.<br>
 *               When a service message interface is defined,
 *               it may contain a field that is not understood
 *               by the client but only understood by
 *               the system on the server's side.
 *               The format provides an information to the
 *               messaging system on such a server
 *               so that it can generically tag necessary
 *               information to the message.
 * @param msg    pointer to the message body to send.<br>
 *               Note that if message is successfully sent,
 *               the caller should not reference the message any
 *               longer.<br>
 *               However, if the function fails, the caller
 *               still owns the reference to the message.
 *               That is, caller may attempt another send,
 *               or may free the memory block, etc.
 * @return @ref ICALL_ERRNO_SUCCESS when successful.<br>
 *         @ref ICALL_ERRNO_INVALID_SERVICE when the 'dest'
 *         is unregistered service.<br>
 *         @ref ICALL_ERRNO_INVALID_PARAMETER when the 'src'
 *         is an out of range entity id or when 'dest' is
 *         is a service that does not receive a message
 *         (such as ICall primitive service).<br>
 *         Note that as far as 'src' is within the range,
 *         this function won't notice the 'src' entity id
 *         as invalid.
 */


ICall_Errno
ICall_sendServiceMsg(ICall_EntityID src,
                     ICall_ServiceEnum dest,
                     ICall_MSGFormat format, void *msg)
{
  ICall_EntityID dstentity = ICall_searchServiceEntity(dest);

  if (dstentity == ICALL_INVALID_ENTITY_ID)
  {
    return ICALL_ERRNO_INVALID_SERVICE;
  }
  return (ICall_send(src, dstentity, format, msg));
}

/**
 * Retrieves a message received at the message queue
 * associated with the calling thread.
 *
 * Note that this function should be used by an application
 * which does not expect any message from non-server entity.
 *
 * @param src    pointer to a variable to store the service id
 *               of the registered server which sent the retrieved
 *               message
 * @param dest   pointer to a variable to store the entity id
 *               of the destination of the message.
 * @param msg    pointer to a pointer variable to store the
 *               starting address of the message body being
 *               retrieved.
 * @return @ref ICALL_ERRNO_SUCCESS when the operation was successful
 *         and a message was retrieved.<br>
 *         @ref ICALL_ERRNO_NOMSG when there is no queued message
 *         at the moment.<br>
 *         @ref ICALL_ERRNO_CORRUPT_MSG when a message queued in
 *         front of the thread's receive queue was not sent by
 *         a server. Note that in this case, the message is
 *         not retrieved but thrown away.<br>
 *         @ref ICALL_ERRNO_UNKNOWN_THREAD when this function is
 *         called from a thread which has not registered
 *         an entity, either through ICall_enrollService()
 *         or through ICall_registerApp().
 */
ICall_Errno
ICall_fetchServiceMsg(ICall_ServiceEnum *src,
                      ICall_EntityID *dest,
                      void **msg)
{
  ICall_ServiceEnum servId;
  ICall_Errno errno = ICall_fetchMsg((ICall_EntityID*)src, dest, msg);

  if (errno == ICALL_ERRNO_SUCCESS)
  {
    if (ICall_primEntityId2ServiceId(*src, &servId) !=
        ICALL_ERRNO_SUCCESS)
    {
      /* Source entity ID cannot be translated to service id */
      ICall_freeMsg(*msg);
      return ICALL_ERRNO_CORRUPT_MSG;
    }
    *src = servId;
  }
  return (errno);
}

/**
 * Waits for a signal to the semaphore associated with the calling thread.
 *
 * Note that the semaphore associated with a thread is signaled
 * when a message is queued to the message receive queue of the thread
 * or when ICall_signal() function is called onto the semaphore.
 *
 * @param milliseconds  timeout period in milliseconds.
 * @return @ref ICALL_ERRNO_SUCCESS when the semaphore is signaled.<br>
 *         @ref ICALL_ERRNO_TIMEOUT when designated timeout period
 *         has passed since the call of the function without
 *         the semaphore being signaled.
 */
ICall_Errno ICall_wait(uint_fast32_t milliseconds)
{
    TaskP_Handle taskhandle = ICall_taskSelf();
    ICall_TaskEntry *taskentry = ICall_searchTask(taskhandle);
    uint32_t timeout;

    int16_t retVal = 0;

    if (!taskentry)
    {
        return (ICALL_ERRNO_UNKNOWN_THREAD);
    }
    /* Successful */
    if (milliseconds == 0)
    {
        timeout = BIOS_NO_WAIT;
    }
    else if (milliseconds == ICALL_TIMEOUT_FOREVER)
    {
        timeout = BIOS_WAIT_FOREVER;
    }
    else
    {
        /* Convert milliseconds to number of ticks */
        ICall_Errno errno = ICall_msecs2Ticks(milliseconds, &timeout);
        if (errno != ICALL_ERRNO_SUCCESS)
        {
            return (errno);
        }
    }
    EventP_pend(taskentry->syncHandle, ICALL_POSIX_MSG_EVENT_ID, 0, timeout);
    if(retVal != (-1))
    {
        return (ICALL_ERRNO_SUCCESS);
    }

    return (ICALL_ERRNO_TIMEOUT);
}

/**
 * Signals a semaphore.
 * @param msgsem   handle of a synchronous object to signal
 * @return @ref ICALL_ERRNO_SUCCESS
 */
ICall_Errno ICall_signal(ICall_SyncHandle msgSyncHdl)
{
    EventP_post((EventP_Handle)msgSyncHdl, ICALL_POSIX_MSG_EVENT_ID);
    return (ICALL_ERRNO_SUCCESS);
}

/**
 * Registers a service entity
 * @param service  service id of the enrolling service
 * @param fn       handler function which handles function
 *                 calls to the service.
 * @param entity   pointer to a variable to store the assigned entity id
 * @param msgsem   pointer to a variable to store the synchronous object handle
 *                 associated with the calling thread.
 * @return @ref ICALL_ERRNO_SUCCESS when successful.<br>
 *         @ref ICALL_ERRNO_INVALID_PARAMETER when service id is already
 *         registered by another entity.<br>
 *         @ref ICALL_ERRNO_NO_RESOURCE when maximum number of services
 *         are already registered.
 */
ICall_Errno
ICall_enrollService(ICall_ServiceEnum service,
                    ICall_ServiceFunc fn,
                    ICall_EntityID *entity,
                    ICall_SyncHandle *msgSyncHdl)
{
  size_t i;
  ICall_TaskEntry *taskentry = ICall_newTask(ICall_taskSelf());
  ICall_CSState key;

  /* Note that certain service does not handle a message
   * and hence, taskentry might be NULL.
   */
  if (taskentry == NULL)
  {
    return (ICALL_ERRNO_INVALID_PARAMETER);
  }

  key = ICall_enterCSImpl();
  for (i = 0; i < ICALL_MAX_NUM_ENTITIES; i++)
  {
    if (ICall_entities[i].service == ICALL_SERVICE_CLASS_INVALID_ENTRY)
    {
      /* Use this entry */
      ICall_entities[i].service = service;
      ICall_entities[i].task = taskentry;
      ICall_entities[i].fn = fn;
      *entity = (ICall_EntityID) i;
      *msgSyncHdl = taskentry->syncHandle;

      ICall_leaveCSImpl(key);
      return (ICALL_ERRNO_SUCCESS);
    }
    else if (service == ICall_entities[i].service)
    {
      /* Duplicate service enrollment */
      ICall_leaveCSImpl(key);
      return (ICALL_ERRNO_INVALID_PARAMETER);
    }
  }
  /* abort */
  ICALL_HOOK_ABORT_FUNC();
  ICall_leaveCSImpl(key);
  return (ICALL_ERRNO_NO_RESOURCE);

}
#ifdef FREERTOS

/**
 * Allocates a memory block.
 * @param size   size of the block in bytes.
 * @return address of the allocated memory block or NULL
 *         if allocation fails.
 */

void *ICall_heapMalloc(uint32_t size)
{
  // Enter CS to avoid race with allocation from interrupt context
  ICall_CSState key;
  key = ICall_enterCSImpl();

  void* ret = NULL;
  ret = malloc(size);

  ICall_leaveCSImpl(key);

  return ret;
}

/**
 * Frees an allocated memory block.
 * @param msg  pointer to a memory block to free.
 */
void ICall_heapFree(void *msg)
{
  // Enter CS to avoid race with allocation from interrupt context
  ICall_CSState key;
  key = ICall_enterCSImpl();

  if(msg != NULL)
  {
    free(msg);
  }

  ICall_leaveCSImpl(key);
}

/**
 * Get Statistic on Heap.
 * @param stats  pointer to a heapStats_t structure.
 */
void ICall_heapGetStats(ICall_heapStats_t *pStats)
{
#ifdef FREERTOS
  struct xHeapStats pHeapStats;

  ICall_CSState key;
  key = ICall_enterCSImpl();
  vPortGetHeapStats(&pHeapStats);

  ICall_leaveCSImpl(key);
  pStats->totalFreeSize = pHeapStats.xAvailableHeapSpaceInBytes;
  pStats->totalSize = configTOTAL_HEAP_SIZE;
  pStats->largestFreeSize = pHeapStats.xSizeOfLargestFreeBlockInBytes;
#else // ZEPHYR
#define configTOTAL_HEAP_SIZE ((size_t)(0x2000))
  pStats->totalFreeSize = configTOTAL_HEAP_SIZE;
  pStats->totalSize = configTOTAL_HEAP_SIZE;
  pStats->largestFreeSize = configTOTAL_HEAP_SIZE;
#endif //FREERTOS
}

#endif // FREERTOS
/**
 * Allocates a memory block.
 * @param size   size of the block in bytes.
 * @return address of the allocated memory block or NULL
 *         if allocation fails.
 */
void *ICall_malloc(uint_least16_t size)
{
  return (ICall_heapMalloc(size));
}

/**
 * Frees an allocated memory block.
 * @param msg  pointer to a memory block to free.
 */
void ICall_free(void *msg)
{
  ICall_heapFree(msg);
}

/**
 * Allocates a memory block, but check if enough memory will be left after the allocation.
 * @param size   size of the block in bytes.
 * @return address of the allocated memory block or NULL
 *         if allocation fails.
 */

void *ICall_mallocLimited(uint_least16_t size)
{
  ICall_heapStats_t pStats;
  void *msg;
  ICall_getHeapStats(&pStats);
  if(pStats.totalFreeSize - (uint32_t)size > ICALL_POSIX_HEAP_THRESHOLD_U)
  {
      msg = ICall_heapMalloc(size);
  }
  else
  {
    msg = NULL;
  }

  return msg;
}

/**
 * Get Statistic on Heap.
 * @param stats  pointer to a heapStats_t structure.
 */
void ICall_getHeapStats(ICall_heapStats_t *pStats)
{
  ICall_heapGetStats(pStats);
}


#ifdef HEAPMGR_METRICS
/**
 * @brief   obtain heap usage metrics
 * @param   pBlkMax   pointer to a variable to store max cnt of all blocks ever seen at once
 * @param   pBlkCnt   pointer to a variable to store current cnt of all blocks
 * @param   pBlkFree  pointer to a variable to store current cnt of free blocks
 * @param   pMemAlo   pointer to a variable to store current total memory allocated
 * @param   pMemMax   pointer to a variable to store max total memory ever allocated at once
 * @param   pMemUB    pointer to a variable to store the upper bound of memory usage
 */
void ICall_getHeapMgrGetMetrics(uint32_t *pBlkMax,
                                uint32_t *pBlkCnt,
                                uint32_t *pBlkFree,
                                uint32_t *pMemAlo,
                                uint32_t *pMemMax,
                                uint32_t *pMemUB)
{
  ICall_heapMgrGetMetrics( pBlkMax,
                           pBlkCnt,
                           pBlkFree,
                           pMemAlo,
                           pMemMax,
                           pMemUB);
}

#endif
/**
 * Sends a message to an entity.
 * @param src     entity id of the sender
 * @param dest    entity id of the destination of the message.
 * @param format  message format. See ICall_sendServiceMsg().
 * @param msg     pointer to the message body.
 * @return @ref ICALL_ERRNO_SUCCESS when successful.<br>
 *         @ref ICALL_ERRNO_INVALID_PARAMETER when either src
 *              or dest is not a valid entity id or when
 *              dest is an entity id of an entity that does
 *              not receive a message
 *              (e.g., ICall primitive service entity).
 */
ICall_Errno ICall_send(ICall_EntityID src,
                              ICall_EntityID dest,
                              ICall_MSGFormat format,
                              void *msg)
{
  ICall_CSState key;
  ICall_MsgHdr *hdr = (ICall_MsgHdr *) msg - 1;

  if (dest >= ICALL_MAX_NUM_ENTITIES ||
      src >= ICALL_MAX_NUM_ENTITIES)
  {
    return (ICALL_ERRNO_INVALID_PARAMETER);
  }
  key = ICall_enterCSImpl();
  if (!ICall_entities[dest].task)
  {
    ICall_leaveCSImpl(key);
    return (ICALL_ERRNO_INVALID_PARAMETER);
  }

  ICall_leaveCSImpl(key);
  /* Note that once the entry is valid,
   * the value does not change and hence it is OK
   * to leave the critical section.
   */

  hdr->srcentity = src;
  hdr->dstentity = dest;
  hdr->format = format;

#ifdef ICALL_NO_APP_EVENTS
  // for applications, call direct to callback without enqueue/event_pend/dequeue
  if (ICall_entities[dest].service == ICALL_SERVICE_CLASS_APPLICATION)
  {
    if (ICall_entities[dest].appCallback != NULL)
    {
      uint8_t safeToDealloc = ICall_entities[dest].appCallback(0 /*event*/, (uint8_t *)msg);
      if (msg && safeToDealloc)
      {
        ICall_freeMsg(msg);
      }
    }
    return (ICALL_ERRNO_SUCCESS);
  }
#endif // ICALL_NO_APP_EVENTS
  ICall_msgEnqueue(&ICall_entities[dest].task->queue, msg);
  EventP_post((EventP_Handle)ICall_entities[dest].task->syncHandle, ICALL_POSIX_MSG_EVENT_ID);
  return (ICALL_ERRNO_SUCCESS);
}

/**
 * Retrieves a message, queued to receive queue of the calling thread.
 *
 * @param src   pointer to a variable to store the sender entity id
 *              of the received message.
 * @param dest  pointer to a variable to store the destination entity id
 *              of the received message.
 * @param msg   pointer to a pointer variable to store the starting
 *              address of a received message body.
 * @return @ref ICALL_ERRNO_SUCCESS when a message was successfully
 *         retrieved.<br>
 *         @ref ICALL_ERRNO_NOMSG when no message was queued to
 *         the receive queue at the moment.<br>
 *         @ref ICALL_ERRNO_UNKNOWN_THREAD when the calling thread
 *         does not have a received queue associated with it.
 *         This happens when neither ICall_enrollService() nor
 *         ICall_registerApp() was ever called from the calling
 *         thread.
 */
ICall_Errno ICall_fetchMsg(ICall_EntityID *src,
                                         ICall_EntityID *dest,
                                         void **msg)
{
  void *msgTemp;
  TaskP_Handle taskhandle = ICall_taskSelf();
  ICall_TaskEntry *taskentry = ICall_searchTask(taskhandle);
  ICall_MsgHdr *hdr;

  if (!taskentry)
  {
    return (ICALL_ERRNO_UNKNOWN_THREAD);
  }
  /* Successful */
  msgTemp = ICall_msgDequeue(&taskentry->queue);

  if (msgTemp == NULL)
  {
    return (ICALL_ERRNO_NOMSG);
  }
  hdr = (ICall_MsgHdr *) msgTemp - 1;
  *src = hdr->srcentity;
  *dest = hdr->dstentity;
  *msg = msgTemp;
  return (ICALL_ERRNO_SUCCESS);

}

/**
 * Check if Task Queue is empty.
 *
 * @return @ref true when queue is empty
 *
 *         @ref false when queue is not empty
 */
uint8 ICall_IsQueueEmpty()
{
  TaskP_Handle taskhandle = ICall_taskSelf();
  ICall_TaskEntry *taskentry = ICall_searchTask(taskhandle);
  if(taskentry->queue == NULL)
      return true;
  else
      return false;
}

/**
 * Transforms and entityId into a serviceId.
 * Note that this function is useful in case an application
 * waits for messages from both a server and another application,
 * in which case, the application can only use ICall_fetchMsg(),
 * not ICall_fetchServiceMsg() because the latter will return
 * @ref ICALL_ERRNO_CORRUPT_MSG when a message sent by the other
 * application is about to be fetched.<br>
 * This function, then, is useful to retrieve service id
 * matching the source entity id in case the source entity
 * id is not that of the other application.
 *
 * @param entityId   entity id
 * @param servId     pointer to a variable to store
 *                   the resultant service id
 * @return @ref ICALL_ERRNO_SUCCESS if the transformation was successful.<br>
 *         @ref ICALL_ERRNO_INVALID_SERVICE if no matching service
 *         is found for the entity id.
 */
ICall_Errno ICall_entityId2ServiceId(ICall_EntityID entityId,
                                                   ICall_ServiceEnum *servId)
{
  return ICall_primEntityId2ServiceId(entityId, servId);
}

/**
 * Aborts.
 *
 * This is preferred over C runtime abort() function,
 * in an external image since the C runtime abort() is only
 * guaranteed in a root image which contains the C runtime
 * entry function that is executed upon startup.
 */
ICall_Errno
ICall_abort(void)
{
#ifdef HALNODEBUG
#elif  defined(EXT_HAL_ASSERT)
  HAL_ASSERT(HAL_ASSERT_CAUSE_ICALL_ABORT);
#else
  {
    volatile uint8_t j=1;
    while(j);
  }
#endif /* EXT_HAL_ASSERT */
  ICALL_HOOK_ABORT_FUNC();
  return(ICALL_ERRNO_SUCCESS);
}

/**
 * Enables interrupt.
 * @param intnum   interrupt number
 * @return @ref ICALL_ERRNO_SUCCESS.
 */
ICall_Errno
ICall_enableint(int intnum)
{
  Hwi_enableinterrupt(intnum);
  return (ICALL_ERRNO_SUCCESS);
}

/**
 * Disables interrupt
 * @param intnum  interrupt number
 * @return @ref ICALL_ERRNO_SUCCESS
 */
ICall_Errno
ICall_disableint(int intnum)
{
  Hwi_disableinterrupt(intnum);
  return (ICALL_ERRNO_SUCCESS);
}


uint_fast32_t GetTickCount(void)
{
  return ClockP_getSystemTicks();
}


/**
 * Gets the current tick counter value.
 * @return current tick counter value
 */
uint_fast32_t
ICall_getTicks(void)
{
    return GetTickCount();
}

/**
 * Gets the tick period.
 * @return tick period in microseconds.
 */
uint_fast32_t
ICall_getTickPeriod(void)
{
    return ClockP_getSystemTickPeriod();
}

/**
 * Gets the maximum timeout period supported by
 * ICall_setTimerMSecs() function.
 *
 * @return maximum timeout period in milliseconds
 */
uint_fast32_t
ICall_getMaxMSecs(void)
{
    uint_fast64_t tmp = ((uint_fast64_t) 0x7ffffffful) * (ICall_getTickPeriod());
    tmp /= 1000;
    if (tmp >= 0x80000000ul)
    {
        tmp = 0x7ffffffful;
    }
    return ((uint_least32_t) tmp);
}

/**
 * Set up or restart a timer.
 * Note that the timer setup initially by this function may be restarted
 * using ICall_setTimer() function as well.
 *
 * @param msecs  timeout period in milliseconds after which callback function
 *               shall be called.
 * @param cback  callback function pointer
 * @param arg    argument to pass to the callback function
 * @param id     pointer to the timer ID.
 *               If a new timer must be set up, the value of the timer ID
 *               must be set to @ref ICALL_INVALID_TIMER_ID when making this
 *               call and when the function returns successfully, the variable
 *               will be assigned a new timer ID value.
 *               The value other than @ref ICALL_INVALID_TIMER_ID shall be
 *               regarded as a request to restart the earlier set timer.
 * @return @ref ICALL_ERRNO_SUCCESS when successful;<br>
 *         @ref ICALL_ERRNO_INVALID_PARAMETER when msecs is greater than
 *              maximum value supported;<br>
 *         @ref ICALL_ERRNO_NO_RESOURCE when ran out of resource.
 *         Check ICall heap size and OS heap size if this happens.
 *
 * @see ICall_getMaxMSecs()
 */
ICall_Errno
ICall_setTimerMSecs(uint_fast32_t msecs,
                    ICall_TimerCback cback,
                    void *arg,
                    ICall_TimerID *id)
{
  uint32_t ticks;
  uint32_t timeout;
  /* Convert to tick time */
  ICall_Errno errno  = ICall_msecs2Ticks(msecs, &ticks);

  if (errno != ICALL_ERRNO_SUCCESS)
  {
    return (errno);
  }
  timeout = ticks;
  ICall_setTimer(timeout, cback, arg, id);

  return (errno);
}

/**
 * Set up or restart a timer.
 * Note that the timer setup initially by this function may be restarted
 * using ICall_setTimerMSecs() function as well.
 *
 * @param ticks  timeout period in ticks after which the callback function
 *               shall be called.
 * @param cback  callback function pointer
 * @param arg    argument to pass to the callback function
 * @param id     pointer to the timer ID.
 *               If a new timer must be set up, the value of the timer ID
 *               must be set to @ref ICALL_INVALID_TIMER_ID when making this
 *               call and when the function returns successfully, the variable
 *               will be assigned a new timer ID value.
 *               The value other than @ref ICALL_INVALID_TIMER_ID shall be
 *               regarded as a request to restart the earlier set timer.
 * @return @ref ICALL_ERRNO_SUCCESS when successful;<br>
 *         @ref ICALL_ERRNO_NO_RESOURCE when ran out of resource.
 *         Check ICall heap size and OS heap size if this happens.
 *
 * @see ICall_getTickPeriod()
 */


ICall_Errno
ICall_setTimer(uint32_t ticks,
               ICall_TimerCback cback,
               void *arg,
               ICall_TimerID *id)
{

  ICall_ScheduleEntry *entry;

  if (*id == ICALL_INVALID_TIMER_ID)
  {
    ClockP_Params params;

    /* Create a new timer */
    entry = ICall_heapMalloc(sizeof(ICall_ScheduleEntry));
    if (entry == NULL)
    {
      /* allocation failed */
      return (ICALL_ERRNO_NO_RESOURCE);
    }
    ClockP_Params_init(&params);
    params.startFlag = FALSE;
    params.period = 0;
    params.arg = (uintptr_t) entry;

    entry->clockP = ClockP_create(ICall_clockFunc,
                                  ticks,
                                  &params);
    if (!entry->clockP)
    {
      /* abort */
      ICall_abort();
      ICall_heapFree(entry);
      return (ICALL_ERRNO_NO_RESOURCE);
    }
    entry->cback = cback;
    entry->arg = arg;
    *id = (ICall_TimerID) entry;
  }
  else
  {
#ifndef FREERTOS
    ICall_CSState key;
#endif

    entry = (ICall_ScheduleEntry *) *id;

    /* Critical section is entered to disable interrupts that might cause call
     * to callback due to race condition */
#ifndef FREERTOS
    // do not enter to critical section because it stuck in for loop
    // in vListInsert function under the freertos list.c file
    // according to the note in vListInsert function -
    // we cannot call an API function from within a critical section
    key = ICall_enterCriticalSection();
#endif
    ClockP_stop(entry->clockP);
    entry->arg = arg;
#ifndef FREERTOS
    ICall_leaveCriticalSection(key);
#endif
  }
  ClockP_setTimeout(entry->clockP, ticks);

  ClockP_start(entry->clockP);

  return (ICALL_ERRNO_SUCCESS);

}

/**
 * Stops a timer.
 *
 * @param id    timer ID.
 */
void
ICall_stopTimer(ICall_TimerID id)
{
  ICall_ScheduleEntry *entry = (ICall_ScheduleEntry *) id;

  if (id == ICALL_INVALID_TIMER_ID)
  {
    return;
  }

  ClockP_stop(entry->clockP);
}

/**
 * Increments or decrements power activity counter.
 *
 * When power activity counter is greater than zero,
 * the device shall stay in the active power state.
 * The caller has to make sure that it decrements the counter
 * as many times as it has incremented the counter
 * when there is no activity that requires the active power state.
 * It is recommended that each client increments the counter by just one,
 * but it is not mandated to be so.
 *
 * @param incFlag   TRUE to indicate incrementing the counter.<br>
 *                  FALSE to indicate decrementing the counter.
 * @return @ref TRUE if power is required.<br>
 *         @ref FALSE if power is not required.<br>
 */
bool
ICall_pwrUpdActivityCounter(bool incFlag)
{
  ICall_PwrUpdActivityCounterArgs args;
  args.incFlag = incFlag;
  ICallPlatform_pwrUpdActivityCounter(&args);
  return (args.pwrRequired);
}

/**
 * Configures power constraint/dependency set/release actions upon
 * activity counter change.
 *
 * When activity counter value drops to zero, all constraints and
 * dependencies configured by this function shall be released.<br>
 * When activity counter value increments to one, all constraints
 * and dependencies configured by this function shall be set.
 *
 * @param bitmap  a bitmap of constraint or dependency flags.<br>
 *                The flag definition is specific to each platform.
 *                For instance, see ICallCC26xxDefs.h.
 *
 * @return @ref ICALL_ERRNO_SUCCESS when successful<br>
 *         @ref ICALL_ERRNO_INVALID_PARAMETER when an invalid
 *              flag in the bitmap is detected.<br>
 */
ICall_Errno
ICall_pwrConfigACAction(ICall_PwrBitmap_t bitmap)
{
  ICall_PwrBitmapArgs args;
  args.bitmap = bitmap;
  return (ICallPlatform_pwrConfigACAction(&args));
}

/**
 * Sets power constraints and dependencies.
 *
 * @param bitmap  a bitmap of constraint or dependency flags.<br>
 *                The flag definition is specific to each platform.
 *                For instance, see ICallCC26xxDefs.h.
 *
 * @return @ref ICALL_ERRNO_SUCCESS when successful<br>
 *         @ref ICALL_ERRNO_INVALID_PARAMETER when an invalid
 *              flag in the bitmap is detected.<br>
 */
ICall_Errno
ICall_pwrRequire(ICall_PwrBitmap_t bitmap)
{
  ICall_PwrBitmapArgs args;
  args.bitmap = bitmap;
  return (ICallPlatform_pwrRequire(&args));
}

/**
 * Releases power constraints and dependencies.
 *
 * @param bitmap  a bitmap of constraint or dependency flags.<br>
 *                The flag definition is specific to each platform.
 *                For instance, see ICallCC26xxDefs.h.
 *
 * @return @ref ICALL_ERRNO_SUCCESS when successful<br>
 *         @ref ICALL_ERRNO_INVALID_PARAMETER when an invalid
 *              flag in the bitmap is detected.<br>
 */
ICall_Errno
ICall_pwrDispense(ICall_PwrBitmap_t bitmap)
{
  ICall_PwrBitmapArgs args;
  args.bitmap = bitmap;
  return (ICallPlatform_pwrDispense(&args));
}

/**
 * Checks whether HF XOSC is stable.
 * This function must be called after HF XOSC is turned on
 * (through power dependency).
 *
 * @return TRUE when HF XOSC is stable.<br>
 *         FALSE when HF XOSC is not stable.<br>
 */
bool
ICall_pwrIsStableXOSCHF(void)
{
  ICall_GetBoolArgs args;
  (void) ICallPlatform_pwrIsStableXOSCHF(&args);
  return (args.value);
}

/**
 * Switch clock source to HF XOSC.
 * This function must be called after HF XOSC is stable.
 *
 * @return @ref ICALL_ERRNO_SUCCESS
 */
ICall_Errno
ICall_pwrSwitchXOSCHF(void)
{
  ICall_FuncArgsHdr args;
  return (ICallPlatform_pwrSwitchXOSCHF(&args));
}

/**
 * Gets the estimated crystal oscillator startup time.
 *
 * @return estimated crystal oscillator startup time
 */
uint32_t
ICall_pwrGetXOSCStartupTime(uint_fast32_t timeUntilWakeupInMs)
{
  ICall_PwrGetXOSCStartupTimeArgs args;
  args.timeUntilWakeupInMs = timeUntilWakeupInMs;
  (void) ICallPlatform_pwrGetXOSCStartupTime(&args);
  return (args.value);
}

/**
 * Registers a power state transition notify function.
 *
 * The registered notify function shall be called when the power state
 * changes.
 *
 * @param fn  notify function pointer
 * @param obj pointer to data object to be passed to notify function.<br>
 *            This pointer must not be NULL and can point to an aggregate type
 *            containing the @ref ICall_PwrNotifyData.
 *            The object must not be released.
 * @return @ref ICALL_ERRNO_SUCCESS when successful<br>
 *         @ref ICALL_ERRNO_NO_RESOURCE when registration failed<br>
 */
ICall_Errno
ICall_pwrRegisterNotify(ICall_PwrNotifyFn fn, ICall_PwrNotifyData *obj)
{
  ICall_PwrRegisterNotifyArgs args;
  args.hdr.service = ICALL_SERVICE_CLASS_PRIMITIVE;
  args.hdr.func = ICALL_PRIMITIVE_FUNC_PWR_REGISTER_NOTIFY;
  args.fn = fn;
  args.obj = obj;
  return (ICallPlatform_pwrRegisterNotify(&args));
}

/**
 * Retrieves power transition state.
 *
 * @return Implementation specific transition state when successful<br>
 *         Zero when the function is not implemented.<br>
 */
uint_fast8_t
ICall_pwrGetTransitionState(void)
{
  ICall_PwrGetTransitionStateArgs args;
  ICall_Errno errno;

  errno = ICallPlatform_pwrGetTransitionState(&args);

  if (errno == ICALL_ERRNO_SUCCESS)
  {
    return (args.state);
  }
  return (0);
}

/**
 * Creates a new RTOS task.
 *
 * @param entryfn   task entry function.
 * @param priority  task priority as understood by the underlying RTOS
 * @param stacksize stack size as understood by the underlying RTOS
 * @param arg       argument to pass to the task entry function
 *
 * @return @ref ICALL_ERRNO_SUCCESS when successful<br>
 *         @ref ICALL_ERRNO_NO_RESOURCE when creation failed<br>
 */


#ifdef ICALL_RTOS_SEMAPHORE_API
/**
 * Creates a semaphore.
 *
 * @param mode  Semaphore mode<br>
 *              @ref ICALL_SEMAPHORE_MODE_COUNTING, or<br>
 *              @ref ICALL_SEMAPHORE_MODE_BINARY
 * @param initcount initial count value
 * @return created semaphore when successful<br>
 *         NULL when creation failed<br>
 */
ICall_Semaphore
ICall_createSemaphore(uint_fast8_t mode, int initcount)
{
  /* Semaphore_Params is a huge structure.
   * To reduce stack usage, heap is used instead.
   * This implies that ICall_createSemaphore() must be called before heap
   * space may be exhausted.
   */
  ICall_Semaphore sem;
  Semaphore_Params *semParams =
    (Semaphore_Params *) ICall_heapMalloc(sizeof(Semaphore_Params));

  if (semParams == NULL)
  {
    return (NULL);
  }

  Semaphore_Params_init(semParams);
  if (mode == ICALL_SEMAPHORE_MODE_BINARY)
  {
    semParams->mode = Semaphore_Mode_BINARY;
  }

  sem = Semaphore_create(args->initcount, semParams, NULL);
  ICall_heapFree(semParams);

  return (sem);

}
#endif

#ifdef ICALL_RTOS_SEMAPHORE_API
/**
 * Post on a semaphore
 *
 * @param sem           semaphore.
 *
 * @return @ref ICALL_ERRNO_SUCCESS when the operation was successful
 */
ICall_Errno
ICall_postSemaphore(ICall_Semaphore sem)
{
  Semaphore_post(sem);
  return (ICALL_ERRNO_SUCCESS);
}
#endif /* ICALL_RTOS_SEMAPHORE_API */

#ifdef ICALL_RTOS_EVENTS_API
/**
 * Creates a event.
 *
 * @return created event when successful<br>
 *         NULL when creation failed<br>
 */
ICall_Event
ICall_createEvent(void)
{
  ICall_Event event = Event_create(NULL, NULL);
  return (event);


  ICall_CreateEventArgs args;
  ICall_Errno errno;

  args.hdr.service = ICALL_SERVICE_CLASS_PRIMITIVE;
  args.hdr.func = ICALL_PRIMITIVE_FUNC_CREATE_EVENT;
  errno = ICall_dispatcher(&args.hdr);
  if (errno == ICALL_ERRNO_SUCCESS)
  {
    return (args.event);
  }
  return (NULL);
}

/**
 * Post on an event
 *
 * @param event           event.
 *
 * @return @ref ICALL_ERRNO_SUCCESS when the operation was successful
 */
ICall_Errno
ICall_postEvent(ICall_Event event, uint32_t events)
{
  Event_post(event, events);
  return (ICALL_ERRNO_SUCCESS);
}
/**
 * Waits on a event for ICALL_MSG_EVENT_ID
 *
 * @param event           event.
 * @param milliseconds  timeout in milliseconds
 *                      or @ref ICALL_TIMEOUT_FOREVER to wait forever
 *
 * @return @ref ICALL_ERRNO_SUCCESS when the operation was successful
 *         and event was received.<br>
 *         @ref ICALL_ERRNO_TIMEOUT when designated timeout period
 *         has passed since the call of the function without
 *         the event being signaled.
 */
ICall_Errno
ICall_waitEvent(ICall_Event event, uint_fast32_t milliseconds)
{
  uint32_t timeout;

  if (milliseconds == 0)
  {
    timeout = BIOS_NO_WAIT;
  }
  else if (milliseconds == ICALL_TIMEOUT_FOREVER)
  {
    timeout = BIOS_WAIT_FOREVER;
  }
  else
  {
    ICall_Errno errno = ICall_msecs2Ticks(milliseconds, &timeout);
    if (errno != ICALL_ERRNO_SUCCESS)
    {
      return (errno);
    }
  }

  if (Event_pend(event, 0, ICALL_POSIX_MSG_EVENT_ID, timeout))
  {
    return (ICALL_ERRNO_SUCCESS);
  }
  return (ICALL_ERRNO_TIMEOUT);
}
#endif /* ICALL_RTOS_EVENTS_API */

#ifdef ICALL_RTOS_SEMAPHORE_API
/**
 * Waits on a semaphore
 *
 * @param sem           semaphore.
 * @param milliseconds  timeout in milliseconds
 *                      or @ref ICALL_TIMEOUT_FOREVER to wait forever
 *
 * @return @ref ICALL_ERRNO_SUCCESS when the operation was successful
 *         and semaphore was acquired.<br>
 *         @ref ICALL_ERRNO_TIMEOUT when designated timeout period
 *         has passed since the call of the function without
 *         the semaphore being signaled.
 */
ICall_Errno
ICall_waitSemaphore(ICall_Semaphore sem, uint_fast32_t milliseconds)
{
  uint32_t timeout;

  if (milliseconds == 0)
  {
    timeout = BIOS_NO_WAIT;
  }
  else if (milliseconds == ICALL_TIMEOUT_FOREVER)
  {
    timeout = BIOS_WAIT_FOREVER;
  }
  else
  {
    ICall_Errno errno = ICall_msecs2Ticks(milliseconds, &timeout);
    if (errno != ICALL_ERRNO_SUCCESS)
    {
      return (errno);
    }
  }
  if (Semaphore_pend(sem, timeout))
  {
    return (ICALL_ERRNO_SUCCESS);
  }
  return (ICALL_ERRNO_TIMEOUT);
}
#endif /* ICALL_RTOS_SEMAPHORE_API */


/* Util function that take time in ticks and convert it into ms - relate to system clock (returns system clock + converted ms) */
// this function is unused and and can be used in the future. To avoid warnings it was commented.
/*
  static void AbsoluteTimeInMilliPlusTimer(uint_least32_t timeout,struct timespec *tsTimer)
  {

    clock_gettime(CLOCK_REALTIME, tsTimer);

    tsTimer->tv_sec += (timeout / 1000);
    tsTimer->tv_nsec += (timeout % 1000) * 1000000;
  }
*/

/**
 * Waits for and retrieves a message received at the message queue
 * associated with the calling thread, which matches a certain condition.
 *
 * @param milliseconds  timeout period in milliseconds.
 * @param matchFn  pointer to a function that would return TRUE when
 *                 the message matches its condition.
 * @param src    pointer to a variable to store the service id
 *               of the registered server which sent the retrieved
 *               message, or NULL if not interested in storing service id.
 * @param dest   pointer to a variable to store the entity id
 *               of the destination of the message,
 *               of NULL if not interested in storing the destination entity id.
 * @param msg    pointer to a pointer variable to store the
 *               starting address of the message body being
 *               retrieved. The pointer must not be NULL.
 * @return @ref ICALL_ERRNO_SUCCESS when the operation was successful
 *         and a message was retrieved.<br>
 *         @ref ICALL_ERRNO_TIMEOUT when designated timeout period
 *         has passed since the call of the function without
 *         the semaphore being signaled.
 *         @ref ICALL_ERRNO_UNKNOWN_THREAD when this function is
 *         called from a thread which has not registered
 *         an entity, either through ICall_enrollService()
 *         or through ICall_registerApp().
 */
ICall_Errno
ICall_waitMatch(uint_least32_t milliseconds,
                ICall_MsgMatchFn matchFn,
                ICall_ServiceEnum *src,
                ICall_EntityID *dest,
                void **msg)
{
  TaskP_Handle taskhandle = ICall_taskSelf();
  ICall_TaskEntry *taskentry = ICall_searchTask(taskhandle);
  ICall_MsgQueue prependQueue = NULL;
#ifndef ICALL_EVENTS
  uint_fast16_t consumedCount = 0;
#endif
  uint32_t timeout;
  uint_fast32_t timeoutStamp;
  ICall_Errno errno;

  if (!taskentry)
  {
    return (ICALL_ERRNO_UNKNOWN_THREAD);
  }
  /* Successful */
  if (milliseconds == 0)
  {
    timeout = BIOS_NO_WAIT;
  }
  else if (milliseconds == ICALL_TIMEOUT_FOREVER)
  {
    timeout = BIOS_WAIT_FOREVER;
  }
  else
  {
    /* Convert milliseconds to number of ticks */
    errno = ICall_msecs2Ticks(milliseconds, &timeout);
    if (errno != ICALL_ERRNO_SUCCESS)
    {
      return (errno);
    }
  }

  errno = ICALL_ERRNO_TIMEOUT;

  timeoutStamp = ICall_getTicks() + timeout;

#ifdef ICALL_LITE

  while(EventP_pend(taskentry->syncHandle, ICALL_POSIX_WAITMATCH_EVENT_ID, 0, timeout))
#else /* !ICALL_LITE */
  while (ICALL_SYNC_HANDLE_PEND(taskentry->syncHandle, timeout))
#endif /* ICALL_LITE */
  {
    ICall_EntityID fetchSrc;
    ICall_EntityID fetchDst;
    ICall_ServiceEnum servId;
    void *fetchMsg;
    errno = ICall_fetchMsg(&fetchSrc, &fetchDst, &fetchMsg);
    if (errno == ICALL_ERRNO_SUCCESS)
    {
      if (ICall_primEntityId2ServiceId(fetchSrc, &servId) ==
            ICALL_ERRNO_SUCCESS)
      {
        if (matchFn(servId, fetchDst, fetchMsg))
        {
          /* Matching message found*/
          if (src != NULL)
          {
            *src = servId;
          }
          if (dest != NULL)
          {
            *dest = fetchDst;
          }
          *msg = fetchMsg;
          errno = ICALL_ERRNO_SUCCESS;
          break;
        }
      }
      /* Message was received but it wasn't expected one.
       * Add to the prepend queue */
      ICall_msgEnqueue(&prependQueue, fetchMsg);
#ifdef ICALL_EVENTS
    /* Event are binary semaphore, so if several messsages are posted while
     * we are processing one, it's possible that some of them are 'missed' and
     * not processed. Sending a event to ourself force this loop to run until
     * all the messages in the queue are processed.
     */
#ifdef ICALL_LITE
      EventP_post((EventP_Handle)taskentry->syncHandle, ICALL_POSIX_WAITMATCH_EVENT_ID);

#else /* !ICALL_LITE */
     ICALL_SYNC_HANDLE_POST(taskentry->syncHandle);
#endif /* ICALL_LITE*/
#endif /* ICALL_EVENTS */
    }

    /* Prepare for timeout exit */
    errno = ICALL_ERRNO_TIMEOUT;

#ifndef ICALL_EVENTS
    /* Keep the decremented semaphore count */
    consumedCount++;
#endif  /* ICALL_EVENTS */
    if (timeout != BIOS_WAIT_FOREVER &&
        timeout != BIOS_NO_WAIT)
    {
      /* Readjust timeout */
      uint32_t newTimeout = timeoutStamp - ICall_getTicks();

      if (newTimeout == 0 || newTimeout > timeout)
      {
        break;
      }
      timeout = newTimeout;
    }
  }


  /* Prepend retrieved irrelevant messages */
  ICall_msgPrepend(&taskentry->queue, prependQueue);
#ifndef ICALL_EVENTS
  /* Re-increment the consumed semaphores */
  for (; consumedCount > 0; consumedCount--)
  {
    Semaphore_post(taskentry->syncHandle);
  }
#endif /* ICALL_EVENTS */
  return (errno);
}

/**
 * Retrieves an entity ID of (arbitrary) one of the entities registered
 * from the calling thread.
 *
 * Note that, if multiple entities were registered from the same thread,
 * this function shall arbitrarily pick one of the entities.
 *
 * @return A valid entity ID or @ref ICALL_INVALID_ENTITY_ID
 *         when no entity was registered from the calling thread.
 */
ICall_EntityID
ICall_getEntityId(void)
{
  ICall_EntityID id;
  TaskP_Handle taskhandle = ICall_taskSelf();
  ICall_CSState key;
  size_t i;
  key = ICall_enterCSImpl();
  for (i = 0; i < ICALL_MAX_NUM_ENTITIES; i++)
  {
    if (ICall_entities[i].service == ICALL_SERVICE_CLASS_INVALID_ENTRY)
    {
      /* Not found */
      break;
    }
    if (ICall_entities[i].task->task == taskhandle)
    {
      /* Found */
      id = i;
      ICall_leaveCSImpl(key);
      return (id);
    }
  }
  ICall_leaveCSImpl(key);
  return ICALL_INVALID_ENTITY_ID;

}

/**
 * Checks whether the calling thread provides the designated service.
 *
 * @param  service    Service enumeration value for the service.
 *
 * @return Non-zero if the current thread provides the designated service.
 *         Zero, otherwise.
 */
uint_fast8_t
ICall_threadServes(ICall_ServiceEnum service)
{
  uint_fast8_t res = 0;
  TaskP_Handle taskhandle;
  ICall_CSState key;
  size_t i;
  taskhandle = ICall_taskSelf();

  key = ICall_enterCSImpl();
  for (i = 0; i < ICALL_MAX_NUM_ENTITIES; i++)
  {
    if (ICall_entities[i].service == ICALL_SERVICE_CLASS_INVALID_ENTRY)
    {
      /* Not found */
      break;
    }
    else if (ICall_entities[i].service == service)
    {
      res = (uint_fast8_t)
        (ICall_entities[i].task->task == taskhandle);
    }
  }
  ICall_leaveCSImpl(key);
  return (res);
}

/**
 * Retrieves an internal ID understood only by a service entity,
 * corresponding to an ICall entity ID.
 *
 * This function is used when a message body includes a field indicating a
 * software entity and the destination of the message has its own internal
 * message routing mechanism with its own entity enumeration system.
 *
 * @param  service    Service enumeration value for the service.
 *                    Currently @ref ICALL_SERVICE_CLASS_BLE_MSG is supported.
 * @param  entity     ICall entity ID.
 *
 * @return Stack specific 8 bit ID or 0xFF when failed.
 */
uint_fast8_t
ICall_getLocalMsgEntityId(ICall_ServiceEnum service, ICall_EntityID entity)
{
  ICall_GetLocalMsgEntityIdArgs args;
  ICall_Errno errno;
  args.hdr.service = service;
  args.hdr.func = ICALL_MSG_FUNC_GET_LOCAL_MSG_ENTITY_ID;
  args.entity = entity;
  errno = ICall_dispatcher(&args.hdr);
  if (errno == ICALL_ERRNO_SUCCESS)
  {
    return (args.localId);
  }
  return (0xFF);
}

#endif /* ICALL_JT */

#ifdef ICALL_LITE
 /*******************************************************************************
 * @fn          matchLiteCS
 */
static bool matchLiteCS(ICall_ServiceEnum src,
                        ICall_EntityID dest, const void *msg)
{
  (void) src;
  (void) dest;
  ICall_LiteCmdStatus *pMsg = (ICall_LiteCmdStatus *)msg;
  return (pMsg->cmdId == ICALL_LITE_DIRECT_API_DONE_CMD_ID);
}
 /*******************************************************************************
 * @fn          icall_directAPI
 * see headers for details.
 */
uint32_t icall_directAPI( uint8_t service , icall_lite_id_t id, ... )
{
  va_list argp;
  uint32_t res;
  icallLiteMsg_t liteMsg;

  // The following will push all parameter in the runtime stack.
  // This need to be call before any other local declaration of variable....
  va_start(argp, id);

  // Todo - add string for every icall API function, instead of printing function address
  BLE_LOG_INT_INT(0, BLE_LOG_MODULE_APP, "APP : icall_directAPI to BLE func=0x%x, status=%d\n", id, 0);
  // Create the message that will be send to the requested service..
  liteMsg.hdr.len = sizeof(icallLiteMsg_t);
  liteMsg.hdr.next = NULL;
  liteMsg.hdr.dest_id = ICALL_UNDEF_DEST_ID;
  liteMsg.msg.directAPI  = id;
  liteMsg.msg.pointerStack = (uint32_t *)(*((uint32_t *)(&argp)));
  ICall_sendServiceMsg(ICall_getEntityId(), service,
                       ICALL_MSG_FORMAT_DIRECT_API_ID, &(liteMsg.msg));

  // Since stack needs to always have a higher priority than the thread calling
  // the API, when we reach this point the API has been executed by the stack.
  // This implies the following:
  //  - API are not called in critical section or in section where task
  //    switching is disabled
  // It is possible that the stack is blocking on this API, in this case a
  // sync object needs to be used in order for this call to resume only when
  // the API has been process in full.
  {
    ICall_Errno errno;
    void *pCmdStatus = NULL;

    errno = ICall_waitMatch(ICALL_TIMEOUT_PREDEFINE, matchLiteCS, NULL, NULL,
                    (void **)&pCmdStatus);
    if (errno == ICALL_ERRNO_TIMEOUT)
    {
#ifdef HALNODEBUG
#elif  defined(EXT_HAL_ASSERT)
      HAL_ASSERT(HAL_ASSERT_CAUSE_ICALL_TIMEOUT);
#else /* !EXT_HAL_ASSERT */
      ICall_abort();
#endif /* EXT_HAL_ASSERT */
    }
    else if (errno == ICALL_ERRNO_SUCCESS)
    {
      if (pCmdStatus)
      {
        ICall_freeMsg(pCmdStatus);
      }
    }
    else
    {
#ifdef HALNODEBUG
#else /* ! HALNODEBUG */
      ICall_abort();
#endif /* HALNODEBUG */
    }
  }

  // The return parameter is set in the runtime stack, at the location of the
  // first parameter.
  res = liteMsg.msg.pointerStack[0];

  va_end(argp);

  return (res);
}

 /*******************************************************************************
 * @fn          ICall_sendServiceComplete
 * see headers for details.
 */
ICall_Errno ICall_sendServiceComplete(ICall_EntityID src,
                                      ICall_EntityID dest,
                                      ICall_MSGFormat format,
                                      void *msg)
{
  ICall_CSState key;
  ICall_MsgHdr *hdr = (ICall_MsgHdr *) msg - 1;

  if (dest >= ICALL_MAX_NUM_ENTITIES ||
      src >= ICALL_MAX_NUM_ENTITIES)
  {
    return (ICALL_ERRNO_INVALID_PARAMETER);
  }
  key = ICall_enterCSImpl();
  if (!ICall_entities[dest].task)
  {
    ICall_leaveCSImpl(key);
    return (ICALL_ERRNO_INVALID_PARAMETER);
  }

  ICall_leaveCSImpl(key);
  /* Note that once the entry is valid,
   * the value does not change and hence it is OK
   * to leave the critical section.
   */

  hdr->srcentity = src;
  hdr->dstentity = dest;
  hdr->format = format;
  ICall_msgEnqueue(&ICall_entities[dest].task->queue, msg);

  EventP_post((EventP_Handle)ICall_entities[dest].task->syncHandle, ICALL_POSIX_WAITMATCH_EVENT_ID);

  return (ICALL_ERRNO_SUCCESS);
}
#endif /* ICALL_LITE*/
#ifdef ICALL_NO_APP_EVENTS
ICall_Errno ICall_registerAppCback(uint8_t *selfEntity, appCallback_t appCallback)
{
  ICall_EntityID   localSelfEntity;
  ICall_SyncHandle localSyncEvent; // no use
  ICall_Errno status;

  // ******************************************************************
  // N0 STACK API CALLS CAN OCCUR BEFORE THIS CALL TO ICall_registerApp
  // ******************************************************************
  // Register the current thread as an ICall dispatcher application
  // so that the application can send and receive messages.
  status = ICall_registerApp(&localSelfEntity, &localSyncEvent);

  // applicatios should use the task entity ID
  *selfEntity = localSelfEntity;

  // Save app callback to be used instead of enqueue/event_pend/dequeue
  ICall_entities[localSelfEntity].appCallback = appCallback;

  return status;
}
#endif // ICALL_NO_APP_EVENTS
