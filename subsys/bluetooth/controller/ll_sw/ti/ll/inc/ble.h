/******************************************************************************

 @file  ble.h

 @brief This file contains the data structures and APIs for CC26xx
        RF Core Firmware Specification for Bluetooth Low Energy.

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

#ifndef BLE_H
#define BLE_H

/*******************************************************************************
 * INCLUDES
 */
#include <ti/drivers/rcl/RCL.h>
#include <ti/drivers/rcl/commands/ble5.h>
#include "ll_al.h"
#include "ll_common.h"
#include "ll_config.h"

/*******************************************************************************
 * CONSTANTS
 */
#define RCL_329                                                               // This is the requirement ticket number that was open to the RCL
                                                                              // to provide an API to update a peer device RPA address
                                                                              // when LL_INIT_AL_POLICY_USE_PEER_ADDR filter policy is used
#define RCL_BUFFER_MAX_HEADER_PAD_BYTES 2                                     // padding of 2 bytes will be located before the packet header
#define RCL_BUFFER_MAX_PAD_BYTES        (RCL_BUFFER_MAX_HEADER_PAD_BYTES + 1) // 1 is already part of the RCL buffer struct
#define RCL_BUFFER_RX_HEADER_ENTRY_SIZE 6                                     // length + pad num + 3 pad bytes
#define RCL_HEADER_BYTE                 1                                     // Header byte location in the RCL packet
#define RCL_NUM_ADV_RX                  3                                     // The RX size for the advertiser should be multiplied based on the maximum number of adv indications
#ifdef RCL_329
#define RCL_PEER_ADDR_INDEX             0                                     // This is used when initiator uses filter policy is LL_INIT_AL_POLICY_USE_PEER_ADDR
#endif

// Advertisement Data Type
#define BLE_ADV_DATA_TYPE              0
#define BLE_SCAN_RSP_DATA_TYPE         1

// Advertising Configuration
#define ADV_CFG_INCLUDE_BAD_LEN_MSG    0
#define ADV_CFG_DISCARD_BAD_LEN_MSG    1
#define ADV_CFG_DISABLE_CHAN_SELECT    0
#define ADV_CFG_ENABLE_CHAN_SELECT     1
#define ADV_CFG_DISABLE_PRIV_IGN_MODE  0
#define ADV_CFG_ENABLE_PRIV_IGN_MODE   1
#define ADV_CFG_DISABLE_RPA_MODE       0
#define ADV_CFG_ENABLE_RPA_MODE        1

// Scan Configuration
#define SCAN_CFG_AL_POLICY_ANY         0
#define SCAN_CFG_AL_POLICY_USE_AL      1
#define SCAN_CFG_PASSIVE_SCAN          0
#define SCAN_CFG_ACTIVE_SCAN           1
#define SCAN_CFG_DISABLE_RPA_FILTER    0
#define SCAN_CFG_ENABLE_RPA_FILTER     1
#define SCAN_CFG_ALLOW_ILLEGAL_LEN     0
#define SCAN_CFG_DISCARD_ILLEGAL_LEN   1
#define SCAN_CFG_NO_SET_IGNORE_BIT     0
#define SCAN_CFG_AUTO_SET_IGNORE_BIT   1
#define SCAN_CFG_NO_END_ON_REPORT      0
#define SCAN_CFG_END_ON_REPORT         1
#define SCAN_CFG_DISABLE_RPA_MODE      0
#define SCAN_CFG_ENABLE_RPA_MODE       1

// Extended Scan Configuration
#define EXT_SCAN_CFG_CHECK_ADI         BV(0)
#define EXT_SCAN_CFG_AUTO_ADI_UPDATE   BV(1)
#define EXT_SCAN_CFG_DUP_FILTERING     BV(2)
#define EXT_SCAN_CFG_AUTO_AL_IGNORE    BV(3)
#define EXT_SCAN_CFG_AUTO_ADI_PROCESS  BV(4)
#define EXT_SCAN_CFG_EXCLUSIVE_SID     BV(5)
#define EXT_SCAN_CFG_ACCEPT_SYNCINFO   BV(6)

// Init Configuration
#define INIT_CFG_USE_PEER_ADDR         0
#define INIT_CFG_AL_POLICY_USE_AL      1
#define INIT_CFG_NO_DYN_WINOFFSET      0
#define INIT_CFG_USE_DYN_WINOFFSET     1
#define INIT_CFG_ALLOW_ILLEGAL_LEN     0
#define INIT_CFG_DISCARD_ILLEGAL_LEN   1

// RX Queue Configuration Bits
#define RXQ_CFG_CLEAR                     0
#define RXQ_CFG_NO_AUTOFLUSH_IGNORED_PKT  0
#define RXQ_CFG_AUTOFLUSH_IGNORED_PKT  BV(0)
#define RXQ_CFG_AUTOFLUSH_CRC_ERR_PKT  BV(1)
#define RXQ_CFG_AUTOFLUSH_EMPTY_PKT    BV(2)
#define RXQ_CFG_INCLUDE_PKT_LEN_BYTE   BV(3)
#define RXQ_CFG_INCLUDE_CRC            BV(4)
#define RXQ_CFG_APPEND_RSSI            BV(5)
#define RXQ_CFG_APPEND_STATUS          BV(6)
#define RXQ_CFG_APPEND_TIMESTAMP       BV(7)

// Sequence Number Status Bits
#define SEQ_NUM_CFG_CLEAR              0
#define SEQ_NUM_CFG_LAST_RX_SN         BV(0)
#define SEQ_NUM_CFG_LAST_TX_SN         BV(1)
#define SEQ_NUM_CFG_NEXT_TX_SN         BV(2)
#define SEQ_NUM_CFG_FIRST_PKT          BV(3)
#define SEQ_NUM_CFG_AUTO_EMPTY         BV(4)
#define SEQ_NUM_CFG_CTRL_TX            BV(5)
#define SEQ_NUM_CFG_CTRL_ACK_RX        BV(6)
#define SEQ_NUM_CFG_CTRL_ACK_PENDING   BV(7)

// RX Test
#define RX_TEST_END_AFTER_RX_PKT       0
#define RX_TEST_REPEAT_AFTER_RX_PKT    1

// Radio Timer (RAT) Times (in 250ns ticks)
#define RAT_TICKS_IN_1US               4         // Connection Jitter
#define RAT_TICKS_IN_3US               12        // Aux Start Time Workaround for 2M
#define RAT_TICKS_IN_4US               16        // Max Range Delay (1km)
#define RAT_TICKS_IN_6US               24        // Timestamp correction
#define RAT_TICKS_IN_10US              40        // Connection Jitter
#define RAT_TICKS_IN_15_5US            62        // TP/TIM/SLA/BV-05
#define RAT_TICKS_IN_16US              64        // Connection Jitter
#define RAT_TICKS_IN_20US              80
#define RAT_TICKS_IN_30US              120
#define RAT_TICKS_IN_40US              160       // Primary Channel Branch Delay
#define RAT_TICKS_IN_64US              256       // Radio Rx Settle Time
#define RAT_TICKS_IN_72US              288       // Agama AP timing adjustment due to pilot tone
#define RAT_TICKS_IN_85US              340       // Radio Rx Synch Time
#define RAT_TICKS_IN_90US              360       // Agama CC13X2P AP timing adjustment due to pilot tone
#define RAT_TICKS_IN_100US             400       // 1M / 2500 RAT ticks (SCA PPM)
#define RAT_TICKS_IN_120US             480       // Total time to close and open the RCL
#define RAT_TICKS_IN_140US             560       // Rx Back-end Time
#define RAT_TICKS_IN_150US             600       // T_IFS
#define RAT_TICKS_IN_166US             664       // Frequency synthesizer delay for RX window
#define RAT_TICKS_IN_180US             720       // AUX_CONNECT_REQ in 2M
#define RAT_TICKS_IN_200US             800       // LL Topology margin
#define RAT_TICKS_IN_700US             2800      // LL_TEST_MODE JIRA-2756
#define RAT_TICKS_IN_256US             1024      // Radio Overhead + FS Calibration
#define RAT_TICKS_IN_280US             1120      // Radio Overhead + FS Calibration
#define RAT_TICKS_IN_285US             1140
#define RAT_TICKS_IN_300US             1200      // T_MAFS (AE)
#define RAT_TICKS_IN_352US             1408      // CONNECT_IND is 44 bytes
#define RAT_TICKS_IN_500US             2000      // Periodic Adv addition process command
#define RAT_TICKS_IN_625US             2500      // Fundamental BLE Time Slot
#define RAT_TICKS_IN_900US             3600      // Additional Rx Synch overhead for Coded S8
#define RAT_TICKS_IN_1MS               4000      // Multiple of Adv Random Delay
#define RAT_TICKS_IN_1_006MS           4024      // AUX_CONNECT_REQ in Coded S2
#define RAT_TICKS_IN_1_225MS           4900      // Adv HDC offset time from rf count command in case ch.39 excluded
#define RAT_TICKS_IN_1_25MS            5000      // Fundamental BLE Time Unit
#define RAT_TICKS_IN_1_5MS             6000      // max RX window size for periodic scan
#define RAT_TICKS_IN_1_875MS           7500      // DTM Packet Interval
#define RAT_TICKS_IN_2MS               8000      // Max time in 1M phy for fragments periodic adv
#define RAT_TICKS_IN_2_5MS             10000     // DTM Packet Interval
#define RAT_TICKS_IN_2_896MS           11584     // AUX_CONNECT_REQ in Coded S8
#define RAT_TICKS_IN_10MS              40000     // General Purpose Delay
#define RAT_TICKS_IN_12_5MS            50000     // DTM T(l) Compare
#define RAT_TICKS_IN_16MS              64000     // Max time in Coded phy for fragments periodic adv
#define RAT_TICKS_IN_20MS              80000
#define RAT_TICKS_IN_100MS             400000    // Refers to the minimum observation period used by the SDAA module
#define RAT_TICKS_IN_1S                4000000
#define RAT_TICKS_IN_1_28S             5120000   // Directed Advertising Timeout
#define RAT_TICKS_IN_32S               128000000 // Max LSTO
//
#define RAT_TICKS_FOR_CONNECT_IND             RAT_TICKS_IN_352US
#define RAT_TICKS_FOR_AUX_CONN_REQ_1M         RAT_TICKS_IN_352US
#define RAT_TICKS_FOR_AUX_CONN_REQ_2M         RAT_TICKS_IN_180US
#define RAT_TICKS_FOR_AUX_CONN_REQ_S2         RAT_TICKS_IN_1_006MS
#define RAT_TICKS_FOR_AUX_CONN_REQ_S8         RAT_TICKS_IN_2_896MS
#define RAT_TICKS_FOR_PERIODIC_SCAN_WIN_SIZE  RAT_TICKS_IN_1_5MS
#define RAT_TICKS_FOR_SCHED_PROCESS_TIME      RAT_TICKS_IN_200US

// Radio Timer (RAT) Channels
#define RAT_CHAN_5                     5
#define RAT_CHAN_6                     6
#define RAT_CHAN_7                     7

// Miscellaneous
#define BLE_BDADDR_SIZE                6
#define MAX_BLE_CONNECT_IND_SIZE       34 // init addr + own addr + payload
#define MAX_BLE_ADV_PKT_SIZE          255 // payload
#define MAX_EXT_ADV_PKT_SIZE           8  // max size of AE EXT_ADV_PKT PDU
#define AUX_CONN_RSP_PKT_SIZE          14 // AUX_CONN_RSP PDU packet size (constant size)
// HCI Rx Packet Header
// | Packet Type (1) | Handler(2) | Length(2) |
// Note: This is the same define as what's in hci_tl.h: HCI_DATA_MIN_LENGTH!
#define HCI_RX_PKT_HDR_SIZE            5
//
#define BLE_CRC_LEN                    LL_PKT_CRC_LEN
#define BLE_CHAN_MAP_LEN               LL_NUM_BYTES_FOR_CHAN_MAP

/*******************************************************************************
 * MACROS
 */

//
// Receive Queue Entry Configuration
//
#define SET_RXQ_ENTRY_CFG( d, b )                                              \
  (d) |= BV(b)

#define CLR_RXQ_ENTRY_CFG( d, b )                                              \
  (d) &= ~BV(b)

// Sequence Number Status
#define SET_ENTRY_CFG( d, b )                                                  \
  (d) |= BV(b)

#define CLR_ENTRY_CFG( d, b )                                                  \
  (d) &= ~BV(b)

// Whitening Configuration
#define CLR_WHITENING( x )                                                     \
  (x) = 0x80

#define SET_WHITENING_BLE( x )                                                 \
  (x) = 0x00;

#define SET_WHITENING_INIT( x, v )                                             \
  (x) = 0x80 | ((v) & ~0x80)

//
// Advertising Configuration
//
#define CLR_ADV_CFG( c )                                                       \
  (c) = 0

#define SETBIT_ADV_CFG( c, b )                                                 \
  (c) = ((c) & ~BV(b)) | BV(b)

#define SETVAR_ADV_CFG_FILTER_POLICY( c, v )                                   \
  (c) = ((c) & ~0x03) | ((v) & 0x03)

#define SETVAR_ADV_CFG_DEV_ADDR_TYPE( c, v )                                   \
  (c) = ((c) & ~BV(2)) | (((v) & 0x01) << 2)

#define GET_ADV_CFG_DEV_ADDR_TYPE( c )                                         \
  (((c) & BV(2)) >> 2)

#define SETVAR_ADV_CFG_PEER_ADDR_TYPE( c, v )                                  \
  (c) = ((c) & ~BV(3)) | (((v) & 0x01) << 3)

#define SETVAR_ADV_CFG_STRICT_LEN_FILTER( c, v )                               \
  (c) = ((c) & ~BV(4)) | (((v) & 0x01) << 4)

#define SET_ADV_CFG_CHAN_SELECT( c )                                           \
  (c) |= BV(5)

#define CLR_ADV_CFG_CHAN_SELECT( c )                                           \
  (c) &= ~BV(5)

#define SETVAR_ADV_CFG_CHAN_SELECT( c, v )                                     \
  (c) = ((c) & ~BV(5)) | (((v) & 0x01) << 5)

#define SET_ADV_CFG_PRIV_IGN_MODE( c )                                         \
  (c) |= BV(6)

#define CLR_ADV_CFG_PRIV_IGN_MODE( c )                                         \
  (c) &= ~BV(6)

#define SETVAR_ADV_CFG_PRIV_IGN_MODE( c, v )                                   \
  (c) = ((c) & ~BV(6)) | (((v) & 0x01) << 6)

#define SET_ADV_CFG_RPA_MODE( c )                                              \
  (c) |= BV(7)

#define CLR_ADV_CFG_RPA_MODE( c )                                              \
  (c) &= ~BV(7)

#define SETVAR_ADV_CFG_RPA_MODE( c, v )                                        \
  (c) = ((c) & ~BV(7)) | (((v) & 0x01) << 7)


//
// Extended Advvertising Configuration
//

// Note: Bit 5 is bDirected instead of Channel Selection Algo!

#define SET_ADV_CFG_DIRECTED( c )                                              \
  (c) |= BV(5)

#define CLR_ADV_CFG_DIRECTED( c )                                              \
  (c) &= ~BV(5)

#define SETVAR_ADV_CFG_DIRECTED( c, v )                                        \
  (c) = ((c) & ~BV(5)) | (((v) & 0x01) << 5)


//
// Scan Configuration
//
#define CLR_SCAN_CFG( c )                                                      \
  (c) = 0

#define SETBIT_SCAN_CFG( c, b )                                                \
  (c) = ((c) & ~BV(b)) | BV(b)

#define SETVAR_SCAN_CFG_FILTER_POLICY( c, v )                                  \
  (c) = ((c) & ~BV(0)) | ((v) & 0x01);

#define SETVAR_SCAN_CFG_ACTIVE_SCAN( c, v )                                    \
  (c) = ((c) & ~BV(1)) | (((v) & 0x01) << 1)

#define SETVAR_SCAN_CFG_DEV_ADDR_TYPE( c, v )                                  \
  (c) = ((c) & ~BV(2)) | (((v) & 0x01) << 2)

#define GET_SCAN_CFG_DEV_ADDR_TYPE( c )                                        \
  (((c) & BV(2)) >> 2)

#define SETVAR_SCAN_CFG_RPA_FILTER_POLICY( c, v )                              \
  (c) = ((c) & ~BV(3)) | (((v) & 0x01) << 3)

#define SETVAR_SCAN_CFG_STRICT_LEN_FILTER( c, v )                              \
  (c) = ((c) & ~BV(4)) | (((v) & 0x01) << 4)

#define SET_SCAN_CFG_AUTO_SET_AL_IGNORE( c )                                   \
  (c) |= BV(5)

#define CLR_SCAN_CFG_AUTO_SET_AL_IGNORE( c )                                   \
  (c) &= ~BV(5)

#define SET_SCAN_CFG_AUTO_SET_AL_IGNORE( c )                                   \
  (c) |= BV(5)

#define CLR_SCAN_CFG_AUTO_SET_AL_IGNORE( c )                                   \
  (c) &= ~BV(5)

#define SETVAR_SCAN_CFG_AUTO_SET_AL_IGNORE( c, v )                             \
  (c) = ((c) & ~BV(5)) | (((v) & 0x01) << 5)

#define SETVAR_SCAN_CFG_END_ON_REPORT( c, v )                                  \
  (c) = ((c) & ~BV(6)) | (((v) & 0x01) << 6)

#define SET_SCAN_CFG_RPA_MODE( c )                                             \
  (c) |= BV(7)

#define CLR_SCAN_CFG_RPA_MODE( c )                                             \
  (c) &= ~BV(7)

#define SETVAR_SCAN_CFG_RPA_MODE( c, v )                                       \
  (c) = ((c) & ~BV(7)) | (((v) & 0x01) << 7)

//
// Scan Backoff Configuration
//
#define SETVAR_SCAN_BACKOFF_CFG_UL( c, v )                                     \
  (c) = ((c) & ~0x0F) | ((v) & 0x0F)

#define SETVAR_SCAN_BACKOFF_CFG_LAST_SUCCCEEDED( c, v )                        \
  (c) = ((c) & ~BV(4)) | (((v) & 0x01) << 4)

#define SETVAR_SCAN_BACKOFF_CFG_LAST_FAILED( c, v )                            \
  (c) = ((c) & ~BV(5)) | (((v) & 0x01) << 5)

//
// Scan Extended Filter Configuration
//
#define CLR_EXT_SCAN_FILTER_CFG( c )                                           \
  (c) = 0

#define SETVAR_EXT_SCAN_FILTER_CFG(c, v)                                       \
  (c) = ((c) & ~0x3F) | ((v) & 0x3F)

#define CLR_EXT_SCAN_FILTER_CFG_EXCLUSIVE_SID( c )                             \
  (c) &= ~EXT_SCAN_CFG_EXCLUSIVE_SID

#define SET_EXT_SCAN_FILTER_CFG_EXCLUSIVE_SID( c )                             \
  (c) |= EXT_SCAN_CFG_EXCLUSIVE_SID

#define CLR_EXT_SCAN_FILTER_CFG_ACCEPT_SYNCINFO( c )                           \
  (c) &= ~EXT_SCAN_CFG_ACCEPT_SYNCINFO

#define SET_EXT_SCAN_FILTER_CFG_ACCEPT_SYNCINFO( c )                           \
  (c) |= EXT_SCAN_CFG_ACCEPT_SYNCINFO

//
// Adi Status Configuration
//
#define CLR_ADI_STATUS( c )                                                \
  (c) = 0

#define SETVAR_ADI_STATUS(c, v)                                            \
  (c) = ((c) & ~0x7F) | ((v) & 0x7F)

#define SETVAR_ADI_STATUS_LAST_ACCEPTED_SID(c, v)                          \
  (c) = ((c) & ~0x0F) | ((v) & 0x0F)

#define SETVAR_ADI_STATUS_STATE(c, v)                                      \
  (c) = ((c) & ~0x70) | (((v) & 0x07) << 4)

//
// Init Configuration
//
#define CLR_INIT_CFG( c )                                                      \
  (c) = 0

#define SETVAR_INIT_CFG_FILTER_POLICY( c, v )                                  \
  (c) = ((c) & ~BV(0)) | ((v) & 0x01);

#define SETVAR_INIT_CFG_DYN_WINOFFSET( c, v )                                  \
  (c) = ((c) & ~BV(1)) | (((v) & 0x01) << 1)

#define SETVAR_INIT_CFG_DEV_ADDR_TYPE( c, v )                                  \
  (c) = ((c) & ~BV(2)) | (((v) & 0x01) << 2)

#define SETVAR_INIT_CFG_PEER_ADDR_TYPE( c, v )                                 \
  (c) = ((c) & ~BV(3)) | (((v) & 0x01) << 3)

#define SETVAR_INIT_CFG_STRICT_LEN_FILTER( c, v )                              \
  (c) = ((c) & ~BV(4)) | (((v) & 0x01) << 4)

#define SET_INIT_CFG_CHAN_SELECT( c )                                          \
  (c) |= BV(5)

#define CLR_INIT_CFG_CHAN_SELECT( c )                                          \
  (c) &= ~BV(5)

#define SETVAR_INIT_CFG_CHAN_SELECT( c, v )                                    \
  (c) = ((c) & ~BV(5)) | (((v) & 0x01) << 5)

//
// Transmit Test Configuration
//
#define SET_TX_TEST_CFG_OVERRIDE( c )                                          \
  (c) |= BV(0)

#define CLR_TX_TEST_CFG_OVERRIDE( c )                                          \
  (c) &= ~BV(0)

#define SET_TX_TEST_CFG_USE_PRBS9( c )                                         \
  (c) = ((c) & 0x06) | BV(1);

#define SET_TX_TEST_CFG_USE_PRBS15( c )                                        \
  (c) = ((c) & 0x06) | BV(2);

//
// Receive Status Byte Bit Field
//
#define VALID_TIMESTAMP( s )                                                   \
  ((s) & BV(0))

#define LAST_RX_PKT_CRC_ERROR( s )                                             \
  ((s) & BV(1))

#define LAST_RX_PKT_IGNORED( s )                                               \
  ((s) & BV(2))

#define LAST_RX_PKT_EMPTY( s )                                                 \
  ((s) & BV(3))

#define LAST_RX_PKT_CONTROL( s )                                               \
  ((s) & BV(4))

#define LAST_RX_PKT_MD( s )                                                    \
  ((s) & BV(5))

#define LAST_RX_PKT_ACK( s )                                                   \
  ((s) & BV(6))


//
// Adi Status
//
#define ADI_STATUS_LAST_ACCEPTED_SID( s )                                      \
  ((s) & 0x0F)

#define ADI_STATUS_STATE( s )                                                  \
  (((s) >> 4 ) & 0x03)


#define WAIT( t ) {volatile uint32 time = (t); while( time-- );}

/*******************************************************************************
 * TYPEDEFS
 */
/*
** BLE Data Entry Structures
*/

// TX Data
typedef struct
{
  List_List *rfDataBuffers;                           // pointer to RCL Tx data list
  List_List llDataBuffers;                            // LL Tx data list as replica of the RCL Tx data list
  List_List tmpDataBuffers;                           // Temp Tx data list while TX is disabled
} txDataQ_t;

// RX Data
typedef struct
{
  RCL_MultiBuffer *dataBuffers[NUM_RX_DATA_ENTRIES];
  List_List        multiBuffers;
  List_List        finishedBuffers;
  uint16           length;                           // buffer length
} rxDataQ_t;

// Initiator Command CONNECT_IND LL_Data
PACKED_TYPEDEF_STRUCT
{
  List_Elem          __elem__;
  RCL_BufferState    state;                                  ///< Buffer state
  uint16             length   __attribute__ ((aligned (4))); ///< Number of bytes in buffer after the length field
  uint8              numPad;                                 ///< Number of pad bytes before start of the packet
  uint8              pad0;                                   ///< First pad byte, or first byte of the packet if numPad == 0
  uint8              pad1;
  uint8              pad2;
  uint8              header;
  uint8              payloadLen;
  uint8              ownAddr[ LL_DEVICE_ADDR_LEN ];  // own device address
  uint8              peerAddr[ LL_DEVICE_ADDR_LEN ]; // peer device address
  uint32             accessAddress;         // W:  access address used in connection
  uint8              crcInit[BLE_CRC_LEN];  // W:  CRC init value
  uint8              winSize;
  uint16             winOffset;
  uint16             connInterval;
  uint16             latency;
  uint16             timeout;
  uint8              chanMap[BLE_CHAN_MAP_LEN];
  uint8              hopSca;
} connReqData_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

// transmit queue
extern txDataQ_t *txDataQ;
extern rxDataQ_t rxDataQ;
// Connection Data Structures
extern RCL_CmdBle5Connection *linkCmd;
extern RCL_CtxConnection     *linkParam;

// Connection Output
extern RCL_StatsConnection   connOutput;
extern connReqData_t    connReqData[];

// DTM Command, Parameters and Output
extern RCL_CmdBle5DtmTx      txDtmTestCmd;
extern RCL_CmdBle5GenericRx  rxTestCmd;
extern RCL_CtxGenericRx      rxTestParam;
extern RCL_StatsGenericRx    rxTestOut;
extern RCL_CmdBle5TxTest     txTestCmd;

/*******************************************************************************
 * APIs
 */
extern void          llAddTxDataEntry( void *, void * );
extern void          llProcessPeripheralControlPacket( llConnState_t *, uint8 * );
extern void          llProcessCentralControlPacket( llConnState_t *, uint8 * );
extern void          llMoveTempTxDataEntries( llConnState_t * );
extern void         *llSetupScanDataEntryQueue( void );
extern void         *llSetupPeriodicScanDataEntryQueue( void );
extern void         *llSetupInitDataEntryQueue( void );
extern void         *llSetupAdvDataEntryQueue( void );
extern void         *llSetupConnRxDataEntryQueue( uint8 connId );
extern void          llClearRxDataEntry( void *, List_List * );
extern void          llClearScanDataQueue( uint8 );
extern void          llClearPeriodicScanDataQueue( uint8 );


/*******************************************************************************
 */

#endif /* BLE_H */
