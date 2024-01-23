/******************************************************************************

 @file  npi_ss_hci.c

 @brief NPI BLE HCI Subsystem

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

#include <inc/npi_data.h>
#include <inc/npi_task.h>
#include <inc/npi_ble.h>
#include <subsystem/npi_ss_ble_hci.h>

// ****************************************************************************
// defines
// ****************************************************************************

// ****************************************************************************
// typedefs
// ****************************************************************************

//*****************************************************************************
// globals
//*****************************************************************************

extern ICall_EntityID npiAppEntityID;

//*****************************************************************************
// function prototypes
//*****************************************************************************

void NPISS_BLE_HCI_msgFromHost(_npiFrame_t *pNPIMsg);

void NPISS_BLE_HCI_msgFromICall(uint8_t *pGenMsg);

// -----------------------------------------------------------------------------
//! \brief      NPI BLE Subsystem initialization function
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPISS_BLE_HCI_init(void)
{
    // Send BLE Stack the NPI Task Entity ID
    NPI_RegisterTask(npiAppEntityID);

    // Register for messages from Host with RPC_SYS_BLE ssID
    NPITask_regSSFromHostCB(RPC_SYS_BLE_HCI,NPISS_BLE_HCI_msgFromHost);

    // Register for messages from ICall
    NPITask_regSSFromICallCB(ICALL_SERVICE_CLASS_BLE,
                             NPISS_BLE_HCI_msgFromICall);
}

// -----------------------------------------------------------------------------
//! \brief      Call back function provided to NPI Task. All incoming NPI
//!             received by NPI Task with the subsystem ID of this subsystem
//!             will be sent to this call back through the NPI routing system
//!
//!             *** This function MUST free pNPIMsg
//!
//! \param[in]  pNPIMsg    Pointer to a "framed" NPI message
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPISS_BLE_HCI_msgFromHost(_npiFrame_t *pNPIMsg)
{
    // Free NPI Frame after use
    NPITask_freeFrame(pNPIMsg);
}

// -----------------------------------------------------------------------------
//! \brief      Call back function provided to NPI Task. All incoming ICall
//!             messages received by NPI Task from the src ID provided to NPI
//!             will be sent to this call back through the NPI routing system
//!
//!             *** This function MUST free pGenMsg
//!
//! \param[in]  pGenMsg    Pointer to a generic Icall msg
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPISS_BLE_HCI_msgFromICall(uint8_t *pGenMsg)
{
    // Free ICall Msg after use
    ICall_free(pGenMsg);
}
