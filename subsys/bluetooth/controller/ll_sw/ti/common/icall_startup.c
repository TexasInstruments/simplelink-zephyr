/******************************************************************************

 @file  icall_startup.c

 @brief Startup code for CC2650 for use with IAR.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2014 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

// Check if compiler is IAR
#if !(defined(__IAR_SYSTEMS_ICC__) || defined(__TI_COMPILER_VERSION__) || defined (__clang__) || defined (__GNUC__) )
#error "startup.c: Unsupported compiler!"
#endif

/*******************************************************************************
 * INCLUDES
 */

#include "hal_types.h"
#include "rom_jt.h"

#include <icall.h>

/*******************************************************************************
 * EXTERNS
 */

/*******************************************************************************
 * PROTOTYPES
 */

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

#define CRC16_POLYNOMIAL  0x1021

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */
#ifndef STACK_LIBRARY
ICall_Dispatcher ICall_dispatcher;
ICall_EnterCS    ICall_enterCriticalSection;
ICall_LeaveCS    ICall_leaveCriticalSection;
#endif /* STACK_LIBRARY */
/*******************************************************************************
 * @fn          checkCRC16
 *
 * @brief       A simple implementation of the crc-16 checksum algorithm,
 *              courtesy of IAR (circa 2007).
 *
 *              Note:  If you use this function, you need to call it with an
 *                     extra number of zero bytes equal to the size of the
 *                     checksum (in this case 2), to "rotate out the answer".
 *
 * input parameters
 *
 * @param       crc   - The CRC value to start with.
 * @param       pAddr - Pointer to an array of bytes to run the CRC over.
 * @param       len   - The number of bytes to process.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      CRC-16 result.
 */
uint16 slow_crc16( uint16 crc, uint8 *pAddr, uint32 len )
{
  while (len--)
  {
    uint8 i;
    uint8 byte = *(pAddr++);

    for (i=0; i<8; ++i)
    {
      uint32 temp = crc;

      crc <<= 1;

      if ( byte & 0x80 )
      {
        crc |= 1;
      }

      if ( temp & 0x8000 )
      {
        crc ^= CRC16_POLYNOMIAL;
      }

      byte <<= 1;
    }
  }

  return crc;
}


/*******************************************************************************
 * @fn          validChecksum
 *
 * @brief       Check if the ROM checksum is valid.
 *
 *              Note: This routine assumes the CRC directly follows the end
 *                    address!
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE=Valid Checksum; FALSE=Invalid Checksum.
 */
uint8 validChecksum( const uint32 *beginAddr, const uint32 *endAddr )
{
  uint16 crc      = 0;
  uint16 romCRC   = *((uint16 *)(((uint8 *)endAddr)+1));
  uint8  zeros[2] = {0, 0};

  // calculate the ROM checksum
  crc = slow_crc16( crc,
                    (uint8 *)beginAddr,
                    (uint32)endAddr - (uint32)beginAddr + 1 );

  // needed to rotate out the answer
  crc = slow_crc16( crc, zeros, 2 );

  // Compare the calculated checksum with the stored
  return( (crc==romCRC)? TRUE : FALSE );
}


/*******************************************************************************
 * @fn          startup_entry
 *
 * @brief       This is the BLE stack entry point.
 *
 * input parameters
 *
 * @param       arg0   argument containing remote dispatch function pointers
 * @param       arg1   custom initialization parameter
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
#ifndef STACK_LIBRARY
#ifdef __TI_COMPILER_VERSION__
#pragma CODE_SECTION(startup_entry,"EntrySection")
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma location="EntrySection"
#endif
#endif /* STACK_LIBRARY */
void startup_entry( const ICall_RemoteTaskArg *arg0, void *arg1 )
{
  extern int stack_main( void *arg );
#ifndef STACK_LIBRARY
#if defined(__IAR_SYSTEMS_ICC__)
  extern void __iar_data_init3(void);
  __iar_data_init3();
#elif defined(__TI_COMPILER_VERSION__)
  extern void __TI_auto_init(void);
  __TI_auto_init();
#else
#error "Error: Must specify a compiler!"
#endif

#endif /* STACK_LIBRARY */
  ICall_dispatcher = arg0->dispatch;
  ICall_enterCriticalSection = arg0->entercs;
  ICall_leaveCriticalSection = arg0->leavecs;

#if defined( FLASH_ROM_BUILD )
  // initialize the Common ROM
  CommonROM_Init();

  // initialize the BLE Controller ROM
  ROM_Init();
#endif // FLASH_ROM_BUILD

  stack_main( arg1 );
}
