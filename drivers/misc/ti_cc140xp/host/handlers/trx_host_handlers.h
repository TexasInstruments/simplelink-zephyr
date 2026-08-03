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

#ifndef TRX_HOST_HANDLERS_H_
#define TRX_HOST_HANDLERS_H_

#ifdef __cplusplus
extern "C" {
#endif

/********************************** Includes **********************************/

/* Standard C Libraries */
#include <stdint.h>
#include <stdbool.h>

/* Module Header Files */
#include "TRX.h"
#include "trx_host.h"
#include "private/trx_host_private.h"

//*****************************************************************************
//
//! \addtogroup TRX_HOST_HANDLERS
//! \brief Private definitions for processing requests from the TRX
//!
//! @{
//
//*****************************************************************************

/********************************** Defines ***********************************/

/*********************************** Enums ************************************/

/********************************** Typedefs **********************************/

/**
 * @brief Processes a TRX_Request received from the TRX and sets the
 * value of the pointers passed in as a return
 *
 * @param[in] handle Handle to the TRX Host module
 * @param[in] pReq Pointer to the TRX_Request received
 * @param[out] observedEvents Pointer that gets set to the events observed
 *                            when processing the TRX_Request
 * @param[out] subscribedEvents Pointer that gets set to the events subscribed
 *                              to when processing the TRX_Request
 * @param[out] cb Pointer that gets set to the application callback
 * @param[out] pCmdStreamOrConfig Pointer to the associated cmd, stream, or config
 */
typedef void (*requestHandler)(TRX_Host_Handle handle, TRX_Request *pReq,
                              uint64_t *observedEvents, uint64_t *subscribedEvents,
                              TRX_Host_Callback *cb, uintptr_t *pCmdStreamOrConfig);

/********************************** Structs ***********************************/

/***************************** Extern Prototypes ******************************/

/**
 * @brief Returns a pointer to the handler function for the provided request type
 *
 * @param type The type of request
 * @return requestHandler NULL if no handler found, otherwise pointer to the handler
 */
extern requestHandler TRX_Host_Handlers_getHandler(TRX_RequestType type);

//*****************************************************************************
//
//! Close the Doxygen group.
//! @}
//
//*****************************************************************************

#ifdef __cplusplus
}
#endif

#endif // TRX_HOST_HANDLERS_H_

