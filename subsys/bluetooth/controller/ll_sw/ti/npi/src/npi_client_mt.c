/******************************************************************************

 @file  npi_client_mt.c

 @brief Utilities to redirect outbound NPI messages to NPI RTOS thread.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2015 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/


// ****************************************************************************
// includes
// ****************************************************************************
#include "inc/npi_client.h"
#include "inc/npi_data.h"
#include <icall.h>
#include <stdint.h>
#include <string.h>
#include "MT_RPC.h"

// ****************************************************************************
// defines
// ****************************************************************************

// ****************************************************************************
// typedefs
// ****************************************************************************

// ****************************************************************************
// globals
// ****************************************************************************

static ICall_EntityID localEntityID;

// ****************************************************************************
// function prototypes
// ****************************************************************************

// ----------------------------------------------------------------------------
//! \brief      Overload the MT function to Build and Send ZTool Response.
//!             This function relays outgoing MT messages to the NPI task for
//!             further handling.
//!
//! \param[in]  cmdType -  MT Command field
//! \param[in]  cmdId - MT Command ID
//! \param[in]  datalen - lenght MT command
//! \param[in]  pData - pointer to MT commnad buffer
//!
//! \return     void
// ----------------------------------------------------------------------------
void MT_BuildAndSendZToolResponse(uint8_t cmdType, uint8_t cmdId,
                                  uint8_t dataLen, uint8_t *pData)
{
    // allocate an ICall message buffer to send message to NPI task.
    uint8_t *pRspMsg = ICall_allocMsg( dataLen + MTRPC_FRAME_HDR_SZ);

    if(pRspMsg != NULL)
    {
        // populuate the MT header fields.
        pRspMsg[MTRPC_POS_LEN] = dataLen;
        pRspMsg[MTRPC_POS_CMD0] = cmdType;
        pRspMsg[MTRPC_POS_CMD1] = cmdId;

        if(dataLen > 0)
        {
            memcpy(pRspMsg + MTRPC_FRAME_HDR_SZ, pData, dataLen);
        }

        // Send the message
        ICall_sendServiceMsg(localEntityID, ICALL_SERVICE_CLASS_NPI,
                             ICALL_MSG_FORMAT_KEEP, pRspMsg);
    }

    return;
}

// ----------------------------------------------------------------------------
//! \brief      Save local ICall Entity IDs
//!
//! \param[in]  localEntityID ICall entity ID of the local stack task.
//!
//! \return     void
// ----------------------------------------------------------------------------
void NPIClient_saveLocalTaskInfo(ICall_EntityID localTaskEntityID)
{
    localEntityID = localTaskEntityID;
}
