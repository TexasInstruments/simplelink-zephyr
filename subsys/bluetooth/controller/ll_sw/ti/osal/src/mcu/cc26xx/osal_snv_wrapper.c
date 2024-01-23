/******************************************************************************

 @file  osal_snv_wrapper.c

 @brief This module defines the OSAL simple non-volatile memory functions as a
        wrapper to NVOCMP(NV On Chip multi Page)

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2018 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#include "icall_user_config.h"
#include "osal_snv.h"
#include "nvintf.h"
#include "nvocmp.h"

#ifndef SYSTEM_ID
#define SYSTEM_ID NVINTF_SYSID_BLE
#endif

#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE 0x2000
#endif

#ifndef RTLS_PASSIVE
// Get nvintf function pointer struct from global driver table
extern const drvTblPtr_t driverTable;
#endif

NVINTF_nvFuncts_t nvFptrs;

// Convert a threshold percentage to bytes.
#define THRESHOLD2BYTES(x) ((FLASH_PAGE_SIZE) - (((FLASH_PAGE_SIZE)*(x))/100))

/*********************************************************************
 * @fn      osal_snv_init
 *
 * @brief   Initialize NV service.
 *
 * @param   none
 *
 * @return  NVINTF_SUCCESS or other nvintf error code.
 */
uint8 osal_snv_init( void )
{
#ifndef CC23X0
#ifndef RTLS_PASSIVE
  // Get function pointers from driver table
  nvFptrs = *((NVINTF_nvFuncts_t *)(driverTable.nvintfStructPtr));
#endif
#endif

  NVOCMP_loadApiPtrsMin(&nvFptrs);
  return nvFptrs.initNV(NULL);
}

/*********************************************************************
 * @fn      osal_snv_read
 *
 * @brief   Read data from NV.
 *
 * @param   id   - Valid NV item Id.
 * @param   len  - Length of data to read.
 * @param   *pBuf - Data is read into this buffer.
 *
 * @return  NVINTF_SUCCESS or other nvintf error code.
 */
uint8 osal_snv_read( osalSnvId_t id, osalSnvLen_t len, void *pBuf)
{
  NVINTF_itemID_t nv_id;
  nv_id.itemID   = id;
  nv_id.subID    = 0;
  nv_id.systemID = SYSTEM_ID;

  return nvFptrs.readItem(nv_id, 0, len, pBuf);
}

/*********************************************************************
 * @fn      osal_snv_write
 *
 * @brief   Write a data item to NV.
 *
 * @param   id   - Valid NV item Id.
 * @param   len  - Length of data to write.
 * @param   *pBuf - Data to write.
 *
 * @return  NVINTF_SUCCESS or other nvintf error code.
 */
uint8 osal_snv_write( osalSnvId_t id, osalSnvLen_t len, void *pBuf)
{
  NVINTF_itemID_t nv_id;
  nv_id.itemID   = id;
  nv_id.subID    = 0;
  nv_id.systemID = SYSTEM_ID;

  return nvFptrs.writeItem(nv_id, len, pBuf);
}

/*********************************************************************
 * @fn      osal_snv_compact
 *
 * @brief   Compacts NV if its usage has reached a specific threshold.
 *
 * @param   threshold - compaction threshold.
 *
 * @return  NVINTF_SUCCESS if successful,
 *          NVINTF_FAILURE if failed, or
 *          NVINTF_BADPARAM if threshold invalid.
 */
uint8 osal_snv_compact( uint8 threshold )
{
  // convert percentage to approximate byte threshold.
  if (threshold <= 100)
  {
    return nvFptrs.compactNV(THRESHOLD2BYTES(threshold));
  }

  return NVINTF_BADPARAM;
}
