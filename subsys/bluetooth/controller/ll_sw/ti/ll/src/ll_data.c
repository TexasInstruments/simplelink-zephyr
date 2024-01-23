/******************************************************************************

 @file  ll_data.c

 @brief This file contains the Link Layer (LL) data for the Bluetooth
        Low Energy (BLE) Controller.

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

#include <string.h>
#include "bcomdef.h"
#include "ll_common.h"
#include "ll_config.h"
#include "ble.h"
#include "ll_ae.h"
#include "ll_al.h"
#include "ll_privacy.h"
#include "cs/ll_cs_db.h"
//
#include "rom_jt.h"

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * EXTERNS
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * API
 */

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llDataGetConnPtr
 *
 * @brief       This function is used to to get the pointer to the connection
 *              based on the connection ID.
 *
 * input parameters
 *
 * @param       connId - Connection ID that Init will attempt to start.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to the connection data.
 */
llConnState_t *llDataGetConnPtr( uint8 connId )
{
  return( &llConns.llConnection[connId] );
}
#endif
/*******************************************************************************
 * @fn          llDynamicAlloc
 *
 * @brief       This function is used to dynamically allocate memory needed by
 *              the Controller.
 *
 *              Note: This is a one time allocation, the memory of which is
 *                    never freed! So for all intents and purposes, essentially
 *                    static.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 *              LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED
 */
llStatus_t llDynamicAlloc( void )
{
  llStatus_t status;

#if (defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG))

  // allocate Scanner Info
  extScanInfo = MAP_osal_mem_alloc( sizeof(extScanInfo_t) );

  // check that there was enough heap
  if ( extScanInfo )
  {
    memset (extScanInfo, 0, sizeof(extScanInfo_t));
  }
  else // !extScanInfo
  {
    return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
  }
#endif // SCAN_CFG || (FLASH_ROM_BUILD)

#if (defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG))
  // allocate Initiator Info
  extInitInfo = MAP_osal_mem_alloc( sizeof(extInitInfo_t) );

  // check that there was enough heap
  if ( extInitInfo )
  {
    extInitInfo->llTask      = NULL;
    extInitInfo->scanMode    = LL_SCAN_STOP;
    extInitInfo->connId      = 0;
    extInitInfo->scaValue    = LL_SCA_CENTRAL_DEFAULT;
    extInitInfo->pCreateConn = NULL;
  }
  else // !extInitInfo
  {
    return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
  }
#endif // (INIT_CFG) || (FLASH_ROM_BUILD)

  // allocate DTM Info
  dtmInfo = MAP_osal_mem_alloc( sizeof(dtmInfo_t) );

  // check that there was enough heap
  if ( NULL == dtmInfo )
  {
    return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
  }
  else // dtmInfo
  {
    memset (dtmInfo, 0, sizeof(dtmInfo_t));
    dtmInfo->packetType  = LL_DIRECT_TEST_PAYLOAD_UNDEFINED;
    dtmInfo->lastRssi    = LL_RF_RSSI_UNDEFINED;
  }

  // allocate Scheduler task space
  llTaskList.llTasks = (taskInfo_t *)MAP_osal_mem_alloc( sizeof( taskInfo_t ) *
                                           (maxNumConns + LL_NUM_TASK_BLOCKS) );

  // check that there was enough heap
  if ( NULL == llTaskList.llTasks )
  {
    return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
  }
  memset (llTaskList.llTasks, 0, sizeof( taskInfo_t ) *
                                          (maxNumConns + LL_NUM_TASK_BLOCKS));

  // allocate and initialize RX window task when the SDAA module is enable.
  status = MAP_llSDAASetupRXWindowCmd();
  if ( status != LL_STATUS_SUCCESS )
  {
    // return LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED if
    // MAP_llSDAASetupRXWindowCmd() fail to allocate memory
    return ( status );
  }
#ifdef LL_CONN_SIZE
  totalConnSize = sizeInfo.sizeTaskInfo;
#endif // LL_CONN_SIZE

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  // check if one or more connections have been specified
  if ( maxNumConns > 0 )
  {
    // malloc the number of required connections
    llConns.llConnection = (llConnState_t *)MAP_osal_mem_alloc( sizeof( llConnState_t ) * maxNumConns );

    // check that there was enough heap
    if ( NULL == llConns.llConnection )
    {
      return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
    }
    memset (llConns.llConnection, 0, sizeof( llConnState_t ) * maxNumConns);

#ifdef LL_CONN_SIZE
    totalConnSize += sizeInfo.sizeOfLlConnState;
#endif // LL_CONN_SIZE

    // malloc array used to sort active connection IDs
    activeConns = (uint8 *)MAP_osal_mem_alloc( sizeof(uint8) * maxNumConns );

    // check that there was enough heap
    if ( NULL == activeConns )
    {
      return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
    }
    memset (activeConns, 0, sizeof(uint8) * maxNumConns);

#ifdef LL_CONN_SIZE
    totalConnSize += (totalConnSize * maxNumConns);
#endif // LL_CONN_SIZE
    // Connection Commands
    linkCmd = (RCL_CmdBle5Connection *)MAP_osal_mem_alloc( sizeof(RCL_CmdBle5Connection) * maxNumConns );

    // check linkCmd there was enough heap
    if ( NULL == linkCmd )
    {
      return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
    }
    memset (linkCmd, 0, sizeof(RCL_CmdBle5Connection) * maxNumConns);

    // Central Parameters - one per connection
    linkParam = (RCL_CtxConnection *)MAP_osal_mem_alloc( sizeof(RCL_CtxConnection) * maxNumConns );

    if ( NULL == linkParam )
    {
      return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
    }
    memset (linkParam, 0, sizeof(RCL_CtxConnection) * maxNumConns);

    // Connection Transmit Queue - one per connection
    txDataQ = (txDataQ_t *)MAP_osal_mem_alloc( sizeof(txDataQ_t) * maxNumConns );
    if (NULL == txDataQ)
    {
        return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
    }
    memset (txDataQ, 0, sizeof(txDataQ_t) * maxNumConns);
    // Init the CS DB
    MAP_llCsInit();
  }
#endif // ADV_CONN_CFG | INIT_CFG

  // Accept list
  alTable = (alTable_t *)MAP_osal_mem_alloc( sizeof(alTable_t) + sizeof(alEntry_t) *
                                               ( BLE_NUM_AL_ENTRIES ) );
  if ( !alTable )
  {
    return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
  }

  alTable->pAlEntries = (alEntry_t*) ( (uint32_t) alTable +
                                       (uint32_t) sizeof(alTable_t));

  // Accept List for Scanner
  alTableScan = (alTable_t *)MAP_osal_mem_alloc( sizeof(alTable_t) + sizeof(alEntry_t) *
                                               ( BLE_NUM_AL_ENTRIES ) );
  if ( !alTableScan )
  {
    return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
  }

  alTableScan->pAlEntries = (alEntry_t*) ( (uint32_t) alTableScan +
                                           (uint32_t) sizeof(alTable_t));

  // Resolving List
  resolvingList = MAP_osal_mem_alloc( sizeof(rlEntry_t) *
                                     ( BLE_RESOLVING_LIST_SIZE + 1 ) );
  if ( !resolvingList )
  {
    return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
  }

  // RF Patch Compensation
  pRfPathComp = MAP_osal_mem_alloc( sizeof(rfPathComp_t) );

  if ( !pRfPathComp )
  {
    return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
  }

#ifndef CC23X0
  status = MAP_llDmmDynamicAlloc();
  if (!status)
  {
    return( status );
  }
#endif
  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * @fn          llDmmDynamicAlloc
 *
 * @brief       This function is used to dynamically DMM allocate memory needed by
 *              the Controller.
 *
 *              Note: This is a one time allocation, the memory of which is
 *                    never freed! So for all intents and purposes, essentially
 *                    static.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 *              LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED
 */
llStatus_t llDmmDynamicAlloc( void )
{
  uint8 prio;

  // DMM Policy feature
  dmmPolicyManager.adv = MAP_osal_mem_alloc(sizeof(dmmPolicyManagerThreshold_t) * AE_DEFAULT_NUM_ADV_SETS);
  if ( !dmmPolicyManager.adv )
  {
    return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
  }

  dmmPolicyManager.conn = MAP_osal_mem_alloc(sizeof(dmmPolicyManagerThreshold_t) * maxNumConns);
  if ( !dmmPolicyManager.conn )
  {
    return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
  }

  dmmPolicyManager.advHandle = MAP_osal_mem_alloc(AE_DEFAULT_NUM_ADV_SETS);
  if ( !dmmPolicyManager.advHandle )
  {
    return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
  }

  for (prio=0;prio<DMM_POLICY_MAX_REPEAT_PRIORITIES;prio++)
  {
    dmmPolicyManager.connRepeatPrio[prio] = MAP_osal_mem_alloc(maxNumConns);
    if ( !dmmPolicyManager.connRepeatPrio[prio] )
    {
      return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
    }
  }
  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * @fn          llDynamicFree
 *
 * @brief       This function is used to free any dynamically allocated memory.
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
void llDynamicFree( void )
{
#if (defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)) /* || (FLASH_ROM_BUILD) */
  // check Scan
  if ( extScanInfo )
  {
    // free already allocated data
    MAP_osal_mem_free( extScanInfo );
  }
#endif // SCAN_CFG || FLASH_ROM_BUILD

#if (defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)) /* || (FLASH_ROM_BUILD) */
  // check Init
  if ( extInitInfo )
  {
    // free already allocated data
    MAP_osal_mem_free( extInitInfo );
  }
#endif // INIT_CFG || FLASH_ROM_BUILD

  // check DTM
  if ( dtmInfo )
  {
    // free already allocated data
    MAP_osal_mem_free( dtmInfo );
  }
  // check LL Tasks
  if ( llTaskList.llTasks )
  {
    // free already allocated data
    MAP_osal_mem_free( llTaskList.llTasks );
  }

  // check if ptr for RX window task is exist
  if ( pRXWindowTask )
  {
    // free already allocated data
    MAP_osal_mem_free( pRXWindowTask );
  }

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  // check Connections
  if ( llConns.llConnection )
  {
    MAP_osal_mem_free( llConns.llConnection );
  }

  // check Active Connection list
  if ( activeConns )
  {
    MAP_osal_mem_free( activeConns );
  }

  // check Link commands
  if ( linkCmd )
  {
    MAP_osal_mem_free( linkCmd );
  }

  // check Link parameters
  if ( linkParam )
  {
    MAP_osal_mem_free( linkParam );
  }

  // check Connection Transmit Queue
  if ( txDataQ )
  {
    MAP_osal_mem_free( txDataQ );
  }

#endif // (ADV_CONN_CFG | INIT_CFG)

  // Accept List
  if ( alTable )
  {
    MAP_osal_mem_free( alTable );
  }

  // Accept List for Scanner
  if ( alTableScan )
  {
    MAP_osal_mem_free( alTableScan );
  }

  // Resolving list
  if ( resolvingList )
  {
    MAP_osal_mem_free( resolvingList );
  }

  // RF Patch Compensation
  if ( pRfPathComp )
  {
    MAP_osal_mem_free( pRfPathComp );
  }

#ifdef RTLS_CTE
  // Constant Tone Extension
  if ( llCte )
  {
    MAP_osal_mem_free( llCte );
  }
#endif

  // Channel Sounding
  MAP_llCsFreeAll();


  // Free DMM allocations
  MAP_llDmmDynamicFree();

  return;
}

/*******************************************************************************
 * @fn          llDmmDynamicFree
 *
 * @brief       This function is used to free any DMM dynamically allocated memory.
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
void llDmmDynamicFree( void )
{
  uint8 prio;
  // DMM Policy feature
  if ( dmmPolicyManager.adv )
  {
    MAP_osal_mem_free( dmmPolicyManager.adv );
  }

  if ( dmmPolicyManager.conn )
  {
    MAP_osal_mem_free( dmmPolicyManager.conn );
  }

  if ( dmmPolicyManager.advHandle )
  {
    MAP_osal_mem_free( dmmPolicyManager.advHandle );
  }

  for (prio=0;prio<DMM_POLICY_MAX_REPEAT_PRIORITIES;prio++)
  {
    if ( dmmPolicyManager.connRepeatPrio[prio] )
    {
      MAP_osal_mem_free( dmmPolicyManager.connRepeatPrio[prio] );
    }
  }
}
/*******************************************************************************
 */
