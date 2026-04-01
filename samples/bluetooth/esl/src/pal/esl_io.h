/**
 *  \file esl_io.h
 *
 *  This Header File contains Application Layer Declarations for Zephyr.
 */

/*
 *  Copyright (C) 2025. LTI Mindtree Ltd.
 *  All rights reserved.
 */

#ifndef _ESL_IO_H_
#define _ESL_IO_H_

/* ----------------------------------------------- Header File Inclusion */
#include "BT_esl_common.h"

/* ----------------------------------------------- Macros */

/**
 * \addtogroup EtherMind_ESL_PAL_Macros
 * \{
 */
/**
 * \name ESL IO Macros
 * \{
 */
/** ESL IO Debug Trace Log Mapping */
#define ESL_IO_TRC(...)      CONSOLE_TRC(__VA_ARGS__)

/* ----------------------------------------------- Global Definitions */
/** Number of image slots */
/** Size of each image in bytes */
#define IO_IMAGE_MAX_SIZE   1024

/** Number of LEDs on the board */
#if (CONFIG_BT_ESL_MAX_LED_SUPPORTED >= 1) && (CONFIG_BT_ESL_MAX_LED_SUPPORTED <= 256)
#define IO_NUM_LEDS         CONFIG_BT_ESL_MAX_LED_SUPPORTED
#else
#define IO_NUM_LEDS         2
#endif

/** Number of displays on the board */
#if (CONFIG_BT_ESL_MAX_DISPLAY_SUPPORTED >= 1) && (CONFIG_BT_ESL_MAX_DISPLAY_SUPPORTED <= 102)
#define IO_NUM_DISPLAYS     CONFIG_BT_ESL_MAX_DISPLAY_SUPPORTED
#else
#define IO_NUM_DISPLAYS     1
#endif

/** Number of images */
#if (CONFIG_BT_ESL_MAX_IMAGE_SUPPORTED >= 1) && (CONFIG_BT_ESL_MAX_IMAGE_SUPPORTED <= 256)
#define IO_MAX_IMAGES       CONFIG_BT_ESL_MAX_IMAGE_SUPPORTED
#else
#define IO_MAX_IMAGES       2
#endif

/** Number of sensors */
#if (CONFIG_BT_ESL_MAX_SENSOR_SUPPORTED >= 1) && (CONFIG_BT_ESL_MAX_SENSOR_SUPPORTED <= 256)
#define IO_NUM_SENSORS      CONFIG_BT_ESL_MAX_SENSOR_SUPPORTED
#else
#define IO_NUM_SENSORS      2
#endif

/** Sensor data size */
#define IO_SENSOR_DATA_SIZE 1

/** \} */

/** \} */

/* ----------------------------------------------- Structures/Data Types */

/**
 * \addtogroup EtherMind_ESL_PAL_Structures
 * \{
 */
/**
 * \name ESL IO Structures
 * \{
 */
/**
 * \struct ESL_IO_LED_PARAMS
 * \brief Structure to hold parameters for controlling an ESL LED.
 *
 * This structure defines the configuration for an individual LED in the ESL system,
 * including its index, brightness level, on/off state, and duration.
 */
typedef struct _ESL_IO_LED_PARAMS
{
    /** LED Index */
    UCHAR led_idx;

    /** Brightness */
    UCHAR brightness;

    /** ON or OFF LED */
    UCHAR onoff;

    /** Time duration for LED to ON or OFF */
    UINT16 onoff_period;

}ESL_IO_LED_PARAMS;

/** \} */

/** \} */

/* -------------------------------------------- Function Declarations */

/**
 * \addtogroup EtherMind_ESL_PAL_api_defs
 * \{
 */
/**
 * \name ESL IO Function Definations
 * \{
 */

/**
 * \brief Initialize the ESL IO subsystem.
 *
 * \par Description:
 * This function initializes all IO-related resources, buffers, and hardware states.
 */
void esl_io_init(void);


/**
 * \brief Control the display to show or hide an image.
 *
 * \par Description:
 * Controls a display device to show or hide a specific image.
 *
 * \param [in] disp_idx Index of the display device.
 * \param [in] img_idx  Index of the image to display.
 * \param [in] enable   1 to enable (show), 0 to disable (hide).
 * \param [in] image   Pointer to the image data (if applicable).
 *
 * \return BT_ESL_API_SUCCESS on success, error code otherwise.
 */
API_RESULT esl_io_display_control
           (
               /* IN */UCHAR disp_idx,
               /* IN */UCHAR img_idx,
               /* IN */UCHAR enable,
               /* IN */void* image
           );

/**
 * \brief Control an LED's brightness and state.
 *
 * \par Description:
 * Controls the brightness and on/off state of a specific LED using the parameters
 * provided in the ESL_IO_LED_PARAMS structure.
 *
 * \param [in] esl_io_led_params Pointer to the structure containing LED control parameters:
 *
 * \return BT_ESL_API_SUCCESS on success, error code otherwise.
 */
API_RESULT esl_io_led_control(/* IN */ ESL_IO_LED_PARAMS* esl_io_led_params);

/**
 * \brief Read sensor data from a specified sensor.
 *
 * \par Description:
 * Reads data from a sensor and stores it in the provided buffer.
 *
 * \param [in]  sensor_idx Index of the sensor.
 * \param [out] len Pointer to variable to store length of data read.
 * \param [out] data Pointer to buffer to store sensor data.
 *
 * \return BT_ESL_API_SUCCESS on success, error code otherwise.
 */
API_RESULT esl_io_read_sensor_data
           (
               /* IN */  UCHAR   sensor_idx,
               /* OUT */ UCHAR * len,
               /* OUT */ UCHAR * data
           );

/**
 * \brief Open a file handle for an image in storage.
 *
 * \par Description:
 * Opens a file handle for the specified image in storage.
 *
 * \param [in] img_idx Index of the image to open.
 * \param [in] image   Pointer to the image data (if applicable).
 *
 * \return BT_ESL_API_SUCCESS on success, error code otherwise.
 */
API_RESULT esl_io_open_image_from_storage(/* IN */ UCHAR img_idx, /* IN */ void* image);


/**
 * \brief Close the currently open image file handle in storage.
 *
 * \par Description:
 * Closes the currently open image file handle in storage.
 * \param [in] image   Pointer to the image data (if applicable).
 *
 * \return BT_ESL_API_SUCCESS on success, error code otherwise.
 */
API_RESULT esl_io_close_image_from_storage(/* IN */ void* image);


/**
 * \brief Write image data to storage.
 *
 * \par Description:
 * Writes image data to storage at the specified index and offset.
 *
 * \param [in] img_idx Index of the image to write.
 * \param [in] data Pointer to buffer to store image data.
 * \param [in] len Length of data to write.
 * \param [in] offset Offset in the image file to write to.
 * \param [in] image   Pointer to the image data (if applicable).
 *
 * \return BT_ESL_API_SUCCESS on success, error code otherwise.
 */
API_RESULT esl_io_write_img_to_storage
           (
               /* IN */ UCHAR  img_idx,
               /* IN */ void   * data,
               /* IN */ UINT32 len,
               /* IN */ UINT32 offset,
               /* IN */ void   * image
           );

/**
 * \brief Read image data from storage.
 *
 * \par Description:
 * Reads image data from storage at the specified index and offset.
 *
 * \param [in] img_idx Index of the image to read.
 * \param [in] data Pointer to buffer to store image data.
 * \param [in] len Length of data to read.
 * \param [in] offset Offset in the image file to read from.
 * \param [in] image   Pointer to the image data (if applicable).
 *
 * \return BT_ESL_API_SUCCESS on success, error code otherwise.
 */
API_RESULT esl_io_read_img_from_storage
           (
               /* IN */ UCHAR    img_idx,
               /* IN */ void   * data,
               /* IN */ UINT32   len,
               /* IN */ UINT32   offset,
               /* IN */ void   * image
           );

/**
 * \brief Get the size of an image in storage.
 *
 * \par Description:
 * Returns the size of the specified image in storage.
 *
 * \param [in] img_idx Index of the image.
 * \param [in] image   Pointer to the image data (if applicable).
 *
 * \return Size of the image in bytes, or 0 if not found.
 */
UINT32 esl_io_read_img_size_from_storage(/* IN */ UCHAR img_idx, /* IN */ void * image);

/**
 * \brief Delete all images from storage.
 *
 * \par Description:
 * Deletes all images from the storage backend.
 *
 * \param [in] image   Pointer to the image data (if applicable).
 *
 * \return BT_ESL_API_SUCCESS on success, error code otherwise.
 */
API_RESULT esl_io_delete_all_imgs(/* IN */ void * image);

/**
 * \brief Delete a specific image.
 *
 * \par Description:
 * Deletes the specified image.
 *
 * \param [in] image_index Index of the image to delete.
 * \param [in] image   Pointer to the image data (if applicable).
 *
 * \return BT_ESL_API_SUCCESS on success, error code otherwise.
 */
API_RESULT esl_io_delete_image(/* IN */ UCHAR image_index, /* IN */ void * image);

/**
 * \brief Get number of LEDs
 *
 * \par Description:
 * Returns the number of LEDs available on the device.
 *
 * \return Number of LEDs.
 */
UCHAR esl_io_get_num_leds(void);

/**
 * \brief Get number of sensors
 *
 * \par Description:
 * Returns the number of sensors available on the device.
 *
 * \return Number of sensors.
 */
UCHAR esl_io_get_num_sensors(void);

/**
 * \brief Get number of displays
 *
 * \par Description:
 * Returns the number of displays available on the device.
 *
 * \return Number of displays.
 */
UCHAR esl_io_get_num_displays(void);

/** \} */

/** \} */

#endif /* _ESL_IO_H_ */
