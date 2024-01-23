/******************************************************************************

 @file  osal_nv.h

 @brief This module defines the OSAL non-volatile memory functions.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2004 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifndef OSAL_NV_H
#define OSAL_NV_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */

#include "hal_types.h"

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * FUNCTIONS
 */

/*
 * Initialize NV service
 */
extern void osal_nv_init( void *p );

/*
 * Initialize an item in NV
 */
extern uint8 osal_nv_item_init( uint16 id, uint16 len, void *buf );

/*
 * Read an NV attribute
 */
extern uint8 osal_nv_read( uint16 id, uint16 offset, uint16 len, void *buf );

/*
 * Write an NV attribute
 */
extern uint8 osal_nv_write( uint16 id, uint16 offset, uint16 len, void *buf );

/*
 * Get the length of an NV item.
 */
extern uint16 osal_nv_item_len( uint16 id );

/*
 * Delete an NV item.
 */
extern uint8 osal_nv_delete( uint16 id, uint16 len );

#if defined ( OSAL_NV_EXTENDED )
/*
 * Initialize an item in NV (extended format)
 */
extern uint8 osal_nv_item_init_ex( uint16 id, uint16 subId, uint16 len, void *buf );

/*
 * Read an NV attribute (extended format)
 */
extern uint8 osal_nv_read_ex( uint16 id, uint16 subId, uint16 offset, uint16 len, void *buf );

/*
 * Write an NV attribute (extended format)
 */
extern uint8 osal_nv_write_ex( uint16 id, uint16 subId, uint16 offset, uint16 len, void *buf );

/*
 * Get the length of an NV item (extended format).
 */
extern uint16 osal_nv_item_len_ex( uint16 id, uint16 subId );

/*
 * Delete an NV item (extended format).
 */
extern uint8 osal_nv_delete_ex( uint16 id, uint16 subId, uint16 len );

#endif // OSAL_NV_EXTENDED

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* OSAL_NV.H */
