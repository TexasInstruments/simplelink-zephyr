/******************************************************************************

 @file  osal_memory_icall.c

 @brief OSAL Heap Memory management functions implemented on top of
        ICall primitive service.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2013 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/* ------------------------------------------------------------------------------------------------
 *                                          Includes
 * ------------------------------------------------------------------------------------------------
 */

#include "comdef.h"
#include "osal.h"
#include "osal_memory.h"

#ifdef USE_ICALL
  #include <icall.h>
#endif /* USE_ICALL */

/**************************************************************************************************
 * @fn          osal_mem_alloc
 *
 * @brief       This function implements the OSAL dynamic memory allocation functionality.
 *
 * input parameters
 *
 * @param size - the number of bytes to allocate from the HEAP.
 *
 * output parameters
 *
 * None.
 *
 * @return      Pointer to dynamically allocated memory, or NULL if operation
 *              failed.
 */
#ifdef DPRINTF_OSALHEAPTRACE
void *osal_mem_alloc_dbg( uint16 size, const char *fname, unsigned lnum )
#else
void *osal_mem_alloc( uint16 size )
#endif /* DPRINTF_OSALHEAPTRACE */
{
  uint8 *buf;

  // Attempt to allocate memory.
  buf = ICall_malloc(size);

#ifdef MEM_ALLOC_ASSERT
  // If allocation failed, assert.
  if ( buf == NULL )
  {
    // When EXT_HAL_ASSERT is defined, set the assert cause.  Otherwise, trap
    // the assert with argument FALSE.
#ifdef EXT_HAL_ASSERT
    HAL_ASSERT( HAL_ASSERT_CAUSE_OUT_OF_MEMORY );
#else /* !EXT_HAL_ASSERT */
    HAL_ASSERT( FALSE );
#endif /* EXT_HAL_ASSERT */
  }
#endif /* MEM_ALLOC_ASSERT */

  // Return pointer to buffer.
  return (void *)buf;
}

/**************************************************************************************************
 * @fn          osal_mem_allocLimited
 *
 * @brief       This function implements the OSAL dynamic memory allocation functionality.
 *              However, first a check is done to ensure that enough heap will remain after the allocation
 *
 * input parameters
 *
 * @param size - the number of bytes to allocate from the HEAP.
 *
 * output parameters
 *
 * None.
 *
 * @return      Pointer to dynamically allocated memory, or NULL if operation
 *              failed.
 */
void *osal_mem_allocLimited( uint16 size )
{
  uint8 *buf;

  // Attempt to allocate memory.
  buf = ICall_mallocLimited(size);

#ifdef MEM_ALLOC_ASSERT
  // If allocation failed, assert.
  if ( buf == NULL )
  {
    // When EXT_HAL_ASSERT is defined, set the assert cause.  Otherwise, trap
    // the assert with argument FALSE.
#ifdef EXT_HAL_ASSERT
    HAL_ASSERT( HAL_ASSERT_CAUSE_OUT_OF_MEMORY );
#else /* !EXT_HAL_ASSERT */
    HAL_ASSERT( FALSE );
#endif /* EXT_HAL_ASSERT */
  }
#endif /* MEM_ALLOC_ASSERT */

  // Return pointer to buffer.
  return (void *)buf;
}

/**************************************************************************************************
 * @fn          osal_mem_free
 *
 * @brief       This function implements the OSAL dynamic memory de-allocation functionality.
 *
 * input parameters
 *
 * @param ptr - A valid pointer (i.e. a pointer returned by osal_mem_alloc()) to the memory to free.
 *
 * output parameters
 *
 * None.
 *
 * @return      None.
 */
#ifdef DPRINTF_OSALHEAPTRACE
void osal_mem_free_dbg(void *ptr, const char *fname, unsigned lnum)
#else /* DPRINTF_OSALHEAPTRACE */
void osal_mem_free(void *ptr)
#endif /* DPRINTF_OSALHEAPTRACE */
{
  ICall_free(ptr);
}

/**************************************************************************************************
*/
