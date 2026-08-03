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

/*!

@file trx_host_handlers.c

@brief Implements request handlers for requests from the TRX.

 */

/********************************** Includes **********************************/

/* Standard Header Files */
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Module Header Files */
#include "TRX.h"
#include "trx_host.h"
#include "private/trx_host_private.h"
#include "handlers/trx_host_handlers.h"
#include "utility/utils.h"
#include "utility/list/list.h"


/* Zephyr Logging */
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(ti_cc140xp, CONFIG_TI_CC140XP_LOG_LEVEL);

/**
 * @brief Processes a TRX_Request_LastStatus received from the TRX and sets the
 * value of the pointers passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqLastStatus(TRX_Host_Handle handle, TRX_Request *pReq,
                                 uint64_t *observedEvents, uint64_t *subscribedEvents,
                                 TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes a TRX_Request_TransportError received from the TRX and sets
 * the value of the pointers passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_TransportError received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_TransportError
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_TransportError
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqTransportError(TRX_Host_Handle handle, TRX_Request *pReq,
                                     uint64_t *observedEvents, uint64_t *subscribedEvents,
                                     TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_CommandStore and sets the value of the pointers passed
 * in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqCmdStore(TRX_Host_Handle handle, TRX_Request *pReq,
                               uint64_t *observedEvents, uint64_t *subscribedEvents,
                               TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_CommandSubmit and sets the value of the pointers passed
 * in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqCmdSubmit(TRX_Host_Handle handle, TRX_Request *pReq,
                                uint64_t *observedEvents, uint64_t *subscribedEvents,
                                TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_CommandStop and sets the value of the pointers passed
 * in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqCmdStop(TRX_Host_Handle handle, TRX_Request *pReq,
                              uint64_t *observedEvents, uint64_t *subscribedEvents,
                              TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes a TRX_Request_CommandStatus received from the TRX and sets
 * the value of the pointers passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_CommandStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_CommandStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_CommandStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqCmdStatus(TRX_Host_Handle handle, TRX_Request *pReq,
                                uint64_t *observedEvents, uint64_t *subscribedEvents,
                                TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes a TRX_Request_StreamStore received from the TRX as well as
 * a TRX_Request_LastStatus received from the TRX after sending a
 * TRX_Request_StreamStore and sets the value of the pointers passed in as a
 * return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus or TRX_Request_StreamStore received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus or
 *                            TRX_Request_StreamStore
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus or
 *                              TRX_Request_StreamStore
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqStreamStore(TRX_Host_Handle handle, TRX_Request *pReq,
                                  uint64_t *observedEvents, uint64_t *subscribedEvents,
                                  TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes a TRX_Request_StreamFlush received from the TRX and sets
 * the value of the pointers passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_StreamFlush received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_StreamFlush
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_StreamFlush
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqStreamFlush(TRX_Host_Handle handle, TRX_Request *pReq,
                                  uint64_t *observedEvents, uint64_t *subscribedEvents,
                                  TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_ConfigStore and sets the value of the pointers passed
 * in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqConfigStore(TRX_Host_Handle handle, TRX_Request *pReq,
                                  uint64_t *observedEvents, uint64_t *subscribedEvents,
                                  TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_ConfigPersist and sets the value of the pointers passed
 * in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqConfigPersist(TRX_Host_Handle handle, TRX_Request *pReq,
                                    uint64_t *observedEvents, uint64_t *subscribedEvents,
                                    TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_ConfigList and sets the value of the pointers passed
 * in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqConfigList(TRX_Host_Handle handle, TRX_Request *pReq,
                                 uint64_t *observedEvents, uint64_t *subscribedEvents,
                                 TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_ConfigListResponse received from the TRX
 * after sending a TRX_Request_ConfigList and sets the value of the pointers passed
 * in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_ConfigListResponse received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_ConfigListResponse
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_ConfigListResponse
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqConfigListRsp(TRX_Host_Handle handle, TRX_Request *pReq,
                                    uint64_t *observedEvents, uint64_t *subscribedEvents,
                                    TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_DeviceConfigStore and sets the value of the pointers
 * passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqDeviceConfigStore(TRX_Host_Handle handle, TRX_Request *pReq,
                                        uint64_t *observedEvents, uint64_t *subscribedEvents,
                                        TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_DeviceConfigPersist and sets the value of the pointers
 * passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqDeviceConfigPersist(TRX_Host_Handle handle, TRX_Request *pReq,
                                          uint64_t *observedEvents, uint64_t *subscribedEvents,
                                          TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_DeviceConfigGet. On success the map is left on the
 * queue so that processReqDeviceConfigGetResponse can retrieve the callback
 * when the response arrives. On error the callback fires here.
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqDeviceConfigGet(TRX_Host_Handle handle, TRX_Request *pReq,
                                      uint64_t *observedEvents, uint64_t *subscribedEvents,
                                      TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_DeviceConfigGetResponse received from the
 * TRX after sending a TRX_Request_DeviceConfigGet. Pops the last-status map
 * left on the queue by processReqDeviceConfigGet.
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_DeviceConfigGetResponse received
 * @param[out] observedEvents Pointer that gets set to the events observed
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqDeviceConfigGetResponse(TRX_Host_Handle handle, TRX_Request *pReq,
                                              uint64_t *observedEvents, uint64_t *subscribedEvents,
                                              TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_UtilShutdown and sets the value of the pointers passed
 * in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqUtilShutdown(TRX_Host_Handle handle, TRX_Request *pReq,
                                   uint64_t *observedEvents, uint64_t *subscribedEvents,
                                   TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_UtilGetTime and sets the value of the pointers passed
 * in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqUtilTime(TRX_Host_Handle handle, TRX_Request *pReq,
                               uint64_t *observedEvents, uint64_t *subscribedEvents,
                               TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_UtilGetRssi and sets the value of the pointers passed
 * in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqUtilRssi(TRX_Host_Handle handle, TRX_Request *pReq,
                               uint64_t *observedEvents, uint64_t *subscribedEvents,
                               TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_UtilInfoGetVersion and sets the value of the pointers passed
 * in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqUtilVersion(TRX_Host_Handle handle, TRX_Request *pReq,
                                  uint64_t *observedEvents, uint64_t *subscribedEvents,
                                  TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_UtilInfoGetMacUuid and sets the value of the pointers
 * passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqUtilMacUuid(TRX_Host_Handle handle, TRX_Request *pReq,
                                  uint64_t *observedEvents, uint64_t *subscribedEvents,
                                  TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_UtilInfoGetDevice and sets the value of the pointers
 * passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqUtilInfoDevice(TRX_Host_Handle handle, TRX_Request *pReq,
                                     uint64_t *observedEvents, uint64_t *subscribedEvents,
                                     TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_UtilEnterSsbl and sets the value of the pointers
 * passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqUtilEnterSsbl(TRX_Host_Handle handle, TRX_Request *pReq,
                                    uint64_t *observedEvents, uint64_t *subscribedEvents,
                                    TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_UtilPowerMode and sets the value of the pointers
 * passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqUtilPowerMode(TRX_Host_Handle handle, TRX_Request *pReq,
                                    uint64_t *observedEvents, uint64_t *subscribedEvents,
                                    TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_NvAllocate and sets the value of the pointers
 * passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqNvAllocate(TRX_Host_Handle handle, TRX_Request *pReq,
                                 uint64_t *observedEvents, uint64_t *subscribedEvents,
                                 TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_NvInvalidate and sets the value of the pointers
 * passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqNvInvalidate(TRX_Host_Handle handle, TRX_Request *pReq,
                                   uint64_t *observedEvents, uint64_t *subscribedEvents,
                                   TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);
/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_NvInfo and sets the value of the pointers
 * passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqNvInfo(TRX_Host_Handle handle, TRX_Request *pReq,
                             uint64_t *observedEvents, uint64_t *subscribedEvents,
                             TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_NvErase and sets the value of the pointers
 * passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqNvErase(TRX_Host_Handle handle, TRX_Request *pReq,
                              uint64_t *observedEvents, uint64_t *subscribedEvents,
                              TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes a TRX_Request_NvCorrupt received from the TRX after
 * sending and sets the value of the pointers passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_NvCorrupt received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_NvCorrupt
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_NvCorrupt
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqNvCorrupt(TRX_Host_Handle handle, TRX_Request *pReq,
                                uint64_t *observedEvents, uint64_t *subscribedEvents,
                                TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_Ping and sets the value of the pointers
 * passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqPing(TRX_Host_Handle handle, TRX_Request *pReq,
                           uint64_t *observedEvents, uint64_t *subscribedEvents,
                           TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_UtilRfMode and sets the value of the pointers
 * passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqUtilRfMode(TRX_Host_Handle handle, TRX_Request *pReq,
                                uint64_t *observedEvents, uint64_t *subscribedEvents,
                                TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/**
 * @brief Processes the TRX_Request_LastStatus received from the TRX after
 * sending a TRX_Request_DioSetup and sets the value of the pointers
 * passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request_LastStatus received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request_LastStatus
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request_LastStatus
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
static void processReqUtilDioSetup(TRX_Host_Handle handle, TRX_Request *pReq,
                                uint64_t *observedEvents, uint64_t *subscribedEvents,
                                TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/*************************** Variable Declarations ****************************/

/** Map of TRX_RequestType to an index in ::requestHandlerMap */
const uint8_t requestHandlerIndexMap[(Req_PING + 1U)] = {
    [Req_INVALID] = 0U,
    [Req_LAST_STATUS] = 1U,
    [Req_TRANSPORT_ERROR] = 2U,
    [Req_CMD_STORE] = 3U,
    [Req_CMD_SUBMIT] = 4U,
    [Req_CMD_STOP] = 5U,
    [Req_CMD_STATUS] = 6U,
    [Req_STREAM_STORE] = 7U,
    [Req_STREAM_FLUSH] = 8U,
    [Req_CONFIG_STORE] = 9U,
    [Req_CONFIG_PERSIST] = 10U,
    [Req_CONFIG_LIST] = 11U,
    [Req_CONFIG_LIST_RSP] = 12U,
    [Req_CONFIG_FLUSH] = 13U,
    [Req_UTIL_TIME] = 14U,
    [Req_UTIL_RSSI] = 15U,
    [Req_UTIL_INFO_VERSION] = 16U,
    [Req_UTIL_INFO_MAC_UUID] = 17U,
    [Req_UTIL_INFO_DEVICE] = 18U,
    [Req_UTIL_ENTER_SSBL] = 19U,
    [Req_NV_ALLOCATE] = 20U,
    [Req_NV_WRITE] = 21U,
    [Req_NV_FINALIZE] = 22U,
    [Req_NV_INVALIDATE] = 23U,
    [Req_NV_GET] = 24U,
    [Req_NV_INFO] = 25U,
    [Req_NV_ERASE] = 26U,
    [Req_NV_CORRUPT] = 27U,
    [Req_PING] = 28U,
    [Req_UTIL_RF_MODE] = 29U,
    [Req_DIO_SETUP] = 30U,
    [Req_DEVICE_CONFIG_STORE] = 31U,
    [Req_DEVICE_CONFIG_PERSIST] = 32U,
    [Req_DEVICE_CONFIG_GET] = 33U,
    [Req_DEVICE_CONFIG_GET_RSP] = 34U,
    [Req_UTIL_POWER_MODE] = 35U,
    [Req_UTIL_SHUTDOWN] = 36U,
};

/** Parsing functions for all TRX_RequestType */
const requestHandler requestHandlerMap[sizeof(requestHandlerIndexMap)] = {
    processReqTransportError,         // Req_INVALID
    processReqLastStatus,             // Req_LAST_STATUS
    processReqTransportError,         // Req_TRANSPORT_ERROR
    processReqCmdStore,               // Req_CMD_STORE
    processReqCmdSubmit,              // Req_CMD_SUBMIT
    processReqCmdStop,                // Req_CMD_STOP
    processReqCmdStatus,              // Req_CMD_STATUS
    processReqStreamStore,            // Req_STREAM_STORE
    processReqStreamFlush,            // Req_STREAM_FLUSH
    processReqConfigStore,            // Req_CONFIG_STORE
    processReqConfigPersist,          // Req_CONFIG_PERSIST
    processReqConfigList,             // Req_CONFIG_LIST
    processReqConfigListRsp,          // Req_CONFIG_LIST_RSP
    processReqTransportError,         // Req_CONFIG_FLUSH: Flushing a config not supported
    processReqUtilTime,               // Req_UTIL_TIME
    processReqUtilRssi,               // Req_UTIL_RSSI
    processReqUtilVersion,            // Req_UTIL_INFO_VERSION
    processReqUtilMacUuid,            // Req_UTIL_INFO_MAC_UUID
    processReqUtilInfoDevice,         // Req_UTIL_INFO_DEVICE
    processReqUtilEnterSsbl,          // Req_UTIL_ENTER_SSBL
    processReqNvAllocate,             // Req_NV_ALLOCATE
    NULL,                             // Req_NV_WRITE TODO: TRXHOST-599
    NULL,                             // Req_NV_FINALIZE TODO: TRXHOST-599
    processReqNvInvalidate,           // Req_NV_INVALIDATE
    NULL,                             // Req_NV_GET TODO: TRXHOST-599
    processReqNvInfo,                 // Req_NV_INFO
    processReqNvErase,                // Req_NV_ERASE
    processReqNvCorrupt,              // Req_NV_CORRUPT
    processReqPing,                   // Req_PING
    processReqUtilRfMode,             // Req_UTIL_RF_MODE
    processReqUtilDioSetup,           // Req_DIO_SETUP
    processReqDeviceConfigStore,      // Req_DEVICE_CONFIG_STORE
    processReqDeviceConfigPersist,    // Req_DEVICE_CONFIG_PERSIST
    processReqDeviceConfigGet,        // Req_DEVICE_CONFIG_GET
    processReqDeviceConfigGetResponse,// Req_DEVICE_CONFIG_GET_RSP
    processReqUtilPowerMode,          // Req_UTIL_POWER_MODE
    processReqUtilShutdown,           // Req_UTIL_SHUTDOWN
};

/************************* Static Function Definitions ************************/

static void processReqTransportError(TRX_Host_Handle handle, TRX_Request *pReq,
                                     uint64_t *observedEvents, uint64_t *subscribedEvents,
                                     TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{

    LOG_INF( "RF: Processing Req_INVALID | Req_CONFIG_FLUSH | Req_TRANSPORT_ERROR");

    (void)pReq;
    /* A TRX_Request_TransportError was received from the TRX */
    *observedEvents |= TRX_EventTransportError;
    *cb = handle->generalCb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventTransportError;
}

static void processReqCmdStore(TRX_Host_Handle handle, TRX_Request *pReq,
                               uint64_t *observedEvents, uint64_t *subscribedEvents,
                               TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{

    LOG_INF( "RF: Processing Req_CMD_STORE");

    (void)pReq;
    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_CommandStore sent to the TRX */
    *observedEvents |= TRX_EventCmdStoreComplete;
    TRX_Host_Cmd *pCmd = resolveCmd(handle, pMap->id);
    Assert_isTrue(NULL != pCmd, 14U);
    if(NULL != pCmd)
    {
        *cb = (TRX_Host_Callback)pMap->cb;
        *pCmdStreamOrConfig = (uintptr_t)pCmd->cmdStore;
        *subscribedEvents |= pCmd->subscribedEvents;
    } // else unrecognized cmd and it should be ignored

    freeLastStatusMap(pMap);
}

static void processReqCmdSubmit(TRX_Host_Handle handle, TRX_Request *pReq,
                                uint64_t *observedEvents, uint64_t *subscribedEvents,
                                TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{

    LOG_INF( "RF: Processing Req_CMD_SUBMIT");

    (void)pReq;
    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_CommandSubmit sent to the TRX */
    *observedEvents |= TRX_EventCmdSubmitComplete;
    TRX_Host_Cmd *pCmd = resolveCmd(handle, pMap->id);
    Assert_isTrue(NULL != pCmd, 15U);
    if(NULL != pCmd)
    {
        *cb = (TRX_Host_Callback)pMap->cb;
        *pCmdStreamOrConfig = (uintptr_t)pCmd->cmdStore;
        *subscribedEvents |= pCmd->subscribedEvents;
    } // else unrecognized cmd and it should be ignored

    freeLastStatusMap(pMap);
}


static void processReqCmdStop(TRX_Host_Handle handle, TRX_Request *pReq,
                              uint64_t *observedEvents, uint64_t *subscribedEvents,
                              TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{

    LOG_INF( "RF: Processing Req_CMD_STOP");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_CommandSubmit sent to the TRX */
    *observedEvents |= TRX_EventStopStatus;
    TRX_Host_Cmd *pCmd = resolveCmd(handle, pMap->id);
    Assert_isTrue(NULL != pCmd, 16U);
    if(NULL != pCmd)
    {
        *cb = (TRX_Host_Callback)pMap->cb;
        *pCmdStreamOrConfig = (uintptr_t)pCmd->cmdStore;
        *subscribedEvents |= pCmd->subscribedEvents | TRX_EventStopStatus;
    } // else unrecognized cmd and it should be ignored

    freeLastStatusMap(pMap);
}

static void processReqCmdStatus(TRX_Host_Handle handle, TRX_Request *pReq,
                                uint64_t *observedEvents, uint64_t *subscribedEvents,
                                TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{

    LOG_INF( "RF: Processing Req_CMD_STATUS");

    /* A TRX_Request_CommandStatus was received from the TRX */
    TRX_Request_CommandStatus *pCmdStatusRequest = (TRX_Request_CommandStatus *)pReq;

    *observedEvents |= TRX_EventCmdStatus;
    if(0U == pCmdStatusRequest->chain_action)
    {
        // Final command in a chain
        *observedEvents |= TRX_EventFinalCmdStatus;
    }

    TRX_Host_Cmd *pCmd = resolveCmd(handle, ((TRX_Request_CommandStatus *)pReq)->slot);
    Assert_isTrue(NULL != pCmd, 17U);
    if(NULL != pCmd)
    {
        Assert_isTrue(NULL != pCmd->cb, 18U);
        Assert_isTrue(0U != pCmd->subscribedEvents, 19U);
        *cb = pCmd->cb;
        *pCmdStreamOrConfig = (uintptr_t)pCmd->cmdStore;
        *subscribedEvents |= pCmd->subscribedEvents;
    }
}

static void processReqStreamStore(TRX_Host_Handle handle, TRX_Request *pReq,
                                  uint64_t *observedEvents, uint64_t *subscribedEvents,
                                  TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{

    LOG_INF( "RF: Processing Req_STREAM_STORE");

    if(Req_LAST_STATUS == pReq->header.request)
    {
        SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));
        TRX_Host_Stream *pStream = resolveStream(handle, pMap->id);
        Assert_isTrue(NULL != pStream, 20U);

        if(0U == pMap->md)
        {
            /* The final TRX_Request_LastStatus was received from the TRX regarding
            * a TRX_Request_StreamStore sent to the TRX */
            *observedEvents |= TRX_EventStreamStoreComplete;
        }

        if(NULL != pStream)
        {
            *cb = (TRX_Host_Callback)pMap->cb;
            *pCmdStreamOrConfig = (uintptr_t)pStream->data;
            *subscribedEvents |= pStream->subscribedEvents;
        } // else unrecognized stream and it should be ignored
        freeLastStatusMap(pMap);
    }
    else
    {
        TRX_Request_StreamStore *pReqStreamStore = (TRX_Request_StreamStore *)pReq;

        /* A TRX_Request_StreamStore was received from the TRX  */
        *observedEvents |= TRX_EventStreamStoreReceived;
        if(0U == pReqStreamStore->md)
        {
            /* A TRX_Request_StreamStore with the more data bit set to
                * 0 was received from the TRX  */
            *observedEvents |= TRX_EventFinalStreamStoreReceived;
        }

        TRX_Host_Stream *pStream = resolveStream(handle, pReqStreamStore->stream_id);
        Assert_isTrue(NULL != pStream, 21U);
        if(NULL != pStream)
        {
            *cb = pStream->cb;
            *pCmdStreamOrConfig = (uintptr_t)pStream->data;
            *subscribedEvents |= pStream->subscribedEvents;

            if(1U == pReqStreamStore->start)
            {
                // Restart back at the beginning of data buffer
                pStream->index = 0U;
            }

            const uint8_t dataLen = (pReq->header.len - sizeof(TRX_Request_StreamStore));
            if(pStream->len >= (pStream->index + dataLen))
            {
                // Move data into user buffer
                (void)memcpy(&(pStream->data[pStream->index]), pReqStreamStore->data, dataLen);
                pStream->index += dataLen;
            }
            else
            {
                /* A TRX_Request_StreamStore was received from the TRX
                 * but there was not enough space in the receive buffer */
                *observedEvents |= TRX_EventStreamStoreOverflow;
            }
        } // else unrecognized stream and it should be ignored
    }
}

static void processReqStreamFlush(TRX_Host_Handle handle, TRX_Request *pReq,
                                  uint64_t *observedEvents, uint64_t *subscribedEvents,
                                  TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_STREAM_FLUSH");

    if(Req_LAST_STATUS == pReq->header.request)
    {
        /* The TRX_Request_LastStatus was received from the TRX regarding a
         * TRX_Request_StreamFlush sent to the TRX */
        SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));
        TRX_Host_Stream *pStream = resolveStream(handle, pMap->id);

        *observedEvents |= TRX_EventStreamFlushComplete;
        *cb = (TRX_Host_Callback)pMap->cb;
        *subscribedEvents |= TRX_EventStreamFlushComplete;

        if(NULL != pStream)
        {
            *pCmdStreamOrConfig = (uintptr_t)pStream->data;
            /* Release the local stream slot so the id may be reused */
            pStream->id = TRX_RESERVED_ID;
        }
        else
        {
            *pCmdStreamOrConfig = (uintptr_t)NULL;
        }

        freeLastStatusMap(pMap);
        return;
    }

    TRX_Host_Stream *pStream = resolveStream(handle, ((TRX_Request_StreamFlush *)pReq)->stream_id);
    Assert_isTrue(NULL != pStream, 22U);
    if(NULL != pStream)
    {
        *cb = pStream->cb;
        *pCmdStreamOrConfig = (uintptr_t)pStream->data;
        *subscribedEvents |= pStream->subscribedEvents;

        if(TRX_Stream_Rx == pStream->type)
        {
            /* A TRX_Request_StreamFlush was received from the TRX
             * invalidating RX data sent to the Host due to a CRC failure
             * on final reception */
            *observedEvents |= TRX_EventStreamFlushReceived;

            // If the app subscribed to partial rx events, let them know
            // the data is bad by auto subscribing them to the flush event
            if(pStream->subscribedEvents & TRX_EventStreamStoreReceived)
            {
                *subscribedEvents |= TRX_EventStreamFlushReceived;
            }
        }
        else if (TRX_Stream_Tx == pStream->type || TRX_Stream_Tx_Start == pStream->type)
        {
            /* A TRX_Request_StreamFlush was received from the TRX
             * requesting the rest of the data to TX */
            if (TRX_Stream_Tx_Start == pStream->type)
            {
                pStream->type = TRX_Stream_Tx_Cont;
            }
            uint16_t lenToSend = pStream->len - pStream->index;
            TRX_Host_Status streamStatus = buildAndSendStream(handle, pStream, lenToSend, false);
            if(TRX_Host_Success != streamStatus)
            {
                // will continue flushing at top of this cb when more spi containers become available
                handle->streamBeingFlushed = pStream;
            }
        }
        else
        {
            /* TRX_Stream_Tx_Cont or TRX_Stream_Tx_End */
            /* Create callback */
            *observedEvents |= TRX_EventStreamFlushReceived;
        }
    }
}

static void processReqConfigStore(TRX_Host_Handle handle, TRX_Request *pReq,
                                  uint64_t *observedEvents, uint64_t *subscribedEvents,
                                  TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{

    LOG_INF( "RF: Processing Req_CONFIG_STORE");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));
    TRX_Host_Config *pConfig = resolveConfig(handle, pMap->id);

    if(0U == pMap->md)
    {
        /* The final TRX_Request_LastStatus was received from the TRX regarding
         * a TRX_Request_ConfigStore sent to the TRX */
        *observedEvents |= TRX_EventConfigStoreComplete;
    }

    if(NULL != pConfig)
    {
        *cb = (TRX_Host_Callback)pMap->cb;
        *pCmdStreamOrConfig = (uintptr_t)pConfig->data;
        *subscribedEvents |= pConfig->subscribedEvents;
    } // else unrecognized config and it should be ignored

    freeLastStatusMap(pMap);
}

static void processReqConfigPersist(TRX_Host_Handle handle, TRX_Request *pReq,
                                    uint64_t *observedEvents, uint64_t *subscribedEvents,
                                    TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_CONFIG_PERSIST");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* The final TRX_Request_LastStatus was received from the TRX regarding
     * a TRX_Request_ConfigPersist sent to the TRX  */
    *observedEvents |= TRX_EventConfigPersistComplete;
    TRX_Host_Config *pConfig = resolveConfig(handle, pMap->id);
    if(NULL != pConfig)
    {
        *cb = (TRX_Host_Callback)pMap->cb;
        *pCmdStreamOrConfig = (uintptr_t)pConfig->data;
        *subscribedEvents |= pConfig->subscribedEvents;
    } // else unrecognized config and it should be ignored

    freeLastStatusMap(pMap);
}

static void processReqConfigList(TRX_Host_Handle handle, TRX_Request *pReq,
                                 uint64_t *observedEvents, uint64_t *subscribedEvents,
                                 TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{

    LOG_INF( "RF: Processing Req_CONFIG_LIST");

    /* The final TRX_Request_LastStatus was received from the TRX regarding
     * a TRX_Request_ConfigList sent to the TRX  */
    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));
    TRX_Request_LastStatus *pReqLastStatus = (TRX_Request_LastStatus *)pReq;

    // Store the number of configurations to expect a TRX_Request_ConfigListResponse
    handle->generalPurposeGlobal = ((uint32_t)(pReqLastStatus->params[0]) & 0x000000FF);

    freeLastStatusMap(pMap);
}

static void processReqConfigListRsp(TRX_Host_Handle handle, TRX_Request *pReq,
                                    uint64_t *observedEvents, uint64_t *subscribedEvents,
                                    TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{

    LOG_INF( "RF: Processing Req_CONFIG_LIST_RSP");

    /* A TRX_Request_ConfigListResponse was received from the TRX regarding
     * a previous TRX_Request_ConfigList sent to the TRX  */
    TRX_Request_ConfigListResponse *pReqConfigListRsp = (TRX_Request_ConfigListResponse *)pReq;

    uint8_t i;
    TRX_StreamDescriptor *config;
    for(i = 0; i < pReqConfigListRsp->count; i++)
    {
        config = &(pReqConfigListRsp->streams[i]);
        (void)registerConfig(handle, config->stream_id, NULL, config->length,
                             config->reference, config->persisted, NULL, TRX_EventUnassigned);
    }

    handle->generalPurposeGlobal -= pReqConfigListRsp->count;
    if(0 == handle->generalPurposeGlobal)
    {
        TRX_HOST_POST(handle);
    }
}

static void processReqUtilTime(TRX_Host_Handle handle, TRX_Request *pReq,
                               uint64_t *observedEvents, uint64_t *subscribedEvents,
                               TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_UTIL_TIME");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_UtilGetTime sent to the TRX  */
    *observedEvents |= TRX_EventReceivedTrxTime;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventReceivedTrxTime;
    freeLastStatusMap(pMap);
}

static void processReqUtilRssi(TRX_Host_Handle handle, TRX_Request *pReq,
                               uint64_t *observedEvents, uint64_t *subscribedEvents,
                               TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_UTIL_RSSI");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
    * TRX_Request_UtilGetRssi sent to the TRX  */
    *observedEvents |= TRX_EventGetRSSIComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventGetRSSIComplete;
    freeLastStatusMap(pMap);
}

static void processReqUtilVersion(TRX_Host_Handle handle, TRX_Request *pReq,
                                  uint64_t *observedEvents, uint64_t *subscribedEvents,
                                  TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_UTIL_INFO_VERSION");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_UtilInfoGetVersion sent to the TRX  */
    *observedEvents |= TRX_EventGetVersionComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventGetVersionComplete;
    freeLastStatusMap(pMap);
}

static void processReqUtilMacUuid(TRX_Host_Handle handle, TRX_Request *pReq,
                                  uint64_t *observedEvents, uint64_t *subscribedEvents,
                                  TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_UTIL_INFO_MAC_UUID");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_UtilInfoGetMacUuid sent to the TRX  */
    *observedEvents |= TRX_EventGetMacUuidComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventGetMacUuidComplete;
    freeLastStatusMap(pMap);
}

static void processReqUtilInfoDevice(TRX_Host_Handle handle, TRX_Request *pReq,
                                     uint64_t *observedEvents, uint64_t *subscribedEvents,
                                     TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_UTIL_INFO_DEVICE");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_UtilInfoGetDevice sent to the TRX  */
    *observedEvents |= TRX_EventGetDeviceInfoComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventGetDeviceInfoComplete;
    freeLastStatusMap(pMap);
}

static void processReqUtilEnterSsbl(TRX_Host_Handle handle, TRX_Request *pReq,
                                    uint64_t *observedEvents, uint64_t *subscribedEvents,
                                    TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_UTIL_ENTER_SSBL");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_UtilEnterSsbl sent to the TRX  */
    *observedEvents |= TRX_EventEnterSsblComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventEnterSsblComplete;
    freeLastStatusMap(pMap);
}

static void processReqUtilPowerMode(TRX_Host_Handle handle, TRX_Request *pReq,
                                    uint64_t *observedEvents, uint64_t *subscribedEvents,
                                    TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_UTIL_POWER_MODE");

    (void)pReq;

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_UtilPowerMode sent to the TRX  */
    *observedEvents |= TRX_EventPowerModeComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventPowerModeComplete;
    freeLastStatusMap(pMap);
}

static void processReqNvAllocate(TRX_Host_Handle handle, TRX_Request *pReq,
                                     uint64_t *observedEvents, uint64_t *subscribedEvents,
                                     TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_NV_ALLOCATE");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_EventNvAllocateComplete sent to the TRX  */
    *observedEvents |= TRX_EventNvAllocateComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventNvAllocateComplete;
    freeLastStatusMap(pMap);
}

static void processReqNvInvalidate(TRX_Host_Handle handle, TRX_Request *pReq,
                                       uint64_t *observedEvents, uint64_t *subscribedEvents,
                                       TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_NV_INVALIDATE");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_EventNvInvalidateComplete sent to the TRX  */
    *observedEvents |= TRX_EventNvInvalidateComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventNvInvalidateComplete;
    freeLastStatusMap(pMap);
}

static void processReqNvInfo(TRX_Host_Handle handle, TRX_Request *pReq,
                                 uint64_t *observedEvents, uint64_t *subscribedEvents,
                                 TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_NV_INFO");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_NvInfo sent to the TRX  */
    *observedEvents |= TRX_EventNvInfoComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventNvInfoComplete;
    freeLastStatusMap(pMap);
}

static void processReqNvErase(TRX_Host_Handle handle, TRX_Request *pReq,
                              uint64_t *observedEvents, uint64_t *subscribedEvents,
                              TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_NV_ERASE");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_NvErase sent to the TRX  */
    *observedEvents |= TRX_EventNvEraseComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventNvEraseComplete;

    TRX_Request_LastStatus *pReqLastStatus = (TRX_Request_LastStatus *)pReq;
    RequestStatus requestStatus = pReqLastStatus->status;

    if(TRX_STATUS_SUCCESS == requestStatus)
    {
        // Wipe the config "database" clean of persisted configurations
        uint8_t i;
        for(i = 0U; i < TRX_MAX_CONFIGURATIONS; i++)
        {
            if(handle->registeredConfigs[i].persisted)
            {
                handle->registeredConfigs[i].id = TRX_RESERVED_ID;
            }
        }
    }

    freeLastStatusMap(pMap);
}

static void processReqNvCorrupt(TRX_Host_Handle handle, TRX_Request *pReq,
                                uint64_t *observedEvents, uint64_t *subscribedEvents,
                                TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_NV_CORRUPT");

    /* Notice from the TRX that the NV consistency check has failed,
     * need to erase NV  */
    *observedEvents |= TRX_EventNvCorrupt;
    *cb = handle->generalCb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventNvCorrupt;
}

static void processReqPing(TRX_Host_Handle handle, TRX_Request *pReq,
                           uint64_t *observedEvents, uint64_t *subscribedEvents,
                           TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_PING");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_Ping sent to the TRX  */
    *observedEvents |= TRX_EventPingComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventPingComplete;
    freeLastStatusMap(pMap);
}

static void processReqUtilRfMode(TRX_Host_Handle handle, TRX_Request *pReq,
                                     uint64_t *observedEvents, uint64_t *subscribedEvents,
                                     TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_UTIL_RF_MODE");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_UtilRfMode sent to the TRX  */
    *observedEvents |= TRX_EventSetRfModeComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventSetRfModeComplete;
    freeLastStatusMap(pMap);
}

static void processReqUtilDioSetup(TRX_Host_Handle handle, TRX_Request *pReq,
                                     uint64_t *observedEvents, uint64_t *subscribedEvents,
                                     TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_DIO_SETUP");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_DioSetup sent to the TRX  */
    *observedEvents |= TRX_EventDioSetupComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventDioSetupComplete;
    freeLastStatusMap(pMap);
}

static void processReqDeviceConfigStore(TRX_Host_Handle handle, TRX_Request *pReq,
                                        uint64_t *observedEvents, uint64_t *subscribedEvents,
                                        TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_DEVICE_CONFIG_STORE");

    (void)pReq;
    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_DeviceConfigStore sent to the TRX */
    *observedEvents |= TRX_EventDeviceConfigStoreComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventDeviceConfigStoreComplete;
    freeLastStatusMap(pMap);
}

static void processReqDeviceConfigPersist(TRX_Host_Handle handle, TRX_Request *pReq,
                                          uint64_t *observedEvents, uint64_t *subscribedEvents,
                                          TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_DEVICE_CONFIG_PERSIST");

    (void)pReq;
    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_DeviceConfigPersist sent to the TRX */
    *observedEvents |= TRX_EventDeviceConfigPersistComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventDeviceConfigPersistComplete;
    freeLastStatusMap(pMap);
}

static void processReqDeviceConfigGet(TRX_Host_Handle handle, TRX_Request *pReq,
                                      uint64_t *observedEvents, uint64_t *subscribedEvents,
                                      TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_DEVICE_CONFIG_GET");

    /* On success, leave the last-status map on the queue so that
     * processReqDeviceConfigGetResponse can retrieve the application callback
     * when the corresponding TRX_Request_DeviceConfigGetResponse arrives. On
     * error, no response will follow so pop the map and fire the callback with
     * TRX_EventDeviceConfigGetComplete now. */
    TRX_Request_LastStatus *pReqLastStatus = (TRX_Request_LastStatus *)pReq;
    if(TRX_STATUS_SUCCESS != pReqLastStatus->status)
    {
        SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));
        *observedEvents |= TRX_EventDeviceConfigGetComplete;
        *cb = (TRX_Host_Callback)pMap->cb;
        *pCmdStreamOrConfig = (uintptr_t)NULL;
        *subscribedEvents |= TRX_EventDeviceConfigGetComplete;
        freeLastStatusMap(pMap);
    }
}

static void processReqDeviceConfigGetResponse(TRX_Host_Handle handle, TRX_Request *pReq,
                                              uint64_t *observedEvents, uint64_t *subscribedEvents,
                                              TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_DEVICE_CONFIG_GET_RSP");

    (void)pReq;

    /* A TRX_Request_DeviceConfigGetResponse was received from the TRX
     * regarding a previous TRX_Request_DeviceConfigGet sent to the TRX. The
     * map left on the queue by processReqDeviceConfigGet carries the
     * application callback. */
    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));
    Assert_isTrue((NULL != pMap) && (Req_DEVICE_CONFIG_GET == pMap->req), 24U);

    *observedEvents |= TRX_EventDeviceConfigGetComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventDeviceConfigGetComplete;
    freeLastStatusMap(pMap);
}

static void processReqUtilShutdown(TRX_Host_Handle handle, TRX_Request *pReq,
                                   uint64_t *observedEvents, uint64_t *subscribedEvents,
                                   TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing Req_UTIL_SHUTDOWN");

    (void)pReq;

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));

    /* A TRX_Request_LastStatus was received from the TRX regarding a
     * TRX_Request_UtilShutdown sent to the TRX  */
    *observedEvents |= TRX_EventShutdownComplete;
    *cb = (TRX_Host_Callback)pMap->cb;
    *pCmdStreamOrConfig = (uintptr_t)NULL;
    *subscribedEvents |= TRX_EventShutdownComplete;
    freeLastStatusMap(pMap);
}

static void processReqLastStatus(TRX_Host_Handle handle, TRX_Request *pReq,
                                 uint64_t *observedEvents, uint64_t *subscribedEvents,
                                 TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig)
{
    LOG_INF( "RF: Processing TRX_Request_LastStatus");

    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_head(&(handle->lastStatusMapQueue));
    TRX_Request_LastStatus *pReqLastStatus = (TRX_Request_LastStatus *)pReq;

    TRX_RequestType reqType = pReqLastStatus->req;

    if((NULL == pMap) || (reqType != pMap->req))
    {
        // This qualifies as a transport error and the driver needs reset
        *observedEvents |= TRX_EventTransportError;
        *subscribedEvents |= TRX_EventTransportError;
        *cb = handle->generalCb;
        goto endProcessLastStatus;
    }

    if(TRX_STATUS_SUCCESS != pReqLastStatus->status)
    {
        *observedEvents |= TRX_EventLastStatusError;
    }

    requestHandler handler = TRX_Host_Handlers_getHandler(reqType);
    if(NULL != handler)
    {
        // Call this request's parser
        handler(handle, pReq, observedEvents, subscribedEvents, cb, pCmdStreamOrConfig);
    }

endProcessLastStatus:
    return;
}

/************************* Extern Function Definitions ************************/

requestHandler TRX_Host_Handlers_getHandler(TRX_RequestType type)
{
    requestHandler handler = NULL;
    if((uint8_t)type < sizeof(requestHandlerIndexMap))
    {
        uint8_t handlerIndex = requestHandlerIndexMap[type];
        handler = requestHandlerMap[handlerIndex];
    }
    return(handler);
}
