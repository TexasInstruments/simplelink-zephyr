/******************************************************************************
 Group: CMCU LPRF
 Target Device: cc13xx_cc26xx

 ******************************************************************************
 
 Copyright (c) 2023-2026, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************
 
 
 *****************************************************************************/

#ifndef TRX_HOST_H_
#define TRX_HOST_H_

#ifdef __cplusplus
extern "C" {
#endif

/********************************** Includes **********************************/

/* Standard C Libraries */
#include <stdint.h>
#include <stdbool.h>

/* Module Header Files */
#include "TRX.h"

//*****************************************************************************
//
//! \addtogroup TRX_HOST_API
//! \brief Public TRX Host Driver APIs
//!
//! @details The TRX Host Driver provides public APIs for the host to control
//! radio activity on the transceiver (TRX) as well as utility APIs for operations
//! including time synchronization and stopping commands.
//!
//! To use the TRX Host Driver, include the header file as follows:
//! @code
//! #include "trx_host.h"
//! @endcode
//! @{
//
//*****************************************************************************

/********************************** Defines ***********************************/

/** @anchor TRX_RESERVED_ID
 *  @name ID Reserved for Host driver
 *  @{
 */
/** Slot, stream, or config id reserved for host driver use */
#define TRX_RESERVED_ID (0xFF)
/** @} */

/** @anchor TRX_MAX_NUM_COMMANDS
 *  @name Maximum Number of RF Commands
 *  @{
 */
/** The maximum number of command slots available on the TRX */
#define TRX_MAX_NUM_COMMANDS    (6U)
/** @} */

/** @anchor TRX_MAX_CONFIGURATIONS
 *  @name Maximum Number of Configurations
 *  @{
 */
/** The Maximum Number of configurations available on TRX */
#define TRX_MAX_CONFIGURATIONS    (15U)
/** @} */

/** @anchor TRX_MAX_CONFIGURATIONS
 *  @name Maximum Number of Configurations
 *  @{
 */
/** The Maximum Number of configurations available on TRX */
#define TRX_MAX_STREAMS           (15U)
/** @} */

/** @anchor TRX_POWER
 *  @name Special power values for max and min transmit power
 *  @{
 */
/** The Maximum power for a transmit operation on TRX */
#define TRX_MAX_POWER    (125)
/** The minimum power for a transmit operation on TRX */
#define TRX_MIN_POWER    (-128)
/** @} */

/** @anchor TRX_EVENTS
 *  @name TRX Radio and Host Driver Events
 *  @{
 */
/** Unassigned Event */
#define TRX_EventUnassigned               (0)
/** A TRX_Request_TransportError was received from the TRX */
#define TRX_EventTransportError           (1 << 0)
/** A TRX_Request_LastStatus was received from the TRX with an error status */
#define TRX_EventLastStatusError          (1 << 1)
/** A TRX_Request_CommandStatus was received from the TRX */
#define TRX_EventCmdStatus                (1 << 2)
/** A TRX_Request_CommandStatus was received from the TRX with chain_action=0 indicating the last command in a chain */
#define TRX_EventFinalCmdStatus           (1 << 3)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_CommandStop sent to the TRX  */
#define TRX_EventStopStatus               (1 << 4)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_CommandStore sent to the TRX */
#define TRX_EventCmdStoreComplete         (1 << 5)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_CommandSubmit sent to the TRX */
#define TRX_EventCmdSubmitComplete        (1 << 6)
/** A TRX_Request_StreamStore was received from the TRX  */
#define TRX_EventStreamStoreReceived      (1 << 7)
/** A TRX_Request_StreamStore with the more data bit set to 0 was received from the TRX  */
#define TRX_EventFinalStreamStoreReceived (1 << 8)
/** A TRX_Request_StreamStore was received from the TRX but there was not enough space in the receive buffer */
#define TRX_EventStreamStoreOverflow      (1 << 9)
/** A TRX_Request_StreamFlush was received from the TRX invalidating RX data sent to the Host due to a CRC failure on final reception */
#define TRX_EventStreamFlushReceived      (1 << 10)
/** The final TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_StreamStore sent to the TRX  */
#define TRX_EventStreamStoreComplete      (1 << 11)
/** The final TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_ConfigStore sent to the TRX  */
#define TRX_EventConfigStoreComplete      (1 << 12)
/** The final TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_ConfigPersist sent to the TRX  */
#define TRX_EventConfigPersistComplete    (1 << 13)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_Ping sent to the TRX  */
#define TRX_EventPingComplete             (1 << 14)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_UtilGetRssi sent to the TRX  */
#define TRX_EventGetRSSIComplete          (1 << 15)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_UtilInfoGetVersion sent to the TRX  */
#define TRX_EventGetVersionComplete       (1 << 16)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_UtilShutdown sent to the TRX  */
#define TRX_EventShutdownComplete         (1 << 17)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_UtilInfoGetMacUuid sent to the TRX  */
#define TRX_EventGetMacUuidComplete       (1 << 18)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_UtilInfoGetDevice sent to the TRX  */
#define TRX_EventGetDeviceInfoComplete    (1 << 19)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_UtilEnterSsbl sent to the TRX  */
#define TRX_EventEnterSsblComplete        (1 << 20)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_UtilPowerMode sent to the TRX  */
#define TRX_EventPowerModeComplete        (1 << 21)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_UtilGetTime sent to the TRX  */
#define TRX_EventReceivedTrxTime          (1 << 22)
/** Event indicating the application should capture its current time that can be associated with the time returned from the TRX on ::TRX_EventReceivedTrxTime */
#define TRX_EventCaptureHostTime          (1 << 23)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_NvAllocate sent to the TRX  */
#define TRX_EventNvAllocateComplete       (1 << 24)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_NvInvalidate sent to the TRX  */
#define TRX_EventNvInvalidateComplete     (1 << 25)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_NvInfo sent to the TRX  */
#define TRX_EventNvInfoComplete           (1 << 26)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_NvErase sent to the TRX  */
#define TRX_EventNvEraseComplete          (1 << 27)
/** Notice from the TRX that the NV consistency check has failed, need to erase NV  */
#define TRX_EventNvCorrupt                (1 << 28)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_UtilRfMode sent to the TRX  */
#define TRX_EventSetRfModeComplete        (1 << 29)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Host_utilDioSetup sent to the TRX  */
#define TRX_EventDioSetupComplete         (1 << 30)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_DeviceConfigStore sent to the TRX  */
#define TRX_EventDeviceConfigStoreComplete   (1ULL << 31)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_DeviceConfigPersist sent to the TRX  */
#define TRX_EventDeviceConfigPersistComplete (1ULL << 32)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_DeviceConfigGet sent to the TRX  */
#define TRX_EventDeviceConfigGetComplete     (1ULL << 33)
/** A TRX_Request_LastStatus was received from the TRX regarding a TRX_Request_StreamFlush sent to the TRX  */
#define TRX_EventStreamFlushComplete         (1ULL << 34)

/** @} */

/*********************************** Enums ************************************/

/*!
 *  @brief Statuses that can be returned from TRX Host APIs
 */
typedef enum
{
    TRX_Host_Success  = 0U,       //!< Function returned successfully
    TRX_Host_Invalid_Handle = 1U, //!< Invalid handle passed to API
    TRX_Host_No_Resources = 2U,   //!< Error when allocating buffers for SPI
                                  //!< transmission or not enough RAM space on the TRX
    TRX_Host_Invalid_Cmd = 3U,    //!< Invalid cmd provided to API
    TRX_Host_Invalid_Id = 4U,     //!< Invalid stream of config identifier provided to API
    TRX_Host_Config_Exists = 5U,  //!< A configuration with the same ID already exists in the TRX persistent storage
    TRX_Host_Invalid_Param = 6U,  //!< Invalid parameter provided to API
} TRX_Host_Status;

/********************************** Typedefs **********************************/

/** Data type for the handle to the TRX Host driver object
 *
 *  @sa TRX_Host_init()
 */
typedef struct TRX_Host_Object_t *TRX_Host_Handle;

/**
 * @brief Callback provided from the TRX Host driver to the application.
 *
 * @note Calling context: HWI, SWI
 *
 * @param[in] handle  Handle to the TRX Host driver
 * @param[in] pCmdStreamOrConfig  Pointer to a TRX_Request_CommandStore or
 *                                data buffer associated with the events provided
 *                                to this callback
 * @param[in] request  Pointer to the request received from the TRX
 * @param[in] events  The events observed from the TRX
 * @param[in] arg  Optional arg passed to the TRX Host driver when opened
 */
typedef void (*TRX_Host_Callback)(TRX_Host_Handle handle,
                                  uintptr_t pCmdStreamOrConfig,
                                  TRX_Request *request,
                                  uint64_t events, uintptr_t arg);

/********************************** Structs ***********************************/

/** Parameter structure used in the initialization of the TRX Host driver */
typedef struct TRX_Host_Params_t
{
    /** SPI data rate in Hz */
    uint32_t bitRate;
    /** Callback for requests received from the TRX that don't map to a user
     * initiated operation including the following events:
     * -# ::TRX_EventTransportError
     * -# ::TRX_EventNvCorrupt
     */
    TRX_Host_Callback generalCb;
    /** Optional argument to pass to all callbacks from the driver */
    uintptr_t arg;
} TRX_Host_Params;

/***************************** Extern Prototypes ******************************/

/**
 * @brief Initialize and open the TRX Host driver
 *
 * @note Calling context: Task
 *
 * @param pParams Parameters for initializing the TRX Host driver
 * @return TRX_Host_Handle, NULL when a failure occurs during opening
 */
extern TRX_Host_Handle TRX_Host_open(TRX_Host_Params *pParams);

/**
 * @brief Close the TRX Host driver
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 */
extern void TRX_Host_close(TRX_Host_Handle handle);

/**
 * @brief Register a stream for reception of data from the TRX via a receive
 * cmd. The following events can be subscribed to for a receive stream:
 * -# ::TRX_EventStreamStoreReceived
 * -# ::TRX_EventFinalStreamStoreReceived
 * In addition streams are automatically subscribed to the following events:
 * -# ::TRX_EventLastStatusError
 * -# ::TRX_EventStreamStoreOverflow
 * -# ::TRX_EventStreamFlushReceived
 *
 * @note Calling context: Task, HWI, SWI
 *
 * @param handle Handle to the TRX Host driver
 * @param id The unique identifier of the stream range [0, TRX_MAX_STREAMS)
 * @param pData Pointer to a buffer to store the received data in
 * @param len The max size of data that can fit in the buffer
 * @param callback The callback to execute any time a subscribed event is observed
 * @param subscribedEvents The events this stream is subscribed to
 * @return TRX_Host_Status indicating success or failure of registration
 */
extern TRX_Host_Status TRX_Host_registerRxStream(TRX_Host_Handle handle, uint8_t id,
                                                 uint8_t *pData, uint16_t len,
                                                 TRX_Host_Callback callback,
                                                 uint64_t subscribedEvents);

/**
 * @brief Store a configuration on the TRX via one or more
 * ::TRX_Request_ConfigStore requests. A success status returned from this
 * function does not indicate the configuration has successfully been stored on
 * the TRX. The following events can be subscribed to for a configuration:
 * -# ::TRX_EventConfigStoreComplete
 * -# ::TRX_EventConfigPersistComplete
 * In addition configurations are automatically subscribed to the following events:
 * -# ::TRX_EventLastStatusError
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param id The unique identifier of the config within range [0, TRX_MAX_CONFIGURATIONS)
 * @param pData Pointer to a buffer containing the configuration data
 * @param len The length of the data
 * @param reference The unique reference for this config on the TRX. This value
 *                  can arbitrarily be chosen by the user and is used in
 *                  combination with the id to uniquely identify configurations
 *                  on the TRX. If a subsequent call to this API occurs with
 *                  the same id/reference pair, the API will return a
 *                  ::TRX_Host_Config_Exists status.
 * @param force Store the config on the TRX even if there's already a config with
 *              a matching id and reference existing on the TRX. This invalidates
 *              the existing config.
 * @param callback The callback to execute any time a subscribed event is observed
 * @param subscribedEvents The events this configuration is subscribed to
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_ConfigStore requests
 */
extern TRX_Host_Status TRX_Host_storeConfig(TRX_Host_Handle handle, uint8_t id,
                                            uint8_t *pData, uint16_t len,
                                            uint32_t reference, bool force,
                                            TRX_Host_Callback callback,
                                            uint64_t subscribedEvents);

/**
 * @brief Store a stream on the TRX via one or more
 * ::TRX_Request_StreamStore requests. A success status returned from this
 * function does not indicate the stream has successfully been stored on
 * the TRX. The ::TRX_EventStreamStoreComplete can be subscribed to as an
 * indication that the TRX received all the stream data. Streams are
 * automatically subscribed to the following events:
 * -# ::TRX_EventLastStatusError
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param id The unique identifier of the config within range [0, TRX_MAX_STREAMS)
 * @param pData Pointer to a buffer containing the stream data
 * @param len The length of the data
 * @param retention The retention policy for this stream on the TRX. For large
 *                  payloads (greater than 500 bytes) it is recommended to use a
 *                  ::Stream_Retention_Flush_Streaming policy to avoid
 *                  consuming too much RAM on the TRX.
 * @param callback The callback to execute any time a subscribed event is observed
 * @param subscribedEvents The events this stream is subscribed to
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_StreamStore requests
 */
extern TRX_Host_Status TRX_Host_storeStream(TRX_Host_Handle handle, uint8_t id,
                                            uint8_t *pData, uint16_t len,
                                            TRX_Stream_Retention retention,
                                            TRX_Host_Callback callback,
                                            uint64_t subscribedEvents);

/**
 * @brief Store continuation of a continuous stream on the TRX via one or more
 * ::TRX_Request_StreamStore requests. The stream should first be started with 
 * ::TRX_Host_storeStream where retention is ::Stream_Retention_Flush_Streaming_Continuous.
 * A success status returned from this function does not indicate the stream has 
 * successfully been stored on the TRX. The ::TRX_EventStreamStoreComplete can be
 * subscribed to as an indication that the TRX received all the stream data. 
 * If the function returns TRX_Host_No_Resources, the call can be tried again after
 * receiving a ::TRX_EventStreamFlushReceived event. Several such events may be needed
 * before the call is successful.
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param id The unique identifier of the config; must be the same as for ::TRX_Host_storeStream
 * @param pData Pointer to a buffer containing the stream data
 * @param len The length of the data
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_StreamStore requests
 */
extern TRX_Host_Status TRX_Host_continueStream(TRX_Host_Handle handle, uint8_t id,
                                               uint8_t *pData, uint16_t len, bool endStream);

/**
 * @brief Store one or more commands on the TRX via one or more
 * ::TRX_Request_CommandStore requests. A stored command will remain in it's
 * assigned slot on the TRX until a new command is stored to the same slot or
 * the TRX is reset. This enables to same command to be submitted multiple times
 * without needing to store it to the TRX again. A success status returned from
 * this function does not indicate the command has successfully been stored on
 * the TRX. The following events can be subscribed to for a command:
 * -# ::TRX_EventCmdStatus
 * -# ::TRX_EventFinalCmdStatus
 * -# ::TRX_EventCmdStoreComplete
 * -# ::TRX_EventCmdSubmitComplete
 * In addition commands are automatically subscribed to the following events:
 * -# ::TRX_EventLastStatusError
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param pCmdStores Pointer to an array of TRX_Request_CommandStore requests
 * @param numCmds Number of commands in the pCmdStores array
 * @param pCmdToSubmit Optional pointer to the command to submit via a
 *                     ::TRX_Request_CommandSubmit request. NULL if not
 *                     submitting a command
 * @param callback The callback to execute any time a subscribed event is observed
 * @param subscribedEvents The events this command is subscribed to
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_CommandStore requests
 */
extern TRX_Host_Status TRX_Host_storeCmds(TRX_Host_Handle handle,
                                          TRX_Request_CommandStore *pCmdStores,
                                          uint8_t numCmds,
                                          TRX_Request_CommandStore *pCmdToSubmit,
                                          TRX_Host_Callback callback,
                                          uint64_t subscribedEvents);

/**
 * @brief Submit a command to the TRX radio via a ::TRX_Request_CommandSubmit
 * request that was previously stored using the ::TRX_Host_storeCmds() API. A
 * success status returned from this function does not indicate the command was
 * successfully submitted to the radio on the TRX. The
 * ::TRX_EventCmdSubmitComplete event can be subscribed to when storing the
 * command with ::TRX_Host_storeCmds() to receive a notification on command
 * submission.
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param slot Slot the cmd resides in on the TRX in the range [0, TRX_MAX_NUM_COMMANDS)
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_CommandSubmit request
 */
extern TRX_Host_Status TRX_Host_submitCmd(TRX_Host_Handle handle, uint8_t slot);

/**
 * @brief Stop a command on the TRX radio via a ::TRX_Request_CommandStop
 * request. A success status returned from this function does not indicate the
 * command was successfully stopped on the TRX. The
 * ::TRX_EventCmdStatus event can be subscribed to when storing the
 * command with ::TRX_Host_storeCmds() to receive a notification on end of
 * command execution. In addition the callback associated with the command will
 * always execute with the ::TRX_EventStopStatus event to indicate the status
 * of the stop request regardless of success or failure.
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param slot Slot the cmd resides in on the TRX in the range [0, TRX_MAX_NUM_COMMANDS)
 * @param stopType How command execution shall be stopped
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_CommandStop request
 */
extern TRX_Host_Status TRX_Host_stopCmd(TRX_Host_Handle handle, uint8_t slot,
                                        TRX_Command_StopType stopType);

/**
 * @brief Ping the TRX via a ::TRX_Request_Ping request. A success status
 * returned from this function does not indicate the TRX was successfully
 * pinged. The provided callback will be executed with a
 * ::TRX_EventPingComplete event on reception of a ping response from
 * the TRX.
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param pingData Used to customize the TRX response, see ::TRX_Request_Ping
 * @param callback The callback to execute any when the ::TRX_EventPingComplete
 *                 event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_Ping request
 */
extern TRX_Host_Status TRX_Host_ping(TRX_Host_Handle handle, uint8_t pingData,
                                     TRX_Host_Callback callback);

/**
 * @brief Write a configuration to NV on the TRX via a
 * ::TRX_Request_ConfigPersist request. A success status returned from this
 * function does not indicate the configuration was successfully persisted. The
 * ::TRX_EventConfigPersistComplete event can be subscribed to when storing
 * the configuration via the ::TRX_Host_storeConfig() API to receive a
 * notification when the configuration is persisted to NV.
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param id The unique identifier of the config within range [0, TRX_MAX_CONFIGURATIONS)
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_ConfigPersist request
 */
extern TRX_Host_Status TRX_Host_persistConfig(TRX_Host_Handle handle, uint8_t id);

/**
 * @brief Start time synchronization with the TRX via a
 * ::TRX_Request_UtilGetTime request. A success status returned from this
 * function does not indicate successful time sync with the TRX. The provided
 * callback will be executed with the following events:
 * -# ::TRX_EventCaptureHostTime
 * -# ::TRX_EventReceivedTrxTime
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param callback The callback to execute any when the ::TRX_EventCaptureHostTime
 *                 and ::TRX_EventReceivedTrxTime events are observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_UtilGetTime request
 */
extern TRX_Host_Status TRX_Host_startTimeSync(TRX_Host_Handle handle, TRX_Host_Callback callback);

/**
 * @brief Get the current RSSI from the TRX via a ::TRX_Request_UtilGetRssi
 * request. A success status returned from this function does not indicate
 * the RSSI has been retrieved from the TRX. The provided
 * callback will be executed with the following event:
 * -# ::TRX_EventGetRSSIComplete
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param callback The callback to execute any when the
 *                 ::TRX_EventGetRSSIComplete event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_UtilGetRssi request
 */
extern TRX_Host_Status TRX_Host_utilGetRssi(TRX_Host_Handle handle, TRX_Host_Callback callback);

/**
 * @brief Get the version of firmware on the TRX via a ::TRX_Request_UtilInfoGetVersion
 * request. A success status returned from this function does not indicate
 * the version has been retrieved from the TRX. The provided
 * callback will be executed with the following event:
 * -# ::TRX_EventGetVersionComplete
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param callback The callback to execute any when the
 *                 ::TRX_EventGetVersionComplete event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_UtilInfoGetVersion request
 */
extern TRX_Host_Status TRX_Host_utilGetVersion(TRX_Host_Handle handle, TRX_Host_Callback callback);

/**
 * @brief Get the version of firmware on the TRX via a ::TRX_Request_UtilInfoGetMacUuid
 * request. A success status returned from this function does not indicate
 * the version has been retrieved from the TRX. The provided
 * callback will be executed with the following event:
 * -# ::TRX_EventGetMacUuidComplete
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param callback The callback to execute any when the
 *                 ::TRX_EventGetMacUuidComplete event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_UtilInfoGetMacUuid request
 */
extern TRX_Host_Status TRX_Host_utilGetMacUuid(TRX_Host_Handle handle, TRX_Host_Callback callback);


/**
 * @brief Get the device information of the TRX via a ::TRX_Request_UtilInfoGetDevice
 * request. A success status returned from this function does not indicate
 * the version has been retrieved from the TRX. The provided
 * callback will be executed with the following event:
 * -# ::TRX_EventGetDeviceInfoComplete
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param callback The callback to execute any when the
 *                 ::TRX_EventGetDeviceInfoComplete event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_UtilInfoGetDevice request
 */
extern TRX_Host_Status TRX_Host_utilGetDeviceInfo(TRX_Host_Handle handle, TRX_Host_Callback callback);

/**
 * @brief Instruct the TRX to enter the SSBL on next reset via a
 * ::TRX_Request_UtilEnterSsbl request. A success status returned from this
 * function does not indicate the TRX has entered the SSBL. The caller must
 * call ::TRX_Host_close() and reset the TRX owhen the provided callback is
 * executed with the following event:
 * -# ::TRX_EventEnterSsblComplete
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param callback The callback to execute any when the
 *                 ::TRX_EventEnterSsblComplete event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_UtilEnterSsbl request
 */
extern TRX_Host_Status TRX_Host_utilEnterSsbl(TRX_Host_Handle handle, TRX_Host_Callback callback);

/**
 * @brief Send a ::TRX_Request_UtilPowerMode request to the TRX to configure
 * automatic power mode handling. A success status returned from this function
 * does not indicate that the power mode was successfully applied on the TRX.
 * The provided callback will be executed with the following event:
 * -# ::TRX_EventPowerModeComplete
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param mode Power mode configuration (dwell time and power policy)
 * @param callback The callback to execute when the
 *                 ::TRX_EventPowerModeComplete event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_UtilPowerMode request
 */
extern TRX_Host_Status TRX_Host_utilSetPowerMode(TRX_Host_Handle handle,
                                                 TRX_PowerMode mode,
                                                 TRX_Host_Callback callback);

/**
 * @brief Wake the TRX and block until it is ready to receive commands
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status TRX_Host_utilWakeTrx(TRX_Host_Handle handle);

/**
 * @brief Send a ::TRX_Request_NvErase request to the TRX. A success status
 * returned from this function does not indicate the NV storage was successfully
 * erased. The provided callback will be executed with the following event:
 * -# ::TRX_EventNvEraseComplete
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param callback The callback to execute any when the
 *                 ::TRX_EventNvEraseComplete event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_NvErase request
 */
extern TRX_Host_Status TRX_Host_eraseNv(TRX_Host_Handle handle, TRX_Host_Callback callback);

/**
 * @brief Send a ::TRX_Request_UtilRfMode request to the TRX. A success status
 * returned from this function does not indicate that the RF Mode was set correctly on the TRX.
 * The provided callback will be executed with the following event:
 * -# ::TRX_EventSetRfModeComplete
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param callback The callback to execute any when the
 *                 ::TRX_EventSetRfModeComplete event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_UtilRfMode request
 */
extern TRX_Host_Status TRX_Host_utilSetRfMode(TRX_Host_Handle handle, TRX_RfMode rfMode, TRX_Host_Callback callback);


/**
 * @brief Send a ::TRX_Host_utilDioSetup request to the TRX to setup the DIOs 
 * on the TRX
 * A success status
 * returned from this function does not indicate the request was successfully
 * executed. The provided callback will be executed with the following event:
 * -# ::TRX_EventDioSetupComplete
 * @note The host should issue a stop command towards the running/scheduled command 
 * slot before issuing this API
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param dioConfig Configurations for the different DIOs avialable on the TRX
 * @param callback The callback to execute when the
 *                 ::TRX_EventDioSetupComplete event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Host_utilDioSetup request
 */
extern TRX_Host_Status TRX_Host_utilDioSetup(TRX_Host_Handle handle,
                                            TRX_DioConfig dioConfig,
                                            TRX_Host_Callback callback);

/**
 * @brief Send a ::TRX_Request_UtilShutdown request to the TRX to transition
 * the device into shutdown mode. A success status returned from this function
 * does not indicate that the TRX has entered shutdown; the provided callback
 * will be executed with the following event:
 * -# ::TRX_EventShutdownComplete
 *
 * The TRX will return an error via LastStatus if a radio command is running
 * or scheduled when the shutdown request is received. The host should issue
 * a stop command towards the running/scheduled command slot before calling
 * this API.
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param callback The callback to execute when the
 *                 ::TRX_EventShutdownComplete event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_UtilShutdown request
 */
extern TRX_Host_Status TRX_Host_utilShutdown(TRX_Host_Handle handle,
                                             TRX_Host_Callback callback);

/**
 * @brief Send a ::TRX_Request_DeviceConfigStore request to the TRX to store the
 * device-wide configuration (DIO, clock, power mode, RF mode) as a single
 * bundle. A success status returned from this function does not indicate that
 * the configuration was successfully stored on the TRX. The provided callback
 * will be executed with the following event:
 * -# ::TRX_EventDeviceConfigStoreComplete
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param data The device configuration to store on the TRX
 * @param callback The callback to execute when the
 *                 ::TRX_EventDeviceConfigStoreComplete event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_DeviceConfigStore request
 */
extern TRX_Host_Status TRX_Host_storeDeviceConfig(TRX_Host_Handle handle,
                                                  TRX_DeviceConfigData data,
                                                  TRX_Host_Callback callback);

/**
 * @brief Send a ::TRX_Request_DeviceConfigPersist request to the TRX to persist
 * the currently stored device configuration to non-volatile (NV) storage. A
 * success status returned from this function does not indicate that the
 * configuration was successfully persisted. The provided callback will be
 * executed with the following event:
 * -# ::TRX_EventDeviceConfigPersistComplete
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param allowReplace If false and a persisted device config already exists on
 *                     the TRX, the request will fail. If true, any existing
 *                     persisted device config will be replaced.
 * @param callback The callback to execute when the
 *                 ::TRX_EventDeviceConfigPersistComplete event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_DeviceConfigPersist request
 */
extern TRX_Host_Status TRX_Host_persistDeviceConfig(TRX_Host_Handle handle,
                                                    bool allowReplace,
                                                    TRX_Host_Callback callback);

/**
 * @brief Send a ::TRX_Request_DeviceConfigGet request to the TRX to retrieve
 * the currently stored device configuration. A success status returned from
 * this function does not indicate that the configuration has been retrieved
 * from the TRX. The provided callback will be executed with the following
 * event:
 * -# ::TRX_EventDeviceConfigGetComplete
 *
 * When the ::TRX_EventDeviceConfigGetComplete event fires, the ::TRX_Request
 * pointer passed to the callback can be cast to
 * ::TRX_Request_DeviceConfigGetResponse to read the persisted flag, modified
 * flag, and the returned ::TRX_DeviceConfigData.
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param callback The callback to execute when the
 *                 ::TRX_EventDeviceConfigGetComplete event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_DeviceConfigGet request
 */
extern TRX_Host_Status TRX_Host_getDeviceConfig(TRX_Host_Handle handle,
                                                TRX_Host_Callback callback);

/**
 * @brief Send a ::TRX_Request_StreamFlush request to the TRX to discard all
 * fragments of a previously stored stream on the TRX. A success status returned
 * from this function does not indicate the stream was flushed on the TRX; the
 * provided callback will be executed with one of the following events:
 * -# ::TRX_EventStreamFlushComplete
 * -# ::TRX_EventLastStatusError
 *
 * On ::TRX_EventStreamFlushComplete the local stream slot for @p id is
 * released so the same identifier may be reused with a subsequent
 * ::TRX_Host_storeStream call.
 *
 * @note Calling context: Task
 *
 * @param handle Handle to the TRX Host driver
 * @param id The unique identifier of the stream to flush; must match a
 *           stream previously registered via ::TRX_Host_storeStream
 * @param callback The callback to execute when the
 *                 ::TRX_EventStreamFlushComplete event is observed
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_StreamFlush request
 */
extern TRX_Host_Status TRX_Host_flushStream(TRX_Host_Handle handle, uint8_t id,
                                            TRX_Host_Callback callback);

//*****************************************************************************
//
//! Close the Doxygen group.
//! @}
//
//*****************************************************************************

#ifdef __cplusplus
}
#endif

#endif // TRX_HOST_H_

