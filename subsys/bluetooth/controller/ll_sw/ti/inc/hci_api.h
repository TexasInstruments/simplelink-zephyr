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

 @file  _hci_api.h

 @brief HCI layer interface APIs

 *****************************************************************************/

#ifndef HCI_API_H_
#define HCI_API_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "comdef.h"
#include <stddef.h>

/*******************************************************************************
 * TYPEDEFS
 */

typedef struct hciController2HostCallbacks
{
  int (*send)(uint8 *pHciPkt, uint16 pktLen);
} hciController2HostCallbacks_t;

/*******************************************************************************
 * API FUNCTIONS
 */

/*******************************************************************************
 * @fn          HCI_Controller2HostCallbacksInit
 *
 * @brief       This function initializes callback structure
 *
 * input parameters
 *
 * @param       hciController2HostCallbacks_t - A pointer to callback functions structure.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      SUCCESS / FAILURE.
 */
uint32 HCI_Controller2HostCallbacksInit( hciController2HostCallbacks_t *pController2HostCallbacks);

/*******************************************************************************
 * @fn          HCI_ControllerToHostRegisterCb
 *
 * @brief       This function register callback function to HCI events
 *
 * input parameters
 *
 * @param       hciController2HostCallbacks_t pCbs - A pointer to callback functions structure.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      SUCCESS / FAILURE.
 */
uint32 HCI_ControllerToHostRegisterCb( const hciController2HostCallbacks_t *pCbs );

/********************************************************************************
 * @fn      HCI_HostToController
 *
 * @brief   Send raw HCI packet to the controller.
 *
 * @param   pHciPkt - A pointer to a raw buffer of HCI command or data packet.
 * @param   pktLen  - The hci packet length
 *
 * @return  0 for success, negative number for error.
 */
int HCI_HostToController(uint8_t *pHciPkt, uint16_t pktLen);

#ifdef __cplusplus
}
#endif

#endif /* HCI_API_H_ */
