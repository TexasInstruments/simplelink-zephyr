/******************************************************************************

 @file  ll_scan_end_causes.c

 @brief This file contains the Link Layer (LL) handlers for the various scanner
        end causes that result from a PHY task completion completion completion
        completion.

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
#ifdef USE_RCL
#include <ti/drivers/rcl/RCL.h>
#include <ti/drivers/rcl/commands/ble5.h>
#else
#include <ti/drivers/rf/RF.h>
#include "rf_api.h"
#endif //USE_RCL
#include "bcomdef.h"
#include "hal_mcu.h"
#include "hci_event.h"
#include "../../ll/inc/ll_common.h"
#include "../../ll/inc/ll_enc.h"
#include "../../ll/inc/ll_privacy.h"
#include "../../ll/inc/ll_rat.h"
#include "../../ll/inc/ll_timer_drift.h"
#include "../../ll/inc/ll_ae.h"
#include "hal_gpio_wrapper.h"
#include "rom_jt.h"
#include "hci_event.h"

/*******************************************************************************
 * EXTERNS
 */

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * Functions
 */

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * @fn          llExtScan_PostProcess
 *
 * @brief       This routine is used to post process the Extended Scan command.
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
void llExtScan_PostProcess( void )
{
  volatile uint32 currentTime;
  uint8 nextScanChannel = 0;

#ifdef DEBUG_GPIO_ADV_SCAN
  GPIO_writeDio(HAL_GPIO_2, 0);
#endif // DEBUG_GPIO_ADV_SCAN

#ifdef USE_RCL
  // clear all RX entries
  llClearScanDataQueue(TRUE);
#endif
  // check if still active
  if ( extScanInfo->scanMode == LL_SCAN_STOP )
  {
    // free task and teardown privacy if need be
    MAP_llEndExtScanTask();

    // check if we're not just temporarily halted for duration+period
    if ( extScanInfo->pEnable->enable == LL_SCAN_STOP &&
         extScanInfo->pEnable->period == 0 )
    {
      // invoke callback, if there is one
      MAP_llExtAdvCBack( LL_CBACK_EXT_SCAN_END, NULL );
    }

    // schedule another task, if any
    MAP_llScheduler();

    return;
  }

  // update health check
  MAP_llHealthUpdate(LL_STATE_SCAN);
  // clear command status value
#ifdef USE_RCL
  extScanCmd.common.status = RCL_CommandStatus_Idle;
#else
  extScanCmd.rfOpCmd.status = RFSTAT_IDLE;
#endif
  // get current time with some margin
  currentTime = MAP_llGetCurrentTime() + RAT_TICKS_IN_625US;

  // check that the scan was aborted by other activity
  // and the scan is back to back (mesh use case)
  if (((extScanInfo->pScanParam->extScanParam[extScanIndex].scanWindow) ==
       (extScanInfo->pScanParam->extScanParam[extScanIndex].scanInterval)) &&
       (llGetRfCmdPreemptionEnable()))
  {
    // call the scheduler (do not set the start time)
    // the start time will be set as immediately by the scheduler
    MAP_llScheduler();
    return;
  }
  // check if there's more time before the end of the scan window and
  // that end time is not before the current time
  // Note: True when first parameter is greater than second.
#ifdef USE_RCL
  if ( (MAP_llTimeCompare(extScanCmd.common.timing.absStartTime +
                          extScanCmd.common.timing.relGracefulStopTime, currentTime ) ) &&
       (extScanCmd.common.timing.relHardStopTime != 0) &&
       (MAP_llTimeCompare(extScanCmd.common.timing.absStartTime +
                          extScanCmd.common.timing.relHardStopTime, currentTime ) ))
  {
    // update start time to the future
    extScanCmd.common.timing.absStartTime = currentTime;
#else
  if ( MAP_llTimeCompare( extScanParam.timeoutTime, currentTime ) &&
       ( extScanParam.endTime != 0 ) &&
       ( MAP_llTimeCompare( extScanParam.endTime, currentTime ) ) )
  {
    // update start time to the future
    // Note: Once the CM0 follows an auxPtr to a secondary channel, it never
    //       returns to the primary channel! (sigh)
    // Note: RAT Compare cannot handle Past Start! (sigh)
    // Note: Scan Window trigger is absolute, so there's no need to adjust it.
    extScanCmd.rfOpCmd.startTime = currentTime;
#endif
    // restart immediately
    // Note: Already the current task.
    // Note: Scan Window trigger is absolute.
    MAP_llScheduleTask( extScanInfo->llTask );
  }
  else // we're done so on to the next scan interval
  {
    // update the next Scan channel
    nextScanChannel = LL_ADV_BASE_CHAN + llGetNextOrPreviousExtScanChannelIndex(LL_GET_NEXT_SCAN_CHAN);

    // set the next Scan channel
#ifdef USE_RCL
    extScanCmd.channel = nextScanChannel;
#else
    extScanCmd.chan = nextScanChannel;
#endif

    // check if there's more than one Scan primary PHY
    if ( extScanInfo->pScanParam->scanPhys == (LL_PHY_1_MBPS+LL_PHY_CODED) )
    {
      // both 1M and Coded are being used
      // cycle through all three primary channels before switching PHY
      if (nextScanChannel == LL_ADV_BASE_CHAN)
      {
        extScanIndex ^= 1;
      }

#ifdef USE_RCL
      // update the scan type based on changed extScanIndex
      extScanCmd.activeScan = extScanInfo->pScanParam->extScanParam[extScanIndex].scanType;

      // update Start Time based on Scan Interval
      extScanCmd.common.timing.absStartTime = extScanInfo->scanStartTime +
                                             (extScanInfo->pScanParam->extScanParam[extScanIndex].scanInterval * RAT_TICKS_IN_625US);
      extScanInfo->scanStartTime = extScanCmd.common.timing.absStartTime;
      extScanCmd.common.phyFeatures = extScanIndex << 1;
#else
      // update the type of scan based on changed extScanIndex
      SETVAR_SCAN_CFG_ACTIVE_SCAN( extScanParam.scanCfg,
                                   extScanInfo->pScanParam->extScanParam[extScanIndex].scanType );

      // update Start Time based on Scan Interval
      extScanCmd.rfOpCmd.startTime = extScanInfo->scanStartTime +
                                    (extScanInfo->pScanParam->extScanParam[extScanIndex].scanInterval * RAT_TICKS_IN_625US);
      extScanInfo->scanStartTime = extScanCmd.rfOpCmd.startTime;

      // set primary PHY
      // Note: The index is 0 for 1M and 1 for Coded, so 2*extScanIndex provides
      //       a value of 0 for PHY 1M, and 2 for PHY Coded.
      // OPT: ALLOW USER TO SPECIFY THE CODED SCHEME FOR AUX_SCAN_REQ?
      extScanCmd.phyMode = extScanIndex << 1;

      // set range delay based on PHY
      extScanCmd.rangeDelay = ( extScanIndex == 0 ) ? LL_UNCODED_RANGE_DELAY_RAT_TICKS : LL_CODED_RANGE_DELAY_RAT_TICKS;
#endif
    }
    else // only one PHY is active, so leave the index alone
    {
#ifdef USE_RCL
      // update Start Time based on Scan Interval
      extScanCmd.common.timing.absStartTime = extScanInfo->scanStartTime +
                                             (extScanInfo->pScanParam->extScanParam[extScanIndex].scanInterval * RAT_TICKS_IN_625US);
      // save the absolute start time of the scanner for post-processing
      extScanInfo->scanStartTime = extScanCmd.common.timing.absStartTime;
#else
      // update Start Time based on Scan Interval
      extScanCmd.rfOpCmd.startTime = extScanInfo->scanStartTime +
                                     (extScanInfo->pScanParam->extScanParam[extScanIndex].scanInterval * RAT_TICKS_IN_625US);

      // save the absolute start time of the scanner for post-processing
      // Note: The start time has to be adjusted for AE packets because the CMO
      //       ends after following a secondary channel. Thus, the time the
      //       event first started has to be preserved.
      extScanInfo->scanStartTime = extScanCmd.rfOpCmd.startTime;
#endif
    }

    // set window
#ifdef USE_RCL
    extScanCmd.common.timing.relGracefulStopTime =
#else
    extScanParam.timeoutTime =
      extScanCmd.rfOpCmd.startTime +
#endif
      (extScanInfo->pScanParam->extScanParam[extScanIndex].scanWindow * RAT_TICKS_IN_625US);

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

#ifdef USE_RCL
        // In devices using RF Driver (rflib instead of rcl), the command
        // itself will point to extScanInfo->ownAddr which naturally updates
        // the RPA.
        // When using RCL, the RF Command needs to be directly updated
        // Copy the address into the command.
        MAP_osal_memcpy( extScanCmd.ctx->ownA,
                         resolvingList[LOCAL_RL_INDEX].RPA,
                         B_ADDR_LEN );
#endif
      }
    }

    // check if our duration has expired within the period
    // invoke callback, if there is one
    MAP_llExtAdvCBack( LL_CBACK_EXT_SCAN_WINDOW_END, NULL );

    // schedule this task
    MAP_llScheduler();
  }

  return;
}
#ifdef USE_PERIODIC_SCAN
/*******************************************************************************
 * @fn          llPeriodicScan_PostProcess
 *
 * @brief       This routine is used to post process the periodic Advertising scan
 *              command.
 *
 * @design      /ref did_286039104
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
void llPeriodicScan_PostProcess( void )
{
  llPeriodicScanSet_t *pPeriodicScan = llPeriodicScan.currentScan;
  llPeriodicScanSet_t *pTmpPeriodicScan = NULL;

  if ((pPeriodicScan->state == PERIODIC_SCAN_STATE_SYNCED) ||
      (pPeriodicScan->state == PERIODIC_SCAN_STATE_SYNCING_ACTIVE))
  {
    // check that there is no request for ending current periodic scan
    if (pPeriodicScan->terminate == 0)
    {
      // check status of last received sync indication
      // accept the packet also in case of CRC error
      if (((taskEndStatus == BLESTAT_DONE_OK) && (pPeriodicScan->rxCount > 0)) ||
          ((taskEndStatus == BLESTAT_DONE_RXERR) && (llPeriodicScan.rfOutput.nRxAdvNok > 0)))
      {
        pPeriodicScan->numMissed = 0;
        // decrease the priority
        if (pPeriodicScan->intPriority > LL_QOS_LOW_PRIORITY)
        {
          pPeriodicScan->intPriority = LL_QOS_LOW_PRIORITY;
        }
      }
      else
      {
        pPeriodicScan->numMissed++;
      }
      // in case we are in a middle of syncing process
      if (pPeriodicScan->state == PERIODIC_SCAN_STATE_SYNCING_ACTIVE)
      {
        if (pPeriodicScan->numMissed == 0)
        {
          halIntState_t cs;

          // protect from host that could send create sync cancel
          HAL_ENTER_CRITICAL_SECTION(cs);

          // change state to sync
          pPeriodicScan->state = PERIODIC_SCAN_STATE_SYNCED;
          // add the create sync set to the sorted list by handle
          // case the list is emptyor the first set handle GT 0
          if ((llPeriodicScan.scanList == NULL) ||
             ((llPeriodicScan.scanList != NULL) && (llPeriodicScan.scanList->handle > 0)))
          {
            pPeriodicScan->handle = 0;
            pPeriodicScan->next = llPeriodicScan.scanList;
            // set the list head
            llPeriodicScan.scanList = pPeriodicScan;
          }
          else
          {
            pTmpPeriodicScan = llPeriodicScan.scanList;

            // find available handle
            while (pTmpPeriodicScan->next != NULL)
            {
              // check that the handle is sequential
              if (pTmpPeriodicScan->handle + 1 < pTmpPeriodicScan->next->handle)
              {
                break;
              }
              else
              {
                pTmpPeriodicScan = pTmpPeriodicScan->next;
              }
            }
            // set the handle
            pPeriodicScan->handle = pTmpPeriodicScan->handle + 1;
            // locate the new set in the proper place
            pPeriodicScan->next = pTmpPeriodicScan->next;
            pTmpPeriodicScan->next = pPeriodicScan;
          }
          // update num active
          llPeriodicScan.scanNumActive++;
          pTmpPeriodicScan = llPeriodicScan.createSync;
          // set the create sync to NULL
          llPeriodicScan.createSync = NULL;

          HAL_EXIT_CRITICAL_SECTION(cs);
        }
        else if (pPeriodicScan->numMissed >= PERIODIC_SYNCING_LIMIT_NUM_EVENTS)
        {
          pPeriodicScan->terminate = LL_STATUS_ERROR_CONN_FAILED_TO_BE_ESTABLISHED;
        }
      }
      // in case current priodic scan is already synced
      else if (pPeriodicScan->state == PERIODIC_SCAN_STATE_SYNCED)
      {
        uint8 cteCount = 0;

        if ((taskEndStatus != BLESTAT_DONE_OK) || (pPeriodicScan->rxCount == 0))
        {
          // check for timeout
          if ((pPeriodicScan->numMissed * pPeriodicScan->interval * 1250) >
              (pPeriodicScan->syncCmd.timeout * 10000))
          {
            pPeriodicScan->terminate = LL_STATUS_ERROR_CONNECTION_TIMEOUT;
          }
        }
#ifdef RTLS_CTE
        while (llCteSamples.autoCopyCompleted > 0)
        {
          if (cteCount < pPeriodicScan->cteInfo.count)
          {
            MAP_llGetCteInfo( CTE_TASK_ID_CONNECTIONLESS, pPeriodicScan );
            cteCount++;
          }
          else
          {
            // we should not get to this point because
            // the RF was configured (cteCopyLimitCount) to get max cte count (cteInfo.count)
            MAP_RFHAL_NextDataEntryDone( (dataEntryQ_t *)llCteSamples.autoCopy.pSamplesQueue );
            // decrease number of completed buffers
            llCteSamples.autoCopyCompleted--;
          }
        }
#endif
      }
    }
    // check if there was a request for terminate the current periodic set
    if (pPeriodicScan->terminate)
    {
      MAP_llEndPeriodicScanTask(pPeriodicScan);
    }
    else
    {
      // drift time in RAT ticks per event (no drift in case drift learning in progress)
      int16 drift = (pPeriodicScan->driftLearnCounter <= PERIODIC_SCAN_DRIFT_LEARNING_MAX_NUM)?0:pPeriodicScan->driftFactor;

      // update next sync indication receive time
      if (pPeriodicScan->numMissed == 0)
      {
        pPeriodicScan->startTime += ((pPeriodicScan->syncCmd.skip + 1) * ((pPeriodicScan->interval * RAT_TICKS_IN_1_25MS) + drift));
        // update event counter
        pPeriodicScan->eventCounter += (pPeriodicScan->syncCmd.skip + 1);
      }
      else
      {
        pPeriodicScan->startTime += ((pPeriodicScan->interval * RAT_TICKS_IN_1_25MS) + drift);
        // update event counter
        pPeriodicScan->eventCounter++;
      }
      pPeriodicScan->rfCmd.rfOpCmd.startTime = pPeriodicScan->startTime;

      // initialize the status
      pPeriodicScan->rfCmd.rfOpCmd.status = RFSTAT_IDLE;

      //check if channel map should be updated
      if ((pPeriodicScan->chanMap.updated) &&
          (pPeriodicScan->chanMapUpdateEvent <= pPeriodicScan->eventCounter))
      {
        // finish the channel map update procedure
        llSetPeriodicScanChmapUpdate(pPeriodicScan,FALSE,NULL,0);
      }
      // set the next data channel
      pPeriodicScan->rfCmd.chan = llSetNextPeriodicAdvChan( &pPeriodicScan->chanMap.current,
                                                             pPeriodicScan->syncInfo.accessAddr,
                                                             pPeriodicScan->eventCounter );
    }
  }
  pPeriodicScan->rxCount = 0;
  // schedule this task
  MAP_llScheduler();

  // send event to host
  if (pTmpPeriodicScan != NULL)
  {
    uint8 phy = pTmpPeriodicScan->phy + ((pTmpPeriodicScan->phy == BLE5_S2_PHY) ? 0 : 1);
    HCI_PeriodicAdvSyncEstablishedEvent(LL_STATUS_SUCCESS,
                                        pTmpPeriodicScan->handle,
                                        pTmpPeriodicScan->syncCmd.sid,
                                        pTmpPeriodicScan->syncCmd.addrType,
                                        pTmpPeriodicScan->syncCmd.addr,
                                        phy,
                                        pTmpPeriodicScan->interval,
                                        pTmpPeriodicScan->syncInfo.sca);
  }

  return;
}
#endif // USE_PERIODIC_SCAN
#endif // SCAN_CFG

/*******************************************************************************
 */
