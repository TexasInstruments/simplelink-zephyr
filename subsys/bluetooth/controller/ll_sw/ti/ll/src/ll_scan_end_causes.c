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
#include "map_direct.h"
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

#ifdef SCAN_OPTIMIZER
      // When using SCAN_OPTIMIZER and the device has a gap between two following scan windows:
      // a random number, with a range of (scan interval - scan window), will be added to the next scan start time
      // instead of using the scan interval.
      if(extScanInfo->pScanParam->extScanParam[extScanIndex].scanInterval > extScanInfo->pScanParam->extScanParam[extScanIndex].scanWindow)
      {
          if(llTimeCompare(extScanCmd.common.timing.absStartTime, currentTime))
          {
              uint32 scanTimeDiff = extScanCmd.common.timing.absStartTime - currentTime;
              uint32 delay = (LL_ENC_GeneratePseudo32RandNum() % (scanTimeDiff));
              extScanCmd.common.timing.absStartTime = currentTime + delay + 2 * RAT_TICKS_IN_1_5MS;;
          }
      }
#endif

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
      }
    }
    // check if there was a request for terminate the current periodic set
    if (pPeriodicScan->terminate)
    {
      MAP_llEndPeriodicScanTask(pPeriodicScan);
    }
    else
    {
      // Calculate the SCA drift using the advertiser SCA and the device SCA
      // Interval is given in 1.25ms units, multiply by 2 to convert to 625us units
      uint32 scaDrift = MAP_llCalcPeriodicScaDriftPerInterval(pPeriodicScan->syncInfo.sca, CONVERT_1_25MS_TO_0_625MS( pPeriodicScan->interval ));

      // Save the lastStartTime
      uint32 lastStartTime = pPeriodicScan->rfCmd.common.timing.absStartTime;

      // Next sync indication receive time will depends if the packet was missed or not

      // delayUnits is the number of missed events or number of skips, it will be used to set the
      // correct SCA delay and the next start time
      uint16 delayUnits;

      // Packet was received
      if (pPeriodicScan->numMissed == 0)
      {
        delayUnits =  pPeriodicScan->syncCmd.skip + 1;

        // Set stop time to base value
        pPeriodicScan->rfCmd.common.timing.relGracefulStopTime = PER_SUCCESS_SOFTSTOPTIME_DEFAULT;

        // Calculate the next start time for the scanner based on the periodic interval and the number of skips
        pPeriodicScan->rfCmd.common.timing.absStartTime = lastStartTime + (delayUnits * pPeriodicScan->interval)
                                                          * RAT_TICKS_IN_1_25MS;
        // update the eventCounter based on number of skips
        pPeriodicScan->eventCounter += delayUnits;

      }
      else // Periodic event was missed
      {
        delayUnits =  pPeriodicScan->numMissed;

        // When periodic event missed, trying to catch the next periodic event even if skip value has been set.
        pPeriodicScan->rfCmd.common.timing.absStartTime = lastStartTime + (pPeriodicScan->interval) * RAT_TICKS_IN_1_25MS;

        // Trying to catch the next periodic event so increase the eventCounter in one only.
        pPeriodicScan->eventCounter ++;

      }

      /* Widening process - extend the window from both sides if needed */

      // Open the window earlier, depends on the ScaDrift and the number of skips or missed events.
      pPeriodicScan->rfCmd.common.timing.absStartTime = pPeriodicScan->rfCmd.common.timing.absStartTime -
                                                        (delayUnits * scaDrift);

      // Extend the window size, depends on the number of skips or missed events. multiple by 2 to handle delayed drift
      // as well.
      pPeriodicScan->rfCmd.common.timing.relGracefulStopTime = pPeriodicScan->rfCmd.common.timing.relGracefulStopTime +
                                                               (delayUnits  * scaDrift * 2);

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
