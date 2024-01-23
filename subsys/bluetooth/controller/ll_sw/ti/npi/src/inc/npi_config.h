/******************************************************************************

 @file  npi_config.h

 @brief This file contains the Network Processor Interface (NPI), which
        abstracts the physical link between the Application
        Processor (AP) and the Network Processor (NP). The NPI serves as the
        HAL's client for the SPI and UART drivers, and provides
        API and callback services for its client.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2015 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef NPI_CONFIG_H
#define NPI_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

// ****************************************************************************
// includes
// ****************************************************************************

#include <ti_drivers_config.h>

// ****************************************************************************
// defines
// ****************************************************************************

#if !defined(NPI_SPI_CONFIG)
#define NPI_SPI_CONFIG CONFIG_SPI_1
#endif

#ifndef NPI_FLOW_CTRL
  #ifdef POWER_SAVING
    #define NPI_FLOW_CTRL       1
  #else
    #define NPI_FLOW_CTRL       0
  #endif
#elif !(NPI_FLOW_CTRL == 0) && !(NPI_FLOW_CTRL == 1)
  #error "NPI ERROR: NPI_FLOW_CTRL can only be assigned 0 (disabled) or 1 (enabled)"
#endif

#if (NPI_FLOW_CTRL == 1)
  #ifdef NPI_USE_SPI
    //NOTE: Board_SPI1 SPI module must be used for NPI.
    #define MRDY_PIN CONFIG_GPIO_BTN1
    #define SRDY_PIN CONFIG_GPIO_BTN2
  #elif defined(NPI_USE_UART)
    #define MRDY_PIN CONFIG_GPIO_BTN1
    #define SRDY_PIN CONFIG_GPIO_BTN2
  #endif
  #define SRDY_ENABLE()                   GPIO_write(SRDY_PIN, 0) /* RTS low */
  #define SRDY_DISABLE()                  GPIO_write(SRDY_PIN, 1) /* RTS high */
#else
  #define SRDY_ENABLE()
  #define SRDY_DISABLE()
#endif // NPI_FLOW_CTRL = 1


#if defined( CC26X2 ) || defined( CC13X2 ) || defined( CC13X2P ) || defined( CC33xx )
  #ifndef NPI_TL_BUF_SIZE
    #define NPI_TL_BUF_SIZE         1680
  #endif
#elif defined ( CC23X0 )
  #ifndef NPI_TL_BUF_SIZE
    #define NPI_TL_BUF_SIZE         660
  #endif
#else // CC26XX_R2 || CC1350LP_7XD ...
  #ifndef NPI_TL_BUF_SIZE
    #define NPI_TL_BUF_SIZE         330
  #endif
#endif // CC26X2 || CC13X2


#define NPI_SPI_PAYLOAD_SIZE    255
#define NPI_SPI_HDR_LEN         4

#ifdef NPI_USE_SPI
  #if (NPI_TL_BUF_SIZE - NPI_SPI_HDR_LEN) < NPI_SPI_PAYLOAD_SIZE
    #define NPI_MAX_FRAG_SIZE       (NPI_TL_BUF_SIZE - NPI_SPI_HDR_LEN)
  #else
    #define NPI_MAX_FRAG_SIZE       NPI_SPI_PAYLOAD_SIZE
  #endif
#elif NPI_USE_UART
  #define NPI_MAX_FRAG_SIZE       NPI_TL_BUF_SIZE
#else
  #error "NPI ERROR: NPI_USE_UART or NPI_USE_SPI must be defined."
#endif

// ****************************************************************************
// typedefs
// ****************************************************************************

#ifdef FREERTOS
typedef uint8_t Char;
#endif
//*****************************************************************************
// globals
//*****************************************************************************

//*****************************************************************************
// function prototypes
//*****************************************************************************

#ifdef __cplusplus
}
#endif

#endif /* NPI_CONFIG_H */
