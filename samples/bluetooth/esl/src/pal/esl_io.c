/**
 *  \file esl_main.c
 *

 */

/*
 *  Copyright (C) 2025. Mindtree Ltd.
 *  All rights reserved.
 */

/* --------------------------------------------- Header File Inclusion */
#include "esl_io.h"


/* --------------------------------------------- Global Definitions */

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec leds[] =
{
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
#if IO_NUM_LEDS > 1
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
#endif /* IO_NUM_LEDS > 1 */
};

/* --------------------------------------------- External Global Variables */

/* --------------------------------------------- Exported Global Variables */

/* --------------------------------------------- Static Global Variables */

UCHAR sample_sensor_data[] =
{
    /* (Present Device Operating Temperature) sensor type sample data */
    0x64,
    /* (Present Correlated Color Temperature) sensor type sample data */
    0x32,
    /* (Present Indoor Ambient Temperatur) sensor type sample data */
    0x14,
};

/* --------------------------------------------- Function Protoype */
/**
 * \brief Initialize the display hardware or subsystem.
 *
 * \par Description:
 * Initializes the display hardware or subsystem for use by the application.
 *
 * \return BT_ESL_API_SUCCESS on success, error code otherwise.
 */
API_RESULT esl_io_display_init(void);


/**
 * \brief Initialize the LED hardware or subsystem.
 *
 * \par Description:
 * Initializes the LED hardware or subsystem for use by the application.
 *
 * \return BT_ESL_API_SUCCESS on success, error code otherwise.
 */
API_RESULT esl_io_led_init(void);


/**
 * \brief Initialize the sensor hardware or subsystem.
 *
 * \par Description:
 * Initializes the sensor hardware or subsystem for use by the application.
 *
 * \return BT_ESL_API_SUCCESS on success, error code otherwise.
 */
API_RESULT esl_io_sensor_init(void);

/* --------------------------------------------- Functions */

/* Initialize IO */
void esl_io_init(void)
{
   /* LED init */
   esl_io_led_init();

   /* Sensor init */
   esl_io_sensor_init();

   /* Display init */
   esl_io_display_init();

   ESL_IO_TRC("[ESL IO]: IO initialized\n");
}

/* Display init API */
API_RESULT esl_io_display_init(void)
{
    /* TODO: Implement display initialization */
    return BT_ESL_API_SUCCESS;
}

/* LED init API */
API_RESULT esl_io_led_init(void)
{
    API_RESULT retval;
    int i, ret;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    /* LED initialization */
    for (i = 0; i < ARRAY_SIZE(leds); i++)
    {
        if (!gpio_is_ready_dt(&leds[i]))
        {
            ESL_IO_TRC("[ESL IO]: LED %d is not ready\n", i);
            retval = BT_ESL_API_FAILURE;
            break;
        }

        ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        if (ret < 0)
        {
            ESL_IO_TRC("[ESL IO]: Failed to configure LED %d: %d\n", i, ret);
            retval = BT_ESL_API_FAILURE;
            break;
        }
    }

    return retval;
}

/* Sensor init API */
API_RESULT esl_io_sensor_init(void)
{
    /* TODO: Implement sensor initialization */
    return BT_ESL_API_SUCCESS;
}

/* Display control API */
API_RESULT esl_io_display_control
           (
               /* IN */UCHAR disp_idx,
               /* IN */UCHAR img_idx,
               /* IN */UCHAR enable,
               /* IN */void* image
           )
{
    API_RESULT retval;

    BT_ESL_IGNORE_UNUSED_PARAM(image);

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    /** Check display index */
    if (IO_NUM_DISPLAYS <= disp_idx)
    {
        ESL_IO_TRC("[ESL IO]: Error: Invalid display index (%d), max is %d\n", disp_idx, IO_NUM_DISPLAYS - 1);
        retval = BT_ESL_API_FAILURE; /* Invalid index */
    }

    /** Check image index */
    if (IO_MAX_IMAGES <= img_idx)
    {
        ESL_IO_TRC("[ESL IO]: Error: Invalid image index (%d), max is %d\n", img_idx, IO_MAX_IMAGES - 1);
        retval = BT_ESL_API_FAILURE; /* Invalid index */
    }

    /* Dummy print */
    ESL_IO_TRC("[ESL IO]: Display control - Display Index: %d, Image Index: %d, Enable: %d\n", disp_idx, img_idx, enable);

    /* TODO: Implement display control logic */
    return retval;
}

/* LED control API */
API_RESULT esl_io_led_control(/* IN */ ESL_IO_LED_PARAMS* esl_io_led_params)
{
    API_RESULT retval;
    int ret;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    /** Check LED index */
    if (IO_NUM_LEDS <= esl_io_led_params->led_idx)
    {
        ESL_IO_TRC(
        "[ESL IO]: Error: Invalid LED index (%d), max is %d\n",esl_io_led_params->led_idx, IO_NUM_LEDS - 1);

        retval = BT_ESL_API_FAILURE; /* Invalid index */
    }

    /** Check brightness */
    if (100 < esl_io_led_params->brightness)
    {
        ESL_IO_TRC(
        "[ESL IO]: Error: Invalid brightness (%d), max is 100\n", esl_io_led_params->brightness);
        retval = BT_ESL_API_FAILURE; /* Invalid value */
    }

    ESL_IO_TRC(
    "[ESL IO]: LED control - LED Index: %d, Brightness: %d, On/Off: %d, On/Off Period: %d\n",
    esl_io_led_params->led_idx, esl_io_led_params->brightness,
    esl_io_led_params->onoff, esl_io_led_params->onoff_period);
    /** Turn on/off LED */
    ret = gpio_pin_set_dt
          (
             &leds[esl_io_led_params->led_idx],
             (esl_io_led_params->onoff != BT_ESL_FALSE) ? esl_io_led_params->brightness : BT_ESL_FALSE
          );

    return retval;
}

/* Sensor control API */
API_RESULT esl_io_read_sensor_data
           (
               /* IN */  UCHAR   sensor_idx,
               /* OUT */ UCHAR * len,
               /* OUT */ UCHAR  * data
           )
{
    API_RESULT retval;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    if ((NULL == len) || (NULL == data))
    {
        retval = BT_ESL_API_FAILURE;
    }

    /** Check sensor index */
    if (IO_NUM_SENSORS <= sensor_idx)
    {
        ESL_IO_TRC("[ESL IO]: Error: Invalid sensor index (%d), max is %d\n", sensor_idx, IO_NUM_SENSORS - 1);
        retval = BT_ESL_API_FAILURE; /* Invalid index */
    }
    if (BT_ESL_API_SUCCESS == retval)
    {
        BT_ESL_mem_copy
        (
            data,
            &sample_sensor_data[sensor_idx * IO_SENSOR_DATA_SIZE],
            IO_SENSOR_DATA_SIZE
        );

        *len = IO_SENSOR_DATA_SIZE;
    }

    /* TODO: Implement sensor control logic */

    return retval;
}

/* Open image from storage API */
API_RESULT esl_io_open_image_from_storage(/* IN */ UCHAR img_idx, /* IN */ void * image)
{
    API_RESULT retval;

    BT_ESL_IGNORE_UNUSED_PARAM(image);

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    /** Check image index */
    if (IO_MAX_IMAGES <= img_idx)
    {
        ESL_IO_TRC("[ESL IO]: Error: Invalid image index (%d), max is %d\n", img_idx, IO_MAX_IMAGES - 1);
        retval = BT_ESL_API_FAILURE; /* Invalid index */
    }

    /* Dummy print */
    ESL_IO_TRC("[ESL IO]: Opened image from storage at index %d\n", img_idx);

    /* TODO: Implement open image from storage */
    return retval;
}

/* Close image from storage API */
API_RESULT esl_io_close_image_from_storage(/* IN */ void * image)
{
    BT_ESL_IGNORE_UNUSED_PARAM(image);

    /* dummy print */
    ESL_IO_TRC("[ESL IO]: Closed image from storage\n");
    /* TODO: Implement close image from storage */
    return BT_ESL_API_SUCCESS;
}

/* Write image to storage API */
API_RESULT esl_io_write_img_to_storage
           (
               /* IN */ UCHAR  img_idx,
               /* IN */ void   * data,
               /* IN */ UINT32 len,
               /* IN */ UINT32 offset,
               /* IN */ void   * image
           )
{
    API_RESULT retval;

    BT_ESL_IGNORE_UNUSED_PARAM(data);
    BT_ESL_IGNORE_UNUSED_PARAM(image);

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    /** Check image index */
    if (IO_MAX_IMAGES <= img_idx)
    {
        ESL_IO_TRC("[ESL IO]: Error: Invalid image index (%d), max is %d\n", img_idx, IO_MAX_IMAGES - 1);
        retval = BT_ESL_API_FAILURE; /* Invalid index */
    }

    /* dummy print */
    ESL_IO_TRC("[ESL IO]: Writing image to storage at index %d, length %d, offset %d\n", img_idx, len, offset);

    /* TODO: Implement write image to storage */
    return retval;
}

/* Read image from storage API */
API_RESULT esl_io_read_img_from_storage
           (
               /* IN */ UCHAR    img_idx,
               /* IN */ void   * data,
               /* IN */ UINT32   len,
               /* IN */ UINT32   offset,
               /* IN */ void   * image
           )
{
    API_RESULT retval;

    BT_ESL_IGNORE_UNUSED_PARAM(image);

    /** Init */
    retval = BT_ESL_API_SUCCESS;
    if (NULL == data)
    {
        ESL_IO_TRC("[ESL IO]: Error: Data pointer is NULL\n");
        retval = BT_ESL_API_FAILURE;
    }

    if ((IO_MAX_IMAGES <= img_idx) || (len == 0) || (offset < 0))
    {
        ESL_IO_TRC("[ESL IO]: Error: Invalid parameters for reading image %d\n", img_idx);
        retval = BT_ESL_API_FAILURE; /* Invalid parameters */
    }

    /* TODO: Implement read image from storage */

    return retval;
}

/* Read image size from storage API */
UINT32 esl_io_read_img_size_from_storage(/* IN */ UCHAR img_idx, /* IN */ void * image)
{
    API_RESULT retval;

    BT_ESL_IGNORE_UNUSED_PARAM(image);

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    /** Check image index */
    if (IO_MAX_IMAGES <= img_idx)
    {
        ESL_IO_TRC("[ESL IO]: Error: Invalid image index (%d), max is %d\n", img_idx, IO_MAX_IMAGES - 1);
        retval = BT_ESL_API_FAILURE; /* Invalid index */
    }

    /* Dummy print */
    ESL_IO_TRC("[ESL IO]: Reading image size from storage at index %d\n", img_idx);
    /* TODO: Implement read image size from storage */
    return 0;
}

/* Clear an image buffer */
API_RESULT esl_io_delete_image(/* IN */ UCHAR image_index, /* IN */ void * image)
{
    BT_ESL_IGNORE_UNUSED_PARAM(image);

   if (IO_MAX_IMAGES <= image_index)
   {
       ESL_IO_TRC("[ESL IO]: Error: Invalid image index (%d), max is %d\n", image_index, IO_MAX_IMAGES - 1);
       return BT_ESL_API_FAILURE; /* Invalid index */
   }

   ESL_IO_TRC("[ESL IO]: Image cleared at index %d\n", image_index);
   return BT_ESL_API_SUCCESS; /* Success */
}

/* Delete all images from storage API */
API_RESULT esl_io_delete_all_imgs(/* IN */ void * image)
{
    BT_ESL_IGNORE_UNUSED_PARAM(image);

    /* Dummy print */
    ESL_IO_TRC("[ESL IO]: Deleted all images from storage\n");
    /* TODO: Implement delete all images from storage */
    return BT_ESL_API_SUCCESS;
}

/* Get number of LEDs */
UCHAR esl_io_get_num_leds(void)
{
    return IO_NUM_LEDS;
}

/* Get number of sensors */
UCHAR esl_io_get_num_sensors(void)
{
    return IO_NUM_SENSORS;
}

/* Get number of displays */
UCHAR esl_io_get_num_displays(void)
{
    return IO_NUM_DISPLAYS;
}
