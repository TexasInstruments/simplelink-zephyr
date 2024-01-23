/******************************************************************************

 @file  ll_test_mode.h

 @brief This file contains the Link Layer (LL) types, constants,
        API's etc. to support LL-only Test Mode.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2009 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*********************************************************************
 *
 * WARNING!!!
 *
 * THE API'S FOUND IN THIS FILE ARE FOR INTERNAL STACK USE ONLY!
 * FUNCTIONS SHOULD NOT BE CALLED DIRECTLY FROM APPLICATIONS, AND ANY
 * CALLS TO THESE FUNCTIONS FROM OUTSIDE OF THE STACK MAY RESULT IN
 * UNEXPECTED BEHAVIOR.
 *
 */

#ifndef LL_TEST_MODE_H
#define LL_TEST_MODE_H

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * INCLUDES
 */
#include "comdef.h"
#include "../../ll/inc/ll.h"

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

// Test Mode
#define NUM_BYTES_FOR_CHAN_MAP               5
#define ENC_RAND_LEN                         8
#define ENC_EDIV_LEN                         2
#define ENC_LTK_LEN                          16
#define ENC_IV_LEN                           8
#define ENC_SKD_LEN                          16
#define ENC_SK_LEN                           16

/*******************************************************************************
 * TYPEDEFS
 */

/*
** Test Mode Data
*/
typedef struct
{
  // Encryption stuff
  struct
  {
    uint8 IV[ ENC_IV_LEN ];                         // combined central and peripheral IV values concatenated
    uint8 SKD [ ENC_SKD_LEN ];                      // combined central and peripheral SKD values concatenated
    uint8 RAND[ ENC_RAND_LEN ];                     // random vector from Central
    uint8 EDIV[ ENC_EDIV_LEN ];                     // encrypted diversifier from Central
    uint8 SK[ ENC_SK_LEN ];                         // session key derived from LTK and SKD
    uint8 LTK[ ENC_LTK_LEN ];
  } encInfo;

  // Central stuff
  uint8  winSize;                                    // specify a window size value
  uint16 winOffset;                                  // specify a window offset value
  uint8  updateChanMap[ NUM_BYTES_FOR_CHAN_MAP ];    // specify a data channel map update
  uint8  SCA;                                        // specify a sleep clock accuracy
  uint16 instant;                                    // specify the number of events before a control update takes place
  uint8  scanChanOverride;                           // flag to indicate scan channel is being overridden
  uint8  scanChanValue;                              // scan channel to be used during an override
  uint8  flipTxMIC;                                  // flag to indicate a MIC error should be forced

  // Peripheral stuff

} testInfo_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * Test Callbacks
 */

/*******************************************************************************
 * @fn          LL_TEST_ConnectionEventCback Callback
 *
 * @brief       This Callback is used by the LL for test support. It provides
 *              the current connection ID and the current event count for test
 *              software.
 *
 * input parameters
 *
 * @param       connId       - The current connection ID.
 * @param       currentEvent - The current event count.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
extern void LL_TEST_ConnectionEventCback( uint16 connId,
                                          uint16 currentEvent );


/*******************************************************************************
 * @fn          LL_TEST_AdvEventCback Callback
 *
 * @brief       This Callback is used by the LL for test support. It provides
 *              an opportunity for the test software to make updates while
 *              advertising.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
extern void LL_TEST_AdvEventCback( void );


/*******************************************************************************
 * @fn          LL_TEST_ScanEventCback Callback
 *
 * @brief       This Callback is used by the LL for test support. It provides
 *              an opportunity for the test software to make updates while
 *              scanning.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
extern void LL_TEST_ScanEventCback( void );


/*******************************************************************************
 * @fn          LL_TEST_PostScanEventCback Callback
 *
 * @brief       This Callback is used by the LL for test support. It provides
 *              an opportunity for the test software to make updates while
 *              scanning after the Task Done post-processing has completed.
 *              This is necessary for test cases that require the scanner to
 *              stop then start again, which should not be done within the
 *              context of Task Done post-processing.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
extern void LL_TEST_PostScanEventCback( void );


#ifdef __cplusplus
}
#endif

#endif /* LL_TEST_MODE_H */
