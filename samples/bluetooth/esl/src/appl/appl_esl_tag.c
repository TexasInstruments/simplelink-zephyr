/**
 *  \file appl_esl_tag.c
 *
 */

/*
 *  Copyright (C) 2025. LTIMindtree Ltd.
 *  All rights reserved.
 */

/* --------------------------------------------- Header File Inclusion */
#include "appl_esl_tag.h"
#include "esl_io.h"

#ifdef BT_ESL_SUPPORT_TAG_ROLE

/* --------------------------------------------- Global Definitions */
/** Factory reset disconnection time interval */
#define APPL_ESL_TAG_FACTORY_RESET_DISCONN_INTERVAL                    3U

/* --------------------------------------------- External Global Variables */

/* --------------------------------------------- Exported Global Variables */

/* --------------------------------------------- Static Global Variables */

/** ESL TAG instance */
DECL_STATIC BT_ESL_INSTANCE   tag_instance;
DECL_STATIC BT_ESL_DISPLAY    appl_esl_display[IO_NUM_DISPLAYS];
DECL_STATIC BT_ESL_LED        appl_esl_led[IO_NUM_LEDS];
DECL_STATIC BT_ESL_SENSOR     appl_esl_sensor[IO_NUM_SENSORS];

APPL_ESL_DISPLAY_IMAGE_PARAMS appl_esl_display_image[IO_NUM_DISPLAYS];
APPL_ESL_DISPLAY_DATA_STRUCT  appl_esl_display_info[IO_NUM_DISPLAYS];
APPL_ESL_LED_CONTROL_PARAM    appl_esl_led_control[IO_NUM_LEDS];

/** Factory reset ongoing flag */
DECL_STATIC UCHAR appl_esl_factory_reset_in_progress;

/** Factory reset disconnection timer handle */
DECL_STATIC BT_ESL_TIMER_HANDLE appl_esl_factory_reset_disconn_timer;

/** ESL tag callbacks  */
DECL_STATIC BT_ESL_TAG_CALLBACKS appl_esl_cb;

/**
 * \brief Flag to indicate whether single I/O mode is enabled or disabled.
 *
 * This static variable controls the behavior of the ESL tag when operating
 * in single I/O mode. If set to `BT_ESL_TRUE`, the tag will restrict its
 * operations to a single I/O (e.g., one display, one LED, etc.). If set to
 * `BT_ESL_FALSE`, the tag will operate with multiple I/Os as supported.
 *
 * \note This flag is introduced for specific PTS test cases that require
 * single I/O mode. Instead of changing the configuration at compile time,
 * this flag allows configuring the ESL tag behavior at runtime before a
 * power cycle.
 *
 * Default value: `BT_ESL_FALSE` (multi-I/O mode).
 */
DECL_STATIC UCHAR single_io_support_flag = BT_ESL_FALSE;

#ifdef APPL_ESL_TAG_OTS_SUPPORT
/** OTS server cb */
DECL_STATIC BT_ESL_OTS_SERVER_CALLBACK appl_esl_ots_server_cb;
/** OTS metadata */
DECL_STATIC BT_ESL_OTS_METADATA obj_meta_data[APPL_ESL_IMAGE_MAX];
#endif /* APPL_ESL_TAG_OTS_SUPPORT */

DECL_CONST UCHAR device_name[] = "ESL tag - 0001";

DECL_CONST UCHAR appl_esl_display_info_data_structure[] =
{
    /* Record 1 */
    /* Width = 1000 */
    0x00, 0x10,
    /*  Height = 1000 */
    0x00, 0x10,
    /* Display type =  black white */
    0x01,
    /* Record 2 */
    0x00, 0x10, 0x00, 0x10, 0x02,
    /* Record 3 */
    0x0a, 0x00, 0x00, 0x0b, 0x01,
    /* Record 4 */
    0x0a, 0x00, 0x00, 0x0b, 0x01,
    /* Record 5 */
    0x0a, 0x00, 0x00, 0x0b, 0x01
};

DECL_CONST UCHAR appl_esl_led_info_structure[] =
{
    /**
     *  bit 7 and 6 led type,
     *  bit 5 and 4 Blue,
     *  bit 3 and 2 Green,
     *  bit 1 and 0 Red
     */
    /* LED type = Monochrome, Color = Green */
    0x4C,
    /* LED type = Monochrome, Color = Red */
    0x43,
    /* LED type = sRGB, Color = Red Green Blue */
    0x3F,
    /* LED type = sRGB, Color = Red Blue */
    0x33,
    /* LED type = Monochrome, Color = Blue */
    0x70
};

DECL_CONST UCHAR appl_esl_sensor_info_structure[] =
{
    /* 1-byte Size and 2-byte Sensor type(Present Device Operating Temperature)*/
    0x00, 0x54, 0x00,
    /* 1-byte Size and 2-byte Sensor type(Present Correlated Color Temperature) */
    0x00, 0x51, 0x00,
    /* 1-byte Size and 2-byte Sensor type(Present Indoor Ambient Temperature) */
    0x00, 0x56, 0x00
};

DECL_CONST UCHAR appl_esl_sensor_data[] =
{
    0x64,
    0x32,
    0x14
};

/* --------------------------------------------- Function Prototype */

/* --------------------------------------------- Functions */
void appl_esl_tag_init_est_instance(void)
{
    /* Initialize the tag instance */
    BT_ESL_TAG_INIT_ESL_INST(&tag_instance);

    /* Fill at least 1 for all required members */
    /* Use macros to initialize display, led, and sensor info */
#ifdef BT_ESL_SUPPORTS_DISPLAYS
    tag_instance.max_image_supported = (APPL_ESL_IMAGE_MAX - 1);
    appl_esl_populate_display_info();
#endif /* BT_ESL_SUPPORTS_DISPLAYS */

#ifdef BT_ESL_SUPPORTS_LEDS
#if 0
    /* Use macro to set Monochrome, Green only */
    tag_instance.led_info.esl_led[0].led_type = BT_ESL_PACK_LED_TYPE(BT_ESL_MONOCHROME, 0, 1, 0);
#endif
    appl_esl_populate_led_info();
#endif /* BT_ESL_SUPPORTS_LEDS */

#ifdef BT_ESL_SUPPORTS_SENSORS
    appl_esl_populate_sensor_info();
#endif /* BT_ESL_SUPPORTS_SENSORS */

    appl_esl_init_display_image_struct();
    appl_esl_init_led_struct();

#ifdef APPL_ESL_TAG_OTS_SUPPORT
    /* OTS init */
    appl_esl_tag_ots_init();
    APPL_ESL_TRC("[APPL]: OTS Initialized\n");

    /* Add objects */
    appl_esl_tag_ots_add_objects();
    APPL_ESL_TRC("[APPL]: OTS Objects added\n");
#endif /* APPL_ESL_TAG_OTS_SUPPORT */
}

#ifdef APPL_ESL_TAG_OTS_SUPPORT
void appl_esl_tag_ots_init(void)
{
    API_RESULT retval;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    /* OTS initialization */
    retval = BT_esl_ots_init_pl();
    if (BT_ESL_API_SUCCESS != retval)
    {
        APPL_ESL_TRC(
        "[APPL]: BT_esl_ots_init_pl failed - retval 0x%04X\n", retval);
    }

    /* Register callback */
    appl_esl_ots_server_cb.image_selected = appl_esl_tag_ots_image_selected_cb;
    appl_esl_ots_server_cb.image_write = appl_esl_tag_ots_image_written_cb;

    retval = BT_esl_register_ots_server_callback_pl(&appl_esl_ots_server_cb);
    if (BT_ESL_API_SUCCESS != retval)
    {
        APPL_ESL_TRC(
        "[APPL]: BT_esl_register_ots_server_callback_pl failed - retval 0x%04X\n", retval);
    }
}

void appl_esl_tag_ots_add_objects(void)
{
    API_RESULT retval;
    UCHAR i;

    for (i = 0; i < APPL_ESL_IMAGE_MAX; i++)
    {
        /* Initialize metadata */
        snprintf(obj_meta_data[i].name, sizeof(obj_meta_data[i].name), "Image_%d", i);
        obj_meta_data[i].cur_size = 1024;
        obj_meta_data[i].alloc_size = 1024;
    }

    retval = BT_esl_ots_create_obj_pl(APPL_ESL_IMAGE_MAX, obj_meta_data);
    if (BT_ESL_API_SUCCESS != retval)
    {
        APPL_ESL_TRC(
        "[APPL]: BT_esl_ots_create_obj_pl failed - retval 0x%04X\n", retval);
    }
}
void appl_esl_tag_ots_image_selected_cb(BT_ESL_BD_ADDR *bd_addr, UCHAR image_index)
{
    APPL_ESL_TRC("[APPL]: OTS Image selected callback\n");

    if (NULL !=  bd_addr)
    {
        APPL_ESL_TRC("\tPeer address: "
        BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER "\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr));
    }

    APPL_ESL_TRC("\tImage index selected: %d\n", image_index);

    (BT_ESL_IGNORE_RETURN_VALUE)esl_io_open_image_from_storage(image_index, NULL);
}

API_RESULT appl_esl_tag_ots_image_written_cb
           (
                BT_ESL_BD_ADDR *bd_addr,
                UCHAR image_index,
                void * data,
                UINT32 len,
                UINT32 offset,
                UINT32 rem
           )
{

    APPL_ESL_TRC("[APPL]: OTS Image write callback\n");

    if (NULL !=  bd_addr)
    {
        APPL_ESL_TRC("\tpeer address: "
        BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER "\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr));
    }

    APPL_ESL_TRC("\tImage index: %d\n", image_index);
    APPL_ESL_TRC("\tData length: %d\n", len);
    APPL_ESL_TRC("\tData offset: %d\n", offset);
    APPL_ESL_TRC("\tData remaining: %d\n", rem);

    if(0 == rem)
    {
        APPL_ESL_TRC("[APPL]: Image upload complete\n");
        (BT_ESL_IGNORE_RETURN_VALUE)esl_io_write_img_to_storage
                                    (
                                         image_index,
                                         data,
                                         offset + len, /* Total length */
                                         0U,
                                         NULL
                                    );
        (BT_ESL_IGNORE_RETURN_VALUE)esl_io_close_image_from_storage(NULL);
    }

    return BT_ESL_API_SUCCESS;
}
#endif /* BT_ESL_OTS_SERVER_CALLBACK */

void appl_esl_init_display_image_struct(void)
{
    UINT32 i;
    UCHAR max_no_of_displays;

    max_no_of_displays = appl_esl_tag_get_number_of_io(BT_ESL_DISPLAY_INFO_TYPE);

    for (i = 0; i < max_no_of_displays; i++)
    {
        appl_esl_stop_timer(appl_esl_display_image[i].image_control_timer_handle);
        APPL_ESL_INIT_DISPLAY_IMAGE_PARAMS(&appl_esl_display_image[i]);
        APPL_ESL_INIT_DISPLAY_DATA_STRUCT(&appl_esl_display_info[i]);
    }
}

void appl_esl_init_led_struct(void)
{
    UINT32 i;
    UCHAR max_no_of_leds;

    max_no_of_leds = appl_esl_tag_get_number_of_io(BT_ESL_LED_INFO_TYPE);

    for (i = 0; i < max_no_of_leds; i++)
    {
        appl_esl_stop_timer(appl_esl_led_control[i].led_control_timer_handle);
        appl_esl_stop_timer(appl_esl_led_control[i].led_repeat_duration_timer_handle);
        APPL_ESL_INIT_LED_CONTROL_PARAMS(&appl_esl_led_control[i]);
    }
}

void appl_esl_populate_sensor_info(void)
{
    UINT32 i, j;

    j = 0;

    tag_instance.sensor_info.esl_sensor    = appl_esl_sensor;
    tag_instance.sensor_info.no_of_sensors = appl_esl_tag_get_number_of_io(BT_ESL_SENSOR_INFO_TYPE);

    /* Set Sensor type and size */
    for (i = 0; i < tag_instance.sensor_info.no_of_sensors; i++)
    {
        tag_instance.sensor_info.esl_sensor[i].sensor_size = appl_esl_sensor_info_structure[j];
        j++;

        /* Sensor type is of 2 bytes */
        BT_ESL_mem_copy
        (
            &tag_instance.sensor_info.esl_sensor[i].sensor_type.type_16,
            &appl_esl_sensor_info_structure[j],
            2
        );
        j += 2;
    }
}

void appl_esl_populate_display_info(void)
{
    UINT32 i, j;

    j = 0;

    tag_instance.display_info.esl_display    = appl_esl_display;
    tag_instance.display_info.no_of_displays = appl_esl_tag_get_number_of_io(BT_ESL_DISPLAY_INFO_TYPE);

    for (i = 0; i < tag_instance.display_info.no_of_displays; i++)
    {
        BT_ESL_mem_copy
        (
            &tag_instance.display_info.esl_display[i].width,
            &appl_esl_display_info_data_structure[j],
            2
        );
        j += 2;

        BT_ESL_mem_copy
        (
            &tag_instance.display_info.esl_display[i].height,
            &appl_esl_display_info_data_structure[j],
            2
        );
        j += 2;

        tag_instance.display_info.esl_display[i].display_type =
            appl_esl_display_info_data_structure[j];
        j++;

        appl_esl_display_info[i].image_displayed = BT_ESL_FALSE;
    }
}

void appl_esl_populate_led_info(void)
{
    UINT32 i;

    tag_instance.led_info.esl_led    = appl_esl_led;
    tag_instance.led_info.no_of_leds = appl_esl_tag_get_number_of_io(BT_ESL_LED_INFO_TYPE);

    /* Set LED type */
    for (i = 0; i < tag_instance.led_info.no_of_leds; i++)
    {
        tag_instance.led_info.esl_led[i].led_type = appl_esl_led_info_structure[i];
    }
}

void appl_esl_tag_init()
{
    API_RESULT retval;

#ifdef BT_ESL_HAVE_DYNAMIC_GLOBAL_ARRAY
    BT_ESL_DYNAMIC_CONFIG appl_esl_dynamic_config;
#endif /* BT_ESL_HAVE_DYNAMIC_GLOBAL_ARRAY */

    /* IO init */
    esl_io_init();

    /* Initialize the ESL Tag related Instance Data Structure */
    appl_esl_tag_init_est_instance();

    /* Register the tag callbacks */
    /* Initialize appl_cb with minimal required callbacks */
    appl_esl_cb.esl_connect_ind_cb = appl_esl_tag_connection_ind_cb;
    appl_esl_cb.esl_disconnect_ind_cb = appl_esl_tag_disconnection_ind_cb;
    appl_esl_cb.esl_control_point_configured_ind_cb = appl_esl_tag_control_point_configured_ind_cb;
    appl_esl_cb.esl_configured_ind_cb = appl_esl_tag_configured_ind_cb;
    appl_esl_cb.els_read_esl_info_ind_cb = appl_els_read_esl_info_ind_cb;
    appl_esl_cb.els_write_ap_sync_key_ind_cb = appl_els_write_ap_sync_key_ind_cb;
    appl_esl_cb.els_write_esl_addr_ind_cb = appl_els_write_esl_addr_ind_cb;
    appl_esl_cb.esl_display_image_cmd_cb = appl_esl_tag_display_image_cmd_cb;
    appl_esl_cb.esl_factory_reset_cmd_cb = appl_esl_tag_factory_reset_cmd_cb;
    appl_esl_cb.esl_led_control_cmd_cb = appl_esl_tag_led_control_cmd_cb;
    appl_esl_cb.esl_ping_cmd_cb = appl_esl_tag_ping_cmd_cb;
    appl_esl_cb.esl_read_sensor_data_cmd_cb = appl_esl_tag_read_sensor_data_cmd_cb;
    appl_esl_cb.esl_refresh_image_cmd_cb = appl_esl_tag_refresh_display_cmd_cb;
    appl_esl_cb.esl_service_reset_cmd_cb = appl_esl_tag_service_reset_cmd_cb;
    appl_esl_cb.esl_synchronized_ind_cb = appl_esl_tag_synchronized_ind_cb;
    appl_esl_cb.esl_unassociate_from_ap_cmd_cb = appl_esl_tag_unassociate_from_ap_cmd_cb;
    appl_esl_cb.esl_unknown_cmd_cb = appl_esl_tag_unknown_cmd_cb;
#ifdef BT_ESL_SUPPORT_VENDOR_SPECIFIC_COMMANDS
    appl_esl_cb.esl_vendor_specific_command_cb = appl_esl_tag_vendor_specific_command_cb;
#endif /* BT_ESL_SUPPORT_VENDOR_SPECIFIC_COMMANDS */
    appl_esl_cb.esl_write_abs_time_ind_cb = appl_esl_tag_write_abs_time_ind_cb;
    appl_esl_cb.esl_write_esl_rsp_key_ind_cb = appl_esl_tag_write_esl_rsp_key_ind_cb;
    appl_esl_cb.sync_terminated_ind_cb = appl_esl_sync_terminated_ind_cb;
    appl_esl_cb.unassociated_ind_cb = appl_esl_unassociated_ind_cb;
    appl_esl_cb.unsynchronized_ind_cb = appl_esl_unsynchronized_ind_cb;
    appl_esl_cb.esl_update_complete_cmd_cb = appl_esl_tag_update_complete_cmd_cb;
#ifdef BT_ESL_HAVE_DYNAMIC_GLOBAL_ARRAY
    /** Initialize with default values for dynamic configuration */
    BT_ESL_INIT_DYNAMIC_CONFIG_LIMITS(&appl_esl_dynamic_config);

    /* Initialize the ESL Module */
    BT_esl_init((void *)&appl_esl_dynamic_config);

#else /* BT_ESL_HAVE_DYNAMIC_GLOBAL_ARRAY */
    /** Initialize the ESL Module */
    BT_esl_init(NULL);
#endif /* BT_ESL_HAVE_DYNAMIC_GLOBAL_ARRAY */

    /* Initialize the ESL Tag */
    retval = BT_esl_tag_init(&tag_instance);
    if (BT_ESL_TAG_SUCCESS != retval)
    {
        APPL_ESL_TRC("[APPL]: BT_esl_tag_init failed - retval 0x%04X\n", retval);
    }

    retval = BT_esl_tag_register_cb(&appl_esl_cb);
    if (BT_ESL_TAG_SUCCESS != retval)
    {
        APPL_ESL_TRC("[APPL]: BT_esl_tag_register_cb failed - retval 0x%04X\n", retval);
    }
    appl_esl_factory_reset_in_progress   = BT_ESL_FALSE;
    appl_esl_factory_reset_disconn_timer = BT_ESL_TIMER_HANDLE_INIT_VAL;
}

API_RESULT appl_esl_tag_start_advertise(void)
{
    API_RESULT retval;

    /* (BT_ESL_IGNORE_RETURN_VALUE)appl_esl_tag_stop_advertise(); */

    /* Update the complete name */
    (BT_ESL_IGNORE_RETURN_VALUE)BT_esl_update_complete_name_pl
    (
        (UCHAR*)device_name,
        sizeof(device_name) - 1
    );
    /* Start advertising */
    retval = BT_esl_start_advertise_pl();
    if (BT_ESL_API_SUCCESS != retval)
    {
        APPL_ESL_TRC("[APPL]: BT_esl_start_advertise_pl retval - 0x%04X\n", retval);
    }

    return retval;
}

API_RESULT appl_esl_tag_stop_advertise(void)
{
    API_RESULT retval;

    /* Stop advertising */
    retval = BT_esl_stop_advertise_pl();
    if (BT_ESL_API_SUCCESS != retval)
    {
        APPL_ESL_TRC("[APPL]: BT_esl_stop_advertise_pl retval - 0x%04X\n", retval);
    }

    return retval;
}

/* Minimal stub implementations for required callbacks */
void appl_esl_tag_connection_ind_cb
     (
         UCHAR status,
         void* blob
     )
{
    BT_ESL_IGNORE_UNUSED_PARAM(status);

    BT_ESL_BD_ADDR* addr;

    APPL_ESL_TRC("[APPL]: esl_connect_ind_cb\n");

    addr = (BT_ESL_BD_ADDR*)blob;
    if (NULL != addr)
    {
        APPL_ESL_TRC("[APPL]: Connected to ESL tag with address: "
        BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER "\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(addr));

        if (BT_ESL_TRUE == BT_ESL_CHECK_SYNCHRONISED_BIT())
        {
            /* Hits here if Tag is already synchronized and reconnected back to AP */
            /* Reset the synchronized bit in basic state */
            BT_ESL_RESET_SET_SYNCHRONISED_BIT();
        }
    }
    else
    {
        APPL_ESL_TRC("[APPL]: No Address\n");
    }

}

void appl_esl_tag_disconnection_ind_cb(void * blob)
{
    BT_ESL_BD_ADDR* addr;

    APPL_ESL_TRC("[APPL]: esl_disconnect_ind_cb\n");

    addr = (BT_ESL_BD_ADDR*)blob;
    if (NULL != addr)
    {
        APPL_ESL_TRC(
        "[APPL]: Disconnected from ESL tag with address: "
        BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER "\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(addr));
    }
    else
    {
        APPL_ESL_TRC("[APPL]: No address\n");
    }
}

void appl_esl_tag_control_point_configured_ind_cb
     (
         UCHAR  char_id,
         UINT16 cccd_value,
         void   * blob
     )
{
    BT_ESL_IGNORE_UNUSED_PARAM(char_id);

    BT_ESL_BD_ADDR* addr;

    APPL_ESL_TRC("[APPL]: esl_control_point_configured_ind_cb\n");

    addr = (BT_ESL_BD_ADDR*)blob;
    if (NULL != addr)
    {
        APPL_ESL_TRC("[APPL]: Control point configured for ESL tag with address: "
        BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER "\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(addr));

        APPL_ESL_TRC(
        "[APPL]: Control Point CCCD is configured with CCCD Value: 0x%04X\n", cccd_value);
    }
}

void appl_esl_tag_configured_ind_cb(void * blob)
{
    BT_ESL_BD_ADDR* addr;

    APPL_ESL_TRC("[APPL]: appl_esl_tag_configured_ind_cb\n");

    addr = (BT_ESL_BD_ADDR*)blob;
    if (NULL != addr)
    {
        APPL_ESL_TRC("[APPL]: Configured for ESL tag with address: "
        BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER "\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(addr));
    }
}

void appl_els_read_esl_info_ind_cb
     (
         UCHAR char_type,
         void  * blob
     )
{
    BT_ESL_IGNORE_UNUSED_PARAM(blob);

    APPL_ESL_TRC("[APPL]: appl_els_read_esl_info_ind_cb\n");

    APPL_ESL_INF(
    "[APPL]: Received Read Request for char ID: %d\n", char_type);

}

void appl_els_write_ap_sync_key_ind_cb
           (
               UINT16 status,
               void   * blob
           )
{
    BT_ESL_IGNORE_UNUSED_PARAM(blob);

    APPL_ESL_TRC("[APPL]: appl_els_write_ap_sync_key_ind_cb\n");

    APPL_ESL_TRC(
    "[APPL]: AP Sync Key Material is written with retval 0x%04X\n", status);

}

void appl_els_write_esl_addr_ind_cb
     (
         UINT16 status,
         void   * blob
     )
{
    BT_ESL_IGNORE_UNUSED_PARAM(blob);

    APPL_ESL_TRC("[APPL]: appl_els_write_esl_addr_ind_cb\n");

    APPL_ESL_TRC(
    "[APPL]: ESL Address is written with status 0x%04X\n", status);

}

API_RESULT appl_esl_tag_display_image_cmd_cb
           (
               UCHAR  * data,
               UINT16 datalen,
               void   * blob
           )
{
    UCHAR opcode;
    API_RESULT retval;
    UINT16 length;
    UCHAR esl_id;

    retval = BT_ESL_API_SUCCESS;

    BT_ESL_IGNORE_UNUSED_PARAM(blob);

    APPL_ESL_TRC("[APPL]: appl_esl_tag_display_image_cmd_cb\n");
    opcode = data[0];
    /* Extract ESL ID from the request */
    esl_id = data[1];
    /* Extract Length from the opcode */
    length = BT_ESL_GET_CP_LEN(opcode);

    if (BT_ESL_TRUE == appl_esl_factory_reset_in_progress)
    {
        APPL_ESL_INF("[APPL]: Factory Reset command is active\n");

        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_UNSPECIFIED_ERROR);

        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        if ((datalen != length) || (tag_instance.esl_address.esl_id != esl_id))
        {
            APPL_ESL_ERR(
            "[APPL]: Invalid data length for Display image command: %d, expected: %d\n",
            datalen, length);

            /* Send Error response */
            BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_PARAMETERS);

            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            (opcode == BT_ESL_DISPLAY_IMAGE) ? appl_esl_display_image_handler(&data[2], datalen) :
                                               appl_esl_display_timed_image_handler(&data[2], datalen);
        }
    }

    return retval;
}

API_RESULT appl_esl_tag_factory_reset_cmd_cb
           (
               UCHAR  command_source,
               UCHAR  * data,
               UINT16 data_len,
               void   * blob
           )
{
    API_RESULT retval;
    UCHAR opcode;
    UINT16 length;
    UCHAR esl_id;

    BT_ESL_IGNORE_UNUSED_PARAM(blob);

    retval = BT_ESL_API_SUCCESS;

    APPL_ESL_TRC("[APPL]: appl_esl_tag_factory_reset_cmd_cb\n");

    /* Extract the opcode */
    opcode = data[0];
    /* Extract ESL ID from the request */
    esl_id = data[1];

    /* Extract Length from the opcode */
    length = BT_ESL_GET_CP_LEN(opcode);

    if (BT_ESL_TRUE == appl_esl_factory_reset_in_progress)
    {
        APPL_ESL_INF("[APPL]: Previous Factory Reset command is active\n");

        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_UNSPECIFIED_ERROR);

        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        if ((data_len != length) || (tag_instance.esl_address.esl_id != esl_id))
        {
            APPL_ESL_ERR(
            "[APPL]: Invalid data length for Factory Reset command: %d, expected: %d\n",
            data_len, length);

            /* Send Error response */
            BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_PARAMETERS);

            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            /**
             * If command is for broadcast OR received in synchronized state
             * don't do factory reset.
             */

            if (
                (BT_ESL_PAWR == command_source) &&
                (BT_ESL_TRUE == BT_ESL_CHECK_SYNCHRONISED_BIT())
                )
            {
                /* If synchronized and Synchronized bit is set in basic state */
                APPL_ESL_INF("[APPL]: Factory Reset Request received from PAWR\n");
                /* Send Error response */
                BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_STATE);
            }
            else if (BT_ESL_ECP == command_source)
            {
                /* Handle factory reset request from the tag */
                APPL_ESL_INF("[APPL]: Factory Reset Request received from Tag\n");

                /**
                 * Response not needed for Factory Reset
                 *
                 * If not synchronized state and not broadcast
                 * 1. ESL shall initiate disconnection of the link with the AP.
                 * 2. ESL shall remove all bonding information with the AP
                 * 3. Delete values of the AP Sync Key Material, ESL Response Key Material,
                 *    and ESL Address characteristics
                 */

                 /**
                  * Set the variable here so that if AP sends
                  * any other command before the disconnection
                  * ESL to respond with error
                  */
                appl_esl_factory_reset_in_progress = BT_ESL_TRUE;

                /* Start a Timer to Initiate disconnection from ESL */
                if (BT_ESL_TIMER_HANDLE_INIT_VAL != appl_esl_factory_reset_disconn_timer)
                {
                    /* Stop the timer if already running */
                    BT_ESL_stop_timer(appl_esl_factory_reset_disconn_timer);
                    appl_esl_factory_reset_disconn_timer = BT_ESL_TIMER_HANDLE_INIT_VAL;
                }

                /** Initiate timer for disconnection */
                retval = BT_ESL_start_timer
                         (
                             &appl_esl_factory_reset_disconn_timer,
                             APPL_ESL_TAG_FACTORY_RESET_DISCONN_INTERVAL,
                             appl_esl_factory_reset_disconn_timeout_handler,
                             NULL,
                             0U
                         );
                if (BT_ESL_API_SUCCESS != retval)
                {
                    APPL_ESL_ERR (
                    "[APPL]: Failed to start factory reset disconnection timer\n");
                }

            }
            else
            {
                APPL_ESL_ERR("[APPL]: Invalid Command Source for Factory Reset Request\n");
                retval = BT_ESL_API_FAILURE;
            }
        }
    }

    return retval;
}

void appl_esl_factory_reset_disconn_timeout_handler (void * args, UINT16 size)
{
    BT_ESL_IGNORE_UNUSED_PARAM(args);
    BT_ESL_IGNORE_UNUSED_PARAM(size);

    APPL_ESL_TRC(
    "[APPL]: Initiating disconnection for Factory Reset on Timer Expiry\n");

    /* Initiate disconnection from ESL */
    (BT_ESL_IGNORE_RETURN_VALUE)BT_esl_tag_disconnect();
}

API_RESULT appl_esl_tag_led_control_cmd_cb
           (
               UCHAR  * data,
               UINT16 datalen,
               void   * blob
           )
{
    UCHAR opcode, esl_id;
    API_RESULT retval;
    UINT16 length;

    BT_ESL_IGNORE_UNUSED_PARAM(blob);

    APPL_ESL_TRC("[APPL]: appl_esl_tag_led_control_cmd_cb\n");

    retval = BT_ESL_API_SUCCESS;
    opcode = data[0];
    /* Extract ESL ID from the request */
    esl_id = data[1];

    /* Extract Length from the opcode */
    length = BT_ESL_GET_CP_LEN(opcode);

    if ((BT_ESL_LED_CONTROL == opcode) || (BT_ESL_LED_TIMED_CONTROL == opcode))
    {
        if (BT_ESL_TRUE == appl_esl_factory_reset_in_progress)
        {
            APPL_ESL_INF("[APPL]: Factory Reset command is active\n");

            /* Send Error response */
            BT_esl_tag_send_esl_error_response(BT_ESL_UNSPECIFIED_ERROR);

            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            if ((datalen != length) || (tag_instance.esl_address.esl_id != esl_id))
            {
                APPL_ESL_ERR(
                "[APPL]: Invalid data length for LED Control command: %d, expected: %d\n",
                datalen, length);

                /* Send Error response */
                BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_PARAMETERS);

                retval = BT_ESL_API_FAILURE;
            }
            else
            {
                (opcode == BT_ESL_LED_CONTROL)?appl_esl_led_control_req_handler(data, datalen) :
                                               appl_esl_led_timed_control_req_handler(data, datalen);
            }
        }
    }

    return retval;
}

API_RESULT appl_esl_tag_ping_cmd_cb
           (
               UCHAR  * data,
               UINT16 data_len,
               void   * blob
           )
{
    BT_ESL_BD_ADDR * addr;
    UINT16 length;
    UCHAR opcode, esl_id;
    API_RESULT retval;

    retval = BT_ESL_API_SUCCESS;

    APPL_ESL_TRC("[APPL]: appl_esl_tag_ping_cmd_cb\n");

    addr = (BT_ESL_BD_ADDR*)blob;
    opcode = data[0];
    /* Extract ESL ID from the request */
    esl_id = data[1];

    /* Extract Length from the opcode */
    length = BT_ESL_GET_CP_LEN(opcode);

    if (NULL != addr)
    {
        APPL_ESL_TRC("[APPL]: Ping command received from : "
        BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER "\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(addr));
    }
    if (BT_ESL_TRUE == appl_esl_factory_reset_in_progress)
    {
        APPL_ESL_INF("[APPL]: Factory Reset command is active\n");

        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_UNSPECIFIED_ERROR);

        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        if ((data_len != length) || (tag_instance.esl_address.esl_id != esl_id))
        {
            APPL_ESL_ERR(
            "[APPL]: Invalid data length for Ping command: %d, expected: %d\n",
            data_len, length);

            /* Send Error response */
            BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_PARAMETERS);

            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            /* Send Basic state response */
            BT_esl_tag_send_basic_state(BT_esl_tag_get_basic_state());
        }
    }

    return retval;
}

API_RESULT appl_esl_tag_read_sensor_data_cmd_cb
           (
               UCHAR  * data,
               UINT16 data_length,
               void   * blob
           )
{
    BT_ESL_BD_ADDR* addr;
    UINT16 length;
    UCHAR opcode, esl_id, sensor_id;
    API_RESULT retval;
    BT_ESL_SENSOR_DATA appl_sensor_data;
    UCHAR max_no_of_sensors_supported;

    retval = BT_ESL_API_SUCCESS;

    APPL_ESL_TRC("[APPL]: appl_esl_tag_read_sensor_data_cmd_cb\n");

    addr = (BT_ESL_BD_ADDR*)blob;
    opcode = data[0];
    /* Extract ESL ID from the request */
    esl_id = data[1];
    /* Extract Sensor Index */
    sensor_id = data[2];

    /* Extract Length from the opcode */
    length = BT_ESL_GET_CP_LEN(opcode);

    if (NULL != addr)
    {
        APPL_ESL_TRC("[APPL]: Read sensor data command received from : "
        BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER "\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(addr));
    }
    if (BT_ESL_TRUE == appl_esl_factory_reset_in_progress)
    {
        APPL_ESL_INF("[APPL]: Factory Reset command is active\n");

        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_UNSPECIFIED_ERROR);

        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        max_no_of_sensors_supported = appl_esl_tag_get_number_of_io(BT_ESL_SENSOR_INFO_TYPE);

        if (
                (data_length != length) ||
                (tag_instance.esl_address.esl_id != esl_id) ||
                (sensor_id >= max_no_of_sensors_supported)
           )
        {
            APPL_ESL_ERR(
            "[APPL]: Invalid data length for Read Sensor Data command: %d, expected: %d\n",
            data_length, length);

            /* Send Error response */
            BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_PARAMETERS);

            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            retval = esl_io_read_sensor_data
                     (
                         sensor_id,
                         &appl_sensor_data.data_length,
                         appl_sensor_data.sensor_data
                     );

            if (BT_ESL_API_SUCCESS == retval)
            {
                BT_esl_tag_send_sensor_value
                (
                    sensor_id,
                    appl_sensor_data.sensor_data,
                    appl_sensor_data.data_length
                );
            }
            else
            {
                APPL_ESL_ERR(
                "[APPL]: Failed to read sensor data for sensor ID: %d\n", sensor_id);
                /* Send Error response */
                BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_PARAMETERS);
            }
        }
    }

    return retval;
}

API_RESULT appl_esl_tag_refresh_display_cmd_cb
           (
               UCHAR  * data,
               UINT16 data_len,
               void   * blob
           )
{
    BT_ESL_BD_ADDR* addr;
    UINT16 length;
    UCHAR opcode, esl_id, display_index;
    API_RESULT retval;
    UCHAR max_no_of_display_supported;

    retval = BT_ESL_API_SUCCESS;

    APPL_ESL_TRC("[APPL]: appl_esl_tag_refresh_display_cmd_cb\n");

    addr = (BT_ESL_BD_ADDR*)blob;
    opcode = data[0];
    /* Extract ESL ID from the request */
    esl_id = data[1];
    /* Extract Sensor Index */
    display_index = data[2];

    /* Extract Length from the opcode */
    length = BT_ESL_GET_CP_LEN(opcode);

    if (NULL != addr)
    {
        APPL_ESL_TRC("[APPL]: Refresh Display command received from : "
        BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER "\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(addr));
    }
    if (BT_ESL_TRUE == appl_esl_factory_reset_in_progress)
    {
        APPL_ESL_INF("[APPL]: Factory Reset command is active\n");

        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_UNSPECIFIED_ERROR);

        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        max_no_of_display_supported = appl_esl_tag_get_number_of_io(BT_ESL_DISPLAY_INFO_TYPE);

        if (
            (data_len != length) ||
            (tag_instance.esl_address.esl_id != esl_id) ||
            (display_index >= max_no_of_display_supported)
            )
        {
            APPL_ESL_ERR(
            "[APPL]: Invalid data length for Refresh Display command: %d, expected: %d\n",
            data_len, length);

            /* Send Error response */
            BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_PARAMETERS);

            retval = BT_ESL_API_FAILURE;
        }
        else if (BT_ESL_FALSE == appl_esl_display_info[display_index].image_displayed)
        {
            /* Indicates that there is no image displayed on the specified display */
            APPL_ESL_INF("[APPL]: No Image being Displayed on the display index %d\n", display_index);

            /* Send Error response */
            BT_esl_tag_send_esl_error_response(BT_ESL_UNSPECIFIED_ERROR);
        }
        else
        {
            BT_esl_tag_send_display_state
            (
                display_index,
                appl_esl_display_image[display_index].image_index
            );
        }
    }

    return retval;
}

API_RESULT appl_esl_tag_service_reset_cmd_cb
           (
               UCHAR  command_source,
               UCHAR  * data,
               UINT16 data_len,
               void   * blob
           )
{
    BT_ESL_IGNORE_UNUSED_PARAM(command_source);

    BT_ESL_BD_ADDR* addr;
    UCHAR opcode, esl_id;
    UINT16 length;
    API_RESULT retval;

    retval = BT_ESL_API_SUCCESS;
    opcode = data[0];
    /* Extract ESL ID from the request */
    esl_id = data[1];

    APPL_ESL_TRC("[APPL]: appl_esl_tag_service_reset_cmd_cb\n");

    addr = (BT_ESL_BD_ADDR*)blob;

    /* Extract Length from the opcode */
    length = BT_ESL_GET_CP_LEN(opcode);

    if (BT_ESL_TRUE == appl_esl_factory_reset_in_progress)
    {
        APPL_ESL_INF("[APPL]: Factory Reset command is active\n");

        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_UNSPECIFIED_ERROR);

        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        if ((data_len != length) || (tag_instance.esl_address.esl_id != esl_id))
        {
            APPL_ESL_ERR(
            "[APPL]: Invalid data length for Service Reset command: %d, expected: %d\n",
            data_len, length);

            /* Send Error response */
            BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_PARAMETERS);

            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            if (NULL != addr)
            {
                APPL_ESL_TRC("[APPL]: Received Service Reset request from : "
                BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER "\n",
                BT_ESL_DEVICE_ADDR_PRINT_STR(addr));
            }

            if (BT_ESL_CHECK_SERVICE_NEEDED_BIT())
            {
                /* Reset the Service Needed bit in basic state */
                BT_ESL_RESET_SERVICE_NEEDED_BIT();

                /* Send Basic state response */
                BT_esl_tag_send_basic_state(BT_esl_tag_get_basic_state());
            }
        }
    }

    return retval;
}

void appl_esl_tag_synchronized_ind_cb
     (
         UINT16 status,
         void   * blob
     )
{
    BT_ESL_BD_ADDR* addr;
    API_RESULT retval;

    BT_ESL_IGNORE_UNUSED_PARAM(status);

    retval = BT_ESL_API_SUCCESS;

    APPL_ESL_TRC("[APPL]: appl_esl_tag_synchronized_ind_cb\n");

    addr = (BT_ESL_BD_ADDR*)blob;

    if (NULL != addr)
    {
        APPL_ESL_TRC("[APPL]: Synchronized with ESL tag with address: "
            BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER "\n",
            BT_ESL_DEVICE_ADDR_PRINT_STR(addr));
    }

    if (BT_ESL_TRUE == BT_ESL_CHECK_SYNCHRONISED_BIT())
    {
        APPL_ESL_TRC("[APPL]: Already synchronized with AP\n");
    }
    else
    {
        /* Set the synchronized bit in basic state */
        BT_ESL_SET_SYNCHRONISED_BIT();

        APPL_ESL_TRC("[APPL]: Synchronized Bit is set in Basis state\n");
        APPL_ESL_TRC("[APPL]: Synchronized with AP\n");
    }

}

API_RESULT appl_esl_tag_unassociate_from_ap_cmd_cb
           (
               UCHAR  * data,
               UINT16 data_len,
               void   * blob
           )
{
    BT_ESL_IGNORE_UNUSED_PARAM(blob);
    BT_ESL_IGNORE_UNUSED_PARAM(data);
    BT_ESL_IGNORE_UNUSED_PARAM(data_len);

    API_RESULT retval;

    retval = BT_ESL_API_SUCCESS;

    APPL_ESL_TRC("[APPL]: appl_esl_tag_unassociate_from_ap_cmd_cb\n");

    APPL_ESL_TRC(
    "[APPL]: Unassociate from AP opcode written\n");

    if (BT_ESL_TRUE == appl_esl_factory_reset_in_progress)
    {
        APPL_ESL_INF("[APPL]: Factory Reset command is active\n");

        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_UNSPECIFIED_ERROR);

        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        /* Send Basic state response */
        BT_esl_tag_send_basic_state(BT_esl_tag_get_basic_state());
    }

    return retval;
}

API_RESULT appl_esl_tag_update_complete_cmd_cb
           (
                UCHAR  * data,
                UINT16 data_len,
                void   * blob
           )
{
    BT_ESL_BD_ADDR* addr;
    UINT16 length;
    UCHAR opcode, esl_id;
    API_RESULT retval;

    retval = BT_ESL_API_SUCCESS;

    APPL_ESL_TRC("[APPL]: appl_esl_tag_update_complete_cmd_cb\n");

    addr = (BT_ESL_BD_ADDR*)blob;
    opcode = data[0];
    /* Extract ESL ID from the request */
    esl_id = data[1];

    /* Extract Length from the opcode */
    length = BT_ESL_GET_CP_LEN(opcode);

    if (NULL != addr)
    {
        APPL_ESL_TRC("[APPL]: Update Complete command received from : "
        BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER "\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(addr));
    }
    if (BT_ESL_TRUE == appl_esl_factory_reset_in_progress)
    {
        APPL_ESL_INF("[APPL]: Factory Reset command is active\n");

        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_UNSPECIFIED_ERROR);

        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        if ((data_len != length) || (tag_instance.esl_address.esl_id != esl_id))
        {
            APPL_ESL_ERR(
            "[APPL]: Invalid data length for Update Complete command: %d, expected: %d\n",
            data_len, length);

            /* Send Error response */
            BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_PARAMETERS);

            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            /* No response will be sent for Update Complete command */
        }
    }

    return retval;
}


API_RESULT appl_esl_tag_unknown_cmd_cb
           (
               UCHAR  * data,
               UINT16   data_len,
               void   * blob
           )
{
    BT_ESL_IGNORE_UNUSED_PARAM(blob);
    BT_ESL_IGNORE_UNUSED_PARAM(data);
    BT_ESL_IGNORE_UNUSED_PARAM(data_len);

    APPL_ESL_TRC("[APPL]: appl_esl_tag_unknown_cmd_cb\n");

    /* Send Error response */
    BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_OPCODE);

    return BT_ESL_API_SUCCESS;
}

#ifdef BT_ESL_SUPPORT_VENDOR_SPECIFIC_COMMANDS

API_RESULT appl_esl_tag_vendor_specific_command_cb
           (
               UCHAR  * data,
               UINT16 data_len,
               void   * blob
           )
{
    BT_ESL_BD_ADDR * addr;
    UCHAR          * response_data;
    API_RESULT     retval;
    UINT16         length, res_data_length;
    UCHAR          opcode, esl_id, rsp_op;

    BT_ESL_IGNORE_UNUSED_PARAM(response_data);
    BT_ESL_IGNORE_UNUSED_PARAM(res_data_length);
    BT_ESL_IGNORE_UNUSED_PARAM(rsp_op);

    retval = BT_ESL_API_SUCCESS;

    APPL_ESL_TRC("[APPL]: appl_esl_tag_vendor_specific_command_cb\n");

    addr = (BT_ESL_BD_ADDR*)blob;

    opcode = data[0];
    /* Extract ESL ID from the request */
    esl_id = data[1];
    /* Extract Length from the opcode */
    length = BT_ESL_GET_CP_LEN(opcode);

    if (BT_ESL_TRUE == appl_esl_factory_reset_in_progress)
    {
        APPL_ESL_INF("[APPL]: Factory Reset command is active\n");

        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_UNSPECIFIED_ERROR);

        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        if ((data_len != length) || (tag_instance.esl_address.esl_id != esl_id))
        {
            APPL_ESL_ERR(
            "[APPL]: Invalid data length for Vendor command 0x%02X: %d, expected: %d\n",
            opcode, data_len, length);

            /* Send Error response */
            BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_PARAMETERS);

            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            /**
             * TODO:
             * Fetch the required Response Data and its Length.
             * This would be the place to add any Vendor Specific Handling!
             * Place holder here for setting response_data and res_data_length.
             * Currently since no Vendor Specific commands are defined,
             * we are sending Invalid Opcode Error response from this location.
             *
             * But Typically code snippet for this section would be like:
             *
             * response_data = appl_esl_get_vendor_specific_response_data(opcode, &res_data_length);
             * rsp_op = (UCHAR)BT_ESL_SET_VENDOR_SPECIFIC_RSP_OP(res_data_length);
             * retval = BT_esl_tag_send_vendor_specific_response(rsp_op, response_data, res_data_length);
             */

            APPL_ESL_ERR(
            "[APPL]: Invalid Vendor Opcode 0x%02X received, sending error!\n",
            opcode);

            /* Send Error response */
            BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_OPCODE);

            retval = BT_ESL_API_FAILURE;
        }
    }

    return retval;
}
#endif /* BT_ESL_SUPPORT_VENDOR_SPECIFIC_COMMANDS */

void appl_esl_tag_write_abs_time_ind_cb
     (
         UINT32   current_abs_time,
         UINT16   status,
         void   * blob
     )
{
    BT_ESL_IGNORE_UNUSED_PARAM(blob);
    BT_ESL_IGNORE_UNUSED_PARAM(current_abs_time);

    APPL_ESL_TRC("[APPL]: appl_esl_tag_write_abs_time_ind_cb\n");

    APPL_ESL_TRC(
    "[APPL]: Current Absolute Time is written with status 0x%04X\n", status);

}

void appl_esl_tag_write_esl_rsp_key_ind_cb
     (
         UINT16  status,
         void  * blob
     )
{
    BT_ESL_IGNORE_UNUSED_PARAM(blob);

    APPL_ESL_TRC("[APPL]: appl_els_write_ap_sync_key_ind_cb\n");

    APPL_ESL_INF(
    "[APPL]: AP Sync Key Material is written with retval 0x%04X\n", status);

}

void appl_esl_sync_terminated_ind_cb(UINT16 status)
{
    BT_ESL_IGNORE_UNUSED_PARAM(status);

    APPL_ESL_TRC("[APPL]: appl_esl_sync_terminated_ind_cb\n");

    if (BT_ESL_TRUE == BT_ESL_CHECK_SYNCHRONISED_BIT())
    {
        /* Reset the synchronized bit in basic state */
        BT_ESL_RESET_SET_SYNCHRONISED_BIT();
    }
}

void appl_esl_unassociated_ind_cb(UINT16 status)
{
    APPL_ESL_TRC("[APPL]: appl_esl_unassociated_ind_cb\n");

    if (BT_ESL_TAG_TIMEOUT == status)
    {
        APPL_ESL_TRC("[APPL]: Unassociated after timeout\n");
    }

    if (BT_ESL_TRUE == appl_esl_factory_reset_in_progress)
    {
        /* Reset the factory reset variable */
        appl_esl_factory_reset_in_progress = BT_ESL_FALSE;

        /**
         * NOTE:
         * In Unassociate Indication, typically the Tag
         * should delete all the keys and bonding related information
         * of the peer and be ready for fresh connections.
         * In this callback is received when
         * "appl_esl_factory_reset_in_progress" is set to TRUE
         * Any other implementation specific handling for complete
         * Factory Reset needs to be invoked here ( eg: Clearning
         * the LED Drivers, Clearning the Stored Images, Reseting
         * the Sensors etc.
         *
         * For the CLI/SHELL based implementation, the
         * BT_esl_tag_reset() can be invoked through the
         * "esl_tag->reset" command.
         */
    }

    APPL_ESL_TRC("[APPL]: Unassociated from AP\n");

    if (BT_ESL_TRUE == BT_ESL_CHECK_SYNCHRONISED_BIT())
    {
        /* Reset the synchronized bit in basic state */
        BT_ESL_RESET_SET_SYNCHRONISED_BIT();
    }
}

void appl_esl_unsynchronized_ind_cb(UINT16 status)
{
    BT_ESL_IGNORE_UNUSED_PARAM(status);

    APPL_ESL_TRC("[APPL]: appl_esl_unsynchronized_ind_cb\n");

    if (BT_ESL_TRUE == BT_ESL_CHECK_SYNCHRONISED_BIT())
    {
        /* Reset the synchronized bit in basic state */
        BT_ESL_RESET_SET_SYNCHRONISED_BIT();
    }
}

void appl_esl_config_image_on_display(UCHAR flag)
{
    UINT32 i;
    UCHAR max_no_of_display_supported;

    max_no_of_display_supported = appl_esl_tag_get_number_of_io(BT_ESL_DISPLAY_INFO_TYPE);

    for (i = 0; i < max_no_of_display_supported; i++)
    {
        if (flag == 0)
        {
            /* This indicates no image is being displayed on the display */
            appl_esl_display_info[i].image_displayed = BT_ESL_FALSE;
        }
        else if (flag == 1)
        {
            /* This indicates image is being displayed on the display */
            appl_esl_display_info[i].image_displayed = BT_ESL_TRUE;
        }
    }
}

void appl_esl_set_service_needed_flag(void)
{

    if (BT_ESL_FALSE == BT_ESL_CHECK_SERVICE_NEEDED_BIT())
    {
        /* Indicates Service Needed bit is not set */
        APPL_ESL_TRC("[APPL]: Service Needed Bit in basic state is not set(FALSE)\n");

        /* Set the Service Needed bit in basic state */
        BT_ESL_SET_SERVICE_NEEDED_BIT();

    }
}

API_RESULT appl_esl_display_image_handler(UCHAR * data, UINT16 datalen)
{
    API_RESULT retval;
    UCHAR display_index, image_index;
    UCHAR max_no_of_display_supported;

    BT_ESL_IGNORE_UNUSED_PARAM(datalen);

    retval = BT_ESL_API_SUCCESS;

    /* Extract Display index and image index */
    display_index = data[0];
    image_index = data[1];

    max_no_of_display_supported = appl_esl_tag_get_number_of_io(BT_ESL_DISPLAY_INFO_TYPE);

    if (display_index >= max_no_of_display_supported)
    {
        APPL_ESL_ERR("[APPL]: Invalid Display Index\n");
        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_UNSPECIFIED_ERROR);
    }
    else if (image_index >= APPL_ESL_IMAGE_MAX)
    {
        APPL_ESL_ERR("[APPL]: Invalid Image Index\n");
        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_IMAGE_INDEX);
    }
    else
    {
        /* Copy the image index for the display */
        appl_esl_display_image[display_index].image_index = image_index;

        APPL_ESL_INF(
        "[APPL]: Displaying image index 0x%02X on Display index 0x%02X\n", image_index, display_index);

        /* Send the Display State response */
        BT_esl_tag_send_display_state(display_index, image_index);

        /* Setting the image in the display */
        appl_esl_display_info[display_index].image_displayed = BT_ESL_TRUE;

        retval = esl_io_display_control
                 (
                     display_index,
                     image_index,
                     BT_ESL_TRUE,
                     NULL /* image location */
                 );
    }

    return retval;
}

API_RESULT appl_esl_display_timed_image_handler(UCHAR* data, UINT16 datalen)
{
    API_RESULT retval;
    UCHAR display_index, image_index;
    UINT32 absolute_time;
    UINT32 time_offset;
    UCHAR max_no_of_displays_supported;

    BT_ESL_IGNORE_UNUSED_PARAM(datalen);

    retval = BT_ESL_API_SUCCESS;

    /* Extract Display index and image index */
    display_index = data[0];
    image_index = data[1];
    /* Extract Absolute Time received from peer */
    BT_ESL_UNPACK_LE_4_BYTE(&absolute_time, &data[2]);

    max_no_of_displays_supported = appl_esl_tag_get_number_of_io(BT_ESL_DISPLAY_INFO_TYPE);

    if (display_index >= max_no_of_displays_supported)
    {
        APPL_ESL_ERR("[APPL]: Invalid Display Index\n");
        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_UNSPECIFIED_ERROR);
    }
    else if (image_index >= APPL_ESL_IMAGE_MAX)
    {
        APPL_ESL_ERR("[APPL]: Invalid Image Index\n");
        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_IMAGE_INDEX);
    }
    else if (absolute_time > APPL_ESL_MAX_ABSOLUTE_TIME)
    {
        APPL_ESL_ERR("[APPL]: Implausible Absolute Time\n");
        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_IMPLAUSIBLE_ABSOLUTE_TIME);
    }
    else
    {
        if (APPL_ESL_ABSOLUTE_TIME_INIT_VAL == appl_esl_display_image[display_index].start_absolute_time)
        {
            if (APPL_ESL_ZERO_ABSOLUTE_TIME != absolute_time)
            {
                /* 1st time received the request */
                appl_esl_display_image[display_index].start_absolute_time = absolute_time;

                /* Copy the latest image index for the display */
                appl_esl_display_image[display_index].display_index = display_index;
                appl_esl_display_image[display_index].image_index = image_index;
            }
            else
            {
                /* 1st time received the request with zero absolute time */
                APPL_ESL_INF(
                "[APPL]: Received Display Timed Control command with Absolute time as zero\n");

                /* Send the Display State response */
                BT_esl_tag_send_display_state(display_index, image_index);

                return BT_ESL_TAG_SUCCESS;
            }
        }
        else
        {
            /**
             * Indicates that previously received the same CP with some absolute time
             * and timer would have already stared
             *
             * Handling multiple Display Timed Image commands
             * a Display Timed Image command is received while a
             * Display Timed Image command is already pending
             *
             * 1. If the value of the Absolute_Time parameter in the
             *    new command is equal to the value of the
             *    Absolute_Time parameter in the pending Display Timed Image command,
             *    then the newly received command shall replace the old pending command.
             * 2. Otherwise, the ESL shall send the Error response: Queue Full,
             * 3. If the value of the Absolute Time parameter is zero (0x00000000),
             *    then the pending Display Timed Image command shall be deleted.
             */

            /* If previous and current requests absolute time are same: COndition 1 */
            if (appl_esl_display_image[display_index].start_absolute_time == absolute_time)
            {
                /* Copy the latest image index for the display */
                appl_esl_display_image[display_index].image_index = image_index;
            }
            /**
             * If absolute time is 0 then send error response: Condition 2
             * NOTE: Previous and current absolute time are not same sending the error
             */
            else if (APPL_ESL_ZERO_ABSOLUTE_TIME != absolute_time)
            {
                /* Send Error response */
                BT_esl_tag_send_esl_error_response(BT_ESL_QUEUE_FULL);

                return BT_ESL_TAG_SUCCESS;
            }
        }

        /* Hits here if no error response are notified */
        /* Send the Display State response */
        BT_esl_tag_send_display_state(display_index, image_index);

        /* Handle absolute time */
        if (APPL_ESL_ZERO_ABSOLUTE_TIME != absolute_time)
        {
            /**
             * If already started the timer then don't start again
             * Indicates
             * 1. 1st time received the command start the timer
             * 2. previous and current received Display Timed Image command
             * with same same absolute time
             * 3. previously received Display Timed Image command absolute time
             * is still not expired
             */
            if ((BT_ESL_FALSE == appl_esl_display_image[display_index].proc_pending) &&
                (appl_esl_display_image[display_index].image_control_timer_handle == BT_ESL_TIMER_HANDLE_INIT_VAL))
            {
                /* Set proc_pending for display as true */
                appl_esl_display_image[display_index].proc_pending = BT_ESL_TRUE;

                /* update basic state after setting display structure */
                appl_esl_update_basic_state();

                /* Calculate time offset */
                time_offset = appl_esl_get_timeout_value(absolute_time, BT_esl_get_current_time_pl());

                APPL_ESL_INF(
                "[APPL]: Absolute time: 0x%08X, Current time: 0x%08X, Time offset: 0x%08X\n",
                absolute_time, BT_esl_get_current_time_pl(), time_offset);

                /* Start timer for timed control command */
                retval = BT_ESL_start_timer
                         (
                             &appl_esl_display_image[display_index].image_control_timer_handle,
                             (time_offset | BT_ESL_TIMEOUT_MILLISEC), /* in milliseconds */
                             appl_esl_display_timed_control_timer_handler,
                             &display_index,
                             sizeof(display_index)
                         );
            }
            else
            {
                APPL_ESL_INF(
                "[APPL]: Display Index %d in Pending state\n", display_index);
            }
        }
        else
        {
            /* Pending Display Timed Image command shall be deleted: Condition 3 */
            if (BT_ESL_TRUE == appl_esl_display_image[display_index].proc_pending)
            {
                APPL_ESL_INF(
                "[APPL]:Absolute Time set to Zero. Pending LED command will be deleted for \
                display index %d\n", display_index);

                /* Stop the timer */
                appl_esl_stop_timer(appl_esl_display_image[display_index].image_control_timer_handle);

                /* Reset display image structure */
                APPL_ESL_INIT_DISPLAY_IMAGE_PARAMS(&appl_esl_display_image[display_index]);
                APPL_ESL_INIT_DISPLAY_DATA_STRUCT(&appl_esl_display_info[display_index]);

                /* update basic state after resetting display structure */
                appl_esl_update_basic_state();
            }
        }
    }

    return BT_ESL_API_SUCCESS;
}

void appl_esl_display_timed_control_timer_handler(void* t_data, UINT16 t_datalen)
{
    UCHAR display_index;
    UCHAR image_index;

    BT_ESL_IGNORE_UNUSED_PARAM(t_datalen);

    display_index = (*((UCHAR*)t_data));

    appl_esl_display_image[display_index].image_control_timer_handle = BT_ESL_TIMER_HANDLE_INIT_VAL;

    /* Copying only to display the index */
    image_index = appl_esl_display_image[display_index].image_index;

    /* Set the display image to true */
    appl_esl_display_info[display_index].image_displayed = BT_ESL_TRUE;

    APPL_ESL_INF(
    "[APPL]: Timer expired for Display timed control cmd, Display index 0x%02X\n", display_index);

    APPL_ESL_INF(
    "[APPL]: Displaying image index 0x%02X on Display index 0x%02X\n", image_index, display_index);

    esl_io_display_control
    (
        display_index,
        image_index,
        BT_ESL_TRUE /* Display the image */,
        NULL /* Image location */
    );

    /* After timer expiry reset display image data */
    APPL_ESL_INF(
    "[APPL]: Clearing Display image control params for Index %d\n", display_index);

    APPL_ESL_INIT_DISPLAY_IMAGE_PARAMS(&appl_esl_display_image[display_index]);
    APPL_ESL_INIT_DISPLAY_DATA_STRUCT(&appl_esl_display_info[display_index]);

    /* update basic state after resetting display structure */
    appl_esl_update_basic_state();
}

UINT32 appl_esl_get_timeout_value
       (
           UINT32 recd_abs_time,
           UINT32 current_abs_time
       )
{
    UINT32 time_offset;
    time_offset = 0x00000000;

    if (recd_abs_time > current_abs_time)
    {
        time_offset = (recd_abs_time - current_abs_time);
    }
    /**
     * If received absolute time parameter value is lower than the current absolute time,
     * then the command shall be executed only after the absolute time has wrapped around
     * and then reached the value specified in the received absolute time parameter.
     */
    else if (recd_abs_time < current_abs_time)
    {
        time_offset = ((0xFFFFFFFF - current_abs_time) + recd_abs_time);
    }

    return time_offset;
}

void appl_esl_update_basic_state(void)
{
    /* To Set/Reset Pending Display Update bit in Basic State */
    appl_esl_update_pending_display();

    /* To Set/Reset Pending LED Update bit in Basic State */
    appl_esl_update_pending_led();

    /* To Set/Reset Active LED bit in Basic State */
    appl_esl_update_active_led();
}

void appl_esl_update_pending_display(void)
{
    UINT32 i;
    UCHAR display_flag;
    UCHAR max_no_of_displays_supported;

    /* Init */
    display_flag = BT_ESL_FALSE;
    max_no_of_displays_supported = appl_esl_tag_get_number_of_io(BT_ESL_DISPLAY_INFO_TYPE);

    /* Check for pending display update in all displays */
    for (i = 0; i < max_no_of_displays_supported; i++)
    {
        if (BT_ESL_TRUE == appl_esl_display_image[i].proc_pending)
        {
            display_flag = BT_ESL_TRUE;
            if(BT_ESL_FALSE == BT_ESL_CHECK_PENDING_DISPLAY_UPDATE_BIT())
            {
                /**
                 * Set pending display update bit in basic state if not set
                 * Pending display bit will be set only once
                 */
                BT_ESL_SET_PENDING_DISPLAY_UPDATE_BIT();
            }

            APPL_ESL_TRC(
            "[APPL]: Pending Display Update in basic state is set(TRUE)\n");
            break; /* break for loop */
        }
    }
    /**
     * Reset Pending Display Update bit in Basic State,
     * If none of the Display Image requests from AP are active
     * (Reset of Pending Display will be done only if Timeout
     *  for which are display index AP requested Display Image command is done)
     * and If Pending Display Update bit in basic state is set
     */

    if ((BT_ESL_FALSE == display_flag) &&
        (BT_ESL_TRUE == BT_ESL_CHECK_PENDING_DISPLAY_UPDATE_BIT()))
    {
        BT_ESL_RESET_PENDING_DISPLAY_UPDATE_BIT();

        APPL_ESL_TRC(
        "[APPL]: Pending Display Update bit in basic state is reset(False)\n");
    }
}

void appl_esl_update_pending_led(void)
{
    UINT32 i;
    UCHAR led_flag;
    UCHAR max_no_of_leds_supported;

    /* Init */
    led_flag = BT_ESL_FALSE;
    max_no_of_leds_supported = appl_esl_tag_get_number_of_io(BT_ESL_LED_INFO_TYPE);

    /* Check for pending led update in all LEDs */
    for (i = 0; i < max_no_of_leds_supported; i++)
    {
        if (BT_ESL_TRUE == appl_esl_led_control[i].proc_pending)
        {
            led_flag = BT_ESL_TRUE;
            if(BT_ESL_FALSE == BT_ESL_CHECK_PENDING_LED_UPDATE_BIT())
            {
                /**
                 * Set pending led update bit in basic state if not set
                 * Pending LED update bit will be set only once
                 */
                BT_ESL_SET_PENDING_LED_UPDATE_BIT();
            }

            APPL_ESL_TRC("[APPL]: Pending LED Update bit in basic state is set(TRUE)\n");
            break; /* break for loop */
        }
    }

    /**
     * Reset Pending LED Update bit in Basic State,
     * If none of the LED Timed Control requests from AP are active
     * (Reset of Pending LED Update bit  will be done only if Timeout
     *  for which are the LED index AP requested LED Control command is done)
     * and If Pending LED Update bit in basic state is set
     */
    if (
            (BT_ESL_FALSE == led_flag) &&
            (BT_ESL_TRUE == BT_ESL_CHECK_PENDING_LED_UPDATE_BIT())
       )
    {
        BT_ESL_RESET_PENDING_LED_UPDATE_BIT();

        APPL_ESL_TRC(
        "[APPL]: Pending LED Update bit in basic state is reset(False)\n");
    }
}

void appl_esl_update_active_led(void)
{
    UCHAR active_led_flag;
    UINT32 i;
    UCHAR max_no_of_leds_supported;

    /* Init */
    active_led_flag = BT_ESL_FALSE;
    max_no_of_leds_supported = appl_esl_tag_get_number_of_io(BT_ESL_LED_INFO_TYPE);

    /* Check for active led in all LEDs */
    for (i = 0; i < max_no_of_leds_supported; i++)
    {
        if (BT_ESL_TRUE == appl_esl_led_control[i].active_led)
        {
            active_led_flag = BT_ESL_TRUE;

            if(BT_ESL_FALSE == BT_ESL_CHECK_ACTIVE_LED_BIT())
            {
                /* Set active led bit in basic state if not set */
                BT_ESL_SET_ACTIVE_LED_BIT();
            }

            APPL_ESL_INF("[APPL]: Active LED in basic state is Set(TRUE)\n");
            break; /* break for loop */
        }
    }

    /**
     * Reset Active LED bit in Basic State,
     * If none of the LED's Control requests from AP are active and
     * If Active LED bit in basic state is set
     */
    if (
            (BT_ESL_FALSE == active_led_flag) &&
            (BT_ESL_TRUE == BT_ESL_CHECK_ACTIVE_LED_BIT())
        )
    {
        BT_ESL_RESET_ACTIVE_LED_BIT();

        APPL_ESL_TRC("[APPL]: Active LED in basic state is reset(False)\n");
    }
}

API_RESULT appl_esl_led_control_req_handler(UCHAR * data, UINT16 datalen)
{
    UCHAR led_index;
    UCHAR max_no_of_leds_supported;

    BT_ESL_IGNORE_UNUSED_PARAM(datalen);

    led_index = data[2];
    max_no_of_leds_supported = appl_esl_tag_get_number_of_io(BT_ESL_LED_INFO_TYPE);

    if (led_index >= max_no_of_leds_supported)
    {
        APPL_ESL_TRC("[APPL]: Invalid LED Index\n");
        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_PARAMETERS);
    }
    else
    {
        /* Set the Active Led in led structure Indicates LED is Active */
        appl_esl_led_control[led_index].active_led = BT_ESL_TRUE;

        /* update basic state to set Active LED to ON */
        appl_esl_update_basic_state();

        appl_esl_update_led_control_params(led_index, data);

        /* Send the LED State response */
        BT_esl_tag_send_led_state(led_index);

        /* Handling of led control command */
        appl_esl_led_control_handler(led_index);
    }

    return BT_ESL_API_SUCCESS;
}

API_RESULT appl_esl_led_timed_control_req_handler(UCHAR* data, UINT16 datalen)
{
    API_RESULT retval;
    UCHAR led_index;
    UINT32 absolute_time;
    UINT32 time_offset;
    UCHAR max_no_of_led_supported;

    BT_ESL_IGNORE_UNUSED_PARAM(datalen);

    retval = BT_ESL_API_SUCCESS;

    led_index = data[2];
    /* Extract Absolute Time received from peer */
    BT_ESL_UNPACK_LE_4_BYTE(&absolute_time, &data[13]);

    APPL_ESL_INF(
    "[APPL]: Received LED Timed Control request for LED Index is %d\n", led_index);

    max_no_of_led_supported = appl_esl_tag_get_number_of_io(BT_ESL_LED_INFO_TYPE);

    if (led_index >= max_no_of_led_supported)
    {
        APPL_ESL_TRC("[APPL]: Invalid LED Index\n");
        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_INVALID_PARAMETERS);
    }
    else if (absolute_time > APPL_ESL_MAX_ABSOLUTE_TIME)
    {
        APPL_ESL_TRC("[APPL]: Implausible Absolute Time\n");
        /* Send Error response */
        BT_esl_tag_send_esl_error_response(BT_ESL_IMPLAUSIBLE_ABSOLUTE_TIME);
    }
    else
    {
        if (APPL_ESL_ABSOLUTE_TIME_INIT_VAL == appl_esl_led_control[led_index].start_absolute_time)
        {
            if (APPL_ESL_ZERO_ABSOLUTE_TIME != absolute_time)
            {
                /* 1st time received the request */
                appl_esl_update_led_control_params(led_index, data);
            }
            else
            {
                APPL_ESL_TRC(
                "[APPL]: Received LED Timed Control command with Absolute time as zero\n");

                /* Send the LED State response */
                BT_esl_tag_send_led_state(led_index);

                return BT_ESL_API_SUCCESS;
            }
        }
        else
        {
            /* Indicates that previously received the same CP for same LED Index */
            if (appl_esl_led_control[led_index].start_absolute_time == absolute_time)
            {
                APPL_ESL_INF(
                "[APPL]:Absolute Time as previous request for LED index %d\n", led_index);

                /* copy latest parameter */
                appl_esl_update_led_control_params(led_index, data);
            }
            else if (absolute_time > appl_esl_led_control[led_index].start_absolute_time)
            {
                APPL_ESL_INF("[APPL]: Sending Queue Full error response\n");
                BT_esl_tag_send_esl_error_response(BT_ESL_QUEUE_FULL);
                return BT_ESL_API_SUCCESS;
            }
        }

        /* Send the LED State response */
        BT_esl_tag_send_led_state(led_index);

        /* Handle absolute time */
        /* Absolute time in non zero values */
        if (APPL_ESL_ZERO_ABSOLUTE_TIME != absolute_time)
        {
            /* if already started the timer for the same LED index then dont start again */
            if ((appl_esl_led_control[led_index].proc_pending == BT_ESL_FALSE) &&
                (BT_ESL_TIMER_HANDLE_INIT_VAL == appl_esl_led_control[led_index].led_control_timer_handle))
            {
                /* Set proc_pending for led as true */
                appl_esl_led_control[led_index].proc_pending = BT_ESL_TRUE;

                /* update basic state after setting led structure */
                appl_esl_update_basic_state();

                /* Calculate time offset */
                time_offset = appl_esl_get_timeout_value(absolute_time, BT_esl_get_current_time_pl());

                APPL_ESL_INF(
                "[APPL]: LED Absolute time: 0x%08X, Current time: 0x%08X, Time offset: 0x%08X\n",
                absolute_time, BT_esl_get_current_time_pl(), time_offset);

                /* Start timer for timed control command */
                retval = BT_ESL_start_timer
                         (
                             &appl_esl_led_control[led_index].led_control_timer_handle,
                             (time_offset | EM_TIMEOUT_MILLISEC), /* in milliseconds */
                             appl_esl_led_timed_control_timer_handler,
                             &led_index,
                             sizeof(led_index)
                         );
                if (BT_ESL_API_SUCCESS != retval)
                {
                    APPL_ESL_TRC("[APPL]: BT_ESL_start_timer retval 0x%04X", retval);
                }
            }
            else
            {
                APPL_ESL_TRC(
               "[APPL]: LED Index %d in Pending state\n", led_index);
            }
        }
        else
        {
            /* Pending LED Timed Control command shall be deleted.*/
            if (appl_esl_led_control[led_index].proc_pending == BT_ESL_TRUE)
            {
                APPL_ESL_INF(
                "[APPL]: Absolute Time set to Zero. Pending LED command will be deleted for \
                led index %d\n", led_index);

                /* Stop the timer */
                appl_esl_stop_timer(appl_esl_led_control[led_index].led_control_timer_handle);

                /* Reset LED Control structure */
                appl_esl_reset_led_control(led_index);
            }
        }
    }

    return retval;
}

void appl_esl_reset_led_control(UCHAR led_index)
{
    APPL_ESL_TRC(
    "[APPL]: Clearing led control params for LED Index %d\n", led_index);

    APPL_ESL_INIT_LED_CONTROL_PARAMS(&appl_esl_led_control[led_index]);

    /* update basic state to update Active LED state to off */
    appl_esl_update_basic_state();
}

/* Hits here after the expiry of led Absolute Time */
void appl_esl_led_timed_control_timer_handler(void* t_data, UINT16 t_datalen)
{
    UCHAR led_index;

    BT_ESL_IGNORE_UNUSED_PARAM(t_datalen);

    led_index = (*((UCHAR*)t_data));

    /* After expiry of timed control reinitialize timer, start_absolute_time and proc_pending */
    appl_esl_led_control[led_index].led_control_timer_handle = BT_ESL_TIMER_HANDLE_INIT_VAL;
    appl_esl_led_control[led_index].start_absolute_time = APPL_ESL_ABSOLUTE_TIME_INIT_VAL;
    appl_esl_led_control[led_index].proc_pending = BT_ESL_FALSE;

    APPL_ESL_TRC("[APPL]: Timer expired for LED timed control cmd, LED index 0x%02X\n", led_index);

    /* Set the Active Led bit to true. */
    appl_esl_led_control[led_index].active_led = BT_ESL_TRUE;

    /* update basic state to set Active LED bit  */
    appl_esl_update_basic_state(); /* why this is required?? */

    /* After the expiry of absolute time, handle the LED request */
    appl_esl_led_control_handler(led_index);
}


void appl_esl_led_control_handler(UCHAR led_index)
{
    UCHAR bit_on_period, bit_off_period;
    UINT16 repeats_duration;
    UCHAR repeat_type;
    UINT32 time_period;
    API_RESULT retval;
    UCHAR brightness;
    ESL_IO_LED_PARAMS esl_io_led_params;

    retval = BT_ESL_API_SUCCESS;
    /* Init */
    repeats_duration = appl_esl_led_control[led_index].repeat_duration;
    repeat_type = appl_esl_led_control[led_index].repeat_type;

    /* Extract brightness */
    brightness = APPL_ESL_GET_BRIGHTNESS(appl_esl_led_control[led_index].color);

    bit_on_period = appl_esl_led_control[led_index].flash_pattren[6];
    bit_off_period = appl_esl_led_control[led_index].flash_pattren[5];

    if (BT_ESL_MONOCHROME == (tag_instance.led_info.esl_led[led_index].led_type & BT_ESL_MONOCHROME))
    {
        /* For Monochrome LED, color is constant */
        APPL_ESL_INF(
        "[APPL]: Ignoring only color fields for monochrome LED type\n");
    }
    else
    {
        /**
         * The value for Red colour is in bit 0 and 1   (0x01, 0x02, 0x03)
         * The value for Green colour is in bit 2 and 3 (0x04, 0x08, 0x0C)
         * The value for Blue colour is in bit 4 and 5  (0x10, 0x20, 0x30)
         * Brightness is in bit 6 and 7
         */
        APPL_ESL_INF("\tReceived %s colour\n",
        (0x00U == (appl_esl_led_control[led_index].color & BT_ESL_SRGB_RED_LEVEL)) ? "Red" :
        (0x00U == (appl_esl_led_control[led_index].color & BT_ESL_SRGB_GREEN_LEVEL)) ? "Green" :
        (0x00U == (appl_esl_led_control[led_index].color & BT_ESL_SRGB_BLUE_LEVEL)) ? "Blue" : "Unknown");
    }

    APPL_ESL_INF(
    "\tBrightness percentage: %s\n",
    (brightness == 0x00) ? "25" :
    (brightness == 0x01) ? "50" :
    (brightness == 0x02) ? "75" :
    (brightness == 0x03) ? "100" : "Unknown");
    APPL_ESL_INF("\tColour: 0x%02X\n", appl_esl_led_control[led_index].color);
    APPL_ESL_TRC("\tFlashing Pattern: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
    appl_esl_led_control[led_index].flash_pattren[0],
    appl_esl_led_control[led_index].flash_pattren[1],
    appl_esl_led_control[led_index].flash_pattren[2],
    appl_esl_led_control[led_index].flash_pattren[3],
    appl_esl_led_control[led_index].flash_pattren[4]);
    APPL_ESL_INF("\tBit_Off_Period: 0x%02X\n", bit_off_period);
    APPL_ESL_INF("\tBit_On_Period: 0x%02X\n", bit_on_period);
    APPL_ESL_INF("\tRepeat Type: %d\n", repeat_type);
    APPL_ESL_INF("\tRepeat Duration 0x%04X\n", repeats_duration);

    /* Stop the repeat duration timer if previously started */
    appl_esl_stop_timer(appl_esl_led_control[led_index].led_repeat_duration_timer_handle);

    /* Repeat duration is not zero, Repeat Type is 0 = Number of times */
    if ((repeats_duration != 0) && (repeat_type == 0))
    {
        /* Same Flashing pattern shall repeat no of times given in repeat duration */
        APPL_ESL_INF(
        "[APPL]: Repeating flashing pattern %d no of times\n", repeats_duration);

        /**
         * time_period = Total time required for Flashing the Pattern
         * as per bit on and bit off period
         */
        time_period = appl_esl_parse_flashing_pattern
                      (
                          led_index,
                          brightness,
                          &appl_esl_led_control[led_index].flash_pattren[0],
                          bit_off_period,
                          bit_on_period
                      );

        /* repeats_duration = Number of times same Flashing_pattren should repeat */
        /**
         * Example: Flashing pattern is 1110000
         * 3 times LED ON, 4 times LED OFF.
         * Each time LED ON for 2 seconds
         * Each time LED OFF for 1 seconds
         * LED ON ((3times * 2) * 2ms = 12 seconds)
         * LED OFF((4times * 1) * 2ms  = 4 seconds)
         * So total it takes 20 seconds for 1 flashing pattern to complete
         *
         * if repeat duration is 5 times, then
         * 5times * 20 seconds = 100 seconds
         * total 100 seconds LED will be ON and OFF as per pattern
         * After 100 seconds reset Active LED bit in Basic state
         */
        time_period = time_period * repeats_duration;

        APPL_ESL_INF(
        "[APPL]: Turning on LED for 0x%08X milliseconds\n", time_period);

        /* After timer expiry make Active LED off */
        retval = BT_ESL_start_timer
                 (
                     &appl_esl_led_control[led_index].led_repeat_duration_timer_handle,
                     (time_period | EM_TIMEOUT_MILLISEC), /* In Milliseconds */
                     appl_esl_led_control_timer_handler,
                     &led_index,
                     1
                 );
    }

    /* Repeat duration is not zero, Repeat Type is 1 = Time duration in seconds */
    else if ((repeats_duration != 0) && (repeat_type == 1))
    {
        /* Same Flasing pattern shall repeat till the time duration given in repeat duration */
        APPL_ESL_INF(
        "[APPL]: Repeat flashing pattern for %d seconds\n", repeats_duration);

        /* AFter timer expiry make Active LED off */
        retval = BT_ESL_start_timer
                 (
                     &appl_esl_led_control[led_index].led_repeat_duration_timer_handle,
                     repeats_duration,
                     appl_esl_led_control_timer_handler,
                     &led_index,
                     1
                 );
    }

    /* Repeat duration is zero, Repeat Type is 0 = turn off continuously  */
    else if ((repeats_duration == 0) && (repeat_type == 0))
    {
        APPL_ESL_INF("[APPL]: Turning off LED continuously\n");
        /* Set active_led for led as false */
        appl_esl_led_control[led_index].active_led = BT_ESL_FALSE;

        esl_io_led_params.led_idx = led_index;
        esl_io_led_params.brightness = brightness;
        esl_io_led_params.onoff = BT_ESL_FALSE;
        esl_io_led_params.onoff_period = 0; /* 0 means continuous ON */

        esl_io_led_control(&esl_io_led_params);

        /* update basic state to update Active LED bit to off */
        appl_esl_update_basic_state();
    }

    /* Repeat duration is zero, Repeat Type is 0 = Turn on continuously */
    else if ((repeats_duration == 0) && (repeat_type == 1))
    {
        APPL_ESL_INF("[APPL]: Turning on LED continuously\n");

        esl_io_led_params.led_idx = led_index;
        esl_io_led_params.brightness = brightness;
        esl_io_led_params.onoff = BT_ESL_TRUE;
        esl_io_led_params.onoff_period = 0; /* 0 means continuous ON */

        esl_io_led_control(&esl_io_led_params);
        /* update basic state to update Active LED bit to ON */
        appl_esl_update_basic_state();
    }
}

void appl_esl_led_control_timer_handler(void* t_data, UINT16 t_datalen)
{
    UCHAR led_index;

    BT_ESL_IGNORE_UNUSED_PARAM(t_datalen);

    led_index = (*(UCHAR*)t_data);

    /* After the LED control or timed control reset LED structure */
    appl_esl_reset_led_control(led_index);
}

UINT32 appl_esl_parse_flashing_pattern
       (
           UCHAR   led_index,
           UCHAR   brightness,
           UCHAR * flash_pattern,
           UCHAR   bit_off_period,
           UCHAR   bit_on_period
       )
{
    UCHAR pattern[5];
    UINT32 time_period;
    UCHAR bit, foundfirstone = BT_ESL_FALSE;
    int i, j;
    ESL_IO_LED_PARAMS esl_io_led_params;

    time_period = 0;

    BT_ESL_mem_copy(pattern, flash_pattern, 5);

    /* Flashing Pattern will be only 5 bytes, 6th and 7th bytes are bit on and off */
    for (i = 0; i < 5; i++)
    {
        /* Iterate through each bit in the current byte */
        for (j = APPL_ESL_FLASHING_PATTREN_LEN; j >= 0; j--)
        {
            /**
             * ESL shall ignore any leading zeroes in a pattern and treat
             * only the remaining bits as meaningful as per spec.
             */
            bit = (pattern[i] >> j) & 1;
            if (bit == 1)
            {
                /* 1st time found bit set 1 make variable to true */
                foundfirstone = BT_ESL_TRUE;
            }
            if (foundfirstone)
            {
                APPL_ESL_TRC("[APPL]: Flashing pattern bit is %d\n", bit);
                if (1 == bit)
                {
                    /**
                     * Bit is set to 1: Indicates LED ON
                     * Calculate ON period as per number bits are ON
                     *
                     * Example: If only 3 bits are set to 1, that means LED is
                     * ON for 3 times. Each time ON for (bit_on_period * 2) seconds
                     *
                     * Bit_On_Period represents time per bit for which the LED is kept on.
                     * How much time each bit to keep on
                     */
                    time_period += bit_on_period * 2;

                    esl_io_led_params.led_idx = led_index;
                    esl_io_led_params.brightness = brightness;
                    esl_io_led_params.onoff = BT_ESL_TRUE;
                    esl_io_led_params.onoff_period = (bit_on_period * 2);

                    esl_io_led_control(&esl_io_led_params);
                }
                else if (0 == bit)
                {
                    /**
                     * Bit is set to 0: Indicates LED OFF
                     * Bit_Off_Period represents time per bit for which the LED is kept off.
                     */
                    time_period += bit_off_period * 2;

                    esl_io_led_params.led_idx = led_index;
                    esl_io_led_params.brightness = brightness;
                    esl_io_led_params.onoff = BT_ESL_FALSE;
                    esl_io_led_params.onoff_period = (bit_on_period * 2);

                    esl_io_led_control(&esl_io_led_params);
                }
            }
        }
    }

    return time_period;
}


void appl_esl_update_led_control_params(UCHAR led_index, UCHAR* value)
{
    UCHAR opcode;
    opcode = value[0];

    appl_esl_led_control[led_index].led_index = value[2];
    appl_esl_led_control[led_index].color = value[3];
    BT_ESL_mem_copy
    (
        &appl_esl_led_control[led_index].flash_pattren,
        &value[4],
        APPL_ESL_FLASHING_PATTREN_LEN
    );

    BT_ESL_UNPACK_LE_2_BYTE(&appl_esl_led_control[led_index].repeat_duration, &value[11]);

    appl_esl_led_control[led_index].repeat_type = \
        (appl_esl_led_control[led_index].repeat_duration & 0x0001);

    appl_esl_led_control[led_index].repeat_duration = \
        (appl_esl_led_control[led_index].repeat_duration >> 1);

    /* Start absolute time is only for led timed control */
    if (BT_ESL_LED_TIMED_CONTROL == opcode)
    {
        BT_ESL_UNPACK_LE_4_BYTE(&appl_esl_led_control[led_index].start_absolute_time, &value[13]);
    }
}

void appl_esl_stop_timer(BT_ESL_TIMER_HANDLE timer_handle)
{
    if (BT_ESL_TIMER_HANDLE_INIT_VAL != timer_handle)
    {
        BT_ESL_stop_timer(timer_handle);
        timer_handle = BT_ESL_TIMER_HANDLE_INIT_VAL;
    }
}

void appl_esl_reset_tag(void)
{
    /* Init IO */
    esl_io_init();

    BT_esl_tag_reset();

    /** Update basic state */
    BT_ESL_RESET_SERVICE_NEEDED_BIT();
    BT_ESL_RESET_SET_SYNCHRONISED_BIT();
    BT_ESL_RESET_ACTIVE_LED_BIT();
    BT_ESL_RESET_PENDING_LED_UPDATE_BIT();
    BT_ESL_RESET_PENDING_DISPLAY_UPDATE_BIT();

    appl_esl_init_display_image_struct();
    appl_esl_init_led_struct();

    /* Clear the locally maintained flag */
    appl_esl_factory_reset_in_progress = BT_ESL_FALSE;
}

void appl_esl_set_single_io_mode(UCHAR mode)
{
    single_io_support_flag = mode;
}

UCHAR appl_esl_tag_get_number_of_io(UCHAR type)
{
    UCHAR no_of_io;

    /* Init */
    no_of_io = 0U;

    switch(type)
    {
        case BT_ESL_DISPLAY_INFO_TYPE:
            no_of_io = ((BT_ESL_FALSE != single_io_support_flag) ? 1U : esl_io_get_num_displays());
            break;
        case BT_ESL_SENSOR_INFO_TYPE:
            no_of_io = ((BT_ESL_FALSE != single_io_support_flag) ? 1U : esl_io_get_num_sensors());
            break;
        case BT_ESL_LED_INFO_TYPE:
            no_of_io = ((BT_ESL_FALSE != single_io_support_flag) ? 1U : esl_io_get_num_leds());
            break;
        default:
            break;
    }

    return no_of_io;
}

#endif /* BT_ESL_SUPPORT_TAG_ROLE */
