/******************************************************************************

 @file  ble_init.c

 @brief BLE Init code

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2014 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
/*******************************************************************************
 * INCLUDES
 */

#include "bcomdef.h"
#include "icall.h"
#include "osal_tasks.h"
#include "osal_cbtimer.h"
#include "hal_types.h"
#include "ble_dispatch_lite.h"
#ifndef USE_DEFAULT_USER_CFG
#include "ble_user_config.h"
#endif //USE_DEFAULT_USER_CFG
#include "ti/drivers/dpl/SemaphoreP.h"

/*******************************************************************************
 * EXTERNS
 */
extern int ICall_createWorkerThread(void);

/*******************************************************************************
 * PROTOTYPES
 */
typedef void (*bleStack_RemoteTaskEntry)(const ICall_RemoteTaskArg *arg0,
                                      void *arg1);
/*******************************************************************************
 * MACROS
 */

/**
 * Initializer for custom initialization parameters.
 * Each element of the array corresponds to initialization parameter
 * (a pointer) specific to the image to be passed to the entry function
 * defined in @ref ICALL_ADDR_MAPS initializer.
 */
#ifndef USE_DEFAULT_USER_CFG
// BLE user defined configuration

#define BLESTACK_USER_CFG           { &bleStackConfig,         \
                                      &boardConfig,            \
                                      &bleAppServiceInfoTable };

icall_userCfg_t bleStack_user0Cfg     = BLESTACK_USER_CFG;

#define BLE_STACK_USER0_CFG             &bleStack_user0Cfg    //!< user config

#else //USE_DEFAULT_USER_CFG

#define BLE_STACK_USER0_CFG             NULL       //!< user config
#endif // USE_DEFAULT_USER_CFG

#define BLESTACK_TASK_PRIORITIES    { 5 }
#define BLESTACK_TASK_STACK_SIZES   { 1500 }
#define BLESTACK_CUSTOM_INIT_PARAMS { BLE_STACK_USER0_CFG }

/** @internal initialization parameter (pointer) for each remote thread */
#define bleStack_getInitParams(_i) (bleStack_initParams[i])

/** @internal external image count */
#define BLESTACK_REMOTE_THREAD_COUNT \
  (sizeof(bleStack_threadEntries)/sizeof(bleStack_threadEntries[0]))

//extern ICall_RemoteTaskEntry bleStack_startup_entry;
#define BLESTACK_ADDR_MAPS \
{ \
  (bleStack_RemoteTaskEntry) (bleStack_startup_entry) \
}

#define LL_INIT_TIMEOUT_MSEC              10000


/*******************************************************************************
 * LOCAL FUNCTION DEFINITIONS
 */
/**
 * Initializer for an array of @ref ICall_RemoteTaskEntry.
 * Each element of the array corresponds to an entry function
 * of an external image.
 * The function address must be an odd address for CC2650
 * so that call will be made in Thumb mode
 */
void bleStack_startup_entry( const ICall_RemoteTaskArg *arg0, void *arg1 );

/*******************************************************************************
 * CONSTANTS
 */

/**
 * @internal
 * Array of entry function of external images.
 */
static const bleStack_RemoteTaskEntry bleStack_threadEntries[] = BLESTACK_ADDR_MAPS;


/** @internal thread priorities to be assigned to each remote thread */
static const int bleStack_threadPriorities[] = BLESTACK_TASK_PRIORITIES;

/** @internal thread stack max depth for each remote thread */
static const size_t bleStack_threadStackSizes[] = BLESTACK_TASK_STACK_SIZES;

static const void *bleStack_initParams[] = BLESTACK_CUSTOM_INIT_PARAMS;

__attribute__((weak)) const pTaskEventHandlerFn tasksArr[] =
{
  LL_ProcessEvent,                                                  // task 0
#ifndef ICALL_LITE
  HCI_ProcessEvent,                                                 // task 1
#endif //ICALL_LITE
#if defined ( OSAL_CBTIMER_NUM_TASKS )
  OSAL_CBTIMER_PROCESS_EVENT( osal_CbTimerProcessEvent ),           // task 2
#endif /* OSAL_CBTIMER_NUM_TASKS */
#ifdef ICALL_LITE
  ble_dispatch_liteProcess,                                         // task 3
#else /* !ICALL_LITE */
  bleDispatch_ProcessEvent                                          // task 3
#endif /* ICAL_LITE */
};

__attribute__((weak)) const uint8 tasksCnt = sizeof( tasksArr ) / sizeof( tasksArr[0] );

__attribute__((weak))  uint16 *tasksEvents;

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */
static SemaphoreP_Handle bleStack_initDone_sem;

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/**
 * Main entry function for the stack image
 */
int bleStack_main( void *arg )
{
  /* User reconfiguration of BLE Controller and Host variables */
  setBleUserConfig( (icall_userCfg_t *)arg );

  /* Establish OSAL for a stack service that requires accompanying
   * messaging service */
  if (ICall_enrollService(ICALL_SERVICE_CLASS_BLE_MSG,
                          (ICall_ServiceFunc) osal_service_entry,
                          &osal_entity,
                          &osal_syncHandle) != ICALL_ERRNO_SUCCESS)
  {
    /* abort */
    ICall_abort();
  }

#ifndef CONFIG_SOC_CC2340R5
#ifdef CC23X0
#ifndef USE_HSM
  if (LL_initRNGNoise() != LL_STATUS_SUCCESS)
  {
    /* abort */
    ICall_abort();
  }
#endif
#endif
#endif // CONFIG_SOC_CC2340R5

  // Disable interrupts
  halIntState_t state;
  HAL_ENTER_CRITICAL_SECTION(state);

  osal_set_icall_hook(icall_liteMsgParser);

#if !defined( NO_OSAL_SNV ) && !defined( USE_FPGA )
  // Initialize NV System
  osal_snv_init( );
#endif // !NO_OSAL_SNV && !USE_FPGA

  // Initialize the operating system
  osal_init_system();

  HAL_EXIT_CRITICAL_SECTION(state);

  osal_start_system(); // No Return from here

  return(0); // Shouldn't get here.
}

/*******************************************************************************
 * @fn          bleStack_startup_entry
 *
 * @brief       This is the BLE stack entry point.
 *
 * input parameters
 *
 * @param       arg0   argument containing remote dispatch function pointers
 * @param       arg1   custom initialization parameter
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void bleStack_startup_entry( const ICall_RemoteTaskArg *arg0, void *arg1 )
{
  ICall_dispatcher = arg0->dispatch;
  ICall_enterCriticalSection = arg0->entercs;
  ICall_leaveCriticalSection = arg0->leavecs;

  bleStack_main( arg1 );
}

/*
 * Call each of the tasks initialization functions.
 */
void __attribute__((weak)) osalInitTasks( void )
{
  ICall_EntityID entity;
  ICall_SyncHandle syncHandle;
  uint8 taskID = 0;
  uint8 i;

  tasksEvents = (uint16 *)osal_mem_alloc( sizeof( uint16 ) * tasksCnt);
  if ( tasksEvents == NULL )
  {
    // The initialization of the device failed, there is no reason to continue
    while(1);
  }
  osal_memset( tasksEvents, 0, (sizeof( uint16 ) * tasksCnt));

  /* LL Task */
  LL_Init( taskID++ );

  /* HCI Task */
  HCI_Init( 0 );

#if defined ( OSAL_CBTIMER_NUM_TASKS )
  /* Callback Timer Tasks */
  osal_CbTimerInit( taskID );
  taskID += OSAL_CBTIMER_NUM_TASKS;
#endif

  ble_dispatch_liteInit(taskID++);

  // ICall enrollment
  /* Enroll the service that this stack represents */
  ICall_enrollService(ICALL_SERVICE_CLASS_BLE, NULL, &entity, &syncHandle);

  /* Register all other OSAL tasks to use the registered dispatcher entity
   * ID as the source of dispatcher messages, even though the other OSAL
   * tasks didn't register themselves to receive messages from application.
   */
  for (i = 0; i < taskID; i++)
  {
    osal_enroll_senderid(i, entity);
  }
}

/**
 * @brief   Create remote tasks.
 *
 * @par     Note
 * One remote task shall be created per external image. <br>
 * This function must be called after calling
 * @ref ICall_init .
 *
 * The external image information must be stored either in a custom
 * @ref icall_addrs.h header file in an include path when
 * ICALL_FEATURE_SEPARATE_IMGINFO compile flag is not defined,
 * or as a set of external constants.
 *
 * If ICALL_FEATURE_SEPARATE_IMGINFO compile flag is not defined,
 * the following macros must be defined in the ICallAddrs.h file:
 * @ref ICALL_STACK0_ADDR, @ref ICALL_ADDR_MAPS, @ref ICALL_TASK_PRIORITIES
 * and @ref ICALL_TASK_STACK_SIZES.
 *
 * If ICALL_FEATURE_SEPARATE_IMGINFO compile flag is defined,
 * the following constants have to be linked into an image that include ICall
 * module: ICall_imgEntries, ICall_imgTaskPriorities,
 * ICall_imgTaskStackSizes and ICall_numImages.
 */
void bleStack_createRemoteTasks(void)
{
  size_t i;
  ICall_RemoteTask_t remoteTaskTable[BLESTACK_REMOTE_THREAD_COUNT];

  for (i = 0; i < BLESTACK_REMOTE_THREAD_COUNT; i++)
  {
    remoteTaskTable[i].imgTaskPriority      = bleStack_threadPriorities[i];
    remoteTaskTable[i].imgTaskStackSize     = bleStack_threadStackSizes[i];
    remoteTaskTable[i].startupEntry         = bleStack_threadEntries[i];
    remoteTaskTable[i].ICall_imgInitParam   = (void *) bleStack_getInitParams(i);
  }
  ICall_createRemoteTasksAtRuntime(remoteTaskTable, BLESTACK_REMOTE_THREAD_COUNT);
  // create the worker thread
  ICall_createWorkerThread();
}
/*******************************************************************************
 * @fn             bleStack_init
 *
 * @brief          bleStack_init is an initialization function implemented
 *                 within the stack, as oppose to applicational code in
 *                 icall_startup.c and osal_icall_ble.c.
 *                 The new initialization sequence is SYNCHRONOUS, meaning the calling task
 *                 will be blocked until the osal_start_system() will mark the
 *                 initialization completion by calling to
 *                 MAP_bleStack_initCompleteNotify().
 *                 In case the init failed, ble_init() will fault into inifinite loop.
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @return      None.
 *
 * */
int bleStack_Init()
{
  int err;
  bleStack_initDone_sem = SemaphoreP_createBinary(0 /* Semaphore Count */);

  /* Update User Configuration of the stack */
  bleStack_user0Cfg.appServiceInfo->timerTickPeriod = ICall_getTickPeriod();
  bleStack_user0Cfg.appServiceInfo->timerMaxMillisecond  = ICall_getMaxMSecs();

  /* Initialize ICall module */
  ICall_init();

  /* Start tasks of external images */
  bleStack_createRemoteTasks();

  /* Wait for init complete. Will be released in init_done callback */
  err = SemaphoreP_pend(bleStack_initDone_sem, LL_INIT_TIMEOUT_MSEC / ClockP_getSystemTickPeriod());
  if ( err != 0 )
  {
    // The initialization of the BLE Stack failed, update the host.
    return FAILURE;
  }
  return SUCCESS;
}

/*******************************************************************************
 * @fn          bleStack_initCompleteNotify
 *
 * @brief       This function is  called to notify that the Controller
 *              initialization completed with (status)
 *
 * input parameters
 *
 * @param       status - Status of the Controller initialization
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void bleStack_initCompleteNotify(int status)
{
  if (status == SUCCESS)
  {
    SemaphoreP_post(bleStack_initDone_sem);
  }
}
