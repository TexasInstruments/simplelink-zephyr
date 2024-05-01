/******************************************************************************

 @file  ll_sdaa.c

 @brief This file contains the SDAA (Selective Detect And Avoid) moudule,
        This module is responsible for monitoring and limiting TX consumption
        per channel, the module will be activated by SDAA_ENABLE define

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2009 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*****************************************************************************
 * INCLUDES
*/

#ifdef SDAA_ENABLE
#ifndef USE_RCL
#include "ll_sdaa.h"
#include "ll_rat.h"
#include "ll.h"
#include "ble.h"
#include "map_direct.h"
#include "ble_user_config.h"
#include "ll_ae.h"
#include "hal_mcu.h"
#include <math.h>

//*****************************************************************************
// MACROS
//*****************************************************************************

//*****************************************************************************
//! @fn    llsdaaConvertbytesToRat
//
//! @brief This Function
//
//! @param bytes - byte length
//!        phyType - 1M, 2M, Coded
//
//! @return Time in rat
//
//*****************************************************************************
//! This Macro convert bytes to rat depend on phy type
#define SDAA_CONVERT_BYTES_TO_RAT(BYTES,PHY_TYPE) (BYTES * RAT_TICKS_IN_1S)/ \
                                        (phyTypeToByteRateConvertor[PHY_TYPE])

//! This Macro add 64US ticks to TX usage channel table
#define SDAA_ADD_RAT_USAGE_TO_CHANNEL(RAT_USAGE,CHANNEL) \
    gSdaaInternalDB.txUsageTbl[CHANNEL] += (RAT_USAGE + \
                                            RAT_TICKS_IN_64US - \
                                            1 \
                                           ) \
                                           /RAT_TICKS_IN_64US \

//! This is get Macro to extract rat usage from TX usage table
#define SDAA_GET_RAT_USAGE_IN_CHANNEL(CHANNEL) \
    gSdaaInternalDB.txUsageTbl[CHANNEL] * RAT_TICKS_IN_64US

//! The regulation allow the device to transmit more data when the tx power level is lower.
//! this Macro calculate the correct ratio according to the spec formula
#define SDAA_GET_TX_POWER_RATIO(TX_POWER) \
     pow(10,TX_POWER * 0.1) * 0.01
//*****************************************************************************
// CONSTANTS
//*****************************************************************************


#define CHANNEL_STATE_MASK            0x3
#define BITS_PER_CHANNEL_STATE        2   // channel state is 2 bit per channel
#define CHANNELS_STATE_PER_BYTES      (8/BITS_PER_CHANNEL_STATE)

//! @brief Running time of the function RF_getRssi(),
//!        RX window duration (sdaaUsrCfg_t->rxWindowDuration) defined by
//!        the user is divided by this value (In rat), for calculating
//!        the number of samples.
//!
#define LL_SDAA_SAMPLE_DURATION       64

//! @brief Because channel state is 2 bit per channel and there is 40 BLE
//!        channels, 10 bytes is needed for storing the data
//!
#define LL_SDAA_CHANNEL_STATE_SIZE    10

#define LL_SDAA_TX_POWER_THRESHOLD    11 // if the tx power level of the device is lower
                                         //than this threshold there is no need in this module.

#define LL_SDAA_NUMBER_OF_ADVERTISE_TASKS   2

//! @brief Definitions related to the BLE protocol
//!
#define EMPTY_PACKET_SIZE             10 // Bytes
#define NON_EMPTY_PACKET_OVERHEAD     14 // Bytes

#define ONE_MEGA_BYTE_RATE            125000
#define TWO_MEGA_BYTE_RATE            250000
#define ENCODED_BYTE_RATE             15625

//! @brief According to the regulation, when calculating average observation
//!        period, the average dwell time on a channel must be multiplied by
//!        100 relevant only for gSdaaUsrParams->constobservtime == FALSE
//!
#define DWT_TO_OBS_PERIOD             100

//! @brief Minimum time in ms to be consider as observation time,
//!        in addiotion this value is default
//!
#define MIN_OBS_PERIOD                RAT_TICKS_IN_100MS

//! @brief Lookup tables used by the functions responsible for record tx usage,
//!        To avoid saving float table, the table txPowerToUsageConvertor
//!        is multiplied by 1000, and when used it will be divided by 1000
//!

const uint16 txPowerToUsageConvertor[]    = { 1, 125, 158, 200, 251, 316,
                                             398, 500, 633, 800, 1000
                                            };

const uint32 phyTypeToByteRateConvertor[] = { 1,                  //not in use
                                              ONE_MEGA_BYTE_RATE,
                                              TWO_MEGA_BYTE_RATE,
                                              1,                  //not in use
                                              ENCODED_BYTE_RATE
                                             };

/// SCHEDULER CONSTANTS ///

// flags to indicate the state of next primary and secondary task.
// There is 4 flags for each task (prim/sec) which inform
// whether the task is exist, the channel is clear, RX window is necessary and
// allowed.
#define LL_SDAA_SECONDARY_BLOCK_CHANNEL                 BV(0)
#define LL_SDAA_SECONDARY_RX_WINDOW_ALLOWED             BV(1)
#define LL_SDAA_SECONDARY_RX_WIN_NECESSARY              BV(2)
#define LL_SDAA_SECONDARY_EXIST_AND_ALLOWED             BV(3)
#define LL_SDAA_PRIMARY_BLOCK_CHANNEL                   BV(4)
#define LL_SDAA_PRIMARY_RX_WINDOW_ALLOWED               BV(5)
#define LL_SDAA_PRIMARY_RX_WIN_NECESSARY                BV(6)
#define LL_SDAA_PRIMARY_EXIST                           BV(7)
#define LL_SDAA_INIT_STATE                              0
// Optional operation to perform in the scheduler when SDAA module is enable.
#define LL_SDAA_SCHEDULER                               0
#define LL_SDAA_SCHEDULER_UPDATE_TASK_TYPE_TO_PRIM      1
#define LL_SDAA_SCHEDULER_UPDATE_SECONDARY_TASK_TYPE    2
#define LL_SDAA_SCHED_RX_WIN_PRIM                       3
#define LL_SDAA_SCHED_RX_WIN_SECONDARY                  4
#define LL_SDAA_SCHED_LIMIT_PRIM                        5
#define LL_SDAA_SCHED_RX_WIN_UNBLOCKED_CHANNEL          6
#define LL_SDAA_SCHEDULER_ERROR                         7
// Number of rows in the decisoin table which perform an operation per
// given next tasks state.
#define LL_SDAA_DECISION_TABLE_SIZE                     25

//*****************************************************************************
// TYPEDEFS
//*****************************************************************************

//! @brief This struct includes the data about the observation period,
//!        when it ends and what it duration (duration can be dynamic or
//!        constant depending on gSdaaUsrParams->constobservtime)
//!
typedef struct
{
    uint32 endTime;
    uint32 duration;

} sdaaObserPeriod_t;

//! @brief This struct includes the data for dwell time calculation
//!        data is saved in a compressed form 64us uints
//!        relevant only for gSdaaUsrParams->constobservtime == FALSE
//!
typedef struct
{
    uint8  numOfDWT;
    uint16 sumOfDWT;

} sdaaDWTAcc_t;

// Scheduler typedefs ///
//! @brief This struct contains the internal DB that
//!        the module needs to function properly
//!
typedef struct
{
    //!
    //!
    uint8 channelInSample;


    //!
    //!
    uint8 localTxUsageTresh;

    //! Maximum number of task and connection for dwell time
    //! record and calculation relevant only for
    //! gSdaaUsrParams->constobservtime == FALSE
    //!
    uint8 maxNumTasksAndConns;

    //! Store state(sdaaState_e) for each channel 2 bit per channel
    //!
    uint8 channelState[LL_SDAA_CHANNEL_STATE_SIZE];

    //! for more information @sdaaTXAcc_t struct
    //!
    uint16 txUsageTbl[LL_TOTAL_NUM_RF_CHAN];

    //!
    //!
    uint16 blockChannels[LL_TOTAL_NUM_RF_CHAN];

    //!
    //!
    uint32 maxTHForBlockChanInRat;
    //!for more information @sdaaDWTAcc_t struct
    //!
    sdaaDWTAcc_t* dwellTimeAcc;

    //! for more information @sdaaObserPeriod_t struct
    //!
    sdaaObserPeriod_t obserPeriod;

} sdaaInternalDB_t;

// This struct consists of paramteres for RX window task, and indicator for
// the scheduler.
typedef struct sdaaTaskParams
{
    uint32              startTimePrim; // the start time of RX window command
                                       // of the primary channel.
    uint32              startTimeSec;  // the start time of RX window command
                                       // of the secondary channel.
    uint8               primChan;      // the channel of the primary task.
    uint8               secChan;       // the channel of the secondary task.
    uint8               newStartType;  // new start type of the next task.
                                       // optional - LL_SCHED_START_IMMED,
                                       //            LL_SCHED_START_EVENT,
                                       //            LL_SCHED_START_PRIMARY.
} sdaaTaskParams_t;

// Decision table entry. Each entry is suitable for some cases and included the
// specific operation to perform when there is a match.
typedef struct decisionTableEntry
{
    uint8 value;                       // the result of masking with state.
    uint8 mask;                        // specific mask of entry in decision
                                       // table.
    uint8 operation;                   // the operation to perform in case
                                       // there is a match between value and
                                       // the results of the masking with the
                                       // state.
} decisionTableEntry_t;

/*******************************************************************************
 * decisionTable
 *
 * @brief       The following decision table used to perform an operation to
 *              the scheduler according to state of the next tasks.
 *              The state of the next tasks is represented by bitmap of 8 bits.
 *              The bitmap divided so that the 4 MSB bits describes the primary
 *              task state, and the 4 LSB bits describes the secondary task
 *              state.
 *              The 4 bits consists flags which indicates the existance of the
 *              task, the necessarity and abilityh to open an RX window, and
 *              whether the channel is blocked.
 *              optional values for each flag are 1 / 0 / x (Don't Care).
 *              These four flags will be desribes bellow:
 *              task exist             - The task is exist and there is
 *                                       sufficient time to schedule the task,
 *                                       considering the priorities of the tasks.
 *              Blocked channel        - The channel of the task is in block
 *                                       state, so sending data on this channel
 *                                       is prohibited.
 *              RX window is necessary - The channel of the task is in overload
 *                                       state, so RX window is necessary.
 *              RX window is allowed   - There is sufficient time to schedule
 *                                       the RX window the task.
 *
 *              There are few optional operation thach can be performed:
 *              LL_SDAA_SCHEDULER                            - The next task
 *                                                             will be scheduled
 *                                                             from the scheduler
 *                                                             function without
 *                                                             changes.
 *              LL_SDAA_SCHEDULER_UPDATE_TASK_TYPE_TO_PRIM   - The next task
 *                                                             will be scheduled
 *                                                             from the scheduler
 *                                                             function, but the
 *                                                             task type will be
 *                                                             change to primary
 *                                                             task.
 *              LL_SDAA_SCHEDULER_UPDATE_SECONDARY_TASK_TYPE - The next task
 *                                                             will be scheduled
 *                                                             from the scheduler
 *                                                             function, but the
 *                                                             task type of the
 *                                                             secondary task
 *                                                             will be update.
 *              LL_SDAA_SCHED_RX_WIN_PRIM                    - The next task
 *                                                             will be RX window
 *                                                             for primary task.
 *              LL_SDAA_SCHED_RX_WIN_SECONDARY               - The next task
 *                                                             will be RX window
 *                                                             for secondary task.
 *              LL_SDAA_SCHED_LIMIT_PRIM                     - The next task will
 *                                                             be primary task,
 *                                                             but the TX queue
 *                                                             is unlink.
 *              LL_SDAA_SCHED_RX_WIN_UNBLOCKED_CHANNEL       - schedule RX window
 *                                                             for unblocked
 *                                                             channel.
 *              LL_SDAA_SCHEDULER_ERROR                      - The next task is
 *                                                             unknown since
 *                                                             there is an error.
 *
 * Each entry in the decision table consists of 3 parameters.
 * value        - the value calculate by convert all the x (don't care) fields
 *                to 0, and set the rest of the fields (0, 1) to 1.
 * mask         - the mask calculate by convert all the x (don't care) fields
 *                to 0, and keep the rest of the flags.
 * operation    - the operation to perform in scheduler when match is found.
 *
 * The file "decisiontable.xlsx" represent all the possible states from 8 bits.
 * Each case has an calculate of value, and mask, the operation to perform, and
 * explanation to performing that operation.
 * The file consists of tab which includes the full decision table decleration.
 * The file calculate autumaticly value and mask for each entry.
 *
 * The following decision table is took from the file "decisiontable.xlsx".
 * Each entry describes value, mask, and operation, respectively.
 *
 *  {   value   ,   mask    ,   operation   }
*/
const decisionTableEntry_t decisionTable[] =
{
    {   0x00    ,   0x88    ,   LL_SDAA_SCHEDULER_ERROR                         }, // ID0
    {   0x08    ,   0x8D    ,   LL_SDAA_SCHEDULER                               }, // ID1
    {   0x09    ,   0x8D    ,   LL_SDAA_SCHED_RX_WIN_UNBLOCKED_CHANNEL          }, // ID2
    {   0x0C    ,   0x8E    ,   LL_SDAA_SCHED_RX_WIN_SECONDARY                  }, // ID3
    {   0x0E    ,   0x8E    ,   LL_SDAA_SCHED_RX_WIN_SECONDARY                  }, // ID4
    {   0x80    ,   0xD8    ,   LL_SDAA_SCHEDULER                               }, // ID5
    {   0x88    ,   0xDD    ,   LL_SDAA_SCHEDULER                               }, // ID6
    {   0x89    ,   0xDD    ,   LL_SDAA_SCHEDULER_UPDATE_TASK_TYPE_TO_PRIM      }, // ID7
    {   0x8C    ,   0xDE    ,   LL_SDAA_SCHEDULER_UPDATE_TASK_TYPE_TO_PRIM      }, // ID8
    {   0x8E    ,   0xDE    ,   LL_SDAA_SCHED_RX_WIN_SECONDARY                  }, // ID9
    {   0x90    ,   0xD8    ,   LL_SDAA_SCHED_LIMIT_PRIM                        }, // ID10
    {   0x98    ,   0xDD    ,   LL_SDAA_SCHEDULER                               }, // ID11
    {   0x99    ,   0xDD    ,   LL_SDAA_SCHED_LIMIT_PRIM                        }, // ID12
    {   0x9C    ,   0xDE    ,   LL_SDAA_SCHED_LIMIT_PRIM                        }, // ID13
    {   0x9E    ,   0xDE    ,   LL_SDAA_SCHED_RX_WIN_SECONDARY                  }, // ID14
    {   0xC0    ,   0xE8    ,   LL_SDAA_SCHED_LIMIT_PRIM                        }, // ID15
    {   0xC8    ,   0xED    ,   LL_SDAA_SCHEDULER                               }, // ID16
    {   0xC9    ,   0xED    ,   LL_SDAA_SCHED_LIMIT_PRIM                        }, // ID17
    {   0xCC    ,   0xEE    ,   LL_SDAA_SCHED_LIMIT_PRIM                        }, // ID18
    {   0xCE    ,   0xEE    ,   LL_SDAA_SCHED_RX_WIN_SECONDARY                  }, // ID19
    {   0xE0    ,   0xE8    ,   LL_SDAA_SCHED_RX_WIN_PRIM                       }, // ID20
    {   0xE8    ,   0xED    ,   LL_SDAA_SCHEDULER_UPDATE_SECONDARY_TASK_TYPE    }, // ID21
    {   0xE9    ,   0xED    ,   LL_SDAA_SCHED_RX_WIN_PRIM                       }, // ID22
    {   0xEC    ,   0xEE    ,   LL_SDAA_SCHED_RX_WIN_PRIM                       }, // ID23
    {   0xEE    ,   0xEE    ,   LL_SDAA_SCHED_RX_WIN_SECONDARY                  }, // ID24
};

//*****************************************************************************
// GLOBAL VARIABLES
//*****************************************************************************

//! @brief Pointer to an external struct for the module defined by the
//!        user (can be defined in sysconfig/opt file/pre defined)
//!
sdaaUsrCfg_t*    gSdaaUsrParams = NULL;

//! @brief Main internal DB for more information @sdaaInternalDB_t
//!
sdaaInternalDB_t gSdaaInternalDB = {0};

extern sortedAdv_t *pNextAdvSet;
//*****************************************************************************
// LOCAL FUNCTIONS PROTOTYPES
//*****************************************************************************

static void          llsdaaHandleUsageException(uint8 channel);
static void          llsdaaHandleObservationPeriod(void);
static sdaaChState_e llsdaaHandleBlockChannels(uint8 channel);

static void   llsdaaSetObsPeriodAndCounterReset(void);
static void   llsdaaSetChannelState(uint8 channel, sdaaChState_e state);

static uint32 llsdaaGetMinDwellTime(void);
static uint32 llsdaaGetAvgDwellTimeInTask(uint8 task);

// prototype for SDAA control TX
uint8 llHandleSDAAControlTX( llConnState_t *nextConnPtr,
                             taskInfo_t    *secTask,
                             uint8          startTaskType);
uint8 llSDAAGetTaskState(llConnState_t *nextConnPtr,
                         taskInfo_t *secTask,
                         sdaaTaskParams_t *pTaskParams,
                         uint8 startType);
uint8 LL_SDAA_getDecision(uint8 schedState);
uint8 llSDAASufficientTimeRXWindow(taskInfo_t *nextTask, uint32 *pStartTime);
void llSDAAConfigRXWindow(uint32 startTime, uint8 channel, uint8 setRfChannel);
void llSDAASchdRxWindow(void);
//*****************************************************************************
// API'S FUNCTIONS
//*****************************************************************************

//*****************************************************************************
//! @fn    LL_SDAA_Init
//
//! @brief This API Initializes the SDAA module, with input configurations
//         from sysconfig/opt file/pre defined
//
//*****************************************************************************
void LL_SDAA_Init()
{
    //Store global pointer for user config
    gSdaaUsrParams = llUserConfig.sdaaCfgPtr;

    //Clear memory
    osal_memset(&gSdaaInternalDB, 0, sizeof(gSdaaInternalDB));

    gSdaaInternalDB.maxNumTasksAndConns = llUserConfig.maxNumConns + LL_SDAA_NUMBER_OF_ADVERTISE_TASKS;

    //Divide usage threshold by 2 for prevent scenario of cross this limit
    //because observation period is over
    gSdaaInternalDB.localTxUsageTresh = gSdaaUsrParams->txUsageTresh >> 1;

    //Set observation period and threshold values respectively
    llsdaaSetObsPeriodAndCounterReset();

    //Set default value
    LL_SDAA_SetChannelInSample(LL_SDAA_NONE_ACTIVE_CHANNEL);

    //For a dynamic observation period, the array of average dwell time must be
    //allocate and initialized
    if(gSdaaUsrParams->constobservtime == FALSE)
    {
        gSdaaInternalDB.dwellTimeAcc =
        (sdaaDWTAcc_t *)
                (MAP_osal_mem_alloc(
                    sizeof(sdaaDWTAcc_t) *
                    gSdaaInternalDB.maxNumTasksAndConns));

        osal_memset(
            (sdaaDWTAcc_t *) &gSdaaInternalDB.dwellTimeAcc,
            0,
            sizeof(sdaaDWTAcc_t) * gSdaaInternalDB.maxNumTasksAndConns);
    }
}

//*****************************************************************************
//! @fn    LL_SDAA_RecordTxUsage
//
//! @brief This API is divided into 3 parts:
//!
//!            1. Call to the function that handles the observation period,
//!               if it is over, set new obs period and clear all databases.
//!            2. Calculate and record TX usage based on input variables.
//!            3. Call to the function that handles usage exception
//!               if necessary.
//
//! @param numOfBytes - The number of bytes sent over the air,
//!                     0 for empty packet.
//
//!        phyType    - phyType is affected by a convert table between
//!                     the sending rate and air time.
//
//!        txPower    - txPower affects the usage by the convert ratio table,
//!                     the weaker the power, the smaller the ratio.
//
//!        channel    - The data channel to which the usage will be added.
//
//*****************************************************************************
void LL_SDAA_RecordTxUsage(uint16 numOfBytes, uint8 phyType,
                              uint8 txPower, uint8 channel)
{
    uint8 headerSize = 0;
    uint32 ratUsage = 0;

    // if tx power level is lower than this threshold the device is not high
    // power so there is no need to record it's usage.
    if( txPower < LL_SDAA_TX_POWER_THRESHOLD )
    {
      return;
    }

    //if currentObsperiod is over set new obs period and clear all databases
    llsdaaHandleObservationPeriod();

    //Checking if it is an empty package and add overhead accordingly
    headerSize = (numOfBytes == 0) ? EMPTY_PACKET_SIZE :
                                     NON_EMPTY_PACKET_OVERHEAD;

    //Calculation of rat usage using a conversion table based on phyType
    ratUsage = SDAA_CONVERT_BYTES_TO_RAT((numOfBytes + headerSize), phyType);
    //Record usage on the channel in the suitable ratio according to txPower.
    // the number will be truncated to an integer inside the macro
    SDAA_ADD_RAT_USAGE_TO_CHANNEL((ratUsage *
            SDAA_GET_TX_POWER_RATIO(txPower)),channel);
    //Call to the function that handles usage exception if necessary.
    llsdaaHandleUsageException(channel);
}

//*****************************************************************************
//! @fn    LL_SDAA_AddDwtRecord
//
//! @brief This API add Dwt Record to the specific task
//
//! @param dwt   - DwellTime in rat ticks
//
//!        index - Connection handle ID or Task
//
//*****************************************************************************
void LL_SDAA_AddDwtRecord(uint32 dwT, uint8 task, uint8 index)
{
    if(gSdaaUsrParams->constobservtime == FALSE)
    {
        switch( task )
        {
           case LL_TASK_ID_CENTRAL:
           case LL_TASK_ID_PERIPHERAL:
              break;

           case LL_TASK_ID_ADVERTISER:
               index = llUserConfig.maxNumConns;
               break;

           case LL_TASK_ID_PERIODIC_ADVERTISER:
               index = llUserConfig.maxNumConns + 1;
               break;

        }

        gSdaaInternalDB.dwellTimeAcc[index].numOfDWT++;
        //Convert Dwelltime to Compressed units and save it
        gSdaaInternalDB.dwellTimeAcc[index].sumOfDWT += (dwT +
                                                       RAT_TICKS_IN_64US -
                                                       1
                                                      )
                                                      /RAT_TICKS_IN_64US;
    }

}

//*****************************************************************************
//! @fn    LL_SDAA_SampleRXWindow
//
//! @brief This API used to sample the RSSI values when the RX window is open.
//!        RSSI values equal to RF_GET_RSSI_ERROR_VAL will be count as
//!        invalid samples.
//!        RSSI values greater than RSSI Threshold will be count as
//!        noisy samples.
//!        the function determine the channel state according to
//!        the counters (invalid samples/ noisy samples) and update
//!        the channel state in SDAA DB's.
//
//*****************************************************************************
void LL_SDAA_SampleRXWindow(void)
{
    halIntState_t cs;
    int8  rssiValue = 0;
    uint8 sampleIdx = 0;
    uint8 rxChannel = 0;

    //Noisy samples counter
    uint8 numOfNoisySamples = 0;

    //Invalid samples counter(RSSI value equal to RF_GET_RSSI_ERROR_VAL)
    uint8 numOfInvalidSamples = 0;

    //Number of samples in RX Window
    uint8 numOfSamples = 0;

    //Set channelState to SDAA_CH_OVERLOAD as a default state
    sdaaChState_e channelState = SDAA_CH_OVERLOAD;
    //Start Critical Section.
    HAL_ENTER_CRITICAL_SECTION(cs);
    //Get the current channel of the RX window listening
    rxChannel = gSdaaInternalDB.channelInSample;

    //Check if the channel is valid
    if(rxChannel == LL_SDAA_NONE_ACTIVE_CHANNEL ||
       rxChannel >= LL_TOTAL_NUM_RF_CHAN)
    {
        //Exit sampling function because there is not an active channel,
        //so the RSSI values is irrelevant.
        return;
    }

    //Calculation of the number of samples that can be samples in the RX window
    numOfSamples = (gSdaaUsrParams->rxWindowDuration * RAT_TICKS_IN_10US)
                   /LL_SDAA_SAMPLE_DURATION;

    //The following samples should be valid samples.
    //RSSI sample value greater than SDAA_RSSI_THRESHOLD will be
    //count as noisy samples.
    for (sampleIdx = 0 ; sampleIdx < numOfSamples ; sampleIdx++)
    {
        rssiValue = RF_getRssi(rfHandle);

        //Accumulate the number of invalid samples.
        if(rssiValue == RF_GET_RSSI_ERROR_VAL)
        {
            numOfInvalidSamples++;
        }
        else
        {
            //Accumulate the number of noisy samples.
            if (rssiValue > gSdaaUsrParams->rssithreshold)
            {
                numOfNoisySamples++;

                //Number of noisy samples bigger than threshold will cause
                //the channel state to be update to SDAA_CH_BLOCK.
                if (numOfNoisySamples >= gSdaaUsrParams->numberofnoisysamples)
                {
                    //Mark the channel as block channel
                    channelState = SDAA_CH_BLOCK;
                    gSdaaInternalDB.blockChannels[rxChannel] =
                            (MAP_llGetCurrentTime() +
                                    gSdaaUsrParams->blockingchanneltime *
                                    RAT_TICKS_IN_1S) >> 16;

                    //Break in case the there was enough noisy samples to
                    //determine that the channel is block
                    break;
                }
            }
        }
    }

    //The channel state will be update to SDAA_CH_UNKNOWN when half
    //of the samples is invalid.
    if (channelState != SDAA_CH_BLOCK  && numOfInvalidSamples <= sampleIdx/2)
    {
        //Most of the samples is valid and clear so channel state will be
        //update to SDAA_CH_CLEAR mark the channel as clear channel
        channelState = SDAA_CH_CLEAR;
    }

    //Update channel state
    llsdaaSetChannelState( rxChannel, channelState );

    //Exit function at the end of the sample process and the channel state
    //updated
    LL_SDAA_SetChannelInSample(LL_SDAA_NONE_ACTIVE_CHANNEL);
    // End Critical Section.
    HAL_EXIT_CRITICAL_SECTION(cs);
    return;
}

//*****************************************************************************
//! @fn    LL_SDAA_GetChannelState
//
//! @brief This API is used to get the channel state from SDAA DB
//
//! @param channel - The channel on which the data will be received
//
//! @return sdaaChState_e - SDAA_CH_CLEAR
//!                         SDAA_CH_OVERLOAD
//!                         SDAA_CH_BLOCK
//!                         SDAA_CH_INVALID
//
//*****************************************************************************
sdaaChState_e LL_SDAA_GetChannelState(uint8 channel)
{
    //! The byte idx of channel state into the channel state DB
    uint8 channelStateIdx = 0;
    //! The byte into the channel state DB that consists of the channel state
    uint8 channelStateByte = 0;
    //! The LSB of the channel state in the channelStateByte
    uint8 stateLSBIdx = 0;
    //! The state of the channel examine by the detect module
    sdaaChState_e channelState = SDAA_CH_INVALID;

    //Verify input parameters are valid
    if(channel >= LL_TOTAL_NUM_RF_CHAN)
    {
        return channelState;
    }

    //Get channel state from sdaa DB.
    channelStateIdx = channel / CHANNELS_STATE_PER_BYTES;
    channelStateByte = gSdaaInternalDB.channelState[channelStateIdx];
    stateLSBIdx = (channel % CHANNELS_STATE_PER_BYTES)
                    * BITS_PER_CHANNEL_STATE ;
    channelState = (sdaaChState_e)((channelStateByte >> stateLSBIdx)
                                   & CHANNEL_STATE_MASK);

    //Check if the channel state is blocked
    if (channelState == SDAA_CH_BLOCK)
    {
        //The channel is blocked, so check if the time of block state
        // is passed and handle channel state.
        channelState = llsdaaHandleBlockChannels(channel);
    }

    //Return the channel state.
    return channelState;
}

//*****************************************************************************
//! @fn    LL_SDAA_GetRXWindowDuration
//
//! @brief This API used to get the RX window duration in rats.
//
//! @return uint16 RXWindowDuration
//
//*****************************************************************************
uint16 LL_SDAA_GetRXWindowDuration(void)
{
    return gSdaaUsrParams->rxWindowDuration * RAT_TICKS_IN_10US;
}

//*****************************************************************************
//! @fn    LL_SDAA_SetChannelInSample
//
//! @brief This API used to set the channel which in sample mode.
//
//! @param channel - channel which in sample mode
//
//*****************************************************************************
void LL_SDAA_SetChannelInSample(uint8 channel)
{
    gSdaaInternalDB.channelInSample = channel;
}

//*****************************************************************************
//! @fn    LL_SDAA_GetTimeOutOfBlockedState
//
//! @brief This API used to get the time at which the elapsed time during which
//!        the channel state is block.
//
//! @param channel - The block channel.
//
//! @return uint32 time.
//
//*****************************************************************************
uint32 LL_SDAA_GetTimeOutOfBlockedState(uint8 channel)
{
    return (gSdaaInternalDB.blockChannels[channel] << 16);
}

//*****************************************************************************
// LOCAL FUNCTIONS
//*****************************************************************************

//*****************************************************************************
//! @fn    llsdaaHandleUsageException
//
//! @brief This function that checks whether the size of the new information
//!        we want to send and the information we have already sent on the
//!        same channel at the same observation period together exceeds
//!        the allowed threshold
//
//! @param channel - channel which examine and handle
//
//*****************************************************************************
static void llsdaaHandleUsageException(uint8 channel)
{
    uint32 currentUsageInRat = SDAA_GET_RAT_USAGE_IN_CHANNEL(channel);

    if (currentUsageInRat >= gSdaaInternalDB.maxTHForBlockChanInRat &&
        LL_SDAA_GetChannelState(channel) == SDAA_CH_CLEAR)
    {
          llsdaaSetChannelState( channel, SDAA_CH_OVERLOAD);
    }
}

//*****************************************************************************
//! @fn    llsdaaHandleObservationPeriod
//
//! @brief This function checks if current Observation period is over.
//         If obseravtion period is over call llsdaaSetObsPeriodAndCounterReset
//
//*****************************************************************************
static void llsdaaHandleObservationPeriod()
{
    uint32 currentTime = MAP_llGetCurrentTime();
    uint32 endtime = gSdaaInternalDB.obserPeriod.endTime;

    //Check if Observation period is over
    if(llTimeCompare(currentTime,endtime))
    {
        //Set new observation period and reset all counters
        llsdaaSetObsPeriodAndCounterReset();
    };
}

//*****************************************************************************
//! @fn    llsdaaHandleBlockChannels
//
//! @brief This function
//
//! @param channel - channel which examine and handle
//
//! @return channelState - the new channel state
//*****************************************************************************
static sdaaChState_e llsdaaHandleBlockChannels(uint8 channel)
{
    sdaaChState_e channelState = SDAA_CH_BLOCK;
    uint8 timeOut;

    //The channel is busy for gSdaaUsrParams->blockingchanneltime,
    //then the channel state will be update to overload state.
    timeOut = llTimeCompare(MAP_llGetCurrentTime(),
                           (gSdaaInternalDB.blockChannels[channel] << 16));

    //Check if time of busy channel is passed so
    //the channel state need to be update to block state.
    if(timeOut)
    {
        //The time is passed so update channel state to overload.
        llsdaaSetChannelState(channel,SDAA_CH_OVERLOAD);
        channelState = SDAA_CH_OVERLOAD;

        //Reset the time of the busy channel since the channel
        //isn't busy anymore.
        gSdaaInternalDB.blockChannels[channel] = 0;
    }

    return channelState;
}

//*****************************************************************************
//! @fn    llsdaaSetObsPeriodAndCounterReset
//
//! @brief This function set the next Observation Period duration and time
//
//*****************************************************************************
static void llsdaaSetObsPeriodAndCounterReset(void)
{
    uint32 newDuration = 0;

    if(gSdaaUsrParams->constobservtime == FALSE)
    {
        newDuration = llsdaaGetMinDwellTime();
        osal_memset((sdaaDWTAcc_t*) &gSdaaInternalDB.dwellTimeAcc,
                    0,
                    sizeof(gSdaaInternalDB.dwellTimeAcc) *
                    gSdaaInternalDB.maxNumTasksAndConns);
    }
    else
    {
        newDuration = gSdaaUsrParams->observationtime * ( RAT_TICKS_IN_100MS / 2 );
    }
    gSdaaInternalDB.obserPeriod.duration = newDuration;
    gSdaaInternalDB.obserPeriod.endTime  = MAP_llGetCurrentTime() +
                                           newDuration;

    gSdaaInternalDB.maxTHForBlockChanInRat = (uint32)(
                                             (gSdaaInternalDB.localTxUsageTresh) * (0.01) *
                                              newDuration);
    osal_memset((uint16*) gSdaaInternalDB.txUsageTbl, 0, sizeof(gSdaaInternalDB.txUsageTbl));
}

//*****************************************************************************
//! @fn    llsdaaSetChannelState
//
//! @brief This is used to set the channel state in sdaa DB.
//
//! @param channel - channel which handle
//!        state   - SDAA_CH_CLEAR
//!                  SDAA_CH_OVERLOAD
//!                  SDAA_CH_BLOCK
//!                  SDAA_CH_INVALID
//
//*****************************************************************************
static void llsdaaSetChannelState(uint8 channel, sdaaChState_e state)
{
    uint8 channelStateIdx = 0;  //The byte idx of channel state into the
                                //channel state DB

    uint8 stateLSBIdx = 0 ;     //The LSB of the channel state in the
                                //channelStateByte

    //Verify input parameters are valid
    if(channel >= LL_TOTAL_NUM_RF_CHAN)
    {
        return;
    }

    channelStateIdx = channel / CHANNELS_STATE_PER_BYTES;
    stateLSBIdx = (channel % CHANNELS_STATE_PER_BYTES)
                    * BITS_PER_CHANNEL_STATE ;

    //Set zero's in channel state
    gSdaaInternalDB.channelState[channelStateIdx] &=
                            ~(CHANNEL_STATE_MASK << stateLSBIdx);

    //Set one's in channel state
    gSdaaInternalDB.channelState[channelStateIdx] |=
                            (((state) & CHANNEL_STATE_MASK) << stateLSBIdx);
}

//*****************************************************************************
//! @fn    llsdaaCalculateMinDwellTime
//
//! @brief This function takes the minimum average dwelltime on all the
//         active task
//
//! @return chosenMinDwT - minimum average dwelltime
//
//*****************************************************************************
static uint32 llsdaaGetMinDwellTime()
{
    uint32 chosenMinDwT = 0xFFFFFFFF;
    uint32 currMinDwt = 0;

    //Iterate on all task and connection and find the minimum dwell time
    for (uint8 task = 0; task < gSdaaInternalDB.maxNumTasksAndConns; task++)
    {
        currMinDwt = llsdaaGetAvgDwellTimeInTask(task);

        if (currMinDwt > 0 && currMinDwt < chosenMinDwT)
        {
            chosenMinDwT = currMinDwt;
        }
    }
    if(chosenMinDwT != 0xFFFFFFFF &&
       chosenMinDwT * DWT_TO_OBS_PERIOD > MIN_OBS_PERIOD)
    {
        chosenMinDwT = chosenMinDwT*DWT_TO_OBS_PERIOD;
    }
    else
    {
        chosenMinDwT =  gSdaaUsrParams->observationtime * RAT_TICKS_IN_100MS;
    }

    return chosenMinDwT;
}

//*****************************************************************************
//! @fn    llsdaaGetAvgDwellTimeInTask
//
//! @brief This function
//
//! @param task -
//
//! @return avgDwellTime - average dwell time of input task
//*****************************************************************************
static uint32 llsdaaGetAvgDwellTimeInTask(uint8 task)
{
    uint32 avgDwellTime = gSdaaInternalDB.dwellTimeAcc[task].sumOfDWT
                         /gSdaaInternalDB.dwellTimeAcc[task].numOfDWT;

    return avgDwellTime * RAT_TICKS_IN_64US;
}

//***********************SDAA scheduler*********************************//
/*******************************************************************************
 * @fn          llHandleSDAAControlTX
 *
 * @brief       This function used to handle and control SDAA (Selective Detect
 *              And Avoid) in the scheduler.
 *              The function will determine whether to schedule the
 *              primary/secondary task, opening RX window for overload channel,
 *              TX limitation the task with block channel.
 *
 * input parameters
 *
 * @param   nextConnPtr            - ptr to the next connection.
 * @param   secTask                - ptr to the next secondary task.
 * @param   startType              - the current start type of the task type.
 *
 * @return  the task type of the next task
 *          LL_SDAA_SCHED_HANDLED  - when RX window scheduled.
 *          LL_SCHED_START_IMMED   - schedule secondary task immediately.
 *          LL_SCHED_START_EVENT   - schedule secondary task at start time of
 *                                   the task.
 *          LL_SCHED_START_PRIMARY - schedule primary task.
 */
uint8 llHandleSDAAControlTX(llConnState_t   *nextConnPtr,
                            taskInfo_t      *secTask,
                            uint8           startTaskType)
{
    sdaaTaskParams_t taskParams = {0};             // task parameters of the
                                                   // primary and secondary task
    uint8  schedState = LL_SDAA_INIT_STATE;        // bitmap of the task states
    uint8  operation = LL_SDAA_SCHEDULER;          // the operation to perform
                                                   // to handle SDAA
    uint8  channel = LL_SDAA_NONE_ACTIVE_CHANNEL;  // the channel of RX window
                                                   // task.
    uint8  newStartType = startTaskType;           // the new start type
                                                   // of the next type.
    uint32 startTime  = 0;                         // the start time of RX
                                                   // window task.

    // get the bitmap of the task state.
    schedState = llSDAAGetTaskState(nextConnPtr,
                                    secTask,
                                    &taskParams,
                                    startTaskType);

    // find match in the decision table and get the operation to perform
    // according to the schedState bitMap
    operation = LL_SDAA_getDecision(schedState);

    // perform the operation
    switch (operation)
    {
        case LL_SDAA_SCHEDULER:
        {
            // the scheduller will continue without change, so the
            // newStertType will be equal to the initial atrtType.
            newStartType = startTaskType;
            break;
        }

        case LL_SDAA_SCHEDULER_UPDATE_TASK_TYPE_TO_PRIM:
        {
            // the scheduler need to change the next task type by
            // update the start type to primary
            // before the scheduling task.
            newStartType = LL_SCHED_START_PRIMARY;
            break;
        }

        case LL_SDAA_SCHEDULER_UPDATE_SECONDARY_TASK_TYPE:
        {
            // the scheduler need to change the next task type by
            // update the start type of the
            // secondary task type
            // (LL_SCHED_START_IMMED, LL_SCHED_START_EVENT)
            // before the scheduling task.
            newStartType = taskParams.newStartType;
            break;
        }

        case LL_SDAA_SCHED_RX_WIN_PRIM:
        case LL_SDAA_SCHED_RX_WIN_SECONDARY:
        {
            // get the channel and the start time of the RX
            // window according to the task type (prim/sec)
            if ( operation == LL_SDAA_SCHED_RX_WIN_PRIM)
            {
                channel = taskParams.primChan;
                startTime = taskParams.startTimePrim;
            }
            else
            {
                channel = taskParams.secChan;
                startTime = taskParams.startTimeSec;
            }

            // config the RX window command time and channel
            llSDAAConfigRXWindow(startTime,
                                 channel,
                                 TRUE);

            // update the channel in sample.
            MAP_LL_SDAA_SetChannelInSample(channel);

            // schedule the RX window task.
            llSDAASchdRxWindow();

            // the RX window scheduled so the shceduler handled
            // already (no need to schedule another task)
            newStartType = LL_SDAA_SCHED_HANDLED;
            break;
        }

        case LL_SDAA_SCHED_LIMIT_PRIM:
        {
            // limit the primary task by unlink the TX queue of
            // the connection ptr.
            if (nextConnPtr && nextConnPtr->llTask && nextConnPtr->llTask->command
                    && ((ble5OpCmd_t*)(nextConnPtr->llTask->command))->pParams)
            {
                linkParam_t *pParam = (linkParam_t *)(((ble5OpCmd_t*)
                                            (nextConnPtr->llTask->command))->pParams);
                pParam->pTXQ = NULL;
            }

            // the next task will be primary task,
            // empty packets will be send on the channel.
            newStartType = LL_SCHED_START_PRIMARY;
            break;
        }

        case LL_SDAA_SCHED_RX_WIN_UNBLOCKED_CHANNEL:
        {
            uint8 i = 0;
            uint8 randChannel = 0;

            // start from random channel
            MAP_LL_Rand(&randChannel, LL_TOTAL_NUM_RF_CHAN / BITS_PER_BYTE);

            randChannel = randChannel % LL_MAX_NUM_DATA_CHAN;

            for (i = 0; i < LL_MAX_NUM_DATA_CHAN; i++)
            {
                channel = (randChannel + i) % LL_MAX_NUM_DATA_CHAN;
                if (LL_SDAA_GetChannelState(channel) != SDAA_CH_BLOCK)
                {
                    break;
                }
            }

            // set the task start time before the block state timeout
            // take a snapshot of the current time
            // Note: Add one tick of pad.
            uint32 curTime = MAP_llGetCurrentTime() + RAT_TICKS_IN_625US;

            // calculate duration for RX window task
            // the duration includes the delay of frequency synthesizer +
            // overhead
            uint32 rxWindowDuration = MAP_LL_SDAA_GetRXWindowDuration()
                               + RAT_TICKS_IN_166US * 2;

            // the RX window command should start before the nextTask
            startTime = LL_SDAA_GetTimeOutOfBlockedState(taskParams.secChan) -
                        rxWindowDuration - LL_SCHED_OVERHEAD;

            // check if the task time already expired, if so start the task immediately.
            // task's cutoff, but check if there's enough time relative to current time
            // Note: This is needed so we don't try to start the rxWinCmd task at a
            //       time that may have already expired. That is, it is possible that
            //       the secondary task may have not been scheduled because of a
            //       conflict with the primary task. In this case, the rxWinCmd task's
            //       start time is long since expired.
            if ( MAP_llTimeCompare( startTime,
                                    curTime + LL_SCHED_PRE_CUTOFF - LL_SCHED_START_IMMED_PAD ) == FALSE )
            {
              // the next task is either in the past, or not far enough into the
              // future, so start it immediately
              // base the start time on the current time
                startTime = MAP_llGetCurrentTime() + LL_SCHED_START_IMMED_PAD;
            }

            // config the RX window command time and channel
            llSDAAConfigRXWindow(startTime,
                                 channel,
                                 TRUE);

            // update the channel in sample.
            MAP_LL_SDAA_SetChannelInSample(channel);

            // schedule the RX window task.
            llSDAASchdRxWindow();

            // the RX window scheduled so the shceduler handled
            // already (no need to schedule another task)
            newStartType = LL_SDAA_SCHED_HANDLED;

            break;
        }

        case LL_SDAA_SCHEDULER_ERROR:
            // Sanity Check:
            // Fatal error - Not possible to have no active tasks in the
            // scheduler at this point.
        default:
        {
            // Sanity Check:
            // Fatal error - Not possible to have no operation.
            // because all the cases are covered in the decision table
            LL_ASSERT( FALSE );
            break;
        }
    }

    return newStartType;
}

/*******************************************************************************
 * @fn          LL_SDAA_getDecision
 *
 * @brief       This function used to find the next operation according to the
 *              decision table and the bitMap scheduler task state.
 *              The function iterate over the entries in the decision table
 *              until match will be found.
 *              The next operation will be determined according to the operation
 *              of the decision table entry.
 *
 * input parameters
 *
 * @param       schedState - bitMap of the scheduler task states.
 *
 *
 * @return      the next operation according to SDAA decision table.
 */
uint8 LL_SDAA_getDecision(uint8 schedState)
{
    uint8   value = 0;          // the results of masking of schedState bitMap.

    // init the next operation value to error, since it's impossible to
    // not find a match in the decisio table because all the cases are
    // covered.
    uint8 nextOperation = LL_SDAA_SCHEDULER_ERROR;

    // iterate over the decision table until match will be found
    for ( uint8 i=0 ; i < LL_SDAA_DECISION_TABLE_SIZE ; i++ )
    {
        // calculate value by masking the schedState bitMap with the
        // appropriate mask entry
        value = schedState & decisionTable[i].mask;

        // check if there is a match between the value of the decision
        // table and the value calculated as a results of masking.
        if (value == decisionTable[i].value)
        {
            // match found. so uodate the nextOperation according to
            // appropriate entry and stop itereting.
            nextOperation = decisionTable[i].operation;
            break;
        }
    }

    return nextOperation;
}

/*********************************************************************
 * @fn      llSDAAGetTaskState
 *
 * @brief   This function used to fill the bitMap of the scheduler
 *          state by set flags about the channel state of primary and
 *          the secondary task. the flags indicated the availability
 *          of the channel, necessarity of RX window, and the ability
 *          to schedule the RX window.
 *
 * input parameters
 *
 * @param   nextConnPtr    - ptr to the next connection.
 * @param   secTask        - ptr to the next secondary task.
 * @param   pTaskParams    - ptr of the task parameters (start time,
 *                           channels, new start type of the next task).
 * @param   startType      - the current start type of the task type.
 *
 * output parameters
 *
 * @param   None
 *
 * return   sdaaSchedState - the scheduler state which indicates the
 *                           necessarity and ability of schedule RX
 *                           window before the primarry/secondary
 *                           tasks.
 */
uint8 llSDAAGetTaskState(llConnState_t    *nextConnPtr,
                         taskInfo_t       *secTask,
                         sdaaTaskParams_t *pTaskParams,
                         uint8            startType)
{
    taskInfo_t          *primTask = NULL;
    uint8               sdaaSchedState = LL_SDAA_INIT_STATE;
    uint8               channelState = SDAA_CH_INVALID;

    // init secondary channel to LL_SDAA_NONE_ACTIVE_CHANNEL.
    // the channel will be set only when the secondary task is
    // periodic / extended.
    pTaskParams->secChan = LL_SDAA_NONE_ACTIVE_CHANNEL;

    // if there is an active connection, check if the next channel
    // is overload and update rxWindow task command
    if(nextConnPtr != NULL)
    {
        sdaaSchedState |= LL_SDAA_PRIMARY_EXIST;
        primTask = nextConnPtr->llTask;
        pTaskParams->primChan = nextConnPtr->nextChan;
        // get channel state
        channelState = LL_SDAA_GetChannelState(pTaskParams->primChan);

        // check if RX window is necessary and allowed for the primary task.
        if (channelState == SDAA_CH_OVERLOAD)
        {
            // the channel is overload, so RX window is necessary.
            sdaaSchedState |= LL_SDAA_PRIMARY_RX_WIN_NECESSARY;

            // check if there is enough time to schedule RX window before the primary task.
            if (llSDAASufficientTimeRXWindow(primTask, &pTaskParams->startTimePrim) == TRUE)
            {
                sdaaSchedState |= LL_SDAA_PRIMARY_RX_WINDOW_ALLOWED;

                // config start time and end time for RX window task,
                // to allow the scheduler state calculator to determine if there is sufficient
                // time to insert tasks before the RX window task.
                // the channel input is invalid so the sample process will not start
                llSDAAConfigRXWindow(pTaskParams->startTimePrim,
                                     LL_SDAA_NONE_ACTIVE_CHANNEL,
                                     FALSE);
            }
        } // end primary in overload channel state

        // check if the channel is blocked
        else if (channelState == SDAA_CH_BLOCK)
        {
            sdaaSchedState |= LL_SDAA_PRIMARY_BLOCK_CHANNEL;
        }
    }

    // check if the secondary task is exist, and allowed.
    if ( (startType != LL_SCHED_START_PRIMARY) && (secTask != NULL))
    {
        // the secondary task is exist. Now, verify that the task is allowed.
        // the secondary task is allowed in one of this two options
        // the first is when the primary task there isn't RX window
        // the second is when the priary task there is a RX window,
        // but still, there is sufficient time to schedule the
        // secondary task before the RX window.
        // comment - if the newStartType is primary, then there isn't sufficient
        //           time to schedule the secondary task before the RX window.
        if (sdaaSchedState & LL_SDAA_PRIMARY_RX_WINDOW_ALLOWED)
        {
            // The primary task has RX window, so it's necessary to check if the
            // secondary task have sufficient time before the RX window.
            // save pTaskParams->newStartType in case there is a need to update the
            // start type (for example, LL_SCHED_START_EVENT -> LL_SCHED_START_IMMED)
            pTaskParams->newStartType = MAP_llFindStartType(secTask, pRXWindowTask);
        }

        // check if the secondary task is allowed.
        if ( ((sdaaSchedState & LL_SDAA_PRIMARY_RX_WINDOW_ALLOWED) &&
              (pTaskParams->newStartType != LL_SCHED_START_PRIMARY)) ||
             (!(sdaaSchedState & LL_SDAA_PRIMARY_RX_WINDOW_ALLOWED)))
        {
            // set the flag exist and allowed for the secondary task.
            sdaaSchedState |= LL_SDAA_SECONDARY_EXIST_AND_ALLOWED;
        }
    }

    // initial the channel state to clear.
    // the channel state will be update in case the channel of the secondary task is data
    // channel.
    channelState = SDAA_CH_CLEAR;

    // check if the secondary task is exist and allowed
    if (sdaaSchedState & LL_SDAA_SECONDARY_EXIST_AND_ALLOWED)
    {
        // get channel according to the task
        if(secTask->taskID == LL_TASK_ID_PERIODIC_ADVERTISER)
        {
            // periodic task
#ifdef USE_PERIODIC_ADV
            llPeriodicAdvSet_t *pPeriodicAdv = MAP_llGetCurrentPeriodicAdv();
            pTaskParams->secChan = pPeriodicAdv->currentChan;
#endif
        }
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
        else if((secTask->taskID == LL_TASK_ID_ADVERTISER) &&
           (TST_AE_PROPS_LEGACY(pNextAdvSet->AdvEntry->pAdvParam->eventProps) == FALSE))
        {
            // extended task
            pTaskParams->secChan = pNextAdvSet->AdvEntry->auxChanIndex;
        }
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

        // get channel state
        if (pTaskParams->secChan != LL_SDAA_NONE_ACTIVE_CHANNEL)
        {
            channelState = LL_SDAA_GetChannelState(pTaskParams->secChan);
        }

        // check if RX window is necessary and allowed for the secondary task.
        if (channelState == SDAA_CH_OVERLOAD)
        {
            // the channel is overload, so RX window is necessary.
            sdaaSchedState |= LL_SDAA_SECONDARY_RX_WIN_NECESSARY;

            // check if there is sufficient time to schedule RX window for secondary task
            // before secondary task
            if ((llSDAASufficientTimeRXWindow(secTask, &pTaskParams->startTimeSec) == TRUE))
            {
                sdaaSchedState |= LL_SDAA_SECONDARY_RX_WINDOW_ALLOWED;
            }
        }

        // check if the channel is blocked
        else if (channelState == SDAA_CH_BLOCK)
        {
            sdaaSchedState |= LL_SDAA_SECONDARY_BLOCK_CHANNEL;
        }
    }

    return sdaaSchedState;
}

/*********************************************************************
 * @fn      llSDAAConfigRXWindow
 *
 * @brief   This function used to configurate paramaters of RX window
 *          command. The function set start time and end time of the
 *          command, and start the frequency synthisizer on the
 *          channel using the CMD_FS chainned before the RX window
 *          command.
 *
 * input parameters
 *
 * @param   startTime        - the start time of the first command in
 *                             the chain of RX window task.
 * @param   channel          - BLE channel 0,1,..39.
 * @param   setRfChannel     - boolean flag indicates whether to set
 *                             channel in frequency synthisizer command
 *
 *
 * output parameters
 *
 * @param   None
 *
 * @return  None
 */
void llSDAAConfigRXWindow(uint32 startTime, uint8 channel, uint8 setRfChannel)
{
    rfOpCmd_freqSynthCtrl_t *fsRfCmd =
                  ((rfOpCmd_freqSynthCtrl_t *)pRXWindowTask->command);
    rfOpCmd_RxTest_t *rxWinCmd =
                  ((rfOpCmd_RxTest_t *)fsRfCmd->rfOpCmd.pNextRfOp);


    // set start and end time to the command
    // the end time set to the end of RX window and includes the
    // frequency synthesizer delay + overhead
    fsRfCmd->rfOpCmd.startTime = startTime;
    rxWinCmd->endTime = startTime + MAP_LL_SDAA_GetRXWindowDuration() +
                        RAT_TICKS_IN_166US * 2;

    // set frequency channel if the setRfChannel is TRUE
    // verify that the channel is valid
    if ((setRfChannel == TRUE) &&
            channel < LL_TOTAL_NUM_RF_CHAN)
    {
        // set frequency into frequency synthisizer command
        fsRfCmd->freq = llBleToRfChannel(channel);
    }
}

/*********************************************************************
 * @fn      llSDAASchdRxWindow
 *
 * @brief   This function used to schedule the RX window task
 *          and setup RatCompare function in order to indicate that
 *          the RF command started.
 *
 * input parameters
 *
 * @param   None
 *
 * output parameters
 *
 * @param   None
 *
 * @return  None
 */
void llSDAASchdRxWindow(void)
{
   llState = LL_STATE_SDAA_RX_WINDOW;

   // schedule the RX window Task and save the rfCmdHandle to
   // handle with RXCmdDone event.
   MAP_llScheduleTask(pRXWindowTask);

   // set RatCompare callback when the RF command will start,
   // and save the rfRatHandle to handle with RXCmdDone event
   llSetupRatCompare(pRXWindowTask);
}

/*********************************************************************
 * @fn      llSDAASufficientTimeRXWindow
 *
 * @brief   This function used to check if there is sufficient time to
 *          schedule RX window before the next task.
 *          when there is sufficient time for the schedule RX window,
 *          The function updates the pointers of start and end time.
 *
 * input parameters
 *
 * @param   nextTask         - ptr to the next task.
 *
 * output parameters
 *
 * @param   pStartTime       - ptr to the start time of RX window cmd
 *                             (in case there is sufficient time to
 *                             schedule The RX window).
 *
 * @return TRUE              - if there is sufficient time to open RX
 *                             window.
 *         FALSE             - if there is not sufficient time to open
 *                             RX window.
 */
uint8 llSDAASufficientTimeRXWindow(taskInfo_t *nextTask, uint32 *pStartTime)
{
    uint16 rxWindowDuration;
    uint32 timeGap;
    uint32 curTime;
    uint32 rxWinCmdStartTime;

    // get cmd ptr for primary cmd and RX window cmd .
#if defined(CTRL_V50_CONFIG) && (CTRL_V50_CONFIG & (PHY_2MBPS_CFG | PHY_LR_CFG))
    ble5OpCmd_t *nextCmd = ((ble5OpCmd_t*) nextTask->command);
#else // !PHY_2MBPS_CFG & !PHY_LR_CFG
    bleOpCmd_t *nextCmd  = ((bleOpCmd_t *)nextTask->command);
#endif // PHY_2MBPS_CFG | PHY_LR_CFG

    // take a snapshot of the current time
    // Note: Add one tick of pad.
    curTime = MAP_llGetCurrentTime() + RAT_TICKS_IN_625US;

    // calculate timeGap for RX window task
    // the duration includes the delay of frequency synthesizer +
    // overhead
    rxWindowDuration = MAP_LL_SDAA_GetRXWindowDuration()
                       + RAT_TICKS_IN_166US * 2;
    timeGap = MAX( rxWindowDuration, LL_SCHED_OVERHEAD );

    // the RX window command should start before the nextTask
    rxWinCmdStartTime = nextCmd->rfOpCmd.startTime -
                        rxWindowDuration - LL_SCHED_OVERHEAD;

    // check if there's enough time for the RX window task to run
    // Note: While it is assumed the primary task's start time is before the
    //       current time (otherwise the task would hang), we still have to handle
    //       counter wrap.
    if ( ((MAP_llTimeDelta( nextCmd->rfOpCmd.startTime, curTime ) > timeGap) &&
         (MAP_llTimeCompare( rxWinCmdStartTime, nextCmd->rfOpCmd.startTime - timeGap ) == FALSE)) )
    {
      // the rxWinCmd task has enough time to start relative to the primary
      // task's cutoff, but check if there's enough time relative to current time
      // Note: This is needed so we don't try to start the rxWinCmd task at a
      //       time that may have already expired. That is, it is possible that
      //       the secondary task may have not been scheduled because of a
      //       conflict with the primary task. In this case, the rxWinCmd task's
      //       start time is long since expired.
      if ( MAP_llTimeCompare( rxWinCmdStartTime,
                              curTime + LL_SCHED_PRE_CUTOFF - LL_SCHED_START_IMMED_PAD ) == FALSE )
      {
        // the next task is either in the past, or not far enough into the
        // future, so start it immediately
        // base the start time on the current time
        *pStartTime = MAP_llGetCurrentTime() + LL_SCHED_START_IMMED_PAD;
      }
      else // the next task's start time is far enough into the future
      {
        // so there's enough time to start it based on its own interval
        // update the start time command before the primary task.
        *pStartTime = rxWinCmdStartTime;
      }

      // there is enough time to schedule the RX command
      return TRUE;
    }
    // the rxWinCmd task hasn't enough time to start relative to the next Task
    return FALSE;
}
#endif //USE_RCL
#endif //SDAA_ENABLE
