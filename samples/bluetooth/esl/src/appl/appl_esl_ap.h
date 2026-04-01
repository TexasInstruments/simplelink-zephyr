/**
 *  \file appl_esl_ap.h
 *
 *  This Header File contains Application Layer Declarations for Zephyr.
 */

/*
 *  Copyright (C) 2025. LTI Mindtree Ltd.
 *  All rights reserved.
 */

#ifndef _H_APPL_ESL_AP_
#define _H_APPL_ESL_AP_

/* ----------------------------------------------- Header File Inclusion */
#include "appl_main.h"

/* ----------------------------------------------- Macros */
/**Macro to enable OTS in appl - now controlled via CMakeLists.txt */
/* #define APPL_ESL_AP_OTS_SUPPORT */

/** Number of groups: Mapped to the ones in Project Configuration or Preprocessor */
#if (CONFIG_BT_ESL_MAX_GROUPS_SUPPORTED >= 1) && (CONFIG_BT_ESL_MAX_GROUPS_SUPPORTED <= 128)
#define APPL_ESL_MAX_NO_OF_GROUPS CONFIG_BT_ESL_MAX_GROUPS_SUPPORTED
#else
#define APPL_ESL_MAX_NO_OF_GROUPS                       BT_ESL_MAX_GROUPS_SUPPORTED
#endif

/** Number of tags per group: Mapped to the ones in Project Configuration */
#if (CONFIG_BT_ESL_MAX_ESL_TAGS_SUPPORTED >= 1) && (CONFIG_BT_ESL_MAX_ESL_TAGS_SUPPORTED <= 255)
#define APPL_ESL_MAX_NO_OF_TAGS_PER_GROUP CONFIG_BT_ESL_MAX_ESL_TAGS_SUPPORTED
#else
#define APPL_ESL_MAX_NO_OF_TAGS_PER_GROUP               BT_ESL_MAX_ESL_TAGS_SUPPORTED
#endif

/* Number of Sensor: Mapped to the ones in Project Configuration*/
#if (CONFIG_BT_ESL_MAX_SENSOR_SUPPORTED >= 1) && (CONFIG_BT_ESL_MAX_SENSOR_SUPPORTED <= 256)
#define APPL_ESL_MAX_SENSOR_SUPPORTED CONFIG_BT_ESL_MAX_SENSOR_SUPPORTED
#else
#define APPL_ESL_MAX_SENSOR_SUPPORTED                                   256U
#endif


#if (CONFIG_BT_ESL_MAX_LED_SUPPORTED >= 1) && (CONFIG_BT_ESL_MAX_LED_SUPPORTED <= 256)
#define APPL_ESL_MAX_LED_SUPPORTED CONFIG_BT_ESL_MAX_LED_SUPPORTED
#else
#define APPL_ESL_MAX_LED_SUPPORTED                                      256U
#endif

#if (CONFIG_BT_ESL_MAX_IMAGE_SUPPORTED >= 1) && (CONFIG_BT_ESL_MAX_IMAGE_SUPPORTED <= 256)
#define APPL_ESL_MAX_IMAGE_SUPPORTED CONFIG_BT_ESL_MAX_IMAGE_SUPPORTED
#else
#define APPL_ESL_MAX_IMAGE_SUPPORTED                                    256U
#endif

#if (CONFIG_BT_ESL_MAX_DISPLAY_SUPPORTED >= 1) && (CONFIG_BT_ESL_MAX_DISPLAY_SUPPORTED <= 102)
#define APPL_ESL_MAX_DISPLAY_SUPPORTED CONFIG_BT_ESL_MAX_DISPLAY_SUPPORTED
#else
#define APPL_ESL_MAX_DISPLAY_SUPPORTED                                  102U
#endif

/** Macro to get sm state string of the tag */
#define APPL_ESL_AP_GET_SM_STATE_STRING(sm_state)                      \
         (sm_state == BT_ESL_AP_UNASSOCIATE) ? "UNASSOCIATE" :         \
         (sm_state == BT_ESL_AP_CONNECTED) ? "CONNECTED" :             \
         (sm_state == BT_ESL_AP_CONFIGURING) ? "CONFIGURING" :         \
         (sm_state == BT_ESL_AP_IN_SYNCRONIZING) ? "IN_SYNCRONIZING" : \
         (sm_state == BT_ESL_AP_SYNCHRONIZED) ? "SYNCHRONIZED" :       \
         (sm_state == BT_ESL_AP_UNSYNCHRONIZED) ? "UNSYNCHRONIZED" :   \
         "UNKNOWN STATE"

/* ----------------------------------------------- Global Definitions */

/* ----------------------------------------------- Structures/Data Types */

/* -------------------------------------------- Function Declarations */

#ifdef BT_ESL_SUPPORT_AP_ROLE
API_RESULT appl_esl_ap_init(void);
API_RESULT appl_esl_ap_start_periodic_adv(void);
API_RESULT appl_esl_ap_stop_periodic_adv(void);

API_RESULT appl_esl_ap_scan_esl_device(UCHAR flag);
API_RESULT appl_esl_ap_add_esl_tag(BT_ESL_ADDR *esl_addr, BT_ESL_BD_ADDR * peer_addr);
API_RESULT appl_esl_ap_remove_esl_tag(BT_ESL_ADDR *esl_addr);
API_RESULT appl_esl_ap_connect_esl(BT_ESL_ADDR *esl_addr);
API_RESULT appl_esl_ap_disconnect_esl(BT_ESL_ADDR *esl_addr);
API_RESULT appl_esl_ap_config(BT_ESL_ADDR *esl_addr);
API_RESULT appl_esl_ap_discover_esl_service(BT_ESL_ADDR *esl_addr);
UINT32 appl_esl_ap_get_current_abs_time(void);
API_RESULT appl_esl_ap_sync_with_esl(BT_ESL_ADDR *esl_addr);
API_RESULT appl_esl_ap_get_display_information(BT_ESL_ADDR *esl_addr);
API_RESULT appl_esl_ap_get_led_information(BT_ESL_ADDR *esl_addr);
API_RESULT appl_esl_ap_get_sensor_information(BT_ESL_ADDR *esl_addr);
API_RESULT appl_esl_ap_get_image_information(BT_ESL_ADDR *esl_addr);
API_RESULT appl_esl_ap_send_ping(BT_ESL_ADDR *esl_addr, UCHAR multiple_commands_flag);
API_RESULT appl_esl_ap_send_unassociate(BT_ESL_ADDR *esl_addr, UCHAR multiple_commands_flag);
API_RESULT appl_esl_ap_send_service_reset(BT_ESL_ADDR *esl_addr, UCHAR multiple_commands_flag);
API_RESULT appl_esl_ap_send_factory_reset(BT_ESL_ADDR *esl_addr);
API_RESULT appl_esl_ap_send_read_sensor_data(BT_ESL_ADDR *esl_addr, UCHAR sensor_index, UCHAR multiple_commands_flag);
API_RESULT appl_esl_ap_send_refresh_display(BT_ESL_ADDR *esl_addr, UCHAR display_index, UCHAR multiple_commands_flag);
API_RESULT appl_esl_ap_send_display_image(BT_ESL_ADDR *esl_addr, UCHAR display_index, UCHAR image_index, UCHAR multiple_commands_flag);
API_RESULT appl_esl_ap_send_display_timed_image(BT_ESL_ADDR *esl_addr, UCHAR display_index, UCHAR image_index, UINT32 absolute_time, UCHAR multiple_commands_flag);
API_RESULT appl_esl_ap_send_led_control(BT_ESL_ADDR *esl_addr, UCHAR led_index, UCHAR color_brightness, UCHAR *flashing_pattern, UINT16 repeat_type, UCHAR multiple_commands_flag);
API_RESULT appl_esl_ap_send_led_timed_control(BT_ESL_ADDR *esl_addr, UCHAR led_index, UCHAR color_brightness, UCHAR *flashing_pattern, UINT16 repeat_type, UINT32 absolute_time, UCHAR multiple_commands_flag);
#ifdef BT_ESL_SUPPORT_VENDOR_SPECIFIC_COMMANDS
API_RESULT appl_esl_ap_send_vendor_specific_command(BT_ESL_ADDR *esl_addr, UCHAR opcode, UCHAR *data, UINT16 data_len, UCHAR multiple_commands_flag);
#endif /* BT_ESL_SUPPORT_VENDOR_SPECIFIC_COMMANDS */
API_RESULT appl_esl_ap_send_esl_command(void);
API_RESULT appl_esl_ap_clear_esl_command_buf(void);
API_RESULT appl_esl_ap_reset(void);
API_RESULT appl_esl_ap_reset_esl_tag(BT_ESL_ADDR *esl_addr);
API_RESULT appl_esl_ap_get_esl_tag_state(BT_ESL_ADDR *esl_addr, UCHAR *state);
/* Callback prototypes */
void appl_connected_ind_cb(BT_ESL_ADDR *esl_addr, UCHAR status, void *blob);
void appl_disconnected_ind_cb(BT_ESL_ADDR *esl_addr, void *blob);
void appl_discovered_ind_cb(BT_ESL_ADDR esl_addr, UINT16 status, void *blob);
void appl_configured_ind_cb(BT_ESL_ADDR *esl_addr, UCHAR error, UINT16 result, void *blob);
void appl_synchronised_ind_cb(BT_ESL_ADDR *esl_addr, UINT16 status, void *blob);
void appl_response_ind_cb(BT_ESL_ADDR *esl_addr, BT_ESL_RSP *esl_rsp, UCHAR no_of_esl_rsp, void *blob);
void appl_synchronised_response_ind_cb(UCHAR group_id, UCHAR response_slot, BT_ESL_RSP *esl_rsp, UCHAR no_of_esl_rsp, UINT16 status);
void appl_esl_device_found_ind_cb(BT_ESL_BD_ADDR * peer_addr, UCHAR *adv_data, UINT16 adv_length);
void appl_info_ind_cb(BT_ESL_ADDR esl_addr, UCHAR info_type, void *data, void *blob);
void appl_esl_ap_mtu_exchange_complete_cb(BT_ESL_BD_ADDR *bd_addr, UINT16 mtu);

/**
 * \brief Initialize ESL AP tag table
 *
 * \par This function initializes the ESL AP tag table.
 */
void appl_es_ap_tag_table_init(void);

/**
 * \brief Print ESL AP state machine state
 *
 * \par This function prints the current state of the ESL AP state machine.
 *
 * \param[in] state Current state of the ESL AP state machine
 */
void appl_esl_ap_print_sm_state(UCHAR state);
/**
 * \brief Print ESL AP configuration state
 *
 * \par This function prints the current configuration state of the ESL AP.
 *
 * \param[in] state Current configuration state of the ESL AP
 */
void appl_esl_ap_print_config_state(UCHAR state);
void appl_esl_ap_parse_info_data
     (
        void * info_data,
        UCHAR  info_type
     );
void appl_esl_ap_parse_response_data
     (
        BT_ESL_RSP * esl_rsp,
        UCHAR no_of_esl_rsp
     );
/** Add device to advertisers list */
API_RESULT appl_add_device_to_advertisers_list(BT_ESL_BD_ADDR * peer_addr);
/* Get ESL tag from BD address */
BT_ESL_TAG * appl_esl_ap_get_esl_tag_from_bd_address(BT_ESL_BD_ADDR * bd_addr);
/** Display advertising list */
void appl_esl_ap_display_advertising_list(void);
/** Set multiple commands flag */
void appl_esl_ap_set_multiple_commands(UCHAR flag);
/** Check if multiple commands are enabled */
UCHAR appl_esl_ap_is_multiple_commands_enabled(void);
#ifdef APPL_ESL_AP_OTS_SUPPORT
/** OTS discovery complete cb */
void appl_esl_ap_ots_disc_complete
     (
         BT_ESL_BD_ADDR * bd_addr,
         UINT16 result
     );
/** Image upload complete cb */
void appl_esl_ap_image_upload_complete
     (
         BT_ESL_BD_ADDR * bd_addr,
         UCHAR state,
         void * blob
     );
/** OTS init  */
API_RESULT appl_esl_ap_ots_init(void);
/* Discover OTS */
API_RESULT appl_esl_ap_discover_ots(BT_ESL_ADDR * esl_addr);
/* Upload image */
API_RESULT appl_esl_ap_upload_image(BT_ESL_ADDR * esl_addr, UCHAR image_idx, UINT16 image_size);
/* Free dynamically allocated image data */
void appl_esl_ap_free_image_data(void);
#endif /* APPL_ESL_AP_OTS_SUPPORT */
/* Discover DIS */
API_RESULT appl_esl_ap_discover_dis(BT_ESL_ADDR * esl_addr);
#endif /* BT_ESL_SUPPORT_AP_ROLE */

#endif /* _H_APPL_ESL_AP_ */
