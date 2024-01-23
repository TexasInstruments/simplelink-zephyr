/******************************************************************************

 @file  osal_bufmgr.h

 @brief This file contains the buffer management definitions.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2008 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifndef OSAL_BUFMGR_H
#define OSAL_BUFMGR_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */

/*********************************************************************
 * CONSTANTS
 */

/*
 * Allocate a block of memory.
 */
#define BM_alloc                osal_bm_alloc

/*
 * Free a block of memory.
 */
#define BM_free                 osal_bm_free

/*********************************************************************
 * VARIABLES
 */


/*********************************************************************
 * MACROS
 */


/*********************************************************************
 * TYPEDEFS
 */
// pre release buffer callback function type
typedef void (*bm_notify_t)(void);

/*********************************************************************
 * VARIABLES
 */

/*********************************************************************
 * FUNCTIONS
 */

/*
 * Allocate a block of memory.
 */
extern void *osal_bm_alloc( uint16 size );

/*
 * Add or remove header space for the payload pointer.
 */
extern void *osal_bm_adjust_header( void *payload_ptr, int16 size );

/*
 * Add or remove tail space for the payload pointer.
 */
extern void *osal_bm_adjust_tail( void *payload_ptr, int16 size );

/*
 * Free a block of memory.
 */
extern void osal_bm_free( void *payload_ptr );

/*
 * Register pre release buffer callback.
 */
extern void osal_bm_reg_callback(bm_notify_t pCBack);
/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* OSAL_BUFMGR_H */
