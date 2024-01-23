/******************************************************************************

 @file  hci_event_internal.h

 @brief This file contains the HCI Event internal h file

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2009 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifndef HCI_EVENT_INTERNAL_H
#define HCI_EVENT_INTERNAL_H

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * INCLUDES
 */


/*******************************************************************************
 * MACROS
 */
#define LL_PHY_UPDATE_TYPE_CODED 3
// Note 7.7.65.12 - LE Phy Update Complete Event - LE Coded PHY type defined in TX_PHY and RX_PHY should be 0x03
#define LL_ConvertPhy(phy) ((phy == LL_PHY_CODED)? LL_PHY_UPDATE_TYPE_CODED : phy)
/*******************************************************************************
 * CONSTANTS
 */
#define HCI_EVENT_DATA_OFFSET    3
#define HCI_LE_EVENT_CODE_INDEX  3
#define HCI_EVENT_LE_DATA_OFFSET 4
// CTE report event samples indexing
#define HCI_CTE_SAMPLES_COUNT_REF_PERIOD         (8)    //number of samples in referece period according to spec
#define HCI_CTE_FIRST_SAMPLE_IDX_REF_PERIOD      (1)    //reference period start samples index for sample rate 1Mhz,2Mhz and 3Mhz
#define HCI_CTE_FIRST_SAMPLE_IDX_REF_PERIOD_4MHZ (0)    //reference period start samples index for sample rate 4Mhz
#define HCI_CTE_FIRST_SAMPLE_IDX_SLOT_1US        (37)   //1us start samples index for sample rate 1Mhz,2Mhz and 3Mhz
#define HCI_CTE_FIRST_SAMPLE_IDX_SLOT_2US        (45)   //2us start samples index for sample rate 1Mhz,2Mhz and 3Mhz
#define HCI_CTE_FIRST_SAMPLE_IDX_SLOT_1US_4MHZ   (36)   //1us start samples index for sample rate 4Mhz
#define HCI_CTE_FIRST_SAMPLE_IDX_SLOT_2US_4MHZ   (44)   //2us start samples index for sample rate 4Mhz

// CTE report event samples offsets
#define HCI_CTE_SAMPLE_JUMP_REF_PERIOD           (4)    //peek 1 sample every 4 samples
#define HCI_CTE_SAMPLE_JUMP_SLOT_1US             (8)    //peek 1 sample every 8 samples
#define HCI_CTE_SAMPLE_JUMP_SLOT_2US             (16)   //peek 1 sample every 16 samples

#define HCI_CTE_MAX_SAMPLES_PER_EVENT            (96)   //max samples data length in one event
#define HCI_CTE_MAX_RF_BUFFER_SIZE               (512)  //first buffer size (MCE RAM)
#define HCI_CTE_MAX_RF_EXT_BUFFER_SIZE           (512)  //second buffer size (RFE RAM)
#define HCI_CTE_SAMPLE_RATE_4MHZ                 (4)
#define HCI_CTE_SAMPLE_RATE_1MHZ                 (1)

#define HCI_PERIODIC_ADV_REPORT_MAX_DATA         (0xFF - HCI_PERIODIC_ADV_REPORT_EVENT_LEN)
#define HCI_PERIODIC_ADV_REPORT_DATA_INCOMPLETE  (1)
/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */



#ifdef __cplusplus
}
#endif

#endif /* HCI_EVENT_INTERNAL_H */
