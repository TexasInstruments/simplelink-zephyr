/**
 *  \file appl_esl.h
 *
 *  This Header File contains Application Layer Declarations for Zephyr.
 */

/*
 *  Copyright (C) 2025. LTI Mindtree Ltd.
 *  All rights reserved.
 */

#ifndef _H_APPL_ESL_TAG_
#define _H_APPL_ESL_TAG_

/* ----------------------------------------------- Header File Inclusion */
#include "appl_main.h"
#include "esl_io.h"

#ifdef BT_ESL_SUPPORT_TAG_ROLE
/* --------------------------------------------- Global Definitions */
/**
 * \brief Flag to enable OTS support in ESL Tag application
 */
/* #define APPL_ESL_TAG_OTS_SUPPORT */

/* ----------------------------------------------- Macros */
#define APPL_ESL_IMAGE_MAX                          IO_MAX_IMAGES

/** Delete pending command absolute time */
#define APPL_ESL_ZERO_ABSOLUTE_TIME                 0x00000000U

/** Invalid absolute time */
#define APPL_ESL_ABSOLUTE_TIME_INIT_VAL             0xFFFFFFFFU

/* Macro to construct LED type byte from type and color flags */
#define BT_ESL_PACK_LED_TYPE(type, blue, green, red)                                                            \
        ((type) |                                                                                               \
        ((blue) ? (type == BT_ESL_MONOCHROME ? BT_ESL_MONOCHROME_BLUE_LEVEL : BT_ESL_SRGB_BLUE_LEVEL) : 0)    | \
        ((green) ? (type == BT_ESL_MONOCHROME ? BT_ESL_MONOCHROME_GREEN_LEVEL : BT_ESL_SRGB_GREEN_LEVEL) : 0) | \
        ((red) ? (type == BT_ESL_MONOCHROME ? BT_ESL_MONOCHROME_RED_LEVEL : BT_ESL_SRGB_RED_LEVEL) : 0))

/* 48 days = 48days * 24hours * 3600seconds * 1000milliseconds  */
#define APPL_ESL_MAX_ABSOLUTE_TIME                  0xF7314000
#define APPL_ESL_FLASHING_PATTREN_LEN               7U

#define APPL_ESL_INIT_DISPLAY_IMAGE_PARAMS(x)                               \
        {                                                                   \
            (x)->display_index = 0xFF;                                      \
            (x)->image_index = 0xFF;                                        \
            (x)->start_absolute_time = APPL_ESL_ABSOLUTE_TIME_INIT_VAL;     \
            (x)->proc_pending = BT_ESL_FALSE;                               \
            (x)->image_control_timer_handle = BT_ESL_TIMER_HANDLE_INIT_VAL; \
        }

#define APPL_ESL_INIT_LED_CONTROL_PARAMS(x)                                           \
        {                                                                             \
            (x)->led_index = 0x00;                                                    \
            (x)->color = 0x00;                                                        \
            BT_ESL_mem_set((x)->flash_pattren, 0x00U, APPL_ESL_FLASHING_PATTREN_LEN); \
            (x)->repeat_type = 0x00;                                                  \
            (x)->repeat_duration = 0x0000;                                            \
            (x)->start_absolute_time = APPL_ESL_ABSOLUTE_TIME_INIT_VAL;               \
            (x)->proc_pending = BT_ESL_FALSE;                                         \
            (x)->active_led = BT_ESL_FALSE;                                           \
            (x)->led_control_timer_handle = BT_ESL_TIMER_HANDLE_INIT_VAL;             \
            (x)->led_repeat_duration_timer_handle = BT_ESL_TIMER_HANDLE_INIT_VAL;     \
        }

#define APPL_ESL_INIT_DISPLAY_DATA_STRUCT(x)       \
        {                                        \
            (x)->image_displayed = BT_ESL_FALSE; \
        }

#define APPL_ESL_GET_BRIGHTNESS(c) \
        (((c) >> (6)) & (0x03))

/* ----------------------------------------------- Global Definitions */

/* ----------------------------------------------- Structures/Data Types */
typedef struct _APPL_ESL_DISPLAY_IMAGE_PARAMS
{
    UCHAR display_index;

    UCHAR image_index;

    UINT32 start_absolute_time;

    UCHAR proc_pending;

    BT_ESL_TIMER_HANDLE image_control_timer_handle;

}APPL_ESL_DISPLAY_IMAGE_PARAMS;

typedef struct _APPL_ESL_LED_CONTROL_PARAM
{
    UCHAR led_index;

    UCHAR color;

    UCHAR flash_pattren[7];

    UCHAR  repeat_type;

    UINT16 repeat_duration;

    UINT32 start_absolute_time;

    UCHAR  proc_pending;

    UCHAR active_led;

    BT_ESL_TIMER_HANDLE led_control_timer_handle;

    BT_ESL_TIMER_HANDLE led_repeat_duration_timer_handle;

}APPL_ESL_LED_CONTROL_PARAM;

typedef struct _APPL_ESL_DISPLAY_DATA_STRUCT
{
    /* Display the image on the specified display */
    UCHAR image_displayed;

}APPL_ESL_DISPLAY_DATA_STRUCT;

/* ----------------------------------------------- Internal Functions */
void appl_esl_tag_init(void);
API_RESULT appl_esl_tag_start_advertise(void);
API_RESULT appl_esl_tag_stop_advertise(void);
void appl_esl_tag_init_est_instance(void);
void appl_esl_update_pending_display(void);
void appl_esl_update_pending_led(void);
void appl_esl_update_active_led(void);

void appl_esl_tag_connection_ind_cb
     (
         UCHAR status,
         void * blob
     );
void appl_esl_tag_disconnection_ind_cb
     (
         void * blob
     );

void appl_esl_tag_synchronized_ind_cb
     (
         UINT16   status,
         void  * blob
     );
void appl_esl_unsynchronized_ind_cb
     (
         UINT16 status
     );

void appl_esl_unassociated_ind_cb(UINT16 status);

void appl_esl_sync_terminated_ind_cb(UINT16 status);

void appl_els_read_esl_info_ind_cb
     (
         UCHAR   char_id,
         void  * blob
     );
void appl_esl_tag_configured_ind_cb(void * blob);
void appl_els_write_esl_addr_ind_cb
     (
         UINT16   status,
         void  * blob
     );
void appl_els_write_ap_sync_key_ind_cb
     (
         UINT16  status,
         void  * blob
     );
void appl_esl_tag_write_esl_rsp_key_ind_cb
     (
         UINT16  status,
         void  * blob
     );
void appl_esl_tag_write_abs_time_ind_cb
     (
         UINT32   current_abs_time,
         UINT16   status,
         void   * blob
     );
void appl_esl_tag_control_point_configured_ind_cb
     (
         UCHAR    char_id,
         UINT16   cccd_value,
         void   * blob
     );
API_RESULT appl_esl_tag_led_control_cmd_cb
           (
               UCHAR  * data,
               UINT16 datalen,
               void   * blob
           );
API_RESULT appl_esl_tag_display_image_cmd_cb
           (
               UCHAR  * data,
               UINT16 datalen,
               void   * blob
           );
API_RESULT appl_esl_tag_read_sensor_data_cmd_cb
           (
               UCHAR  * data,
               UINT16 data_length,
               void   * blob
           );
API_RESULT appl_esl_tag_factory_reset_cmd_cb
           (
               UCHAR  command_source,
               UCHAR  * data,
               UINT16 data_len,
               void   * blob
           );
API_RESULT appl_esl_tag_unassociate_from_ap_cmd_cb
           (
               UCHAR  * data,
               UINT16 data_len,
               void   * blob
           );
API_RESULT appl_esl_tag_service_reset_cmd_cb
           (
               UCHAR  command_source,
               UCHAR  * data,
               UINT16 data_len,
               void   * blob
           );
API_RESULT appl_esl_tag_refresh_display_cmd_cb
           (
               UCHAR  * data,
               UINT16 data_len,
               void   * blob
           );
API_RESULT appl_esl_tag_ping_cmd_cb
           (
               UCHAR  * data,
               UINT16 data_len,
               void   * blob
           );
API_RESULT appl_esl_tag_unknown_cmd_cb
           (
               UCHAR  * data,
               UINT16 data_len,
               void   * blob
           );
API_RESULT appl_esl_tag_vendor_specific_command_cb
           (
               UCHAR  * data,
               UINT16 data_len,
               void   * blob
           );
void appl_esl_init_display_image_struct(void);
void appl_esl_init_led_struct(void);
void appl_esl_populate_sensor_info(void);
void appl_esl_populate_display_info(void);
void appl_esl_populate_led_info(void);
void appl_esl_set_service_needed_flag(void);
void appl_esl_config_image_on_display(UCHAR flag);
API_RESULT appl_esl_display_image_handler(UCHAR * data, UINT16 datalen);
API_RESULT appl_esl_display_timed_image_handler(UCHAR * data, UINT16 datalen);
void appl_esl_display_timed_control_timer_handler(void * t_data, UINT16 t_datalen);
UINT32 appl_esl_get_timeout_value
       (
           UINT32 recd_abs_time,
           UINT32 current_abs_time
       );
void appl_esl_update_basic_state(void);
void appl_esl_update_pending_display(void);
void appl_esl_update_pending_led(void);
void appl_esl_update_active_led(void);
void appl_esl_update_led_control_params(UCHAR led_index, UCHAR * value);
UINT32 appl_esl_parse_flashing_pattern
       (
           UCHAR   led_index,
           UCHAR   brightness,
           UCHAR * flash_pattern,
           UCHAR   bit_off_period,
           UCHAR   bit_on_period
       );
void appl_esl_led_control_timer_handler(void * t_data, UINT16 t_datalen);
void appl_esl_led_control_handler(UCHAR led_index);
void appl_esl_led_timed_control_timer_handler(void * t_data, UINT16 t_datalen);
void appl_esl_reset_led_control(UCHAR led_index);
API_RESULT appl_esl_led_timed_control_req_handler(UCHAR * data, UINT16 datalen);
API_RESULT appl_esl_led_control_req_handler(UCHAR * data, UINT16 datalen);
void appl_esl_update_active_led(void);
void appl_esl_update_pending_led(void);
void appl_esl_update_pending_display(void);
void appl_esl_update_basic_state(void);
UINT32 appl_esl_get_timeout_value
       (
           UINT32 recd_abs_time,
           UINT32 current_abs_time
       );
void appl_esl_display_timed_control_timer_handler(void * t_data, UINT16 t_datalen);
API_RESULT appl_esl_display_timed_image_handler(UCHAR * data, UINT16 datalen);
UINT32 appl_esl_parse_flashing_pattern
       (
           UCHAR   led_index,
           UCHAR   brightness,
           UCHAR * flash_pattern,
           UCHAR   bit_off_period,
           UCHAR   bit_on_period
       );
void appl_esl_stop_timer(BT_ESL_TIMER_HANDLE timer_handle);
void appl_esl_reset_tag(void);
API_RESULT appl_esl_tag_update_complete_cmd_cb
           (
                UCHAR  * data,
                UINT16 data_len,
                void   * blob
           );

void appl_esl_factory_reset_disconn_timeout_handler(void * args, UINT16 size);

/**
 * \name function to set single IO mode
 *
 * \note to be used before power cycle
 *
 * \param [in] mode - mode to be set (0- multi IO, 1- single IO)
 */
void appl_esl_set_single_io_mode(UCHAR mode);

/**
 * \name  function to fetch number of IO`s
 *
 * \param [in] type - type of IO (sensor/display/led)
 *
 * \return number of IO`s supported
 */
UCHAR appl_esl_tag_get_number_of_io(UCHAR type);

#ifdef APPL_ESL_TAG_OTS_SUPPORT
/** OTS initialization function */
void appl_esl_tag_ots_init(void);
/** OTS add objects */
void appl_esl_tag_ots_add_objects(void);
/** OTS image selected callback */
void appl_esl_tag_ots_image_selected_cb(BT_ESL_BD_ADDR *bd_addr, UCHAR image_index);
/** OTS image written callback */
API_RESULT appl_esl_tag_ots_image_written_cb
           (
                BT_ESL_BD_ADDR *bd_addr,
                UCHAR image_index,
                void * data,
                UINT32 len,
                UINT32 offset,
                UINT32 rem
           );
#endif /* APPL_ESL_TAG_OTS_SUPPORT */
#endif /* BT_ESL_SUPPORT_TAG_ROLE */

#endif /* _H_APPL_ESL_TAG_ */
