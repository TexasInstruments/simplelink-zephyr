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

/*!

@file buffer_manager.c

@brief The Buffer Manager module provides buffers of fixed size without needing
to dynamically allocate memory.

 */
/********************************** Includes **********************************/

/* Standard C Libraries */
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* Module Header Files */
#include "hal/platform_util.h"
#include "utility/utils.h"
#include "utility/buffer_manager/buffer_manager.h"

#if defined(BENCHMARK_BM)
#include <ti/benchmarks/benchmark.h>
#endif

/********************************** Defines ***********************************/

/*********************************** Macros ***********************************/

/*********************************** Enums ************************************/

/********************************** Structs ***********************************/

/* @brief Buffer Unit Struct */
typedef struct Buffer_Manager_bufUnit_t
{
    uint8_t buffer[BUFFER_MANAGER_BUFSIZE]; //!< Buffer to store data
    struct Buffer_Manager_bufUnit_t *next;  //!< Pointer to next buffer unit in
                                            //!< buffer pool
}Buffer_Manager_bufUnit;

/*************************** Variable Declarations ****************************/

/* Buffer pool */
static struct Buffer_Manager_bufUnit_t bufPool[BUFFER_MANAGER_NBUFFERS];

/* Head pointer */
static Buffer_Manager_bufUnit *bufPtr = &bufPool[0];

/* Diagnostic counter: buffers currently held by callers (alloc - free). */
static volatile uint8_t numAllocated = 0U;

/********************************* Prototypes *********************************/

/**************************** Function Definitions ****************************/

void Buffer_Manager_init(void)
{
#if defined(BENCHMARK_BM)
    profile_func(Buffer_Manager_init, true);
#endif
    /* Link buffer units in buffer pool */
    uint8_t i;
    for(i = 0; i < BUFFER_MANAGER_NBUFFERS; i++)
    {
        bufPtr = &bufPool[i];
        bufPtr->next = &bufPool[i + 1U];

        /* Clear buffer */
        (void)memset(bufPtr->buffer, 0, BUFFER_MANAGER_BUFSIZE);
    }
    /* Set tail to point to NULL */
    bufPtr->next = NULL;

    /* Set buffer pointer back to head of buffer pool */
    bufPtr = &bufPool[0];
#if defined(BENCHMARK_BM)
    profile_func(Buffer_Manager_init, false);
#endif
}

uint8_t *Buffer_Manager_getBuffer(void)
{
#if defined(BENCHMARK_BM)
    profile_func(Buffer_Manager_getBuffer, true);
#endif
    Buffer_Manager_bufUnit *tempBufUnit;
    unsigned int key;

    /* Disable preemption while retrieving buffer */
    Plat_Util_startCriticalSection((uintptr_t)&key);

    /* Check if head is NULL */
    if (NULL == bufPtr)
    {
        /* Indicates no buffer units are available */
        tempBufUnit = NULL;
    }
    else
    {
        /* Buffer unit available, get buffer and set to passed in pointer */
        tempBufUnit = bufPtr;
        bufPtr = tempBufUnit->next;
        tempBufUnit->next = NULL;
        numAllocated++;
    }

    /* Restore preemption again */
    Plat_Util_endCriticalSection((uintptr_t)&key);

#if defined(BENCHMARK_BM)
    profile_func(Buffer_Manager_getBuffer, false);
#endif
    return((NULL == tempBufUnit) ? NULL : tempBufUnit->buffer);
}


Buffer_Manager_Status Buffer_Manager_freeBuffer(uint8_t *pBufUnit)
{
#if defined(BENCHMARK_BM)
    profile_func(Buffer_Manager_freeBuffer, true);
#endif
    uint8_t i;
    bool isBufUnitAddr = false;
    Buffer_Manager_Status status = Buffer_Manager_AddrInvalid;
    unsigned int key;

    /* Check if pointer is NULL */
    if(NULL != pBufUnit)
    {
        /* Clear buffer */
        (void)memset(pBufUnit, 0, BUFFER_MANAGER_BUFSIZE);

        /* Disable preemption while freeing buffer */
        Plat_Util_startCriticalSection((uintptr_t)&key);

        /* Check if pointer is from buffer pool before adding back to buffer
         * pool
         */
        for(i = 0; i < BUFFER_MANAGER_NBUFFERS; i++)
        {
            // Compare addresses of each index of buffer pool to input pointer
            if(&bufPool[i] == (Buffer_Manager_bufUnit *)pBufUnit)
            {
                isBufUnitAddr = true;
                status = Buffer_Manager_Success;
                break;
            }
        }

        /* Free buffer if pBufUnit address belongs to buffer pool and if buffer
         * unit has not been freed yet
         */
        if(isBufUnitAddr)
        {
            /* Check if pointer has already been added to buffer pool (to
             * prevent double freeing)
             */
            if(NULL != (bufPool[i].next))
            {
                status = Buffer_Manager_AddrFreed;
            }
            else
            {
                /* Add buffer back to buffer pool and clear buffer */
                bufPool[i].next = bufPtr;

                /* Make buffer unit new head */
                bufPtr = &bufPool[i];
                if(numAllocated > 0U)
                {
                    numAllocated--;
                }
            }
        }

        /* Restore preemption again */
        Plat_Util_endCriticalSection((uintptr_t)&key);
    }

#if defined(BENCHMARK_BM)
    profile_func(Buffer_Manager_freeBuffer, false);
#endif
    return(status);
}

uint8_t Buffer_Manager_getNumAllocated(void)
{
    return(numAllocated);
}
