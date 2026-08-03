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

#ifndef UTILS_H_
#define UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

/********************************** Includes **********************************/

/* Standard C Libraries */
#include <stdint.h>
#include <stdbool.h>

/* Module Header Files */

//*****************************************************************************
//
//! \defgroup UTILITY
//! \brief Common utility functions that are used throughout the host driver
//! @{
//
//*****************************************************************************

/********************************** Defines ***********************************/

/** Packed structure for ensuring optimization does not reorder structure
 *  members.
 *
 *  @note User must provide a definition for this macro if using a compiler
 *        other than the ones below.
 */
#if defined (__IAR_SYSTEMS_ICC__)
  #define TYPEDEF_STRUCT_PACKED        __packed typedef struct  //!< IAR specific definition for packed structures
#elif defined(__TI_COMPILER_VERSION) || defined(__TI_COMPILER_VERSION__) || defined(__clang__)
  #define TYPEDEF_STRUCT_PACKED        typedef struct __attribute__((packed))       //!< TI compiler specific definition for packed structures
#elif defined (__GNUC__)
  #define TYPEDEF_STRUCT_PACKED        typedef struct __attribute__((__packed__))   //!< GCC specific definition for packed structures
#else
  #error "Missing definition of TYPEDEF_STRUCT_PACKED for this compiler."
#endif

/** Macro to cause a compile error when argument (condition) is true.
 *
 *  How does it work:
 *  # attempts to create a char array with either size 1 or -1;
 *    size of -1 causes a compile error.
 *  # sizeof(char[]) returns the size of the array but prevents the
 *    creation of an actual char array.
 *  # (void) ahead of sizeof() tells the compiler this is useless code
 *    so it wont throw a warning.
 *  # 1 - 2*!!(condition) converts all non-zero values to 1.
 *
 *  @note See link below on static allocation of opaque data types.
 *  <a href="linkURL">https://scaryreasoner.wordpress.com/2009/02/28/checking-sizeof-at-compile-time/</a>
 */
#define BUILD_BUG_ON(aCondition) ((void)sizeof(char[1 - 2*!!(aCondition)]))

#if defined(DEBUG)
/** Increment a debug counter. */
#define DEBUG_COUNTER_INC(x)    x++
#else
#define DEBUG_COUNTER_INC(x)
#endif

/** Infinite loop without body */
#define INFINITE_LOOP()       for(;;)

/*********************************** Enums ************************************/

/********************************** Typedefs **********************************/

/********************************** Structs ***********************************/

/********************************** Static Inlines ***********************************/
/** Function to increment a variable that is less than a byte in width.
 *
 *  @param[in, out] pX Pointer to the variable to increment
 *  @param[in] wrapValue Value at which to wrap around to 0
 */
static inline void incrementAndWrap(uint8_t *pX, uint8_t wrapValue)
{
    *pX = (*pX + 1) % (wrapValue);
}

/***************************** Extern Prototypes ******************************/

/** Test an expression is true, else execute a failsafe mechanism
 *
 *  @note  This function will spin in an infinite loop on failure; it
 *         has a weak binding and can be overwritten by the user.
 *
 *  @param[in] expr logical expression to test.
 *  @param[in] id   ID assigned to the point of failure.
 */
extern void Assert_isTrue(bool expr, uint8_t id);

//*****************************************************************************
//
//! Close the Doxygen group.
//! @}
//
//*****************************************************************************

#ifdef __cplusplus
}
#endif

#endif // UTILS_H_
