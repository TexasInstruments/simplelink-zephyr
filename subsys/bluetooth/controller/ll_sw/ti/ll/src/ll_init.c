/******************************************************************************

 @file  ll_init.c

 @brief BLE Controller Init code

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2024 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
/*******************************************************************************
 * INCLUDES
 */
#include <string.h>

#include "bcomdef.h"
#include "icall.h"
#include "osal_tasks.h"
#include "osal_cbtimer.h"
#include "hal_types.h"
#include "ble_dispatch_lite.h"
#include "hci_api.h"
#include "ble_init.h"
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
typedef void (*llRemoteTaskEntry)(const ICall_RemoteTaskArg *arg0,
                                      void *arg1);

static void llStartupEntry( const ICall_RemoteTaskArg *arg0, void *arg1 );

static int llStart( void *arg );

/*******************************************************************************
 * CONSTANTS
 */
/**
 * Initializer for custom initialization parameters.
 * Each element of the array corresponds to initialization parameter
 * (a pointer) specific to the image to be passed to the entry function
 * defined in @ref ICALL_ADDR_MAPS initializer.
 */
#ifndef USE_DEFAULT_USER_CFG
// BLE user defined configuration

#define LL_USER_CFG                 { &bleStackConfig,         \
                                      &boardConfig,            \
                                      &bleAppServiceInfoTable };

icall_userCfg_t llUser0Cfg            = LL_USER_CFG;

#define BLE_STACK_USER0_CFG             &llUser0Cfg    //!< user config

#else //USE_DEFAULT_USER_CFG

#define BLE_STACK_USER0_CFG             NULL       //!< user config
#endif // USE_DEFAULT_USER_CFG

#define LL_TASK_PRIORITIES              { 5 }
#define LL_TASK_STACK_SIZES             { 1500 }
#define LL_CUSTOM_INIT_PARAMS           { BLE_STACK_USER0_CFG }

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/** @internal thread priorities to be assigned to each remote thread */
static const int llThreadPriorities[]    = LL_TASK_PRIORITIES;

/** @internal thread stack max depth for each remote thread */
static const size_t llThreadStackSizes[] = LL_TASK_STACK_SIZES;

static const void *llInitParams[]        = LL_CUSTOM_INIT_PARAMS;

/** @internal initialization parameter (pointer) for each remote thread */
#define llGetInitParams(_i)              (llInitParams[i])

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

#define LL_ADDR_MAPS \
{ \
  (llRemoteTaskEntry) (llStartupEntry) \
}

static const llRemoteTaskEntry llThreadEntries[] = LL_ADDR_MAPS;

/** @internal external image count */
#define LL_REMOTE_THREAD_COUNT \
  (sizeof(llThreadEntries)/sizeof(llThreadEntries[0]))

static SemaphoreP_Handle initDoneSemHandle = NULL;

static const bleServicesParams_t *pBleServicesParams = NULL;

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */
__attribute__((weak)) const uint8 tasksCnt = sizeof( tasksArr ) / sizeof( tasksArr[0] );

/**
 * Main entry function for the stack image
 */
static int llStart( void *arg )
{
  int ret = 0;
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

#ifdef CC23X0
#ifndef USE_HSM
  if (LL_initRNGNoise() != LL_STATUS_SUCCESS)
  {
    /* abort */
    ICall_abort();
  }
#endif
#endif

  // Disable interrupts
  halIntState_t state;
  HAL_ENTER_CRITICAL_SECTION(state);

  osal_set_icall_hook(icall_liteMsgParser);

#if !defined( NO_OSAL_SNV ) && !defined( USE_FPGA )
  // Initialize NV System
  osal_snv_init( );
#endif // !NO_OSAL_SNV && !USE_FPGA

  // Initialize the operating system
  ret = osal_init_system();

  HAL_EXIT_CRITICAL_SECTION(state);

  if ( ret == USUCCESS )
  {
    osal_start_system(); // No Return from here
  }

  return ret; // Shouldn't get here.
}

/*******************************************************************************
 * @fn          llStartupEntry
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
static void llStartupEntry( const ICall_RemoteTaskArg *arg0, void *arg1 )
{
  ICall_dispatcher = arg0->dispatch;
  ICall_enterCriticalSection = arg0->entercs;
  ICall_leaveCriticalSection = arg0->leavecs;

  llStart( arg1 );
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
static void llCreateRemoteTasks(void)
{
  size_t i;
  ICall_RemoteTask_t remoteTaskTable[LL_REMOTE_THREAD_COUNT];

  for (i = 0; i < LL_REMOTE_THREAD_COUNT; i++)
  {
    remoteTaskTable[i].imgTaskPriority      = llThreadPriorities[i];
    remoteTaskTable[i].imgTaskStackSize     = llThreadStackSizes[i];
    remoteTaskTable[i].startupEntry         = llThreadEntries[i];
    remoteTaskTable[i].ICall_imgInitParam   = (void *) llGetInitParams(i);
  }
  ICall_createRemoteTasksAtRuntime(remoteTaskTable, LL_REMOTE_THREAD_COUNT);
  // create the worker thread
  (void) ICall_createWorkerThread();
}

/********************************************************************************
 * @fn            BLE_ServicesInit
 *
 * @brief         The `BLE_ServicesInit` function is responsible for initializing
 *                the BLE services based on the input parameters specified
 *                in `pServiceParams`. This function offers an option for a synchronous
 *                initialization sequence, meaning that the calling task will
 *                be blocked until the stack indicates that the initialization
 *                is complete.
 *                If the initialization fails, `BLE_ServicesInit()`
 *                will return a failure status.
 *
 * input parameters
 *
 * @param         pServiceParams - pointer to BLE Params initialization options
 *
 * output parameters
 *
 * @return        SUCCESS / FAILURE.
 *
 * */
uint32 BLE_ServicesInit(const bleServicesParams_t *pServiceParams)
{
  uint32 status = SemaphoreP_TIMEOUT;

  if (NULL != pServiceParams)
  {
    pBleServicesParams = pServiceParams;

    /* Register Application callback to trap asserts raised in the Stack */
    RegisterAssertCback(pBleServicesParams->assertCallback);

    /* Register HCI Driver callbacks to provide hci_driver interface to the LL */
    status = HCI_ControllerToHostRegisterCb( &pBleServicesParams->hciCbs );
    if (status == SUCCESS)
    {
      if (pBleServicesParams->syncInitTimeoutTics != 0 /*NO_WAIT*/)
      {
        initDoneSemHandle = SemaphoreP_createBinary(0 /* Semaphore Count */);
      }
      /* Update User Configuration of the stack */
      llUser0Cfg.appServiceInfo->timerTickPeriod = ICall_getTickPeriod();
      llUser0Cfg.appServiceInfo->timerMaxMillisecond  = ICall_getMaxMSecs();

      /* Initialize ICall module */
      ICall_init();

      /* Start tasks of external images */
      llCreateRemoteTasks();

      if (pBleServicesParams->syncInitTimeoutTics != 0 /*NO_WAIT*/)
      {
        if (NULL != initDoneSemHandle)
        {
          /* Wait for init complete. Will be released in init_done callback */
          status = SemaphoreP_pend(initDoneSemHandle, pBleServicesParams->syncInitTimeoutTics);

          /* Free the resources */
          SemaphoreP_delete(initDoneSemHandle);
          initDoneSemHandle = NULL;
        }
      }
    }
  }
  return ( status == SemaphoreP_OK ) ? SUCCESS : FAILURE;
}

/********************************************************************************
 * @fn            BLE_ServicesParamsInit
 *
 * @brief         The `BLE_ServicesParamsInit` function provides a default
 *                configuration for the bleServicesParams_t structure
 *
 * input parameters
 *
 * @param         pServiceParams - pointer to BLE Params initialization options
 * @param         size           - size of the initialization structure
 *
 *
 * output parameters
 *
 * @return        SUCCESS - in case pServiceParams initialization succeed
 *                FAILURE in case of
 *                   - Compatibility/versions mismatch (the check is done based
 *                     on size of the bleServicesParams_t).
 *                   - Parameters validation
 *
 * */
uint32 BLE_ServicesParamsInit(bleServicesParams_t *pServiceParams, size_t size)
{
  int status = FAILURE;
  if ( ( NULL != pServiceParams ) && ( size == sizeof(bleServicesParams_t) ) )
  {
    memset(pServiceParams, 0, sizeof(bleServicesParams_t));

    status = HCI_Controller2HostCallbacksInit(&pServiceParams->hciCbs);
  }
  return status;
}

/*******************************************************************************
 * @fn          llInitCompleteNotify
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
void llInitCompleteNotify(int status)
{
  if (status == SUCCESS)
  {
    if ((NULL != pBleServicesParams) &&
        (pBleServicesParams->syncInitTimeoutTics != 0 /*NO_WAIT*/) &&
        (NULL != initDoneSemHandle))
    {
      SemaphoreP_post(initDoneSemHandle);
    }
  }
}
