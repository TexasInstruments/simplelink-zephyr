/******************************************************************************

 @file  npi_data.h

 @brief NPI Data structures

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2015 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef NPI_DATA_H
#define NPI_DATA_H

#ifdef __cplusplus
extern "C"
{
#endif

// ****************************************************************************
// includes
// ****************************************************************************

#include <stdint.h>

#include <ti/drivers/UART2.h>
#include <ti/drivers/SPI.h>

// ****************************************************************************
// configuration
// ****************************************************************************

#ifndef NPI_FLOW_CTRL
#  ifdef POWER_SAVING
#    define NPI_FLOW_CTRL       1
#  else
#    define NPI_FLOW_CTRL       0
#  endif
#elif !(NPI_FLOW_CTRL == 0) && !(NPI_FLOW_CTRL == 1)
#  error "NPI ERROR: NPI_FLOW_CTRL can only be assigned 0 (disabled) or 1 (enabled)"
#endif

// ****************************************************************************
// defines
// ****************************************************************************

//! \brief NPI Constants
//!
#define NPI_MSG_SOF_VAL                         0xFE

//! \brief NPI Command Type.
#define NPI_MSG_TYPE_POLL                       0x00
#define NPI_MSG_TYPE_SYNCREQ                    0x01
#define NPI_MSG_TYPE_ASYNC                      0x02
#define NPI_MSG_TYPE_SYNCRSP                    0x03

//! \brief NPI Message Lengths
#define NPI_MSG_CMD_LENGTH                      2
#define NPI_MSG_LEN_LENGTH                      2
#define NPI_MSG_HDR_LENGTH                      NPI_MSG_CMD_LENGTH + \
                                                NPI_MSG_LEN_LENGTH
//! \brief NPI Subsystem IDs
//!
#define RPC_SYS_RES0                            0
#define RPC_SYS_SYS                             1
#define RPC_SYS_MAC                             2
#define RPC_SYS_NWK                             3
#define RPC_SYS_AF                              4
#define RPC_SYS_ZDO                             5
#define RPC_SYS_SAPI                            6
#define RPC_SYS_UTIL                            7
#define RPC_SYS_DBG                             8
#define RPC_SYS_APP                             9
#define RPC_SYS_RCAF                            10
#define RPC_SYS_RCN                             11
#define RPC_SYS_RCN_CLIENT                      12
#define RPC_SYS_BOOT                            13
#define RPC_SYS_ZIPTEST                         14
#define RPC_SYS_DEBUG                           15
#define RPC_SYS_PERIPHERALS                     16
#define RPC_SYS_NFC                             17
#define RPC_SYS_PB_NWK_MGR                      18
#define RPC_SYS_PB_GW                           19
#define RPC_SYS_PB_OTA_MGR                      20
#define RPC_SYS_BLE_SNP                         21
#define RPC_SYS_BLE_HCI                         22
#define RPC_SYS_CM_CMD                          23
#define RPC_SYS_UNDEF2                          24
#define RPC_SYS_RTLS_CTRL                       25
#define RPC_SYS_UNDEF4                          26
#define RPC_SYS_UNDEF5                          27
#define RPC_SYS_UNDEF6                          28
#define RPC_SYS_UNDEF7                          29
#define RPC_SYS_UNDEF8                          30
#define RPC_SYS_SRV_CTRL                        31

//! \brief NPI Return Codes
#define NPI_SUCCESS                             0
#define NPI_ROUTING_FULL                        1
#define NPI_SS_NOT_FOUND                        2
#define NPI_INCOMPLETE_PKT                      3
#define NPI_INVALID_PKT                         4
#define NPI_BUSY                                5
#define NPI_TX_MSG_OVERSIZE                     6
#define NPI_TASK_FAILURE                        7
#define NPI_TASK_INVALID_PARAMS                 8

//! \brief Reserved Subsystem ID
#define NPI_SS_RESERVED_ID                      0x00

//! \brief Reserved ICall ID
#define NPI_ICALL_RESERVED_ID                   0x00

//! \brief Masks for cmd0 bits of NPI message
#define NPI_CMD0_TYPE_MASK                      0xE0
#define NPI_CMD0_TYPE_MASK_CLR                  0x1F
#define NPI_CMD0_SS_MASK                        0x1F
#define NPI_CMD0_SS_MASK_CLR                    0xE0

#define NPI_SERIAL_TYPE_UART                    0
#define NPI_SERIAL_TYPE_SPI                     1  // Not supported

//! \brief Returns the message type of an NPI message
#define NPI_GET_MSG_TYPE(pMsg)          ((pMsg->cmd0 & NPI_CMD0_TYPE_MASK)>> 5)

//! \brief Returns the subsystem ID of an NPI message
#define NPI_GET_SS_ID(pMsg)             ((pMsg->cmd0) & NPI_CMD0_SS_MASK)

//! \brief Sets the message type of an NPI message
#define NPI_SET_MSG_TYPE(pMsg,TYPE)     pMsg->cmd0 &= NPI_CMD0_TYPE_MASK_CLR; \
                                        pMsg->cmd0 |= ( (TYPE & 0x3) << 5 );

//! \brief Sets the subsystem ID of an NPI message
#define NPI_SET_SS_ID(pMsg,SSID)        pMsg->cmd0 &= NPI_CMD0_SS_MASK_CLR; \
                                        pMsg->cmd0 |= ( (SSID & 0x1F) );

//! \brief Sets values for default assert NPI response
#define NPI_ASSERT_PAYLOAD_LEN                  1
#define NPI_ASSERT_CMD1_ID                      0x11

// ****************************************************************************
// typedefs
// ****************************************************************************

//! \brief Generic message NPI Frame. All messages sent over NPI Transport Layer
//         will follow this structure. Any messages passed between NPI Task and
//         subsystems will be of this type.
typedef struct _npiFrame_t
{
    uint16_t              dataLen;
    uint8_t               cmd0;
    uint8_t               cmd1;
    uint8_t               *pData;
} _npiFrame_t;

typedef union
{
  UART2_Params uartParams;
  SPI_Params spiParams;                 // Not supported
} npiInterfaceParams;

//*****************************************************************************
// globals
//*****************************************************************************

//*****************************************************************************
// function prototypes
//*****************************************************************************
extern void NPIData_postAssertNpiMsgEvent(uint8_t assertType);

#ifdef __cplusplus
}
#endif

#endif /* NPI_DATA_H */
