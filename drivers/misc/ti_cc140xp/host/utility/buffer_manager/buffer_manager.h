/******************************************************************************
 Group: CMCU LPRF
 Target Device: cc13xx_cc26xx

 ******************************************************************************
 
 Copyright (c) 2021-2026, Texas Instruments Incorporated
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

#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

/********************************** Includes **********************************/
#include "TRX.h"
#include "transport/spi_transport.h"
#include "hal/platform.h"

//*****************************************************************************
//
//! \addtogroup UTILITY
//! @{
//! \defgroup BUFFER_MANAGER
//! \brief Buffer management simplified
//!
//! @details The buffer manager functions provide users with buffers of fixed
//! size without having to dynamically allocate memory with functions such as
//! malloc. The buffer manager uses a linked list of buffer units to do so.
//! This document describes the features and usage of the buffer manager
//! functions.
//!
//! To use the Buffer Manager, include the header file as follows:
//! @code
//! #include <ti/trx/host/utility/buffer_manager/buffer_manager.h>
//! @endcode
//!
//! The key features of this document are:
//! - @ref Buffer_Manager_init          "Buffer Manager Initialization"
//! - @ref Buffer_Manager_get_buffer    "Buffer Manager Get Buffer"
//! - @ref Buffer_Manager_free_buffer   "Buffer Manager Free Buffer"
//!
//! @anchor Buffer_Manager_init
//! Buffer Manager Initialization
//! ======================
//! The module requires initialization to link buffer units next pointer to point
//! to the next buffer unit in the buffer pool. Initialization will also use memset
//! to clear each buffer unit's buffer with the value of 0 to ensure buffers no
//! unintended data is in buffer when provided to users. An example of how to
//! initialize the module is shown below.
//! @code
//! Buffer_Manager_init();
//! @endcode
//! To change the size of the buffer pool, BUFFER_MANAGER_NBUFFERS should be
//! modified as shown in the example below.
//! @code
//! #define BUFFER_MANAGER_NBUFFERS    (7U)
//! @endcode
//! To change the variable buffer size of the Buffer_Manager_bufUnit structure,
//! BUFFER_MANAGER_BUFSIZE should be modified as shown in the example below.
//! @code
//! #define BUFFER_MANAGER_BUFSIZE     (100U)
//! @endcode
//!
//! @anchor Buffer_Manager_get_buffer
//! Buffer Manager Get Buffer
//! ======================
//! The module is used to get buffers of fixed size. Upon request, the buffer
//! manager sets the input parameter to point to the buffer at the top of the
//! buffer pool. An example of how to get a buffer and set the buffer is shown
//! below.
//! @code
//! // Get buffer
//! Buffer_Manager_bufUnit *pBufUnit;
//! Buffer_Manager_Status status = Buffer_Manager_getBuffer(&pBufUnit);
//! // Check Buffer_Manager_Status for success before using buffer
//! assert(Buffer_Manager_Success == status);
//! // Use buffer, copy payload into buffer
//! uint8_t examplePayload[BUFFER_MANAGER_BUFSIZE] = {0, 1, 2, 3};
//! memcpy(pBufUnit->buffer, examplePayload, sizeof(examplePayload));
//! @endcode
//!
//! @anchor Buffer_Manager_free_buffer
//! Buffer Manager Free Buffer
//! ======================
//! The module is used to free buffers of fixed size after the module that called
//! Buffer_Manager_getBuffer is done utilizing the buffer. An example of how to
//! free a buffer is shown below.
//! @code
//! // Get buffer
//! Buffer_Manager_bufUnit *pBufUnit;
//! Buffer_Manager_Status status = Buffer_Manager_getBuffer(&pBufUnit);
//! //Free buffer
//! status = Buffer_Manager_freeBuffer(&pBufUnit);
//! // Check Buffer_Manager_Status to ensure buffer was freed
//! assert(Buffer_Manager_Success == status);
//! @endcode
//! @{

//*****************************************************************************

/********************************** Defines ***********************************/
 /*!
  *  @brief Buffer pool size
  */
 #define BUFFER_MANAGER_NBUFFERS    (24U)

 /*!
  *  @brief Size of buffer in buffer unit
  *  We need to allocate TRX_SPIBLOCK_SIZE + size of metadata in SPI containers needed for buffer management 
  *  sizeof(SPI_Transport_Container) - sizeof(TRX_SpiBlock) accounts for the metadata of SPI containers needed for buffer management
  */
 #define BUFFER_MANAGER_BUFSIZE     (TRX_SPIBLOCK_SIZE + sizeof(SPI_Transport_Container) - sizeof(TRX_SpiBlock))

/*********************************** Macros ***********************************/

/*********************************** Enums ************************************/

/*!
 *  @brief Status codes for Buffer Manager functions.
 *
 *  Buffer_Manager_Status is reported as a return value for Buffer Manager get
 *  and free buffer functions.
*/
typedef enum
{
    Buffer_Manager_Success       = 0, //!< Function returned successfully
    Buffer_Manager_AddrInvalid   = 1, //!< Invalid address not from buffer pool
    Buffer_Manager_AddrFreed     = 2, //!< Address already freed
    Buffer_Manager_Unavailable   = 3  //!< Buffer units unavailable
} Buffer_Manager_Status;

/********************************** Structs ***********************************/


/********************************* Prototypes *********************************/

/*! @brief Initialize Buffer Manager
 *
 *  Initialize the buffer pool and link buffer units together in buffer pool
 *
 *  @note       Calling context : Task, HWI, SWI
 */
extern void Buffer_Manager_init(void);

/*! @brief Get buffer from Buffer Manager
 *
 *  Get buffer from buffer pool. Returns a uint8_t pointer to a buffer from the
 *  buffer pool
 *
 *  @note       Calling context : Task, HWI, SWI
 *
 *  @return     uint8_t pointer to buffer. NULL indicates failed get buffer.
 */
extern uint8_t *Buffer_Manager_getBuffer(void);

/*! @brief Free buffer back to Buffer Manager
 *
 *  Free buffer back to buffer pool if input pointer is a valid address in the
 *  buffer pool and if it has not already been freed.
 *
 *  @note       Calling context : Task, HWI, SWI
 *
 *  @param[in]  pBufUnit  pointer to buffer
 *
 *  @return     Buffer_Manager_Status indicates if free was successful
 */
extern Buffer_Manager_Status Buffer_Manager_freeBuffer(uint8_t *pBufUnit);

/*! @brief Get number of currently allocated buffers (diagnostic).
 *
 *  Returns the count of buffers currently held by callers (allocated minus
 *  freed). Useful for instrumenting pool exhaustion.
 *
 *  @note       Calling context : Task, HWI, SWI
 *
 *  @return     Number of buffers currently in use.
 */
extern uint8_t Buffer_Manager_getNumAllocated(void);

#ifdef __cplusplus
}
#endif

//*****************************************************************************
//
//! Close the Doxygen group.
//! @}
//
//! Close the Doxygen group.
//! @}
//
//*****************************************************************************

#endif /* BUFFER_MANAGER_H */
