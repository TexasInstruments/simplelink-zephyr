/******************************************************************************

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2024 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
/******************************************************************************

 @file  ble_init.h

 @brief BLE initialization interface APIs

 *****************************************************************************/

#ifndef BLE_INIT_H
#define BLE_INIT_H

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * INCLUDES
 */
#include "hal_types.h"
#include "hal_assert.h"
#include "hci_api.h"

/********************************************************************************
* DEFINES
* */
#define BLE_INIT_NO_WAIT           0
#define BLE_INIT_WAIT_FOREVER      ~(0)

/*********************************************************************
 * TYPEDEFS
 */
typedef struct bleServicesParams
{
  uint32 bleServices;                    // Bitwise flags for services initialization.
  uint32 syncInitTimeoutTicks;           // Timeout in Ticks for the API to wait for the initialization
                                         // sequence to complete.
                                         // BLE_INIT_NO_WAIT - the function will return immediately.
                                         // BLE_INIT_WAIT_FOREVER - the function will return only after the initialization is completed
                                         // Other values will determine a specific timeout for synchronous initialization sequence.
  hciControllerToHostCallbacks_t hciCbs; // Callbacks for the HCI
  assertCback_t assertCallback;          // Callback for user defined assert handling
} bleServicesParams_t;

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * FUNCTIONS
 */


/********************************************************************************
 * @fn            BLE_ServicesInit
 *
 * @brief         The `BLE_ServicesInit` function is responsible for initializing
 *                the BLE services based on the input parameters specified
 *                in `pServiceParams`. This function offers an option for a synchronous
 *                initialization sequence, meaning that the calling task will
 *                be blocked until the stack indicates that the initialization
 *                is complete.
 *                If the initialization fails, `BLE_ServicesInit()`
 *                will return a failure status.
 *
 * input parameters
 *
 * @param         pServiceParams - pointer to BLE Params initialization options
 *
 * output parameters
 *
 * @return        SUCCESS / FAILURE.
 *
 * */
uint32 BLE_ServicesInit(const bleServicesParams_t *pServiceParams);

/********************************************************************************
 * @fn            BLE_ServicesParamsInit
 *
 * @brief         The `BLE_ServicesParamsInit` function provides a default
 *                configuration for the bleServicesParams_t structure
 *
 * input parameters
 *
 * @param         pServiceParams - pointer to BLE Services initialization options
 * @param         size           - size of the initialization structure
 *
 * output parameters
 *
 * @return        SUCCESS - in case pServiceParams initialization succeed
 *                FAILURE in case of
 *                   - Compatibility/versions mismatch (the check is done based
 *                     on size of the bleServicesParams_t).
 *                   - Parameters validation
 *
 * */
uint32 BLE_ServicesParamsInit(bleServicesParams_t *pServiceParams, size_t size);

/*******************************************************************************
 * @fn          RegisterAssertCback
 *
 * @brief       This routine registers the Application's assert handler.
 *
 * input parameters
 *
 * @param       appAssertHandler - Application's assert handler.
 *
 * output parameters
 *
 * @param       appAssertHandler - callback to handle the assert in
 *                                 application level.
 *
 * @return      None.
 */
void RegisterAssertCback(assertCback_t appAssertHandler);

#ifdef __cplusplus
}
#endif

#endif /* BLE_INIT_H */
