/**
 *  \file appl_esl_ap.c
 *

 */

/*
 *  Copyright (C) 2025. Mindtree Ltd.
 *  All rights reserved.
 */

/* --------------------------------------------- Header File Inclusion */
#include "appl_esl_ap.h"

#ifdef BT_ESL_SUPPORT_AP_ROLE

/* --------------------------------------------- Global Definitions */
/** Periodic adv params  */
/**
 * RSP Slot Spacing
 *
 * \note RSP Slot Spacing = interval in ms / 0.125
 */
#define APPL_ESLP_DEFAULT_RSP_SLOT_SPACING              0x20U
/* RSP Slot Count */
#define APPL_ESLP_DEFAULT_RESPONSE_START                0U
#define APPL_ESLP_DEFAULT_RESPONSE_COUNT                10U
/**
 * RSP Slot Delay
 *
 * \note RSP Slot Delay = interval in ms / 1.25
 */
#define APPL_ESLP_DEFAULT_RSP_SLOT_DELAY                0x78U
/** GAP between subevent */
#define APPL_ESLP_GAP_BETWEEN_SUBEVENT_IN_MS            1.25
/**
 * Subevent Interval
 *
 * \note Subevent Interval in ms : ((rsp slot spacing * rsp slot count)
 * + rsp slot delay+ gap between subevent)
 * Subevent Interval = interval in ms / 1.25
 **/
#define APPL_ESLP_DEFAULT_SUBEVENT_INTERVAL             0xD0U
/**
 * Number of Subevents
 *
 * \note Number of subevents is to be more than one for
 * initiation of periodic adv
*/
#define APPL_ESLP_DEFAULT_NUM_OF_SUBEVENTS              2
/**
 * Periodic Interval Min
 *
 * \note Periodic Interval Min interval :
 * (subevent interval * no of subevents)
 * Periodic Interval Min = interval in ms / 1.25
 */
#define APPL_ESLP_DEFAULT_PERIODIC_INTERVAL_MIN         0x01B3U
/**
 * Periodic Interval Max
 *
 * \note Periodic Interval Max interval :
 * (subevent interval * no of subevents)
 * Periodic Interval Max = interval in ms / 1.25
 */
#define APPL_ESLP_DEFAULT_PERIODIC_INTERVAL_MAX         0x01B3U
/** Periodic Interval Property */
#define APPL_ESLP_DEFAULT_PERIODIC_ADV_PROPERTY         0x00U

/** Max ext adv length */
#define APPL_ESL_AP_HCI_MAX_EXT_ADV_DATA_LENGTH                   255U

/** Short local name adv type  */
#define APPL_ESL_AP_HCI_EIR_DATA_TYPE_SHORTENED_LOCAL_NAME        0x08U
/** Complete local name adv type */
#define APPL_ESL_AP_HCI_EIR_DATA_TYPE_COMPLETE_LOCAL_NAME         0x09U

/** Max advertiser count */
#define APPL_ESL_AP_MAX_ADERTISER_COUNT                           5U

/** Min Image Data Size */
#define IMAGE_DATA_SIZE_MIN                                          20
/** Max Image Data Size */
#define IMAGE_DATA_SIZE_MAX                                          512

/* --------------------------------------------- External Global Variables */

/* --------------------------------------------- Exported Global Variables */

/* --------------------------------------------- Static Global Variables */
/** ESL AP Callbacks */
DECL_STATIC BT_ESL_AP_CALLBACKS appl_esl_callbacks =
{
    appl_connected_ind_cb,
    appl_disconnected_ind_cb,
    appl_discovered_ind_cb,
    appl_configured_ind_cb,
    appl_synchronised_ind_cb,
    appl_response_ind_cb,
    appl_synchronised_response_ind_cb,
    appl_esl_device_found_ind_cb,
    appl_info_ind_cb
};

/**  ESL AP tag table */
DECL_STATIC BT_ESL_TAG_TABLE appl_esl_tag_table;

/** ESL AP groups */
DECL_STATIC BT_ESL_GROUP appl_esl_groups[APPL_ESL_MAX_NO_OF_GROUPS];

/** ESL AP tags */
DECL_STATIC BT_ESL_TAG appl_esl_tags[APPL_ESL_MAX_NO_OF_GROUPS][APPL_ESL_MAX_NO_OF_TAGS_PER_GROUP];

/* Default values for keys */
BT_ESL_KEY_MATERIAL default_key =
{
     {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
     {0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22}
};

/** Advertisers list */
DECL_STATIC BT_ESL_BD_ADDR advertisers_list[APPL_ESL_AP_MAX_ADERTISER_COUNT];

/**
 * \brief Multiple command flag
 *
 * This flag indicates whether multiple commands is to be
 * sent in a single transaction. If set to `BT_ESL_FALSE`,
 * only one command can be sent per transaction. When set
 * to `BT_ESL_TRUE`, it signifies that multiple commands,
 * such as ping \ref BT_esl_ap_send_ping and read sensor
 * data \ref BT_esl_ap_send_read_sensor_data, can be
 * queued and sent together in one transaction by calling
 * \ref BT_esl_ap_send_esl_command.
 */
DECL_STATIC UCHAR multiple_cmd_flag = BT_ESL_FALSE;

#ifdef APPL_ESL_AP_OTS_SUPPORT
/** OTS cb */
DECL_STATIC BT_ESL_OTS_CALLBACK appl_ots_cb;
/** Dynamically allocated image data buffer */
static UCHAR *image_data = NULL;
#endif /* APPL_ESL_AP_OTS_SUPPORT */
/* --------------------------------------------- Functions */

void appl_es_ap_tag_table_init(void)
{
    UINT8 i, j;

    /* Initialize ESL tag table */
    appl_esl_tag_table.no_of_groups = APPL_ESL_MAX_NO_OF_GROUPS;
    appl_esl_tag_table.no_of_esl_tags_per_group = APPL_ESL_MAX_NO_OF_TAGS_PER_GROUP;

    /* Initialize ESL groups and tags */
    appl_esl_tag_table.esl_groups = appl_esl_groups;

    for (i = 0; i < APPL_ESL_MAX_NO_OF_GROUPS; i++)
    {
        appl_esl_tag_table.esl_groups[i].esl_tags = appl_esl_tags[i];
        for (j = 0; j < APPL_ESL_MAX_NO_OF_TAGS_PER_GROUP; j++)
        {
            /* Initialize each ESL tag */
            BT_ESL_AP_INIT_ESL_TAG(&appl_esl_tags[i][j]);
        }
    }
#ifdef BT_ESL_AP_SEPERATE_KEY_PER_GROUP
    /* Initialize keys if separate keys per group are used */
    for (i = 0; i < APPL_ESL_MAX_NO_OF_GROUPS; i++)
    {
        BT_ESL_mem_set(&appl_esl_tag_table.esl_groups[i].rsp_key, 0, sizeof(BT_ESL_KEY_MATERIAL));
        BT_ESL_mem_set(&appl_esl_tag_table.esl_groups[i].sync_key, 0, sizeof(BT_ESL_KEY_MATERIAL));
        /* Set default keys */
        BT_ESL_mem_copy(&appl_esl_tag_table.esl_groups[i].rsp_key, &default_key, sizeof(BT_ESL_KEY_MATERIAL));
        BT_ESL_mem_copy(&appl_esl_tag_table.esl_groups[i].sync_key, &default_key, sizeof(BT_ESL_KEY_MATERIAL));
    }
#else /* BT_ESL_AP_SEPERATE_KEY_PER_GROUP */
    /* Initialize keys if separate keys per group are not used */
    BT_ESL_mem_set(&appl_esl_tag_table.rsp_key, 0, sizeof(BT_ESL_KEY_MATERIAL));
    BT_ESL_mem_set(&appl_esl_tag_table.sync_key, 0, sizeof(BT_ESL_KEY_MATERIAL));
    /* Set default keys */
    BT_ESL_mem_copy(&appl_esl_tag_table.rsp_key, &default_key, sizeof(BT_ESL_KEY_MATERIAL));
    BT_ESL_mem_copy(&appl_esl_tag_table.sync_key, &default_key, sizeof(BT_ESL_KEY_MATERIAL));
#endif /* BT_ESL_AP_SEPERATE_KEY_PER_GROUP */
}

/**
 * ESL AP ATT MTU Exchange indication callback
 */
void appl_esl_ap_mtu_exchange_complete_cb(BT_ESL_BD_ADDR *bd_addr, UINT16 mtu)
{
    APPL_ESL_TRC(
    "[APPL]: MTU exchange complete with Peer BD Address: "\
    BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER" (MTU %d)\n",
    BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr), mtu);
}

/**
 * ESL AP Connection indication callback
 */
void appl_connected_ind_cb(BT_ESL_ADDR *esl_addr, UCHAR status, void *blob)
{
    BT_ESL_BD_ADDR * bd_addr;
    API_RESULT     retval;

    retval  = BT_ESL_API_SUCCESS;

    bd_addr = (BT_ESL_BD_ADDR *)blob;

    APPL_ESL_TRC(
    "[APPL]: ESL tag [%d : %d] connected (status %d)\n",
    esl_addr->group_id, esl_addr->esl_id, status);

    /* Check if the BD Address is not NULL and the Connection Status is successful i.e. 0x00U */
    if ((NULL != bd_addr) && (status == 0U))
    {
        APPL_ESL_TRC(
        "[APPL]: ESL tag BD Address: "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr));

        /** TODO:Start pairing */

        retval = BT_esl_gatt_exchange_mtu_pl
                 (
                     bd_addr,
                     128, /* MTU Size */
                     appl_esl_ap_mtu_exchange_complete_cb
                 );
    }
}

/**
 * ESL AP Disconnection indication callback
 */
void appl_disconnected_ind_cb(BT_ESL_ADDR *esl_addr, void *blob)
{
    BT_ESL_BD_ADDR * bd_addr;

    bd_addr = (BT_ESL_BD_ADDR *)blob;

    APPL_ESL_TRC(
    "[APPL]: ESL tag [%d : %d] disconnected \n",
    esl_addr->group_id, esl_addr->esl_id);

#ifdef APPL_ESL_AP_OTS_SUPPORT
    /* Free image data on disconnect */
    appl_esl_ap_free_image_data();
#endif /* APPL_ESL_AP_OTS_SUPPORT */

    if (NULL != bd_addr)
    {
        APPL_ESL_TRC(
        "[APPL]: ESL tag BD Address: "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr));
    }
}

/**
 * ESL AP discovered indication callback
 */
void appl_discovered_ind_cb(BT_ESL_ADDR esl_addr, UINT16 status, void *blob)
{
    BT_ESL_IGNORE_UNUSED_PARAM(blob);

    APPL_ESL_TRC(
    "[APPL]: ESL tag [%d : %d] discovery complete cb %s\n",
    esl_addr.group_id, esl_addr.esl_id,
    (status == BT_ESL_AP_SUCCESS) ? "SUCCESS" : "FAILURE");
}

/**
 * ESL AP Configured indication callback
 */
void appl_configured_ind_cb(BT_ESL_ADDR *esl_addr, UCHAR error, UINT16 result, void *blob)
{
    BT_ESL_AP_CONFIGURE_INFO * config_info;

    config_info = (BT_ESL_AP_CONFIGURE_INFO *)blob;

    APPL_ESL_TRC(
    "[APPL]: ESL tag [%d : %d] configured (error %d, result %d)\n",
    esl_addr->group_id, esl_addr->esl_id, error, result);

    if (NULL != config_info)
    {
        appl_esl_ap_print_config_state(config_info->state);
    }
}

/**
 * ESL AP Synchronized indication callback
 */
void appl_synchronised_ind_cb(BT_ESL_ADDR *esl_addr, UINT16 status, void *blob)
{
    BT_ESL_IGNORE_UNUSED_PARAM(blob);

    APPL_ESL_TRC(
    "[APPL]: ESL tag [%d : %d] synchronized (status 0x%04X)\n",
    esl_addr->group_id, esl_addr->esl_id, status);
}

/**
 * ESL AP Response indication callback when in connected state
 */
void appl_response_ind_cb(BT_ESL_ADDR *esl_addr, BT_ESL_RSP *esl_rsp, UCHAR no_of_esl_rsp, void *blob)
{
    BT_ESL_IGNORE_UNUSED_PARAM(blob);

    APPL_ESL_TRC(
    "[APPL]: ESL tag [%d : %d] response IND\n",
    esl_addr->group_id, esl_addr->esl_id);

    if ((NULL != esl_rsp) && (no_of_esl_rsp > 0))
    {
        APPL_ESL_TRC(
        "[APPL]: Response Opcode: 0x%02X, Data Length: %d\n",
        esl_rsp->response_opcode, BT_ESL_GET_CP_LEN(esl_rsp->response_opcode));

        appl_esl_ap_parse_response_data(esl_rsp, no_of_esl_rsp);
    }
}

/**
 * ESL AP Response indication callback when in synchronized state
 */
void appl_synchronised_response_ind_cb(UCHAR group_id, UCHAR response_slot, BT_ESL_RSP *esl_rsp, UCHAR no_of_esl_rsp, UINT16 status)
{
    UCHAR i;

    /* Init */
    i = 0;

    APPL_ESL_TRC("[APPL]: Group ID: %d, Response Slot: %d, Status: %d\n",
             group_id, response_slot, status);

    APPL_ESL_TRC("[APPL]: %s\n", (BT_ESL_AP_RESPONSE_COMPLETE == status) ? "Response complete" : "Response Pending");

    if ((NULL != esl_rsp) && (no_of_esl_rsp > 0))
    {
        appl_esl_ap_parse_response_data(esl_rsp, no_of_esl_rsp);
    }
}

/** Add device to advertisers list */
API_RESULT appl_add_device_to_advertisers_list(BT_ESL_BD_ADDR * peer_addr)
{
    UCHAR i;

    if (NULL != peer_addr)
    {
        /* Check if device is already present */
        for (i = 0U; i < APPL_ESL_AP_MAX_ADERTISER_COUNT; i++)
        {
            if (BT_ESL_COMPARE_BD_ADDR_AND_TYPE(peer_addr, &advertisers_list[i]))
            {
                return BT_ESL_API_FAILURE;
            }
        }

        /* Add to the device list */
        for (i = 0U; i < APPL_ESL_AP_MAX_ADERTISER_COUNT; i++)
        {
            if (BT_ESL_FALSE == BT_ESL_BD_ADDR_IS_NON_ZERO(advertisers_list[i].addr))
            {
                BT_ESL_COPY_BD_ADDR_AND_TYPE(&advertisers_list[i], peer_addr);
                return BT_ESL_API_SUCCESS;
            }
        }
    }

    return BT_ESL_API_FAILURE;
}

/* Get ESL tag from BD address */
BT_ESL_TAG * appl_esl_ap_get_esl_tag_from_bd_address(BT_ESL_BD_ADDR * bd_addr)
{
    BT_ESL_TAG * esl_tag;
    UCHAR        grp_id;
    UCHAR        esl_id;
    UCHAR        found;

    /* Init */
    esl_tag = NULL;
    grp_id = 0U;
    esl_id = 0U;
    found = BT_ESL_FALSE;

    if (NULL != bd_addr)
    {
        for (grp_id = 0U; grp_id < appl_esl_tag_table.no_of_groups; grp_id++)
        {
            for (esl_id = 0U; esl_id < appl_esl_tag_table.no_of_esl_tags_per_group; esl_id++)
            {
                esl_tag = &appl_esl_tag_table.esl_groups[grp_id].esl_tags[esl_id];

                if (BT_ESL_TRUE == BT_ESL_COMPARE_BD_ADDR_AND_TYPE
                               (
                                    &(esl_tag->esl_bd_addr),
                                    bd_addr
                               ))
                {
                    found = BT_ESL_TRUE;
                    break;
                }
                else
                {
                    esl_tag = NULL;
                }
            }

            /* Break the Loop if already found */
            if (BT_ESL_TRUE == found)
            {
                break;
            }
        }
    }
    else
    {
        APPL_ESL_ERR ("[APPL]: Invalid input parameters");
    }

    return esl_tag;
}

void appl_esl_ap_display_advertising_list(void)
{
    UCHAR i;
    UCHAR count;
    BT_ESL_TAG * tag;

    /* Init */
    tag = NULL;
    count = 0U;

    for (i = 0U; i < APPL_ESL_AP_MAX_ADERTISER_COUNT; i++)
    {
        if (BT_ESL_TRUE == BT_ESL_BD_ADDR_IS_NON_ZERO(advertisers_list[i].addr))
        {
            count++;
            /* Check if the device is in the tag table */
            tag = appl_esl_ap_get_esl_tag_from_bd_address(&advertisers_list[i]);

            CONSOLE_TRC (
            "[APPL]: %d. BD Address: "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"\n",
            count, BT_ESL_DEVICE_ADDR_PRINT_STR(&advertisers_list[i]));

            /* Print ESL ID , GRP ID and current state if tag is in tag table */
            if (NULL != tag)
            {
                CONSOLE_TRC (
                "\t{GRP ID 0x%02X : ESL ID 0x%02X} state: %s\n",
                tag->esl_address.group_id, tag->esl_address.esl_id,
                APPL_ESL_AP_GET_SM_STATE_STRING(tag->sm_state));
            }
        }
    }
}

/**
 * ESL AP scan report indication callback when ESL device found advertising
 */
void appl_esl_device_found_ind_cb(BT_ESL_BD_ADDR * peer_addr, UCHAR *adv_data, UINT16 adv_length)
{
    API_RESULT  retval;
    UCHAR ad_element[APPL_ESL_AP_HCI_MAX_EXT_ADV_DATA_LENGTH];
    UINT16 ad_element_data_len;

    /** Add the device to advertiser list */
    retval = appl_add_device_to_advertisers_list(peer_addr);

    if (BT_ESL_API_FAILURE != retval)
    {
        APPL_ESL_TRC(
        "[APPL]: ESL device found \n"
        "(BD Address: "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER", Adv Data Length: %d)\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(peer_addr), adv_length);

        /* Search for Shortened local name AD type */
        BT_ESL_mem_set(ad_element, 0x00U, sizeof(ad_element));
        retval = BT_esl_find_ad_element_pl
                 (
                     APPL_ESL_AP_HCI_EIR_DATA_TYPE_SHORTENED_LOCAL_NAME,
                     adv_data,
                     (UINT8)adv_length,
                     ad_element,
                     &ad_element_data_len
                 );

        if (BT_ESL_API_SUCCESS == retval)
        {
            /* If size of the local name as size of the ad_element then assiging
            last element 0x00 to make it as string */
            if (ad_element_data_len == sizeof(ad_element))
            {
                ad_element[ad_element_data_len - 1] = '\0';
            }
            CONSOLE_INF("\tShortened Local Name : %s\n", ad_element);
        }

        /* Search for Complete Local name AD type */
        BT_ESL_mem_set(ad_element, 0x00U, sizeof(ad_element));
        retval = BT_esl_find_ad_element_pl
                 (
                     APPL_ESL_AP_HCI_EIR_DATA_TYPE_COMPLETE_LOCAL_NAME,
                     adv_data,
                     (UINT8)adv_length,
                     ad_element,
                     &ad_element_data_len
                 );
        if (BT_ESL_API_SUCCESS == retval)
        {
            /* If size of the local name as size of the ad_element then assiging
            last element 0x00 to make it as string */
            if (ad_element_data_len == sizeof(ad_element))
            {
                ad_element[ad_element_data_len - 1] = '\0';
            }
            CONSOLE_INF("\tComplete Local Name : %s\n", ad_element);
        }
    }
}

/**
 * ESL AP read info indication callback
 */
void appl_info_ind_cb(BT_ESL_ADDR esl_addr, UCHAR info_type, void *data, void *blob)
{
    BT_ESL_IGNORE_UNUSED_PARAM(blob);
    BT_ESL_IGNORE_UNUSED_PARAM(data);

    APPL_ESL_TRC(
    "[APPL]: ESL tag [%d : %d] info IND (type %d)\n",
    esl_addr.group_id, esl_addr.esl_id, info_type);

    appl_esl_ap_parse_info_data(data, info_type);
}

API_RESULT appl_esl_ap_init(void)
{
    API_RESULT retval;

#ifdef BT_ESL_HAVE_DYNAMIC_GLOBAL_ARRAY
    BT_ESL_DYNAMIC_CONFIG appl_esl_dynamic_config;
#endif /* BT_ESL_HAVE_DYNAMIC_GLOBAL_ARRAY */

#ifdef BT_ESL_AP_AUTO_START_PADV
    BT_ESL_PERIODIC_ADV_PARAMS padv_param;
#endif /* BT_ESL_AP_AUTO_START_PADV */

#ifdef BT_ESL_HAVE_DYNAMIC_GLOBAL_ARRAY
    /** Initialize with default values for dynamic configuration */
    BT_ESL_INIT_DYNAMIC_CONFIG_LIMITS(&appl_esl_dynamic_config);

    /* Set the values that is set in application */
    appl_esl_dynamic_config.config_BT_ESL_MAX_GROUPS_SUPPORTED   = APPL_ESL_MAX_NO_OF_GROUPS;
    appl_esl_dynamic_config.config_BT_ESL_MAX_ESL_TAGS_SUPPORTED = APPL_ESL_MAX_NO_OF_TAGS_PER_GROUP;
    appl_esl_dynamic_config.config_BT_ESL_MAX_DISPLAY_SUPPORTED  = APPL_ESL_MAX_DISPLAY_SUPPORTED;
    appl_esl_dynamic_config.config_BT_ESL_MAX_LED_SUPPORTED      = APPL_ESL_MAX_LED_SUPPORTED;
    appl_esl_dynamic_config.config_BT_ESL_MAX_SENSOR_SUPPORTED   = APPL_ESL_MAX_SENSOR_SUPPORTED;
    appl_esl_dynamic_config.config_BT_ESL_MAX_IMAGE_SUPPORTED    = APPL_ESL_MAX_IMAGE_SUPPORTED;

    /* Initialize the ESL Module */
    BT_esl_init((void *)&appl_esl_dynamic_config);

#else /* BT_ESL_HAVE_DYNAMIC_GLOBAL_ARRAY */
    /** Initialize the ESL Module */
    BT_esl_init(NULL);
#endif /* BT_ESL_HAVE_DYNAMIC_GLOBAL_ARRAY */

    /**
     * This flag is disabled by default for now.
     * Starting of PADV for AP is from CLI Command.
     */
#ifdef BT_ESL_AP_AUTO_START_PADV
    /** Init */
    retval                                       = BT_ESL_AP_SUCCESS;
    padv_param.periodic_advertising_interval_max = APPL_ESLP_DEFAULT_PERIODIC_INTERVAL_MAX;
    padv_param.periodic_advertising_interval_min = APPL_ESLP_DEFAULT_PERIODIC_INTERVAL_MIN;
    padv_param.periodic_adv_prty                 = APPL_ESLP_DEFAULT_PERIODIC_ADV_PROPERTY;
    padv_param.num_subevents                     = APPL_ESLP_DEFAULT_NUM_OF_SUBEVENTS;
    padv_param.subevent_interval                 = APPL_ESLP_DEFAULT_SUBEVENT_INTERVAL;
    padv_param.response_slot_delay               = APPL_ESLP_DEFAULT_RSP_SLOT_DELAY;
    padv_param.response_slot_spacing             = APPL_ESLP_DEFAULT_RSP_SLOT_SPACING;
    padv_param.num_response_slots                = APPL_ESLP_DEFAULT_RESPONSE_COUNT;

    /** Start PAWR */
    BT_esl_start_periodic_adv_pl(padv_param);
#endif /* BT_ESL_AP_AUTO_START_PADV */

    /** Initialize ESL AP tag table */
    appl_es_ap_tag_table_init();

    /** Init ESL AP */
    retval = BT_esl_ap_init(&appl_esl_callbacks, &appl_esl_tag_table);
    if (retval != BT_ESL_AP_SUCCESS)
    {
        APPL_ESL_ERR("[APPL]: ESL AP initialization failed- retval 0x%04X\n", retval);
    }

#ifdef APPL_ESL_AP_OTS_SUPPORT
    /** Init OTS  */
    appl_esl_ap_ots_init();
#endif /* APPL_ESL_AP_OTS_SUPPORT */

    return retval;
}

API_RESULT appl_esl_ap_start_periodic_adv(void)
{
    API_RESULT retval;
    BT_ESL_PERIODIC_ADV_PARAMS padv_param;

    /** Init */
    retval                                       = BT_ESL_AP_SUCCESS;
    padv_param.periodic_advertising_interval_max = APPL_ESLP_DEFAULT_PERIODIC_INTERVAL_MAX;
    padv_param.periodic_advertising_interval_min = APPL_ESLP_DEFAULT_PERIODIC_INTERVAL_MIN;
    padv_param.periodic_adv_prty                 = APPL_ESLP_DEFAULT_PERIODIC_ADV_PROPERTY;
    padv_param.num_subevents                     = APPL_ESLP_DEFAULT_NUM_OF_SUBEVENTS;
    padv_param.subevent_interval                 = APPL_ESLP_DEFAULT_SUBEVENT_INTERVAL;
    padv_param.response_slot_delay               = APPL_ESLP_DEFAULT_RSP_SLOT_DELAY;
    padv_param.response_slot_spacing             = APPL_ESLP_DEFAULT_RSP_SLOT_SPACING;
    padv_param.num_response_slots                = APPL_ESLP_DEFAULT_RESPONSE_COUNT;

    /** Start PAWR */
    retval = BT_esl_start_periodic_adv_pl(padv_param);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP start periodic adv failed- retval 0x%04X\n", retval);
    }

    return retval;
}

API_RESULT appl_esl_ap_stop_periodic_adv(void)
{
    API_RESULT retval;

    retval = BT_esl_stop_periodic_adv_pl();
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP stop periodic adv failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_scan_esl_device(UCHAR flag)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** start/stop scan */
    retval = BT_esl_ap_scan_esl_device(flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP scan failed- retval 0x%04X\n", retval);
    }
    else
    {
        /* Clear the adv list only if we are only starting the scan */
        if (BT_ESL_FALSE != flag)
        {
            /* Clear advertising list */
            BT_ESL_mem_set(advertisers_list, 0x00U, sizeof(advertisers_list));
        }
    }
    return retval;
}


API_RESULT appl_esl_ap_add_esl_tag(BT_ESL_ADDR *esl_addr, BT_ESL_BD_ADDR * peer_addr)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Add ESL tag */
    retval = BT_esl_ap_add_esl_tag(esl_addr, peer_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP add tag failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_remove_esl_tag(BT_ESL_ADDR *esl_addr)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;
    /** Remove ESL tag */
    retval = BT_esl_ap_remove_esl_tag(esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP remove tag failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_connect_esl(BT_ESL_ADDR *esl_addr)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Connect to ESL tag */
    retval = BT_esl_ap_connect_esl(esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP connect failed- retval 0x%04X\n", retval);
    }

    return retval;
}

API_RESULT appl_esl_ap_disconnect_esl(BT_ESL_ADDR *esl_addr)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Disconnect from ESL tag */
    retval = BT_esl_ap_disconnect_esl(esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP disconnect failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_config(BT_ESL_ADDR *esl_addr)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Configure ESL tag */
    retval = BT_esl_ap_config(esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP config failed- retval 0x%04X\n", retval);
    }
    return retval;
}


API_RESULT appl_esl_ap_discover_esl_service(BT_ESL_ADDR *esl_addr)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Discover ESL service */
    retval = BT_esl_ap_discover_esl_service(esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP discover service failed- retval 0x%04X\n", retval);
    }

    return retval;
}


UINT32 appl_esl_ap_get_current_abs_time(void)
{
    return BT_esl_ap_get_current_abs_time();
}


API_RESULT appl_esl_ap_sync_with_esl(BT_ESL_ADDR *esl_addr)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Sync with ESL tag */
    retval = BT_esl_ap_sync_with_esl(esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP sync failed- retval 0x%04X\n", retval);
    }
    return retval;
}


API_RESULT appl_esl_ap_get_display_information(BT_ESL_ADDR *esl_addr)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** get display information */
    retval = BT_esl_ap_get_display_information(esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP get display information failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_get_led_information(BT_ESL_ADDR *esl_addr)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Get LED information */
    retval = BT_esl_ap_get_led_information(esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP get LED information failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_get_sensor_information(BT_ESL_ADDR *esl_addr)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Get sensor information */
    retval = BT_esl_ap_get_sensor_information(esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP get sensor information failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_get_image_information(BT_ESL_ADDR *esl_addr)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Get image information */
    retval = BT_esl_ap_get_image_information(esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP get image information failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_send_ping(BT_ESL_ADDR *esl_addr, UCHAR multiple_commands_flag)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Send ping */
    retval = BT_esl_ap_send_ping(esl_addr, multiple_commands_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP send ping failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_send_unassociate(BT_ESL_ADDR *esl_addr, UCHAR multiple_commands_flag)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Send unassociate */
    retval = BT_esl_ap_send_unassociate(esl_addr, multiple_commands_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP send unassociate failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_send_service_reset(BT_ESL_ADDR *esl_addr, UCHAR multiple_commands_flag)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Send service reset */
    retval = BT_esl_ap_send_service_reset(esl_addr, multiple_commands_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP send service reset failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_send_factory_reset(BT_ESL_ADDR *esl_addr)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Send factory reset */
    retval = BT_esl_ap_send_factory_reset(esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP send factory reset failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_send_read_sensor_data(BT_ESL_ADDR *esl_addr, UCHAR sensor_index, UCHAR multiple_commands_flag)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Send read sensor data */
    retval = BT_esl_ap_send_read_sensor_data(esl_addr, sensor_index, multiple_commands_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP send read sensor data failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_send_refresh_display(BT_ESL_ADDR *esl_addr, UCHAR display_index, UCHAR multiple_commands_flag)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Send refresh display */
    retval = BT_esl_ap_send_refresh_display(esl_addr, display_index, multiple_commands_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP send refresh display failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_send_display_image(BT_ESL_ADDR *esl_addr, UCHAR display_index, UCHAR image_index, UCHAR multiple_commands_flag)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Send display image */
    retval = BT_esl_ap_send_display_image(esl_addr, display_index, image_index, multiple_commands_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP send display image failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_send_display_timed_image(BT_ESL_ADDR *esl_addr, UCHAR display_index, UCHAR image_index, UINT32 absolute_time, UCHAR multiple_commands_flag)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Send display timed image */
    retval = BT_esl_ap_send_display_timed_image(esl_addr, display_index, image_index, absolute_time, multiple_commands_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP send display timed image failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_send_led_control(BT_ESL_ADDR *esl_addr, UCHAR led_index, UCHAR color_brightness, UCHAR *flashing_pattern, UINT16 repeat_type, UCHAR multiple_commands_flag)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Send LED control */
    retval = BT_esl_ap_send_led_control(esl_addr, led_index, color_brightness, flashing_pattern, repeat_type, multiple_commands_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP send LED control failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_send_led_timed_control(BT_ESL_ADDR *esl_addr, UCHAR led_index, UCHAR color_brightness, UCHAR *flashing_pattern, UINT16 repeat_type, UINT32 absolute_time, UCHAR multiple_commands_flag)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Send LED timed control */
    retval = BT_esl_ap_send_led_timed_control(esl_addr, led_index, color_brightness, flashing_pattern, repeat_type, absolute_time, multiple_commands_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP send LED timed control failed- retval 0x%04X\n", retval);
    }

    return retval;
}


#ifdef BT_ESL_SUPPORT_VENDOR_SPECIFIC_COMMANDS
API_RESULT appl_esl_ap_send_vendor_specific_command(BT_ESL_ADDR *esl_addr, UCHAR opcode, UCHAR *data, UINT16 data_len, UCHAR multiple_commands_flag)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** end vendor specific command */
    retval = BT_esl_ap_send_vendor_specific_command(esl_addr, opcode, data, data_len, multiple_commands_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP send vendor specific command failed- retval 0x%04X\n", retval);
    }

    return retval;
}
#endif /* BT_ESL_SUPPORT_VENDOR_SPECIFIC_COMMANDS */


API_RESULT appl_esl_ap_send_esl_command(void)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Send ESL command */
    retval = BT_esl_ap_send_esl_command();
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP send ESL command failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_clear_esl_command_buf(void)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Clear ESL command buffer */
    retval = BT_esl_ap_clear_esl_command_buf();
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP clear ESL command buffer failed- retval 0x%04X\n", retval);
    }

    return retval;
}

API_RESULT appl_esl_ap_reset(void)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Reset AP */
    retval = BT_esl_ap_reset();
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP reset failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_reset_esl_tag(BT_ESL_ADDR *esl_addr)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;

    /** Reset ESL tag */
    retval = BT_esl_ap_reset_esl_tag(esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP reset ESL tag failed- retval 0x%04X\n", retval);
    }

    return retval;
}


API_RESULT appl_esl_ap_get_esl_tag_state(BT_ESL_ADDR *esl_addr, UCHAR *state)
{
    API_RESULT retval;
    /** Init */
    retval = BT_ESL_AP_SUCCESS;
    /** Get ESL tag state */
    retval = BT_esl_ap_get_esl_tag_state(esl_addr, state);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: ESL AP get ESL tag state failed- retval 0x%04X\n", retval);
    }
    else
    {
        appl_esl_ap_print_sm_state(*state);
    }
    return retval;
}

void appl_esl_ap_print_sm_state(UCHAR state)
{
    switch (state)
    {
        case BT_ESL_AP_UNASSOCIATE:
            APPL_ESL_TRC("[APPL]: ESL tag state: UNASSOCIATED\n");
            break;
        case BT_ESL_AP_CONNECTED:
            APPL_ESL_TRC("[APPL]: ESL tag state: CONNECTED\n");
            break;
        case BT_ESL_AP_CONFIGURING:
            APPL_ESL_TRC("[APPL]: ESL tag state: CONFIGURING\n");
            break;
        case BT_ESL_AP_IN_SYNCRONIZING:
            APPL_ESL_TRC("[APPL]: ESL tag state: IN_SYNCHRONIZING\n");
            break;
        case BT_ESL_AP_SYNCHRONIZED:
            APPL_ESL_TRC("[APPL]: ESL tag state: SYNCHRONIZED\n");
            break;
        default:
            APPL_ESL_ERR("[APPL]: ESL tag state: UNKNOWN (%d)\n", state);
    }
}

void appl_esl_ap_print_config_state(UCHAR state)
{
    switch (state)
    {
        case BT_ESL_AP_CONFIGURATION_COMPLETE:
            APPL_ESL_TRC("[APPL]: ESL tag config state: COMPLETE\n");
            break;
        case BT_ESL_AP_CP_CONFIGURE_NTF_STATE:
            APPL_ESL_TRC("[APPL]: ESL tag config state: CONFIGURE_NTF\n");
            break;
        case BT_ESL_AP_SET_ESL_ADDRESS_STATE:
            APPL_ESL_TRC("[APPL]: ESL tag config state: WRITE_ESL_ADDRESS\n");
            break;
        case BT_ESL_AP_SET_AP_SYNC_KEY_STATE:
            APPL_ESL_TRC("[APPL]: ESL tag config state: SET_AP_SYNC_KEY\n");
            break;
        case BT_ESL_AP_SET_TAG_RESPONSE_KEY_STATE:
            APPL_ESL_TRC("[APPL]: ESL tag config state: SET_TAG_RESPONSE_KEY\n");
            break;
        case BT_ESL_AP_SET_CURRENT_ABS_TIME_STATE:
            APPL_ESL_TRC("[APPL]: ESL tag config state: SET_CURRENT_ABS_TIME\n");
            break;
        case BT_ESL_AP_CONFIGURATION_IDLE_STATE:
            APPL_ESL_TRC("[APPL]: ESL tag config state: IDLE\n");
            break;
        default:
            APPL_ESL_ERR("[APPL]: ESL tag config state: UNKNOWN (%d)\n", state);
    }
}

void appl_esl_ap_parse_info_data
     (
        void * info_data,
        UCHAR  info_type
     )
{
    UINT16 i;

    if (NULL != info_data)
    {
        switch (info_type)
        {
            case BT_ESL_IMAGE_INFO_TYPE:
            {
                APPL_ESL_INF("\tESL Image Info\n");
                APPL_ESL_INF("\t\tImage Index: %d\n", ((UCHAR *)info_data)[0]);
                break;
            }
            case BT_ESL_SENSOR_INFO_TYPE:
            {
                BT_ESL_SENSOR_INFO * sensor_info;

                sensor_info = (BT_ESL_SENSOR_INFO *)info_data;
                APPL_ESL_INF("\tESL Sensor Info\n");
                for (i = 0; i < sensor_info->no_of_sensors; i++)
                {
                    APPL_ESL_INF("\t\tSensor Index: %d\n", i);
                    APPL_ESL_INF(
                        "\t\tSensor size: %s\n",
                        (sensor_info->esl_sensor[i].sensor_size == BT_ESL_SENSOR_INFO_2_OCTET_SIZE_TYPE) ? "2 OCTET" : "4 OCTET");
                    if (sensor_info->esl_sensor[i].sensor_size == BT_ESL_SENSOR_INFO_2_OCTET_SIZE_TYPE)
                    {
                        APPL_ESL_INF("\t\tSensor type: 0x%04X\n", sensor_info->esl_sensor[i].sensor_type.type_16);
                    }
                    else
                    {
                        APPL_ESL_INF("\t\tSensor type: 0x%08X\n", sensor_info->esl_sensor[i].sensor_type.type_32);
                    }
                }
                break;
            }
            case BT_ESL_LED_INFO_TYPE:
            {
                BT_ESL_LED_INFO * led_info;

                led_info = (BT_ESL_LED_INFO *)info_data;
                APPL_ESL_INF("\tESL LED Info\n");
                for (i = 0; i < led_info->no_of_leds; i++)
                {
                    APPL_ESL_INF("\t\tLED Index: %d\n", i);
                    APPL_ESL_INF("\t\tLED type: %s\n",
                        ((led_info->esl_led[i].led_type & BT_ESL_MONOCHROME) == BT_ESL_MONOCHROME ?
                        "Monochrome" : "eRGB"));
                }
                break;
            }
            case BT_ESL_DISPLAY_INFO_TYPE:
            {
                BT_ESL_DISPLAY_INFO * display_info;

                display_info = (BT_ESL_DISPLAY_INFO *)info_data;
                APPL_ESL_INF("\tESL Display Info\n");
                for (i = 0; i < display_info->no_of_displays; i++)
                {
                    APPL_ESL_INF("\t\tDisplay Index: %d\n", i);
                    APPL_ESL_INF("\t\tDisplay type: 0x%02X\n",
                        (display_info->esl_display[i].display_type));
                    APPL_ESL_INF("\t\tDisplay size: {width : %d, height : %d}\n",
                        display_info->esl_display[i].width,
                        display_info->esl_display[i].height);
                }
                break;
            }
            default:
            {
                APPL_ESL_ERR("\tUnknown Info Type: %d\n", info_type);
            }
        }
    }
}

void appl_esl_ap_parse_response_data
     (
        BT_ESL_RSP * esl_rsp,
        UCHAR no_of_esl_rsp
     )
{
    UCHAR index;
    UCHAR rsp_len;
    UCHAR i;

    /** Init */
    index = 0U;

    if (NULL == esl_rsp)
    {
        APPL_ESL_ERR ("[APPL]: ESL response is NULL\n");
        return;
    }

    for (i = 0U; i < no_of_esl_rsp; i++)
    {
        /* Extract TLV length parameter using opcode length will be tlv_len-1 */
        rsp_len = BT_ESL_GET_CP_LEN(esl_rsp[i].response_opcode);

        APPL_ESL_INF(
            "[APPL]: Response Opcode: 0x%02X, Data Length: %d\n",
            esl_rsp[i].response_opcode, rsp_len);

        /** TODO: Validate length for each opcode */

        switch (esl_rsp[i].response_opcode)
        {
        case BT_ESL_ERROR:
            APPL_ESL_INF("\tERROR {Error code:0x%02X}\n", esl_rsp[i].data[index]);
            break;
        case BT_ESL_LED_STATE:
            APPL_ESL_INF("\tLED STATE {LED index:%d}\n", esl_rsp[i].data[index]);
            break;
        case BT_ESL_BASIC_STATE:
            {
                UINT16 basic_state;
                BT_ESL_UNPACK_LE_2_BYTE(&basic_state, &esl_rsp[i].data[index]);
                APPL_ESL_INF("\tBASIC STATE\n");
                APPL_ESL_INF("\t\tService Needed:%s\n",
                BT_ESL_TRUE == BT_ESL_CHECK_BASIC_STATE_BIT(basic_state, BT_ESL_BASIC_STATE_SERV_NEEDED_BIT) ?
                "ON" : "OFF");
                APPL_ESL_INF("\t\tSynchronised:%s\n",
                BT_ESL_TRUE == BT_ESL_CHECK_BASIC_STATE_BIT(basic_state, BT_ESL_BASIC_STATE_SYNCHRONISED_BIT) ?
                "ON" : "OFF");
                APPL_ESL_INF("\t\tLED active:%s\n",
                BT_ESL_TRUE == BT_ESL_CHECK_BASIC_STATE_BIT(basic_state, BT_ESL_BASIC_STATE_ACTIVE_LED_BIT) ?
                "ON" : "OFF");
                APPL_ESL_INF("\t\tPending LED update:%s\n",
                BT_ESL_TRUE == BT_ESL_CHECK_BASIC_STATE_BIT(basic_state, BT_ESL_BASIC_STATE_PNDG_LED_UPDT_BIT) ?
                "ON" : "OFF");
                APPL_ESL_INF("\t\tPending display update:%s\n",
                BT_ESL_TRUE == BT_ESL_CHECK_BASIC_STATE_BIT(basic_state, BT_ESL_BASIC_STATE_PNDG_DISP_UPDT_BIT) ?
                "ON" : "OFF");
            }
            break;
        case BT_ESL_DISPLAY_STATE:
            APPL_ESL_INF("\tDISPLAY STATE {Display index:%d} {Image index:%d}\n",
            esl_rsp[i].data[index], esl_rsp[i].data[index + 1]);
            break;
        default:
            if (BT_ESL_SENSOR_VALUE == BT_ESL_GET_CP_BASE_TAG(esl_rsp[i].response_opcode))
            {
                APPL_ESL_INF(
                    "\tSENSOR VALUE {Sensor index:%d} {Sensor data length:%d}\n", esl_rsp[i].data[index], rsp_len);
            }
            else
            {
                APPL_ESL_INF("\tUNKNOWN RESPONSE {Opcode:0x%02X}\n", esl_rsp[i].response_opcode);
            }
            break;
        }
    }
}

void appl_esl_ap_set_multiple_commands(UCHAR flag)
{
    multiple_cmd_flag = (flag == BT_ESL_FALSE) ? BT_ESL_FALSE : BT_ESL_TRUE;
    APPL_ESL_INF("[APPL]: Multiple Command Flag set to: %d\n", multiple_cmd_flag);
}

UCHAR appl_esl_ap_is_multiple_commands_enabled(void)
{
    return multiple_cmd_flag;
}

#ifdef APPL_ESL_AP_OTS_SUPPORT
void appl_esl_ap_ots_disc_complete
     (
         BT_ESL_BD_ADDR * bd_addr,
         UINT16 result
     )
{
    API_RESULT retval;
    CONSOLE_TRC("[APPL]: OTS discovery complete received\n");

    if (NULL != bd_addr)
    {
        CONSOLE_TRC (
        "[APPL]: BD Address: "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER
        ", Status : %s\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr),
        (result == BT_ESL_API_SUCCESS) ? "SUCCESS" : "FAILURE");
        if (BT_ESL_API_SUCCESS == result)
        {
            retval = BT_esl_config_ots_pl(bd_addr);
            if (BT_ESL_API_SUCCESS == retval)
            {
                CONSOLE_TRC("[APPL]: OTS configuration successful\n");
            }
            else
            {
                CONSOLE_TRC("[APPL]: OTS configuration failed\n");
            }
        }
    }
}

void appl_esl_ap_free_image_data(void)
{
    if (NULL != image_data)
    {
        BT_ESL_free_mem(image_data);
        image_data = NULL;
        APPL_ESL_TRC("[APPL]: Image data buffer freed\n");
    }
}

void appl_esl_ap_image_upload_complete
     (
         BT_ESL_BD_ADDR * bd_addr,
         UCHAR state,
         void * blob
     )
{
    BT_ESL_IGNORE_UNUSED_PARAM(blob);

    CONSOLE_TRC("[APPL]: OTS image upload complete received\n");

    /* Free the dynamically allocated image data */
    appl_esl_ap_free_image_data();

    if (NULL != bd_addr)
    {
        CONSOLE_TRC (
        "[APPL]: BD Address: "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER
        ", Image Upload State : 0x%02X\n",
        BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr),
        state);
    }
}

API_RESULT appl_esl_ap_ots_init(void)
{
    API_RESULT retval;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    appl_ots_cb.discovery_complete = appl_esl_ap_ots_disc_complete;
    appl_ots_cb.image_upload_complete = appl_esl_ap_image_upload_complete;

    /* Register ots cb */
    retval = BT_esl_register_ots_callback_pl(&appl_ots_cb);
    if (retval != BT_ESL_API_SUCCESS)
    {
        APPL_ESL_ERR(
        "[APPL]: Failed to register OTS callback, retval 0x%04X\n", retval);
    }

    return retval;
}

API_RESULT appl_esl_ap_discover_dis(BT_ESL_ADDR * esl_addr)
{
    API_RESULT retval;
    BT_ESL_TAG * esl_tag;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    if (NULL != esl_addr)
    {
        /* Find associated ESL tag from table */
        if ((appl_esl_tag_table.no_of_groups > (esl_addr)->group_id) &&
            (appl_esl_tag_table.no_of_esl_tags_per_group > (esl_addr)->esl_id))
        {
            esl_tag = &appl_esl_tag_table.esl_groups[(esl_addr)->group_id].esl_tags[(esl_addr)->esl_id];
            /* Discover DIS */
            retval = BT_esl_discover_dis_pl(&esl_tag->esl_bd_addr);
            if (retval != BT_ESL_API_SUCCESS)
            {
                APPL_ESL_ERR(
                "[APPL]: Failed to discover DIS, retval 0x%04X\n", retval);
            }
        }
        else
        {
            APPL_ESL_ERR(
            "[APPL]: Invalid ESL Address\n");
            retval = BT_ESL_API_FAILURE;
        }
    }
    else
    {
        APPL_ESL_ERR(
        "[APPL]: ESL Address is NULL\n");
        retval = BT_ESL_API_FAILURE;
    }


    return retval;
}

API_RESULT appl_esl_ap_discover_ots(BT_ESL_ADDR * esl_addr)
{
    API_RESULT retval;
    BT_ESL_TAG * esl_tag;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    if (NULL != esl_addr)
    {
        /* Find associated ESL tag from table */
        if ((appl_esl_tag_table.no_of_groups > (esl_addr)->group_id) &&
            (appl_esl_tag_table.no_of_esl_tags_per_group > (esl_addr)->esl_id))
        {
            esl_tag = &appl_esl_tag_table.esl_groups[(esl_addr)->group_id].esl_tags[(esl_addr)->esl_id];
            /* Discover OTS */
            retval = BT_esl_discover_ots_pl(&esl_tag->esl_bd_addr);
            if (retval != BT_ESL_API_SUCCESS)
            {
                APPL_ESL_ERR(
                "[APPL]: Failed to discover OTS, retval 0x%04X\n", retval);
            }
        }
        else
        {
            APPL_ESL_ERR(
            "[APPL]: Invalid ESL Address\n");
            retval = BT_ESL_API_FAILURE;
        }
    }
    else
    {
        APPL_ESL_ERR(
        "[APPL]: ESL Address is NULL\n");
        retval = BT_ESL_API_FAILURE;
    }


    return retval;
}


API_RESULT appl_esl_ap_upload_image(BT_ESL_ADDR * esl_addr, UCHAR image_idx, UINT16 image_size)
{
    API_RESULT retval;
    BT_ESL_TAG * esl_tag;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    /* Use minimum size as default if image_size is 0 */
    if (0 == image_size)
    {
        image_size = IMAGE_DATA_SIZE_MIN;
    }

    /* Validate image size */
    if ((image_size < IMAGE_DATA_SIZE_MIN) || (image_size > IMAGE_DATA_SIZE_MAX))
    {
        APPL_ESL_ERR(
        "[APPL]: Invalid image size %d. Range: %d-%d\n",
        image_size, IMAGE_DATA_SIZE_MIN, IMAGE_DATA_SIZE_MAX);
        return BT_ESL_API_FAILURE;
    }

    /* Check if an image upload is already in progress */
    if (NULL != image_data)
    {
        APPL_ESL_ERR("[APPL]: Image upload already in progress\n");
        return BT_ESL_API_FAILURE;
    }

    if (NULL != esl_addr)
    {
        /* Find associated ESL tag from table */
        if ((appl_esl_tag_table.no_of_groups > (esl_addr)->group_id) &&
            (appl_esl_tag_table.no_of_esl_tags_per_group > (esl_addr)->esl_id))
        {
            esl_tag = &appl_esl_tag_table.esl_groups[(esl_addr)->group_id].esl_tags[(esl_addr)->esl_id];

            /* Allocate image data buffer */
            image_data = (UCHAR *)BT_ESL_alloc_mem(image_size);
            if (NULL == image_data)
            {
                APPL_ESL_ERR("[APPL]: Failed to allocate image data buffer (%d bytes)\n", image_size);
                return BT_ESL_API_FAILURE;
            }

            /* Fill with test pattern */
            BT_ESL_mem_set(image_data, 'A', image_size);

            /* Upload image */
            retval = BT_esl_upload_image_pl
                     (
                         &esl_tag->esl_bd_addr,
                         image_idx,
                         image_data,
                         image_size
                     );
            if (retval != BT_ESL_API_SUCCESS)
            {
                APPL_ESL_ERR(
                "[APPL]: Failed to upload image, retval 0x%04X\n", retval);
                /* Free on failure */
                appl_esl_ap_free_image_data();
            }
        }
        else
        {
            APPL_ESL_ERR(
            "[APPL]: Invalid ESL Address\n");
            retval = BT_ESL_API_FAILURE;
        }
    }
    else
    {
        APPL_ESL_ERR(
        "[APPL]: ESL Address is NULL\n");
        retval = BT_ESL_API_FAILURE;
    }


    return retval;
}

#endif /* APPL_ESL_AP_OTS_SUPPORT */
#endif /* BT_ESL_SUPPORT_AP_ROLE */
