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
#include <ti/drivers/rcl/RCL.h>
#include <ti/drivers/rcl/commands/ble5.h>
#include "bcomdef.h"
#include "hal_mcu.h"
#include "hci_event.h"
#include "ll_common.h"
#include "ll_enc.h"
#include "ll_privacy.h"
#include "ll_rat.h"
#include "ll_timer_drift.h"
#include "ll_ae.h"
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
 * @Design:     BLE_LOKI-1455
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

  // clear all RX entries
  llClearScanDataQueue(TRUE);
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
  extScanCmd.common.status = RCL_CommandStatus_Idle;

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

  // calculate if graceful stop time \ hard stop time has not been reached yet
  // Note: True when first parameter is greater than the second.
  uint8 isGracfulTimeNotReached = MAP_llTimeCompare(extScanCmd.common.timing.absStartTime +
                                                    extScanCmd.common.timing.relGracefulStopTime, currentTime );
  uint8 isHardStopTimeNotReached = MAP_llTimeCompare(extScanCmd.common.timing.absStartTime +
                                                     extScanCmd.common.timing.relHardStopTime, currentTime );

  // check if there's more time before the end of the scan window and
  // that end time is not before the current time.
  // takes different cases into account, depends on which parameters we use (relHardStopTime, relGracefulStopTime or both)
  // Note: zero value means that the parameter is not in use
  if( (extScanCmd.common.timing.relHardStopTime == 0 && extScanCmd.common.timing.relGracefulStopTime != 0 && isGracfulTimeNotReached) ||
      (extScanCmd.common.timing.relHardStopTime != 0 && extScanCmd.common.timing.relGracefulStopTime == 0 && isHardStopTimeNotReached) ||
      (extScanCmd.common.timing.relHardStopTime != 0 && extScanCmd.common.timing.relGracefulStopTime != 0 && isHardStopTimeNotReached && isGracfulTimeNotReached)
     )
  {

      // Update start time to the future
      uint32 timeDiff = currentTime - extScanCmd.common.timing.absStartTime;
      extScanCmd.common.timing.absStartTime = currentTime;

      // Update relGracefulStopTime and relHardStopTime with the time left to scan since the
      // last command done received because the RCL stopped scanning after it finished receiving
      // AUX packet and not because the scan window ended.

      // Subtract timeDiff from relative graceful stop time only when it's in use (not zero)
      if(extScanCmd.common.timing.relGracefulStopTime != 0)
      {
          extScanCmd.common.timing.relGracefulStopTime -= timeDiff;
      }

      // Subtract timeDiff from relative hard stop time only when it's in use (not zero)
      if(extScanCmd.common.timing.relHardStopTime != 0)
      {
          extScanCmd.common.timing.relHardStopTime -= timeDiff;
      }

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
    extScanCmd.channel = nextScanChannel;

    // check if there's more than one Scan primary PHY
    if ( extScanInfo->pScanParam->scanPhys == (LL_PHY_1_MBPS+LL_PHY_CODED) )
    {
      // both 1M and Coded are being used
      // cycle through all three primary channels before switching PHY
      if (nextScanChannel == LL_ADV_BASE_CHAN)
      {
        extScanIndex ^= 1;
      }

      // update the scan type based on changed extScanIndex
      extScanCmd.activeScan = extScanInfo->pScanParam->extScanParam[extScanIndex].scanType;

      // update Start Time based on Scan Interval
      extScanCmd.common.timing.absStartTime = extScanInfo->scanStartTime +
                                             (extScanInfo->pScanParam->extScanParam[extScanIndex].scanInterval * RAT_TICKS_IN_625US);
      extScanInfo->scanStartTime = extScanCmd.common.timing.absStartTime;
      // Restart the Graceful Stop Time
      extScanCmd.common.timing.relGracefulStopTime = extScanInfo->pScanParam->extScanParam[extScanIndex].scanWindow * RAT_TICKS_IN_625US;
      extScanCmd.common.phyFeatures = extScanIndex << 1;

    }
    else // only one PHY is active, so leave the index alone
    {
      // update Start Time based on Scan Interval
      extScanCmd.common.timing.absStartTime = extScanInfo->scanStartTime +
                                             (extScanInfo->pScanParam->extScanParam[extScanIndex].scanInterval * RAT_TICKS_IN_625US);
      // save the absolute start time of the scanner for post-processing
      extScanInfo->scanStartTime = extScanCmd.common.timing.absStartTime;
    }

    // set window
    extScanCmd.common.timing.relGracefulStopTime =
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

  // After receiving lastCmdDone, Clear all Periodic Scanner buffers.
  llClearPeriodicScanDataQueue(TRUE);

  if ((pPeriodicScan->state == PERIODIC_SCAN_STATE_SYNCED) ||
      (pPeriodicScan->state == PERIODIC_SCAN_STATE_SYNCING_ACTIVE))
  {
    // check that there is no request for ending current periodic scan
    if (pPeriodicScan->terminate == 0)
    {
      // check status of last received sync indication
      // accept the packet also in case of CRC error
      if (((pPeriodicScan->rfCmd.common.status == RCL_CommandStatus_Finished) && (pPeriodicScan->rxCount > 0)) ||
          ((pPeriodicScan->rfCmd.common.status == RCL_CommandStatus_RxErr) && (llPeriodicScan.rfOutput.nRxNok > 0)))
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
        if ((taskEndStatus != RCL_CommandStatus_Finished) || (pPeriodicScan->rxCount == 0))
        {
          // check for timeout
          if ((pPeriodicScan->numMissed * pPeriodicScan->interval * 1250) >
              (pPeriodicScan->syncCmd.timeout * 10000))
          {
            pPeriodicScan->terminate = LL_STATUS_ERROR_CONNECTION_TIMEOUT;
          }
        }
#ifdef RTLS_CTE
        uint8 cteCount = 0;

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
        pPeriodicScan->rfCmd.common.timing.absStartTime += ((pPeriodicScan->syncCmd.skip + 1) * ((pPeriodicScan->interval * RAT_TICKS_IN_1_25MS) + drift));
        // update event counter
        pPeriodicScan->eventCounter += (pPeriodicScan->syncCmd.skip + 1);
      }
      else
      {
        pPeriodicScan->rfCmd.common.timing.absStartTime += ((pPeriodicScan->interval * RAT_TICKS_IN_1_25MS) + drift);
        // update event counter
        pPeriodicScan->eventCounter++;
      }

      // initialize the status
      pPeriodicScan->rfCmd.common.status = RCL_CommandStatus_Idle;

      //check if channel map should be updated
      if ((pPeriodicScan->chanMap.updated) &&
          (pPeriodicScan->chanMapUpdateEvent <= pPeriodicScan->eventCounter))
      {
        // finish the channel map update procedure
        llSetPeriodicScanChmapUpdate(pPeriodicScan,FALSE,NULL,0);
      }
      // set the next data channel
      pPeriodicScan->rfCmd.channel = llSetNextPeriodicAdvChan( &pPeriodicScan->chanMap.current,
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
