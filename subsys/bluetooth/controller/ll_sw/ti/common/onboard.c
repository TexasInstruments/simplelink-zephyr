/******************************************************************************

 @file  onboard.c

 @brief This file contains the UI and control for the peripherals on the
        EVAL development board
        This file targets the Chipcon MSP430xxx

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2005 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "bcomdef.h"
#include "onboard.h"
#include "osal.h"

#include "hal_board_cfg.h"
#include "hal_key.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */
// Task ID not initialized
#define NO_TASK_ID 0xFF


/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/* Initialize Key stuff */
uint8 OnboardKeyIntEnable = HAL_KEY_MODE;

/*********************************************************************
 * LOCAL VARIABLES
 */

// Registered keys task ID, initialized to NOT USED.
static uint8 registeredKeysTaskID = NO_TASK_ID;

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

extern uint8 LL_Rand( uint8 *, uint8 );

#if   defined FEATURE_ABL
  #include "../../util/abl/app/sbl_app.c"
#elif defined FEATURE_SBL
  #include "../../util/sbl/app/sbl_app.c"
#elif defined FEATURE_EBL
  #include "../../util/ebl/app/sbl_app.c"
#elif defined FEATURE_UBL_MSD
  #include "../../util/ubl/soc_8051/usb_msd/app/ubl_app.c"
#else
  void appForceBoot(void);
#endif


/*********************************************************************
 * @fn       InitBoard
 *
 * @brief    Initialize CC26xx board and HAL.
 *
 * @param   None
 *
 * @return  None
 *
 *********************************************************************/
void InitBoard()
{
  // Enable or Disable HAL Key Interrupts.
  OnboardKeyIntEnable = HAL_KEY_MODE;

  // Configure HAL Keys
#if (defined HAL_KEY) && (HAL_KEY == TRUE)
  HalKeyConfig( OnboardKeyIntEnable, OnBoard_KeyCallback);
#endif // (defined HAL_KEY) && (HAL_KEY == TRUE)
}


/*********************************************************************
 * @fn        Onboard_rand
 *
 * @brief    Random number generator
 *
 * @param   none
 *
 * @return  uint16 - new random number
 *
 *********************************************************************/
uint16 Onboard_rand( void )
{
  uint16 randNum;

  LL_Rand( (uint8 *)&randNum, 2 );

  return ( randNum );
}

/*********************************************************************
 * @fn      TimerElapsed
 *
 * @brief   Get elapsed timer clock counts.  Stub function.
 *
 * @param   none
 *
 * @return  uint32 - new random number
 *
 *********************************************************************/
uint32 TimerElapsed( void )
{
  return 0;
}

/*********************************************************************
 * @fn      _itoa
 *
 * @brief   convert a 16bit number to ASCII
 *
 * @param   num -
 *          buf -
 *          radix -
 *
 * @return  void
 *
 *********************************************************************/
#ifndef STACK_LIBRARY
void _itoa(uint16 num, uint8 *buf, uint8 radix)
{
  char c,i;
  uint8 *p, rst[5];

  p = rst;
  for ( i=0; i<5; i++,p++ )
  {
    c = num % radix;  // Isolate a digit
    *p = c + (( c < 10 ) ? '0' : '7');  // Convert to Ascii
    num /= radix;
    if ( !num )
      break;
  }

  for ( c=0 ; c<=i; c++ )
    *buf++ = *p--;  // Reverse character order

  *buf = '\0';
}
#endif /* STACK_LIBRARY */
/*********************************************************************
 * @fn      Onboard_soft_reset
 *
 * @brief   Effect a soft reset.
 *
 * @param   none
 *
 * @return  none
 *
 *********************************************************************/
void Onboard_soft_reset( void )
{
}

/*********************************************************************
 * @fn      appForceBoot
 *
 * @brief   Common force-boot function for the HCI library to invoke.
 *
 * @param   none
 *
 * @return  void
 *********************************************************************/
void appForceBoot( void )
{
  // Dummy function for HCI library that cannot depend on the SBL build defines.
}

/*********************************************************************
 *                        "Keyboard" Support
 *********************************************************************/

/*********************************************************************
 * Keyboard Register function
 *
 * The keyboard handler is setup to send all keyboard changes to
 * one task (if a task is registered).
 *
 * If a task registers, it will get all the keys. You can change this
 * to register for individual keys.
 *********************************************************************/
uint8 RegisterForKeys( uint8 task_id )
{
  // Allow only the first task
  if ( registeredKeysTaskID == NO_TASK_ID )
  {
    registeredKeysTaskID = task_id;

    return ( true );
  }
  else
    return ( false );
}

/*********************************************************************
 * @fn      OnBoard_KeyCallback
 *
 * @brief   Callback service for keys
 *
 * @param   keys  - keys that were pressed
 *          state - shifted
 *
 * @return  void
 *********************************************************************/
void OnBoard_KeyCallback ( uint8 keys, uint8 state )
{
  uint8 shift;
  (void)state;

  // shift key (S1) is used to generate key interrupt
  // applications should not use S1 when key interrupt is enabled
  shift = (OnboardKeyIntEnable == BSP_KEY_MODE_ISR) ? false : ((keys & HAL_KEY_SW_6) ? true : false);

  //TODO is this necessary?
  if ( OnBoard_SendKeys( keys, shift ) != SUCCESS )
  {
    // Process SW1 here
    if ( keys & HAL_KEY_SW_1 )  // Switch 1
    {
    }
    // Process SW2 here
    if ( keys & HAL_KEY_SW_2 )  // Switch 2
    {
    }
    // Process SW3 here
    if ( keys & HAL_KEY_SW_3 )  // Switch 3
    {
    }
    // Process SW4 here
    if ( keys & HAL_KEY_SW_4 )  // Switch 4
    {
    }
    // Process SW5 here
    if ( keys & HAL_KEY_SW_5 )  // Switch 5
    {
    }
    // Process SW6 here
    if ( keys & HAL_KEY_SW_6 )  // Switch 6
    {
    }
  }
}

/*********************************************************************
 * @fn      OnBoard_SendKeys
 *
 * @brief   Send "Key Pressed" message to application.
 *
 * @param   keys  - keys that were pressed
 *          state - shifted
 *
 * @return  status
 *********************************************************************/
uint8 OnBoard_SendKeys( uint8 keys, uint8 state )
{
  keyChange_t *msgPtr;

  if ( registeredKeysTaskID != NO_TASK_ID )
  {
    // Send the address to the task
    msgPtr = (keyChange_t *)osal_msg_allocate( sizeof(keyChange_t) );
    if ( msgPtr )
    {
      msgPtr->hdr.event = KEY_CHANGE;
      msgPtr->state = state;
      msgPtr->keys = keys;

      osal_msg_send( registeredKeysTaskID, (uint8 *)msgPtr );
    }
    return ( SUCCESS );
  }
  else
    return ( FAILURE );
}

/*********************************************************************
*********************************************************************/
