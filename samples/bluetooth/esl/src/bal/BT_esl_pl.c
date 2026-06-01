/**
 *  \file BT_esl_pl.c
 */

/*
 *  Copyright (C) 2025. LTI Mindtree Ltd.
 *  All rights reserved.
 */

/* --------------------------------------------- Header File Inclusion */
#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/autoconf.h>
#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/ead.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/bluetooth/services/ots.h>
#include <zephyr/sys/atomic.h>
#include "BT_esl_pl.h"

/* --------------------------------------------- Global Definitions */
/**
 * NOTE:
 * This flag enables adjustment for Subevent Codes.
 * In some controller it is observed that the Subevet Start
 * is happening from 1 instead of 0.
 * This needs to be verified from Specification to understand
 * defined behaviour.
 * In Zephyr(v4.0.99), the subevent code is starting from 1.
 */
/* #define BT_ESL_AP_HAVE_SUBEVENT_ADJUSTMENT */

#define GATT_ESL_ADDRESS_CHARACTERISTIC                             0x2BF6U
#define GATT_AP_SYNC_KEY_MATERIAL_CHARACTERISTIC                    0x2BF7U
#define GATT_ESL_RESPONSE_KEY_MATERIAL_CHARACTERISTIC               0x2BF8U
#define GATT_ESL_CURRENT_ADSOLUTE_TIME_CHARACTERISTIC               0x2BF9U
#define GATT_ESL_DISPLAY_INFORMATION_CHARACTERISTIC                 0x2BFAU
#define GATT_ESL_IMAGE_INFORMATION_CHARACTERISTIC                   0x2BFBU
#define GATT_ESL_SENSOR_INFORMATION_CHARACTERISTIC                  0x2BFCU
#define GATT_ESL_LED_INFORMATION_CHARACTERISTIC                     0x2BFDU
#define GATT_ESL_CONTROL_POINT_CHARACTERISTIC                       0x2BFEU

/* UUID size */
#define BT_ESL_PL_UUID_SIZE                                         2U
#define BT_ESL_PL_CCCD_VAL_SIZE                                     2U

#ifdef BT_ESL_SUPPORT_AP_ROLE
#define BT_ESL_MAX_READ_SESSION_BUFFER_SIZE                         512U

#define DISC_OTS_FEATURE_BIT                                        0U
#define DISC_OTS_NAME_BIT                                           1U
#define DISC_OTS_TYPE_BIT                                           2U
#define DISC_OTS_SIZE_BIT                                           3U
#define DISC_OTS_ID_BIT                                             4U
#define DISC_OTS_PROPERTIES_BIT                                     5U
#define DISC_OTS_ACTION_CP_BIT                                      6U
#define DISC_OTS_LIST_CP_BIT                                        7U
#endif /* BT_ESL_SUPPORT_AP_ROLE */
/* --------------------------------------------- External Global Variables */
#ifdef BT_ESL_SUPPORT_AP_ROLE
#endif /* BT_ESL_SUPPORT_AP_ROLE */

/* --------------------------------------------- Exported Global Variables */

#ifdef BT_ESL_SUPPORT_AP_ROLE
/**
 * \brief Structure for tracking ESL attribute handles and discovery state.
 *
 * \par This structure contains the attribute handles discovered during GATT operations
 * and a flag to track whether discovery is currently in progress.
 */
typedef struct _BT_ESL_ATTR_HANDLE_PL
{
    /** Attribute handles discovered during GATT operations. */
    BT_ESL_ATTR_HANDLES attr_handles;

    /** Flag indicating whether discovery is currently in progress (1: in progress, 0: not in progress). */
    UCHAR discovery_in_progress;

    /** Discovery parameters - must persist across function calls */
    struct bt_gatt_discover_params discover_params;

} BT_ESL_ATTR_HANDLE_PL;


/**
 * \brief Structure to manage GATT read session data per BLE connection.
 *
 * \par This structure is used to accumulate fragmented data received via GATT read
 * operations in Bluetooth Low Energy (BLE). It maintains a buffer, the total
 * length of data received so far, and a flag indicating whether a read session
 * is currently active.
 */
typedef struct _BT_ESL_READ_SESSION_PL
{
    /**
     * \brief Parameters for GATT read operations.
     *
     * \par This structure holds the parameters for GATT read operations, including
     * the attribute handle and the data to be read.
     */
    struct bt_gatt_read_params * read_params;
    /**
     * \brief Buffer to accumulate read data fragments.
     *
     * \par This buffer stores the data received from multiple GATT read callbacks
     * until the complete attribute value is received.
     */
    UCHAR buffer[BT_ESL_MAX_READ_SESSION_BUFFER_SIZE];

    /**
     * \brief Total length of accumulated data.
     *
     * \par Indicates how many bytes have been stored in the buffer so far.
     */
    UINT16 length;

    /**
     * \brief Flag indicating if read is in progress.
     *
     * \par Set to true when a read session is active and data is being accumulated.
     * Reset to false once the complete data has been received or the session ends.
     */
    UCHAR read_in_progress;

} BT_ESL_READ_SESSION_PL;

/**
 * \brief Structure for managing GATT write sessions per BLE connection.
 */
typedef struct _BT_ESL_WRITE_SESSION_PL
{
    /**
     * \brief Parameters for GATT write operations.
     *
     * \par This structure holds the parameters for GATT write operations, including
     * the attribute handle and the data to be written.
     */
    struct bt_gatt_write_params * write_params;

    /**
     * \brief Buffer to accumulate write data fragments.
     *
     * \par This buffer stores the data received from multiple GATT write callbacks
     * until the complete attribute value is received.
     */
    UCHAR * buffer;

    /**
     * \brief Total length of accumulated data.
     *
     * \par Indicates how many bytes have been stored in the buffer so far.
     */
    UINT16 length;

    /**
     * \brief Flag indicating if write is in progress.
     *
     * \par Set to true when a write session is active and data is being accumulated.
     * Reset to false once the complete data has been received or the session ends.
     */
    UCHAR write_in_progress;

} BT_ESL_WRITE_SESSION_PL;

#if defined(CONFIG_BT_OTS)
/**
 * \brief OTS upload parameters
 *
 * \par This structure defines the parameters for uploading an image using OTS.
 */
typedef struct _OTS_UPLOAD_PARAMS
{
    /* Object ID to upload */
    UCHAR         obj_id;
    /**
     * Image data buffer
     * (static or allocated(if allocated to be freed in callback))
     */
    const UCHAR * image_data;
    /* Length of the image data */
    UINT32        image_len;
    /* Current upload state */
    UCHAR          active;
} OTS_UPLOAD_PARAMS;

/**
 * \brief OTS discovery session
 *
 * \par This structure holds the parameters for an OTS discovery session.
 */
typedef struct _OTS_DISCOVERY_SESSION
{
    /** disc State */
    atomic_t state;

    /** Discovery in progress */
    UCHAR discovery_in_progress;

    /** Discovery parameters - must persist across function calls */
    struct bt_gatt_discover_params discover_params;

} OTS_DISCOVERY_SESSION;
#endif /* CONFIG_BT_OTS */

typedef struct _DIS_DISCOVERY_SESSION
{
    /** disc State */
    atomic_t state;

    /** Discovery in progress */
    UCHAR discovery_in_progress;

    /** Discovery parameters - must persist across function calls */
    struct bt_gatt_discover_params discover_params;

} DIS_DISCOVERY_SESSION;

#endif /* BT_ESL_SUPPORT_AP_ROLE */

#ifdef BT_ESL_SUPPORT_TAG_ROLE
/**
 * \brief OTS object creation structure
 */
typedef struct _OBJECT_CREATION_DATA
{
    /** Name of the object */
    CHAR * name;

    /** Size of the object */
    struct bt_ots_obj_size size;

    /** Properties of the object */
    UINT32 props;

} OBJECT_CREATION_DATA;

/**
 * \brief NTF params
 */
typedef struct _BT_ESL_NTF_PARAMS_PL
{
    struct bt_conn *conn;

    struct bt_gatt_notify_params params;

} BT_ESL_NTF_PARAMS_PL;
#endif /* BT_ESL_SUPPORT_TAG_ROLE */

/**
 * \brief bonding info
 *
 * \par This structure holds information about the bonding status of a
 * Bluetooth Low Energy (BLE) device.
 */
typedef struct _BT_ESL_BONDING_INFO_PL
{
    /** BD address of the bonded device */
    BT_ESL_BD_ADDR bd_addr;

    /** Flag indicating if the device is bonded (1: bonded, 0: not bonded). */
    UCHAR bonded;

} BT_ESL_BONDING_INFO_PL;

/* --------------------------------------------- Static Global Variables */

/* PL init complete CB */
static PL_INIT_COMPLETE_CB pl_ready_cb = NULL;

/* PL MTU exchange CB */
static PL_MTU_EXCHANGE_COMPLETE_CB mtu_exchange_complete_cb = NULL;

/* Flag - whether ESL pl is initialized */
DECL_STATIC UCHAR esl_pl_init_state = BT_ESL_STACK_INIT_UNDEFINED;

/* Anchor to hold time difference */
static int64_t anchor_time;

/* MTU exchange params */
static struct bt_gatt_exchange_params mtu_exchange_params;

/** SMP info callback */
struct bt_conn_auth_info_cb smp_cb;

/** SMP bonded info */
static BT_ESL_BONDING_INFO_PL smp_bonded_info;

#ifdef BT_ESL_SUPPORT_AP_ROLE
/* Handles used for discovery */
static BT_ESL_ATTR_HANDLE_PL attr_handles[CONFIG_BT_MAX_CONN];

/* Buffers used for read sessions */
static BT_ESL_READ_SESSION_PL read_sessions[CONFIG_BT_MAX_CONN];

/* Buffers used for write sessions */
static BT_ESL_WRITE_SESSION_PL write_sessions[CONFIG_BT_MAX_CONN];

/* PAWR adv set */
static struct bt_le_ext_adv * adv_pawr;

/* PAWR parameters */
static BT_ESL_PERIODIC_ADV_PARAMS pawr_params;

#ifdef APPL_ESL_DO_NOT_USE_DEFAULT_CONN_PARAMS
/* Preferred connection parameters - initialized with BT_LE_CONN_PARAM_DEFAULT values */
static struct bt_le_conn_param preferred_conn_param =
    BT_LE_CONN_PARAM_INIT(BT_GAP_INIT_CONN_INT_MIN, BT_GAP_INIT_CONN_INT_MAX, 0, 400);
#endif /* APPL_ESL_DO_NOT_USE_DEFAULT_CONN_PARAMS */

#if defined(CONFIG_BT_OTS)
/** OTS Client related static */
static struct bt_ots_client ots_client[CONFIG_BT_MAX_CONN];
static OTS_UPLOAD_PARAMS ots_upload_params;
static BT_ESL_OTS_CALLBACK ots_callback;
static OTS_DISCOVERY_SESSION ots_discovery_session[CONFIG_BT_MAX_CONN];
static struct bt_ots_client_cb ots_client_cbs;
#endif /* CONFIG_BT_OTS */

static DIS_DISCOVERY_SESSION dis_discovery_session[CONFIG_BT_MAX_CONN];
#endif /* BT_ESL_SUPPORT_AP_ROLE */

#ifdef BT_ESL_SUPPORT_TAG_ROLE
struct bt_data ad[] =
    {
        BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
        BT_DATA_BYTES(BT_DATA_UUID16_SOME, BT_UUID_16_ENCODE(BT_ESL_GATT_ESL_SERVICE)),
    };
/** Default device name  */
#define BT_ESL_DEFAULT_DEVICE_NAME CONFIG_BT_DEVICE_NAME

/** Max device name length */
#define BT_ESL_MAX_DEVICE_NAME_LEN 20U

/** Control point NTF timer interval */
#define BT_ESL_CP_NTF_INTERVAL (BT_ESL_TIMEOUT_MILLISEC | 100U)

/* Control point NTF timer handle */
static BT_ESL_TIMER_HANDLE cp_ntf_timer_handle;

/* Appl OTS server callback */
static BT_ESL_OTS_SERVER_CALLBACK ots_server_callback;
/* OTS server instance */
static struct bt_ots * ots_server;
/* OTS server Callbacks */
static struct bt_ots_cb ots_callbacks;
/* OTS server local object being created */
static OBJECT_CREATION_DATA * object_being_created;
/* OTS server number of objects */
static UCHAR number_of_objects = 0u;

static struct bt_le_ext_adv * ext_adv_tag;
static struct bt_le_per_adv_sync_cb sync_callbacks;
static uint8_t sync_handle = 0xFF;
static ssize_t esl_address_write_handler_pl
               (
                    struct bt_conn            * conn,
                    const struct bt_gatt_attr * attr,
                    const void                * buf,
                    uint16_t                    len,
                    uint16_t                    offset,
                    uint8_t                     flags
               );
static ssize_t sync_key_write_handler_pl
               (
                    struct bt_conn            * conn,
                    const struct bt_gatt_attr * attr,
                    const void                * buf,
                    uint16_t                    len,
                    uint16_t                    offset,
                    uint8_t                     flags
               );
static ssize_t response_key_write_handler_pl
               (
                    struct bt_conn            * conn,
                    const struct bt_gatt_attr * attr,
                    const void                * buf,
                    uint16_t                    len,
                    uint16_t                    offset,
                    uint8_t                     flags
               );
static ssize_t current_abs_time_write_handler_pl
               (
                    struct bt_conn            * conn,
                    const struct bt_gatt_attr * attr,
                    const void                * buf,
                    uint16_t                    len,
                    uint16_t                    offset,
                    uint8_t                     flags
               );
static ssize_t control_point_write_handler_pl
               (
                    struct bt_conn            * conn,
                    const struct bt_gatt_attr * attr,
                    const void                * buf,
                    uint16_t                    len,
                    uint16_t                    offset,
                    uint8_t                     flags
               );
static ssize_t control_point_cccd_write
               (
                   struct bt_conn *conn,
                   const struct bt_gatt_attr *attr,
                   uint16_t value
               );
static ssize_t display_info_read_handler
               (
                    struct bt_conn *conn,
                    const struct bt_gatt_attr *attr,
                    void * buf,
                    uint16_t len,
                    uint16_t offset
               );
static ssize_t image_info_read_handler
               (
                    struct bt_conn *conn,
                    const struct bt_gatt_attr *attr,
                    void * buf,
                    uint16_t len,
                    uint16_t offset
               );
static ssize_t led_info_read_handler
               (
                    struct bt_conn *conn,
                    const struct bt_gatt_attr *attr,
                    void * buf,
                    uint16_t len,
                    uint16_t offset
               );
static ssize_t sensor_info_read_handler
               (
                    struct bt_conn *conn,
                    const struct bt_gatt_attr *attr,
                    void * buf,
                    uint16_t len,
                    uint16_t offset
                );

/** ESL database declaration */

/** ESL CCCD */
static struct _bt_gatt_ccc cp_ccc =
    BT_GATT_CCC_INITIALIZER(NULL, control_point_cccd_write, NULL);

/** ESL attributes  */
static struct bt_gatt_attr esl_serv_attrs[] =
{
    /* ESL Primary Service Declaration */
    BT_GATT_PRIMARY_SERVICE
    (
        BT_UUID_DECLARE_16(BT_ESL_GATT_ESL_SERVICE)
    ),

    BT_GATT_CHARACTERISTIC
    (
        BT_UUID_DECLARE_16(GATT_ESL_ADDRESS_CHARACTERISTIC),
        BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_WRITE_ENCRYPT | BT_GATT_PERM_PREPARE_WRITE,
        NULL,
        esl_address_write_handler_pl,
        NULL
    ),

    BT_GATT_CHARACTERISTIC
    (
        BT_UUID_DECLARE_16(GATT_AP_SYNC_KEY_MATERIAL_CHARACTERISTIC),
        BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_WRITE_ENCRYPT | BT_GATT_PERM_PREPARE_WRITE,
        NULL,
        sync_key_write_handler_pl,
        NULL
    ),

    BT_GATT_CHARACTERISTIC
    (
        BT_UUID_DECLARE_16(GATT_ESL_RESPONSE_KEY_MATERIAL_CHARACTERISTIC),
        BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_WRITE_ENCRYPT | BT_GATT_PERM_PREPARE_WRITE,
        NULL,
        response_key_write_handler_pl,
        NULL
    ),

    BT_GATT_CHARACTERISTIC
    (
        BT_UUID_DECLARE_16(GATT_ESL_CURRENT_ADSOLUTE_TIME_CHARACTERISTIC),
        BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_WRITE_ENCRYPT | BT_GATT_PERM_PREPARE_WRITE,
        NULL,
        current_abs_time_write_handler_pl,
        NULL
    ),

#ifdef BT_ESL_SUPPORTS_DISPLAYS
    BT_GATT_CHARACTERISTIC
    (
        BT_UUID_DECLARE_16(GATT_ESL_DISPLAY_INFORMATION_CHARACTERISTIC),
        BT_GATT_CHRC_READ,
        BT_GATT_PERM_READ_ENCRYPT,
        display_info_read_handler,
        NULL,
        NULL
    ),

    BT_GATT_CHARACTERISTIC
    (
        BT_UUID_DECLARE_16(GATT_ESL_IMAGE_INFORMATION_CHARACTERISTIC),
        BT_GATT_CHRC_READ,
        BT_GATT_PERM_READ_ENCRYPT,
        image_info_read_handler,
        NULL,
        NULL
    ),
#endif /* BT_ESL_SUPPORTS_DISPLAYS */

#ifdef BT_ESL_SUPPORTS_SENSORS
    BT_GATT_CHARACTERISTIC
    (
        BT_UUID_DECLARE_16(GATT_ESL_SENSOR_INFORMATION_CHARACTERISTIC),
        BT_GATT_CHRC_READ,
        BT_GATT_PERM_READ_ENCRYPT,
        sensor_info_read_handler,
        NULL,
        NULL
    ),
#endif /* BT_ESL_SUPPORTS_SENSORS */

#ifdef BT_ESL_SUPPORTS_LEDS
    BT_GATT_CHARACTERISTIC
    (
        BT_UUID_DECLARE_16(GATT_ESL_LED_INFORMATION_CHARACTERISTIC),
        BT_GATT_CHRC_READ,
        BT_GATT_PERM_READ_ENCRYPT,
        led_info_read_handler,
        NULL,
        NULL
    ),
#endif /* BT_ESL_SUPPORTS_LEDS */

    BT_GATT_CHARACTERISTIC
    (
        BT_UUID_DECLARE_16(GATT_ESL_CONTROL_POINT_CHARACTERISTIC),
        BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_WRITE_ENCRYPT,
        NULL,
        control_point_write_handler_pl,
        NULL
    ),
    BT_GATT_CCC_MANAGED
    (
        &cp_ccc,
        BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT
    ),
};

static struct bt_gatt_service esl_svc = BT_GATT_SERVICE(esl_serv_attrs);
#endif /* BT_ESL_SUPPORT_TAG_ROLE */

/* --------------------------------------------- Global Variables */

#define BT_ESL_PL_TRANSLATE_ATT_ERROR(dst, src)                     \
        if (BT_ESL_ATT_PARAM_VAL_NOT_ALLOWED == src)                \
        {                                                           \
            dst = BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);        \
        }                                                           \
        else if (BT_ESL_ATT_INVALID_OFFSET == src)                  \
        {                                                           \
            dst = BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);           \
        }                                                           \
        else if (BT_ESL_ATT_INVALID_ATTRIBUTE_LEN == src)           \
        {                                                           \
            dst = BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);    \
        }                                                           \
        else                                                        \
        {                                                           \
            dst = BT_GATT_ERR(BT_ATT_ERR_SUCCESS);                  \
        }

/** Reverse a byte stream */
#define bt_esl_reverse_bytestream_pl(d, s, l) sys_memcpy_swap((d), (s), (l))

/* --------------------------------------------- Functions */
static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);
void mtu_exchange_cb
     (
         struct bt_conn *conn,
         uint8_t err,
         struct bt_gatt_exchange_params *params
     );
void esl_register_auth_info_cb(void);
/* Connection/disconnection callbacks */
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

#ifdef BT_ESL_SUPPORT_AP_ROLE
static void request_cb
            (
                 struct bt_le_ext_adv *adv,
                 const struct bt_le_per_adv_data_request *request
            );
static void response_cb(struct bt_le_ext_adv *adv, struct bt_le_per_adv_response_info *info,
            struct net_buf_simple *buf);
/* PAWR advertiser callbacks */
static const struct bt_le_ext_adv_cb adv_cb =
{
#ifdef CONFIG_BT_PER_ADV_RSP
    .pawr_data_request = request_cb,
    .pawr_response = response_cb,
#endif /* CONFIG_BT_PER_ADV_RSP */
};
/* API to check ESL service is in ADV data */
UCHAR BT_esl_check_esl_uuid_in_adv_pl(UCHAR * adv_data, UINT8 adv_length);
static UCHAR find_uuid_in_adv_data(UCHAR ad_type, UCHAR * adv_data, UINT8 adv_length);
void BT_esl_ots_client_init_pl(void);
#endif /* BT_ESL_SUPPORT_AP_ROLE */
#ifdef BT_ESL_SUPPORT_TAG_ROLE
static void esl_setup_pawr_sync(void);
#endif /* BT_ESL_SUPPORT_TAG_ROLE */

/**
 * \brief Callback for Bluetooth stack initialization.
 *
 * \par This function is called when the Bluetooth stack initialization completes, either
 * successfully or with an error. It logs the result and informs the upper layer through
 * the registered callback.
 *
 * \param err Error code indicating the result of the initialization:
 *            - 0: Initialization was successful.
 *            - Non-zero: Initialization failed.
 */
static void bt_ready(int err)
{
    char addr_s[BT_ADDR_LE_STR_LEN];
    bt_addr_le_t addr = {0};
    size_t count = 1;

    if (err)
    {
        ESL_PL_ERR ("[ESL PL]: Bluetooth init failed (err %d)", err);
        /* Inform upper layer */
        if (pl_ready_cb)
        {
            esl_pl_init_state = BT_ESL_STACK_INIT_UNDEFINED;
            pl_ready_cb(BT_ESL_API_FAILURE, &err);
        }
        return;
    }
    else
    {
        /* Update initialization state */
        esl_pl_init_state = BT_ESL_STACK_INIT_ESL_INIT;
    }

    ESL_PL_TRC ("[ESL PL]: Bluetooth initialized");

    bt_id_get(&addr, &count);

    if (count == 0)
    {
        ESL_PL_ERR("[ESL PL]: No valid Bluetooth address found");
        /* Inform upper layer */
        if (pl_ready_cb)
        {
            esl_pl_init_state = BT_ESL_STACK_INIT_UNDEFINED;
            pl_ready_cb(BT_ESL_API_FAILURE, &err);
        }
        return;
    }

    bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));
    ESL_PL_TRC ("[ESL PL]: Local BD address %s", addr_s);

    /* Load settings */
    err = settings_load();
    if (err)
    {
        ESL_PL_ERR("[ESL PL]: Settings load failed (err %d)", err);
        /* Inform upper layer */
        if (NULL != pl_ready_cb)
        {
            esl_pl_init_state = BT_ESL_STACK_INIT_UNDEFINED;
            pl_ready_cb(BT_ESL_API_FAILURE, &err);
        }
        return;
    }
    else
    {
        ESL_PL_TRC("[ESL PL]: Settings loaded successfully");
    }

#ifdef BT_ESL_SUPPORT_TAG_ROLE
    err = bt_gatt_service_register(&esl_svc);
    if (0 != err)
    {
        ESL_PL_ERR ("[ESL PL]: error in registering database");
        /* Inform upper layer */
        if (NULL != pl_ready_cb)
        {
            esl_pl_init_state = BT_ESL_STACK_INIT_UNDEFINED;
            pl_ready_cb(BT_ESL_API_FAILURE, &err);
        }
        return;
    }
    else
    {
        ESL_PL_TRC ("[ESL PL]: database registration success");
    }
#endif /* BT_ESL_SUPPORT_TAG_ROLE */

    /** Register SMP authentication info callbacks */
    esl_register_auth_info_cb();
    /** Initialize bonding info */
    BT_ESL_mem_set(&smp_bonded_info, 0, sizeof(BT_ESL_BONDING_INFO_PL));
    /** Timer Initialization */
    EM_timer_init();
    timer_em_init();

    /* Initialize globals */
    anchor_time = 0;
#ifdef BT_ESL_SUPPORT_AP_ROLE
    adv_pawr = NULL;
    BT_ESL_mem_set(attr_handles, 0, sizeof(attr_handles));
    BT_ESL_mem_set(&pawr_params, 0, sizeof(pawr_params));
    BT_ESL_mem_set(read_sessions, 0, sizeof(read_sessions));
    BT_ESL_mem_set(write_sessions, 0, sizeof(write_sessions));
#if defined(CONFIG_BT_OTS)
    BT_esl_ots_client_init_pl();
#endif /* CONFIG_BT_OTS */

    mtu_exchange_params.func = mtu_exchange_cb;
#endif /* BT_ESL_SUPPORT_AP_ROLE */
#ifdef BT_ESL_SUPPORT_TAG_ROLE
    ext_adv_tag = NULL;
    /* Update default device name */
    BT_esl_update_complete_name_pl(BT_ESL_DEFAULT_DEVICE_NAME, sizeof(BT_ESL_DEFAULT_DEVICE_NAME) - 1);
    esl_setup_pawr_sync();
#endif /* BT_ESL_SUPPORT_TAG_ROLE */

    ESL_PL_TRC("[ESL PL]: ESL PL is initialized");

    /* Inform upper layer */
    if (pl_ready_cb)
    {
        pl_ready_cb(BT_ESL_API_SUCCESS, &err);
    }

    return;
}

/**
 * \brief Initializes the ESL platform layer.
 *
 * \par This function initializes the ESL platform layer and starts the Bluetooth stack.
 *
 * \param cb Callback function to notify the upper layer about the initialization result.
 *
 * \return BT_ESL_API_SUCCESS if the initialization starts successfully, BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_init_pl(PL_INIT_COMPLETE_CB cb)
{
    API_RESULT retval;
    int err;

    ESL_PL_TRC ("[ESL PL]: -> BT_esl_init_pl");

    /* Init */
    retval = BT_ESL_API_SUCCESS;


    /* Check Stack is already initialized */
    if (BT_ESL_STACK_INIT_UNDEFINED != esl_pl_init_state)
    {
        ESL_PL_ERR("[ESL PL]: ESL AP is already initialized");
        retval = BT_ESL_API_FAILURE;
    }

    if (BT_ESL_API_SUCCESS == retval)
    {
        /* Store callback */
        pl_ready_cb = cb;

        /* Initialize Bluetooth stack */
        err = bt_enable(bt_ready);
        if (err)
        {
            ESL_PL_ERR("[ESL PL]: Bluetooth init failed (err %d)", err);
            pl_ready_cb = NULL;
            retval = BT_ESL_API_FAILURE;
        }

        ESL_PL_TRC("[ESL PL]: Bluetooth stack initialization started");
    }

    ESL_PL_TRC ("[ESL PL]: <- BT_esl_init_pl");
    return retval;
}

/**
 * \brief Callback for handling connection events.
 *
 * \par This function is called when a device connects. It processes the connection
 * event and performs necessary initialization.
 *
 * \param conn Pointer to the connection object associated with the connection.
 * \param err Error code indicating the result of the connection.
 */
static void connected(struct bt_conn *conn, uint8_t err)
{
    uint8_t conn_idx;
    BT_ESL_BD_ADDR bd_addr;

    /* Get connection index */
    conn_idx = bt_conn_index(conn);

    if (conn != NULL)
    {
        /* Init ESL attribute handles */
#ifdef BT_ESL_SUPPORT_AP_ROLE
        BT_ESL_AP_INIT_ESL_ATTR_HANDLES(&attr_handles[conn_idx].attr_handles);
        attr_handles[conn_idx].discovery_in_progress = BT_ESL_FALSE;
#endif /* BT_ESL_SUPPORT_AP_ROLE */
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)bt_conn_get_dst(conn)->a.val);
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

        ESL_PL_TRC (
        "[ESL PL]: Connection complete received for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER" (0x%02X)",
        BT_ESL_DEVICE_ADDR_PRINT_STR(&bd_addr), err);

        /* Callback to core */
#ifdef BT_ESL_SUPPORT_AP_ROLE
        BT_esl_ap_connection_handler((UCHAR)err, &bd_addr);
        if (0U == err)
        {
            (BT_ESL_IGNORE_RETURN_VALUE) BT_esl_start_pairing_pl(&bd_addr);
        }
#endif /* BT_ESL_SUPPORT_AP_ROLE */
#ifdef BT_ESL_SUPPORT_TAG_ROLE
        BT_esl_tag_hci_connection_complete_handler((UCHAR)err, &bd_addr);
#endif /* BT_ESL_SUPPORT_TAG_ROLE */
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Connection complete received with NULL conn");
    }
}

/**
 * \brief Callback for handling disconnection events.
 *
 * \par This function is called when a device disconnects. It processes the disconnection
 * event and performs necessary cleanup.
 *
 * \param conn Pointer to the connection object associated with the disconnection.
 * \param reason Reason code for the disconnection.
 */
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    BT_ESL_BD_ADDR  peer_addr;
    uint8_t         conn_idx;

    /* Get connection index */
    conn_idx = bt_conn_index(conn);

    if (conn != NULL)
    {
        BT_ESL_COPY_BD_ADDR(peer_addr.addr, bt_conn_get_dst(conn)->a.val);
        BT_ESL_COPY_TYPE(peer_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

        ESL_PL_TRC (
        "[ESL PL]: Disconnection complete received for "
        BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER" (reason 0x%02X)",
        BT_ESL_DEVICE_ADDR_PRINT_STR(&peer_addr), reason);

        /* Callback to core */
#ifdef BT_ESL_SUPPORT_AP_ROLE
        BT_esl_ap_disconnection_handler(&peer_addr, (UCHAR)reason);
#endif /* BT_ESL_SUPPORT_AP_ROLE */
#ifdef BT_ESL_SUPPORT_TAG_ROLE
        BT_esl_tag_disconnect_complete_handler(&peer_addr, (UCHAR)reason);
#endif /* BT_ESL_SUPPORT_TAG_ROLE */
    }
}

/**
 * \brief Creates a connection with a specified device.
 *
 * \par This function initiates a connection with the specified device using the provided
 * Bluetooth address.
 *
 * \param peer_addr Pointer to the Bluetooth address of the device to connect.
 *
 * \return BT_ESL_API_SUCCESS if the connection starts successfully, BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_create_connection_pl(BT_ESL_BD_ADDR * bd_addr)
{
    API_RESULT retval;
    int err;
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

    if (bd_addr != NULL)
    {
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)bd_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)bd_addr->type);

        /* Stop any ongoing scan before creating connection */
        bt_le_scan_stop();

#ifndef APPL_ESL_DO_NOT_USE_DEFAULT_CONN_PARAMS
        err = bt_conn_le_create(&conn_addr, BT_CONN_LE_CREATE_CONN,
                    BT_LE_CONN_PARAM_DEFAULT, &conn);
#else /* APPL_ESL_DO_NOT_USE_DEFAULT_CONN_PARAMS */
        err = bt_conn_le_create(&conn_addr, BT_CONN_LE_CREATE_CONN,
                    &preferred_conn_param, &conn);
#endif /* APPL_ESL_DO_NOT_USE_DEFAULT_CONN_PARAMS */
        if (err) {
            ESL_PL_ERR (
            "[ESL PL]: Create connection failed "
            BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER" (err %d)",
            BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr), err);
            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            bt_conn_unref(conn);
        }
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Invalid peer address");
        retval = BT_ESL_API_FAILURE;
    }

    return retval;

}

#ifdef APPL_ESL_DO_NOT_USE_DEFAULT_CONN_PARAMS
/**
 * \brief Set preferred connection interval for LE connections.
 *
 * \par This function updates the preferred connection interval (min and max)
 * used by BT_esl_create_connection_pl(). The interval is in units of 1.25 ms.
 *
 * \param interval_min  Minimum connection interval (in units of 1.25 ms).
 * \param interval_max  Maximum connection interval (in units of 1.25 ms).
 */
void BT_esl_set_preferred_conn_interval_pl(UINT16 interval_min, UINT16 interval_max)
{
    preferred_conn_param.interval_min = interval_min;
    preferred_conn_param.interval_max = interval_max;

    ESL_PL_TRC(
    "[ESL PL]: Preferred conn interval updated: min=0x%04X (%d ms), max=0x%04X (%d ms)",
    interval_min, (int)(interval_min * 1.25),
    interval_max, (int)(interval_max * 1.25));
}
#endif /* APPL_ESL_DO_NOT_USE_DEFAULT_CONN_PARAMS */

#ifdef BT_ESL_SUPPORT_AP_ROLE
/**
 * \brief Initiates a connection using PAwR.
 *
 * \par This function starts the connection procedure using PAwR with the specified device.
 *
 * \param peer_addr Pointer to the Bluetooth address of the device to connect.
 *
 * \return BT_ESL_API_SUCCESS if the connection starts successfully, BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_pawr_connect_pl(BT_ESL_BD_ADDR * bd_addr, UCHAR subevent)
{
    API_RESULT retval;
    int err;
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;
    struct bt_conn_le_create_synced_param synced_param;
    struct bt_le_conn_param conn_param;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

    if (bd_addr != NULL)
    {
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)bd_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)bd_addr->type);

        synced_param.peer = &conn_addr;
        synced_param.subevent = subevent;

        /* Choose same interval as PAwR advertiser to avoid scheduling conflicts */
        conn_param.interval_min = pawr_params.subevent_interval;
        conn_param.interval_max = pawr_params.subevent_interval;

        /* Default values */
        conn_param.latency = 0;
        conn_param.timeout = 400;

        err = bt_conn_le_create_synced(adv_pawr, &synced_param, &conn_param, &conn);
        if (err)
        {
            ESL_PL_TRC (
            "[ESL PL]: Create connection failed "
            BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER" (%d)",
            BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr), err);
            retval = BT_ESL_API_FAILURE;
        }

        bt_conn_unref(conn);
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Invalid peer address");
        retval = BT_ESL_API_FAILURE;
    }

    return retval;
}
#endif /* BT_ESL_SUPPORT_AP_ROLE */

/**
 * \brief Disconnects from a connected device.
 *
 * \par This function initiates a disconnection from the specified device.
 *
 * \param peer_addr Pointer to the Bluetooth address of the device to disconnect.
 *
 * \return BT_ESL_API_SUCCESS if the disconnection starts successfully, BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_disconnect_pl(BT_ESL_BD_ADDR * bd_addr)
{
    API_RESULT retval;
    int err;
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

    if (bd_addr != NULL)
    {
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)bd_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)bd_addr->type);

        /* Get conn param */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);

        if (conn != NULL)
        {
            err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            if (err)
            {
                ESL_PL_TRC (
                "[ESL PL]: Disconnection failed "
                BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER" (%d)",
                BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr), err);
                retval = BT_ESL_API_FAILURE;
            }

            /* Release the reference after starting discovery */
            bt_conn_unref(conn);
        }
        else
        {
            ESL_PL_ERR("[ESL PL]: Connection not found");
            retval = BT_ESL_API_FAILURE;
        }
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Invalid peer address");
        retval = BT_ESL_API_FAILURE;
    }

    return retval;

}

/**
 * \brief Callback for MTU exchange operation.
 *
 * \par This function is called when the MTU exchange operation completes, either successfully
 * or with an error. It logs the result of the MTU exchange and retrieves the negotiated MTU size.
 * If a callback is registered by the upper layer, it invokes the callback to inform the upper layer.
 *
 * \param conn Pointer to the connection object associated with the MTU exchange.
 * \param err Error code indicating the result of the MTU exchange:
 *            - 0: MTU exchange was successful.
 *            - Non-zero: MTU exchange failed.
 * \param params Pointer to the MTU exchange parameters (unused in this implementation).
 */
void mtu_exchange_cb
     (
         struct bt_conn *conn,
         uint8_t err,
         struct bt_gatt_exchange_params *params
     )
{
    uint16_t mtu;
    BT_ESL_BD_ADDR peer_addr;

    /* Retrieve the negotiated MTU size */
    mtu = bt_gatt_get_mtu(conn);

    /* Copy the peer address */
    BT_ESL_COPY_BD_ADDR(peer_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
    BT_ESL_COPY_TYPE(peer_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

    if (err == 0U)
    {
        ESL_PL_TRC("[ESL PL]: MTU exchange successful, negotiated MTU: %u", mtu);
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: MTU exchange failed (err %u), default MTU: %u", err, mtu);
    }

    /* Inform the upper layer if a callback is registered */
    if (NULL != mtu_exchange_complete_cb)
    {
        mtu_exchange_complete_cb(&peer_addr, mtu);
    }
}

/**
 * \brief Initiates an MTU exchange with a connected device.
 *
 * \par This function initiates the GATT MTU exchange procedure for a device specified
 * by its Bluetooth address.
 *
 * \param peer_addr Pointer to the Bluetooth address of the device.
 * \param mtu Ignored parameter (MTU negotiation is handled automatically).
 * \param cb Callback function to inform the upper layer about the MTU exchange result.
 *
 * \return BT_ESL_API_SUCCESS if the MTU exchange starts successfully, BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_gatt_exchange_mtu_pl
           (
                BT_ESL_BD_ADDR             * peer_addr,
                UINT16                       mtu,
                PL_MTU_EXCHANGE_COMPLETE_CB  cb
           )
{
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;
    int err;
    API_RESULT retval;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

    ESL_PL_TRC("[ESL PL]: -> BT_esl_gatt_exchange_mtu_pl");

    if (peer_addr == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Invalid peer address");
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        /* Store the callback */
        mtu_exchange_complete_cb = cb;

        /* Copy peer address */
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)peer_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)peer_addr->type);

        /* Get connection object */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);

        if (conn != NULL)
        {
            /* Perform MTU exchange */
            err = bt_gatt_exchange_mtu(conn, &mtu_exchange_params);
            /* Release the reference after the operation */
            bt_conn_unref(conn);

            if (err)
            {
                ESL_PL_ERR("[ESL PL]: MTU exchange failed (err %d)", err);
                retval = BT_ESL_API_FAILURE;
            }
            else
            {
                ESL_PL_TRC("[ESL PL]: MTU exchange successful");
            }
        }
        else
        {
            ESL_PL_ERR("[ESL PL]: Connection not found for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
                    BT_ESL_DEVICE_ADDR_PRINT_STR(peer_addr));
            retval = BT_ESL_API_FAILURE;
        }
    }

    ESL_PL_TRC("[ESL PL]: <- BT_esl_gatt_exchange_mtu_pl");
    return retval;
}

/**
 * \brief Initiates pairing/bonding with a connected device.
 *
 * \par This function initiates the SMP pairing procedure with the specified device
 * to establish a bonded connection for secure communication.
 *
 * \param peer_addr Pointer to the Bluetooth address of the device to pair with.
 *
 * \return BT_ESL_API_SUCCESS if pairing starts successfully, BT_ESL_API_FAILURE otherwise.
 *
 * \note Here we are using security level 2 (BT_SECURITY_L2) for pairing required for ESL.
 */
API_RESULT BT_esl_start_pairing_pl(BT_ESL_BD_ADDR * peer_addr)
{
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;
    int err;
    API_RESULT retval;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

    ESL_PL_TRC("[ESL PL]: -> BT_esl_start_pairing_pl");

    if (peer_addr == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Invalid peer address");
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        /* Copy peer address */
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)peer_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)peer_addr->type);

        /* Get connection object */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);

        if (conn != NULL)
        {
            /* Check if already bonded */
            if (bt_conn_get_security(conn) >= BT_SECURITY_L2)
            {
                ESL_PL_TRC("[ESL PL]: Device already bonded");
                retval = BT_ESL_API_SUCCESS;
            }
            else
            {
                /* Initiate pairing/bonding - bondable mode already enabled globally */
                err = bt_conn_set_security(conn, BT_SECURITY_L2);
                if (-EBUSY == err)
                {
                    ESL_PL_TRC("[ESL PL]: Pairing procedure in progress");
                }
                else if (0U != err)
                {
                    ESL_PL_ERR("[ESL PL]: Failed to start pairing (err %d)", err);
                    retval = BT_ESL_API_FAILURE;
                }
                else
                {
                    ESL_PL_TRC("[ESL PL]: Pairing procedure initiated");
                }
            }

            /* Release the reference after the operation */
            bt_conn_unref(conn);
        }
        else
        {
            ESL_PL_ERR("[ESL PL]: Connection not found for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
                    BT_ESL_DEVICE_ADDR_PRINT_STR(peer_addr));
            retval = BT_ESL_API_FAILURE;
        }
    }

    ESL_PL_TRC("[ESL PL]: <- BT_esl_start_pairing_pl");
    return retval;
}

API_RESULT BT_esl_encrypt_data_pl
           (
               /* IN */  BT_ESL_KEY_MATERIAL* key_material,
               /* IN */  UCHAR * payload,
               /* IN */  UINT16  payload_datalen,
               /* OUT */ UCHAR * encrypted_data
           )
{
    API_RESULT retval;
    int err;
    BT_ESL_KEY_MATERIAL temp_key;

    ESL_PL_TRC("[ESL PL]: -> BT_esl_encrypt_data_pl");

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    /* Check if key material is valid */
    if (key_material == NULL || payload == NULL || encrypted_data == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Invalid parameters for encryption");
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        /**
         * \note:
         * SESSION KEY (Little-Endian to Big-Endian):
         * - Input: key_material->session_key comes in LITTLE-ENDIAN format from upper layer
         * - Process: bt_esl_reverse_bytestream_pl() converts it to BIG-ENDIAN
         * - Output: temp_key.session_key is passed to bt_ead_decrypt in BIG-ENDIAN format
         * - Example: Input LE [0x11,0x22,0x33,0x44] to Output BE [0x44,0x33,0x22,0x11]
         *
         * IV (INITIALIZATION VECTOR) - NO CONVERSION (Remains Little-Endian):
         * - Input: key_material->iv comes in LITTLE-ENDIAN format from upper layer
         * - Process: BT_ESL_mem_copy() preserves original byte order (no reversal)
         * - Output: temp_key.iv is passed to bt_ead_decrypt in LITTLE-ENDIAN format
         * - Example: Input LE [0xAA,0xBB,0xCC,0xDD] to Output LE [0xAA,0xBB,0xCC,0xDD]
         */
        /* Reverse the session keys */
        bt_esl_reverse_bytestream_pl(temp_key.session_key, key_material->session_key, BT_ESL_SESSION_KEY_LEN);
        BT_ESL_mem_copy(temp_key.iv, key_material->iv, BT_ESL_IV_LEN);

        /* Call the encryption function */
        err = bt_ead_encrypt
              (
                  (uint8_t *)temp_key.session_key,
                  (uint8_t *)temp_key.iv,
                  (uint8_t *)payload,
                  (size_t)payload_datalen,
                  (uint8_t *)encrypted_data
              );

        if (0 != err)
        {
            ESL_PL_ERR("[ESL PL]: Encryption failed (err %d)", err);
            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            ESL_PL_TRC("[ESL PL]: Encryption successful");
        }
    }

    ESL_PL_TRC("[ESL PL]: <- BT_esl_encrypt_data_pl");
    return retval;
}

API_RESULT BT_esl_decrypt_data_pl
           (
               /* IN */  BT_ESL_KEY_MATERIAL* key_material,
               /* IN */  UCHAR  * encrypted_payload,
               /* IN */  UINT16   encrypted_payload_datalen,
               /* OUT */ UCHAR  * decrypted_payload,
               /* IN */  UINT16   decrypted_payload_len
           )
{
    API_RESULT retval;
    int err;
    BT_ESL_KEY_MATERIAL temp_key;

    ESL_PL_TRC("[ESL PL]: -> BT_esl_decrypt_data_pl");

    /* Init */
    BT_ESL_IGNORE_UNUSED_PARAM (decrypted_payload_len);
    retval = BT_ESL_API_SUCCESS;

    /* Check if key material is valid */
    if (key_material == NULL || encrypted_payload == NULL || decrypted_payload == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Invalid parameters for decryption");
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        /**
         * \note:
         * SESSION KEY (Little-Endian to Big-Endian):
         * - Input: key_material->session_key comes in LITTLE-ENDIAN format from upper layer
         * - Process: bt_esl_reverse_bytestream_pl() converts it to BIG-ENDIAN
         * - Output: temp_key.session_key is passed to bt_ead_decrypt in BIG-ENDIAN format
         * - Example: Input LE [0x11,0x22,0x33,0x44] to Output BE [0x44,0x33,0x22,0x11]
         *
         * IV (INITIALIZATION VECTOR) - NO CONVERSION (Remains Little-Endian):
         * - Input: key_material->iv comes in LITTLE-ENDIAN format from upper layer
         * - Process: BT_ESL_mem_copy() preserves original byte order (no reversal)
         * - Output: temp_key.iv is passed to bt_ead_decrypt in LITTLE-ENDIAN format
         * - Example: Input LE [0xAA,0xBB,0xCC,0xDD] to Output LE [0xAA,0xBB,0xCC,0xDD]
         */
        /* Reverse the session keys */
        bt_esl_reverse_bytestream_pl(temp_key.session_key, key_material->session_key, BT_ESL_SESSION_KEY_LEN);
        BT_ESL_mem_copy(temp_key.iv, key_material->iv, BT_ESL_IV_LEN);
        /* Call the decryption function */
        err = bt_ead_decrypt
              (
                  (uint8_t *)temp_key.session_key,
                  (uint8_t *)temp_key.iv,
                  (uint8_t *)encrypted_payload,
                  (size_t)encrypted_payload_datalen,
                  (uint8_t *)decrypted_payload
              );

        if (0 != err)
        {
            ESL_PL_ERR("[ESL PL]: Decryption failed (err %d)", err);
            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            ESL_PL_TRC("[ESL PL]: Decryption successful");
        }
    }

    ESL_PL_TRC("[ESL PL]: <- BT_esl_decrypt_data_pl");
    return retval;
}

API_RESULT BT_esl_unpair_pl(BT_ESL_BD_ADDR * bd_addr)
{
    bt_addr_le_t conn_addr;
    int err;
    API_RESULT retval;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    ESL_PL_TRC("[ESL PL]: -> BT_esl_unpair_pl");

    if (bd_addr == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Invalid peer address");
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        /* Copy peer address */
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)bd_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)bd_addr->type);
#ifdef BT_ESL_SUPPORT_TAG_ROLE
        err = bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
#else /* BT_ESL_SUPPORT_TAG_ROLE */
        err = bt_unpair(BT_ID_DEFAULT, &conn_addr);
#endif /* BT_ESL_SUPPORT_TAG_ROLE */
        if (0 != err)
        {
            ESL_PL_ERR("[ESL PL]: Encryption failed (err %d)", err);
            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            ESL_PL_TRC("[ESL PL]: Unpair successful");
        }
    }

    ESL_PL_TRC("[ESL PL]: <- BT_esl_unpair_pl");

    return retval;
}

#ifdef BT_ESL_SUPPORT_AP_ROLE
/**
 * \brief Callback for handling device discovery during scanning.
 *
 * \par This function is called when a device is found during a scan. It processes the
 * discovered device's information and determines whether to connect or take other actions.
 *
 * \param addr Pointer to the Bluetooth address of the discovered device.
 * \param rssi Received Signal Strength Indicator (RSSI) of the discovered device.
 * \param adv_data Pointer to the advertising data of the discovered device.
 */
static void device_found
            (
                 const bt_addr_le_t *    addr,
                 int8_t                  rssi,
                 uint8_t                 type,
                 struct net_buf_simple * ad
            )
{
    BT_ESL_BD_ADDR bd_addr;

    /* We're only interested in connectable events */
    if (type != BT_GAP_ADV_TYPE_ADV_IND &&
        type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND &&
        type != BT_GAP_ADV_TYPE_EXT_ADV &&
        type != BT_GAP_ADV_TYPE_SCAN_RSP)
    {
        return;
    }

    BT_ESL_COPY_BD_ADDR(bd_addr.addr, addr->a.val);
    BT_ESL_COPY_TYPE(bd_addr.type, addr->type);

    if ((NULL == ad) || (ad->len == 0))
    {
        ESL_PL_ERR ("[ESL PL]: No Advertisement data");
        return;
    }
    /* Now you can pass adv_data and adv_length to your ESL UUID check API */
    if (BT_ESL_FALSE != BT_esl_check_esl_uuid_in_adv_pl((UCHAR *)ad->data, (UINT16)ad->len))
    {
        /* Callback to core */
        BT_esl_ap_adv_report_handler(&bd_addr, (UCHAR *)ad->data, (UINT16)ad->len);
    }
}

/**
 * \brief Checks if the ESL service UUID is present in the advertisement data.
 *
 * \par This function searches for the ESL service UUID in the advertisement data by
 * checking both incomplete and complete 16-bit UUIDs.
 *
 * \param adv_data Pointer to the advertisement data buffer.
 * \param adv_length Length of the advertisement data buffer.
 *
 * \return BT_ESL_TRUE if the ESL service UUID is found, BT_ESL_FALSE otherwise.
 */
UCHAR BT_esl_check_esl_uuid_in_adv_pl(UCHAR * adv_data, UINT8 adv_length)
{
    return find_uuid_in_adv_data(BT_ESL_HCI_EIR_DATA_TYPE_INCOMPLETE_16_BIT_UUIDS, adv_data, adv_length) ||
           find_uuid_in_adv_data(BT_ESL_HCI_EIR_DATA_TYPE_COMPLETE_16_BIT_UUIDS, adv_data, adv_length);
}

/**
 * \brief Searches for a specific UUID in the advertising data.
 *
 * \par This function iterates through the advertising data to find an element
 * matching the specified advertising type and checks if the UUID matches
 * the ESL service UUID.
 *
 * \param ad_type The advertising type to search for.
 * \param adv_data Pointer to the advertising data buffer.
 * \param adv_length Length of the advertising data buffer.
 * \return BT_ESL_TRUE if the UUID is found, otherwise BT_ESL_FALSE.
 */
static UCHAR find_uuid_in_adv_data(UCHAR ad_type, UCHAR * adv_data, UINT8 adv_length)
{
    API_RESULT retval;
    UINT32 i;
    UCHAR ad_element[BT_ESL_HCI_MAX_ADVERTISING_DATA_LENGTH];
    UINT16 ad_element_data_len;
    UINT16 uuid;

    retval = BT_esl_find_ad_element_pl(ad_type, adv_data, adv_length, ad_element, &ad_element_data_len);

    if (BT_ESL_API_SUCCESS == retval)
    {
        i = 0U;
        if ((ad_element_data_len % BT_ESL_PL_UUID_SIZE) != 0U)
        {
            ESL_PL_ERR("[ESL PL]: Invalid ad_element_data_len");
            return BT_ESL_FALSE;
        }

        while (ad_element_data_len >= BT_ESL_PL_UUID_SIZE)
        {
            BT_ESL_UNPACK_LE_2_BYTE(&uuid, &ad_element[i]);

            if (BT_ESL_GATT_ESL_SERVICE == uuid)
            {
                return BT_ESL_TRUE;
            }

            ad_element_data_len -= BT_ESL_PL_UUID_SIZE;
            i += BT_ESL_PL_UUID_SIZE;
        }
    }

    return BT_ESL_FALSE;
}
#endif /* BT_ESL_SUPPORT_AP_ROLE */

/**
 * \brief Finds an advertising element in the provided advertising data.
 *
 * \par This function searches for an advertising element of the specified type
 * within the given advertising data buffer and copies the element's data
 * into the provided buffer.
 *
 * \param ad_type The advertising type to search for.
 * \param adv_data Pointer to the advertising data buffer.
 * \param adv_length Length of the advertising data buffer.
 * \param ad_element Pointer to the buffer where the advertising element data will be copied.
 * \param ad_element_data_length Pointer to store the length of the advertising element data.
 *
 * \return BT_ESL_API_SUCCESS if the advertising element is found else BT_ESL_API_FAILURE.
 */
API_RESULT BT_esl_find_ad_element_pl
           (
                /* IN */  UCHAR     ad_type,
                /* IN */  UCHAR   * adv_data,
                /* IN */  UINT8     adv_length,
                /* OUT */ UCHAR   * ad_element,
                /* OUT */ UINT16  * ad_element_data_length
           )
{
    API_RESULT retval;
    UINT16  index;
    UINT8   total_length;

    /* Init */
    retval = BT_ESL_API_FAILURE;
    index = 0;

    if ((adv_data != NULL) && (adv_length != 0))
    {
        /* Iterate through the advertising data to find the specified element */
        while(index < adv_length)
        {
            /* Extract Length */
            total_length = adv_data[index];

            if((index + 1 < adv_length) && (adv_data[index + 1] == ad_type))
            {
                /* Copy the advertising element data into the provided buffer */
                BT_ESL_mem_copy(ad_element, &adv_data[index + 2], (total_length-1));
                /* Store the length of the advertising element data */
                *ad_element_data_length = (total_length - 1);
                retval = BT_ESL_API_SUCCESS;
                break;
            }

            /* Move to next AD element */
            index += (total_length + 1);
        }
    }

    return retval;
}

#ifdef BT_ESL_SUPPORT_AP_ROLE

/**
 * \brief Starts a passive BLE scan to discover devices.
 *
 * \par Stops any ongoing scan and starts a new passive scan.
 *
 * \return BT_ESL_API_SUCCESS if the scan starts successfully, BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_start_scan_pl(void)
{
    int err;
    API_RESULT retval;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    /* Stop any ongoing scan */
    bt_le_scan_stop();

    /* Start scan */
    err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
    if (err)
    {
        ESL_PL_ERR ("[ESL PL]: Scanning failed to start (err %d)", err);
        retval = BT_ESL_API_FAILURE;
    }

    ESL_PL_TRC ("[ESL PL]: Scanning successfully started");

    return retval;
}

/**
 * \brief Stops an ongoing BLE scan.
 *
 * \par This function stops any active BLE scan operation.
 *
 * \return BT_ESL_API_SUCCESS if the scan is stopped successfully, BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_stop_scan_pl(void)
{
    int err;
    API_RESULT retval;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    /* Stop scan */
    err = bt_le_scan_stop();
    if (err)
    {
        ESL_PL_ERR("[ESL PL]: Scanning failed to stop (err %d)", err);
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        ESL_PL_TRC("[ESL PL]: Scanning successfully stopped");
    }

    return retval;
}

/**
 * \brief Starts periodic advertising with the specified parameters.
 *
 * \par This function initializes and starts a periodic advertising set using the provided
 * parameters.
 *
 * \param padv_params Structure containing the periodic advertising parameters.
 *
 * \return BT_ESL_API_SUCCESS if periodic advertising starts successfully, BT_ESL_API_FAILURE otherwise.
 *
 * \note If periodic advertising is already started, the function will return BT_ESL_API_FAILURE.
 *       Ensure that the Bluetooth stack is initialized before calling this function.
 */
API_RESULT BT_esl_start_periodic_adv_pl(BT_ESL_PERIODIC_ADV_PARAMS padv_params)
{
    API_RESULT retval;
    int err;
    struct bt_le_per_adv_param per_adv_params;

    /* Init */
    retval = BT_ESL_API_SUCCESS;
    err = 0U;

    if (NULL == adv_pawr)
    {
        /* Store periodic adv parameters for future use */
        pawr_params = padv_params;

        /* Init periodic adv parameters */
        per_adv_params.interval_min = padv_params.periodic_advertising_interval_min;
        per_adv_params.interval_max = padv_params.periodic_advertising_interval_max;
        per_adv_params.options = padv_params.periodic_adv_prty;
#ifdef CONFIG_BT_PER_ADV_RSP
        per_adv_params.num_response_slots = padv_params.num_response_slots;
        per_adv_params.num_subevents = padv_params.num_subevents;
        per_adv_params.response_slot_delay = padv_params.response_slot_delay;
        per_adv_params.response_slot_spacing = padv_params.response_slot_spacing;
        per_adv_params.subevent_interval = padv_params.subevent_interval;
#else /* CONFIG_BT_PER_ADV_RSP */
        ESL_PL_ERR ("[ESL PL]: Periodic advertising with response not enabled");
#endif /* CONFIG_BT_PER_ADV_RSP */

        /* Create a non-connectable advertising set */
        err = bt_le_ext_adv_create(BT_LE_EXT_ADV_NCONN, &adv_cb, &adv_pawr);
        if (err)
        {
            ESL_PL_TRC ("[ESL PL]: Failed to create advertising set (err %d)", err);
            retval = BT_ESL_API_FAILURE;
        }

        /* Set periodic advertising parameters */
        if (BT_ESL_API_SUCCESS == retval)
        {
            err = bt_le_per_adv_set_param(adv_pawr, &per_adv_params);
            if (err)
            {
                ESL_PL_TRC ("[ESL PL]: Failed to set periodic advertising parameters (err %d)", err);
                retval = BT_ESL_API_FAILURE;
            }
        }

        /* Enable advertisement */
        if (BT_ESL_API_SUCCESS == retval)
        {
            err = bt_le_ext_adv_start(adv_pawr, BT_LE_EXT_ADV_START_DEFAULT);
            if (err)
            {
                ESL_PL_TRC ("[ESL PL]: Failed to start extended advertising (err %d)", err);
                retval = BT_ESL_API_FAILURE;
            }
        }

        /* Enable Periodic Advertising */
        if (BT_ESL_API_SUCCESS == retval)
        {
            err = bt_le_per_adv_start(adv_pawr);
            if (err)
            {
                ESL_PL_TRC ("[ESL PL]: Failed to enable periodic advertising (err %d)", err);
                /* Stop ext adv */
                (BT_ESL_IGNORE_RETURN_VALUE) bt_le_ext_adv_stop(adv_pawr);
                retval = BT_ESL_API_FAILURE;
            }
        }

        /* Cleanup */
        if ((BT_ESL_API_SUCCESS != retval) && (NULL != adv_pawr))
        {
            /* delete the advertising set */
            (BT_ESL_IGNORE_RETURN_VALUE) bt_le_ext_adv_delete(adv_pawr);
            adv_pawr = NULL;
        }
    }
    else
    {
        ESL_PL_TRC ("[ESL PL]: Periodic advertising already started");
        retval = BT_ESL_API_FAILURE;
    }

    return retval;
}

/**
 * \brief Stops periodic advertising and deletes the advertising set.
 *
 * \par This function stops both extended and periodic advertising for the current advertising set
 * and deletes the advertising set to free resources.
 *
 * \return BT_ESL_API_SUCCESS if periodic advertising is stopped and the advertising set is deleted successfully,
 *         BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_stop_periodic_adv_pl(void)
{
    API_RESULT retval;
    int err;

    /* Init */
    retval = BT_ESL_API_SUCCESS;
    err = 0U;

    err = bt_le_per_adv_stop(adv_pawr);
    if (err)
    {
        ESL_PL_TRC ("[ESL PL]: Failed to stop periodic advertising (err %d)", err);
        retval = BT_ESL_API_FAILURE;
    }

    err = bt_le_ext_adv_stop(adv_pawr);
    if (err)
    {
        ESL_PL_TRC ("[ESL PL]: Failed to stop extended advertising (err %d)", err);
        retval = BT_ESL_API_FAILURE;
    }

    if (BT_ESL_API_SUCCESS == retval)
    {
        /* Delete the advertising set */
        err = bt_le_ext_adv_delete(adv_pawr);
        if (err) {
            ESL_PL_TRC ("[ESL PL]: Failed to delete advertising set (err %d)", err);
            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            adv_pawr = NULL;
        }
    }

    /*
     * Demote only those Tags that are currently in BT_ESL_AP_SYNCHRONIZED
     * state to BT_ESL_AP_UNSYNCHRONIZED, since the periodic advertising
     * train has been stopped. Tags in any other state (e.g. CONNECTED,
     * CONFIGURING, UNASSOCIATE) are intentionally left untouched.
     * Passing NULL as the ESL Address performs this conditional update
     * across every valid tag entry in the tag table.
     */
    (void) BT_esl_ap_set_esl_tag_state
           (
               NULL,
               BT_ESL_AP_SYNCHRONIZED,
               BT_ESL_AP_UNSYNCHRONIZED
           );

    return retval;
}

/**
 * \brief Callback for handling subevent data requests from the peer device.
 *
 * \par This function processes the request subevent data received from the controller.
 *
 * \param adv Pointer to the ADV object.
 * \param Request Pointer to the request info.
 */
static void request_cb
            (
                struct bt_le_ext_adv *adv,
                const struct bt_le_per_adv_data_request *request
            )
{
    if ((NULL != adv) && (NULL != request))
    {
#ifdef BT_ESL_AP_HAVE_SUBEVENT_ADJUSTMENT
        BT_esl_ap_subevent_data_request_handler((UCHAR)request->start - 1, (UCHAR)request->count);
#else /* BT_ESL_AP_HAVE_SUBEVENT_ADJUSTMENT */
        BT_esl_ap_subevent_data_request_handler((UCHAR)request->start, (UCHAR)request->count);
#endif /* BT_ESL_AP_HAVE_SUBEVENT_ADJUSTMENT */
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Invalid parameters in request callback");
    }
}

/**
 * \brief Callback for handling periodic advertisement responses
 * from the peer device.
 *
 * \par This function processes the response received from the peer device
 * and inform upper layer.
 *
 * \param adv Pointer to the ADV object.
 * \param info Pointer to the response info.
 * \param buf Pointer to the response data.
 */
static void response_cb
            (
                struct bt_le_ext_adv *adv,
                struct bt_le_per_adv_response_info *info,
                struct net_buf_simple *buf
            )
{
    if ((NULL != adv) && (NULL != info)&& (NULL != buf))
    {
        BT_esl_ap_response_event_handler
        (
            (UCHAR)info->subevent,
            (UCHAR)info->response_slot,
            0U,
            (UCHAR *)buf->data,
            (UINT16)buf->len
        );
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Invalid parameters in response callback");
    }
}

/**
 * \brief Sets subevent data for periodic advertising.
 *
 * \par This function configures subevent data for periodic advertising.
 *
 * \param subevent_data_params Pointer to an array of subevent data parameters.
 * \param num_subevents Number of subevents to configure.
 *
 * \return BT_ESL_API_SUCCESS if subevent data is set successfully, BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_set_subevent_data_pl
           (
                BT_ESL_SUBEVENT_DATA_PARAMS * subevent_data_params,
                UCHAR                         num_subevents
           )
{
    API_RESULT retval;
    int err;
    struct bt_le_per_adv_subevent_data_params * sub_params;
    struct net_buf_simple * sub_data;

    /* Init */
    retval = BT_ESL_API_SUCCESS;
    err = 0;
    sub_data = NULL;
    sub_params = NULL;

    /* Allocate subevent parameters for zephyr */
    sub_params = (struct bt_le_per_adv_subevent_data_params *)BT_ESL_alloc_mem(num_subevents * sizeof(struct bt_le_per_adv_subevent_data_params));
    /* Allocate net bufs for nume of subevents */
    sub_data = (struct net_buf_simple *)BT_ESL_alloc_mem(num_subevents * sizeof(struct net_buf_simple));

    if ((NULL != sub_params) && (NULL != sub_data))
    {
        /* Init subevent data params */
        for (UCHAR i = 0; i < num_subevents; i++)
        {
            sub_params[i].subevent = subevent_data_params[i].subevent;
            sub_params[i].response_slot_start = 0U;
            sub_params[i].response_slot_count = pawr_params.num_response_slots;
            /* Create a net buf for data buffer */
            net_buf_simple_init_with_data
            (
                &sub_data[i],
                subevent_data_params[i].subevent_data,
                subevent_data_params[i].subevent_data_length
            );
            sub_params[i].data = &sub_data[i];
        }

        err = bt_le_per_adv_set_subevent_data(adv_pawr, (uint8_t)num_subevents, sub_params);
        if (err)
        {
            ESL_PL_TRC("[ESL PL]: Failed to set subevent data (err %d)", err);
            retval = BT_ESL_API_FAILURE;
        }
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Memory allocation failed for subevent data params");
        retval = BT_ESL_API_FAILURE;
    }

    BT_ESL_free_mem(sub_params);
    BT_ESL_free_mem(sub_data);

    return retval;
}

/**
 * \brief Performs periodic advertising information transfer.
 *
 * \par This function initiates a periodic advertising information transfer
 * on a device specified by its Bluetooth address.
 *
 * \param bd_addr Pointer to the Bluetooth address of the device to perform the transfer.
 * \param uuid The UUID of the service.
 *
 * \return BT_ESL_API_SUCCESS if the transfer operation starts successfully, BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_padv_set_info_transfer_pl
           (
               BT_ESL_BD_ADDR * bd_addr,
               UINT16           uuid
           )
{
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;
    int err;
    API_RESULT retval;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

    ESL_PL_TRC("[ESL PL]: -> BT_esl_padv_set_info_transfer_pl");

    if ((bd_addr == NULL) || (uuid == 0))
    {
        ESL_PL_ERR("[ESL PL]: Invalid parameters for periodic advertising info transfer");
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        /* Copy peer address */
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)bd_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)bd_addr->type);

        /* Get connection object */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);

        if (conn != NULL)
        {
            /* Perform periodic advertising info transfer */
            err = bt_le_per_adv_set_info_transfer(adv_pawr, conn, (uint16_t)uuid);
            /* Release the reference after the operation */
            bt_conn_unref(conn);

            if (err)
            {
                ESL_PL_ERR("[ESL PL]: Failed to perform periodic advertising info transfer (err %d)", err);
                retval = BT_ESL_API_FAILURE;
            }
            else
            {
                ESL_PL_TRC("[ESL PL]: Periodic advertising info transfer successful for UUID 0x%04x", uuid);
            }
        }
        else
        {
            ESL_PL_ERR(
            "[ESL PL]: Connection not found for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
            BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr));

            retval = BT_ESL_API_FAILURE;
        }
    }

    ESL_PL_TRC("[ESL PL]: <- BT_esl_padv_set_info_transfer_pl");
    return retval;
}
#endif /* BT_ESL_SUPPORT_AP_ROLE */

#ifdef BT_ESL_SUPPORT_TAG_ROLE
API_RESULT BT_esl_update_complete_name_pl(UCHAR * complete_name, UINT16 length)
{
    API_RESULT retval;
    int err;
    CHAR device_name[BT_ESL_MAX_DEVICE_NAME_LEN];

    /* Init */
    retval = BT_ESL_API_SUCCESS;
    BT_ESL_mem_set(device_name, 0x00U, sizeof(device_name));

    if ((NULL == complete_name) || (length == 0U))
    {
        ESL_PL_ERR("[ESL PL]: Invalid complete name or length");
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        BT_ESL_mem_copy
        (
            device_name,
            (CHAR *)complete_name,
            ((length > BT_ESL_MAX_DEVICE_NAME_LEN) ? BT_ESL_MAX_DEVICE_NAME_LEN : length)
        );
        err = bt_set_name(device_name);
        if (0 != err)
        {
            ESL_PL_ERR ("[ESL PL]: setting device name failed err (%d)", err);
            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            ESL_PL_TRC("[ESL PL]: Device name updated");
        }
        /**
         * TODO: Hex dump
         */
    }

    return retval;
}

API_RESULT BT_esl_start_advertise_pl(void)
{
    int err;
    API_RESULT retval;
    struct bt_le_adv_param adv_param;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    adv_param.id = BT_ID_DEFAULT;
    adv_param.sid = 1U;
    adv_param.secondary_max_skip = 1U;

    /**
     * Based on the Zephyr Documentation some of the Macros of Zephyr
     * are changed from a particular version of Zephyr.
     * https://docs.zephyrproject.org/latest/releases/migration-guide-4.0.html
     */
    /**
     * Handle Zephyr version differences for BLE advertising options.
     * The BT_LE_ADV_OPT_CONNECTABLE macro was renamed to BT_LE_ADV_OPT_CONN in Zephyr 4.0.0
     * Use feature detection - if the new macro exists, use it; otherwise use the old one.
     */
#ifdef ZEPHYR_4_0_0_OR_LATER
    /* Zephyr 4.0+ */
    adv_param.options = (BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_NAME);
#else /* ZEPHYR_4_0_0_OR_LATER */
    /* Zephyr < 4.0 */
    adv_param.options = (BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME);
#endif /* ZEPHYR_4_0_0_OR_LATER */

    adv_param.interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
    adv_param.interval_max = BT_GAP_ADV_FAST_INT_MAX_2;
    adv_param.peer = NULL;

    /* Create a non-connectable advertising set */
    err = bt_le_ext_adv_create(&adv_param, NULL, &ext_adv_tag);
    if (err)
    {
        ESL_PL_ERR("[ESL PL]: Failed to create advertising set (err %d)", err);
        retval = BT_ESL_API_SUCCESS;
    }

    if (BT_ESL_API_SUCCESS == retval)
    {
        /* Set advertising data */
        err = bt_le_ext_adv_set_data(ext_adv_tag, ad, ARRAY_SIZE(ad), NULL, 0);
        if (err)
        {
            ESL_PL_ERR("[ESL PL]: Failed to set advertising data (err %d)", err);
            bt_le_ext_adv_delete(ext_adv_tag);
            retval = BT_ESL_API_FAILURE;
        }

        /* Start advertising */
        err = bt_le_ext_adv_start(ext_adv_tag, BT_LE_EXT_ADV_START_DEFAULT);
        if (err)
        {
            ESL_PL_ERR("[ESL PL]: Failed to start extended advertising (err %d)", err);
            bt_le_ext_adv_delete(ext_adv_tag);
            retval = BT_ESL_API_FAILURE;
        }
    }

    if (BT_ESL_API_SUCCESS == retval)
    {
        ESL_PL_TRC("[ESL PL]: Advertising successfully started");
    }

    return retval;
}

API_RESULT BT_esl_stop_advertise_pl(void)
{
    int err;
    API_RESULT retval;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    if (ext_adv_tag == NULL)
    {
        ESL_PL_ERR("[ESL PL]: No advertising set to stop");
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        /* Stop advertising */
        err = bt_le_ext_adv_stop(ext_adv_tag);
        if (err)
        {
            ESL_PL_ERR("[ESL PL]: Failed to stop extended advertising (err %d)", err);
            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            ESL_PL_TRC("[ESL PL]: Advertising successfully stopped");
        }

        /* Delete the advertising set */
        err = bt_le_ext_adv_delete(ext_adv_tag);
        if (err)
        {
            ESL_PL_ERR("[ESL PL]: Failed to delete advertising set (err %d)", err);
            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            ext_adv_tag = NULL;
            ESL_PL_TRC("[ESL PL]: Advertising set deleted successfully");
        }
    }

    return retval;
}

static void sync_cb
            (
                 struct bt_le_per_adv_sync *sync,
                 struct bt_le_per_adv_sync_synced_info *info
            )
{
    char le_addr[BT_ADDR_LE_STR_LEN];
    BT_ESL_BD_ADDR advertiser_addr;

    /* Init */
    BT_ESL_mem_set(&advertiser_addr, 0, sizeof(BT_ESL_BD_ADDR));
    BT_ESL_mem_set(le_addr, 0, sizeof(le_addr));

    bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));
#if defined(CONFIG_BT_PER_ADV_SYNC_RSP) && defined(CONFIG_BT_PER_ADV_SYNC)
    ESL_PL_TRC (
    "[ESL PL]: PER_ADV_SYNC[%u]: [DEVICE]: %s synced, "
    "Interval 0x%04x (%u), PHY %d, sync info sid %d, service_data 0x%04x "
    "num_events %d, sub_int %d, resp delay %d, resp spacing %d",
    bt_le_per_adv_sync_get_index(sync),
    le_addr,
    info->interval,
    info->interval,
    info->phy,
    info->sid,
    info->service_data,
    info->num_subevents,
    info->subevent_interval,
    info->response_slot_delay,
    info->response_slot_spacing);
#endif /* CONFIG_BT_PER_ADV_SYNC_RSP && CONFIG_BT_PER_ADV_SYNC */
    /* store sync handle */
    sync_handle = bt_le_per_adv_sync_get_index(sync);
    BT_esl_tag_padv_sync_tx_received_handler
    (
        BT_ESL_API_SUCCESS,
        (UINT16)bt_conn_index(info->conn),
        &advertiser_addr
    );
}

static void term_cb
            (
                struct bt_le_per_adv_sync                 * sync,
                const struct bt_le_per_adv_sync_term_info * info
            )
{
    char le_addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));

    ESL_PL_TRC (
    "[ESL PL]: PER_ADV_SYNC[%u]: [DEVICE]: %s sync terminated",
    bt_le_per_adv_sync_get_index(sync), le_addr);

    BT_esl_tag_periodic_adv_sync_lost_handler();
}

static void recv_cb
            (
                struct bt_le_per_adv_sync                 * sync,
                const struct bt_le_per_adv_sync_recv_info * info,
                struct net_buf_simple                     * buf
            )
{
    char le_addr[BT_ADDR_LE_STR_LEN];
    BT_ESL_PADV_REPORT_PARAMS subevent_req_params;

    /* Init */
    BT_ESL_mem_set(&subevent_req_params, 0, sizeof(BT_ESL_PADV_REPORT_PARAMS));

    if (0 == buf->len)
    {
        ESL_PL_TRC ("[ESL PL]: Empty sync packet");
        return;
    }

    bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));
    /* Copy Bluetooth address */
    if (NULL != info->addr)
    {
        BT_ESL_COPY_BD_ADDR(subevent_req_params.peer_address.addr, (UCHAR *)info->addr->a.val);
        BT_ESL_COPY_TYPE(subevent_req_params.peer_address.type, (UCHAR)info->addr->type);
    }

#ifdef CONFIG_BT_PER_ADV_SYNC_RSP
    ESL_PL_TRC (
    "[ESL PL]: PER_ADV_SYNC[%u]: [DEVICE]: %s received, tx_power %i, "
    "RSSI %i, CTE %u, Subevent %d, data length %u[0x%04X]",
    bt_le_per_adv_sync_get_index(sync), le_addr, info->tx_power, info->rssi,
    info->cte_type,info->subevent, buf->len, buf->len);
    subevent_req_params.periodic_event_counter = (UINT16)info->periodic_event_counter;
    subevent_req_params.subevent = (UCHAR)info->subevent;
    subevent_req_params.data_status = 0U;
    subevent_req_params.data_length = (UINT16)buf->len;
    subevent_req_params.data = (UCHAR *)buf->data;
#endif /* CONFIG_BT_PER_ADV_SYNC_RSP */

    BT_esl_tag_periodic_adv_report_handler(&subevent_req_params);
}

/**
 * \brief Setup of Callback for periodic advertising sync.
 *
 * \par This functions are called when the periodic advertising sync is established.
 * It sets up the necessary parameters and registers the callbacks for periodic
 * advertising.
 */
static void esl_setup_pawr_sync(void)
{
    sync_callbacks.synced = sync_cb;
    sync_callbacks.term = term_cb;
    sync_callbacks.recv = recv_cb;

    ESL_PL_TRC ("[ESL PL]: ESL TAG Periodic Advertising callbacks register");
    bt_le_per_adv_sync_cb_register(&sync_callbacks);
}

API_RESULT BT_esl_sync_with_subevent_pl
           (
               UINT16  periodic_adv_properties,
               UCHAR   num_subevents,
               UCHAR  * subevent
           )
{
    int err;
    API_RESULT retval;
    struct bt_le_per_adv_sync_subevent_params param;
    struct bt_le_per_adv_sync *per_adv_sync;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    param.properties = (uint16_t)periodic_adv_properties;
    param.num_subevents = (uint8_t)num_subevents;
    param.subevents = (uint8_t *)subevent;

    per_adv_sync = bt_le_per_adv_sync_lookup_index(sync_handle);
    if (NULL != per_adv_sync)
    {
        err = bt_le_per_adv_sync_subevent(per_adv_sync, &param);
        if (err)
        {
            ESL_PL_TRC ("[ESL PL]: bt_le_per_adv_sync_subevent failed (err %d)", err);
            retval = BT_ESL_API_FAILURE;
        }
    }
    else
    {
        ESL_PL_ERR ("[ESL PL]: Invalid sync handle %d", sync_handle);
        retval = BT_ESL_API_FAILURE;
    }

    return retval;
}

API_RESULT BT_esl_set_response_data_pl
           (
               UINT16 periodic_event_counter,
               UCHAR  request_subevent,
               UCHAR  response_subevent,
               UCHAR  response_slot,
               UCHAR  response_data_length,
               UCHAR* response_data
           )
{
    int err;
    API_RESULT retval;
    struct bt_le_per_adv_response_params param;
    static struct net_buf_simple pa_rsp;
    struct bt_le_per_adv_sync * per_adv_sync;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    param.request_event = (uint16_t)periodic_event_counter;
    param.request_subevent = (uint8_t)request_subevent;
    param.response_subevent = (uint8_t)response_subevent;
    param.response_slot = (uint8_t)response_slot;
    pa_rsp.len = (uint16_t)response_data_length;
    pa_rsp.data = (uint8_t *)response_data;

    per_adv_sync = bt_le_per_adv_sync_lookup_index(sync_handle);
    if (NULL != per_adv_sync)
    {
        err = bt_le_per_adv_set_response_data(per_adv_sync, &param, &pa_rsp);

        if (err)
        {
            ESL_PL_TRC ("[ESL PL]: bt_le_per_adv_set_response_data failed (err %d)", err);
            retval = BT_ESL_API_FAILURE;
        }
    }
    else
    {
        ESL_PL_ERR ("[ESL PL]: Invalid sync handle %d", sync_handle);
        retval = BT_ESL_API_FAILURE;
    }

    return retval;
}
API_RESULT BT_esl_terminate_sync_pl(void)
{
    int err;
    API_RESULT retval;
    struct bt_le_per_adv_sync * per_adv_sync;

    /* Init */
    retval = BT_ESL_API_SUCCESS;
    per_adv_sync = NULL;

    per_adv_sync = bt_le_per_adv_sync_lookup_index(sync_handle);

    if (NULL != per_adv_sync)
    {
        err = bt_le_per_adv_sync_delete(per_adv_sync);
        if (err)
        {
            ESL_PL_TRC ("[ESL PL]: bt_le_per_adv_sync_delete failed (err %d)", err);
            retval = BT_ESL_API_FAILURE;
        }
    }
    else
    {
        ESL_PL_ERR ("[ESL PL]: Invalid sync handle %d", sync_handle);
        retval = BT_ESL_API_FAILURE;
    }

    return retval;
}

API_RESULT BT_esl_padv_sync_transfer_subscribe_pl
           (
               BT_ESL_BD_ADDR * peer_bd_address
           )
{
    API_RESULT retval;
    int err;
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;
    struct bt_le_per_adv_sync_transfer_param past_params;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

   if (peer_bd_address != NULL)
    {
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)peer_bd_address->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)peer_bd_address->type);

        /* Get conn param */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);
    }

    /**
     * TODO: Check parameters
     */
    past_params.skip = 0x0000;
    past_params.timeout = 0x0C80;
    past_params.options = BT_LE_PER_ADV_SYNC_TRANSFER_OPT_NONE;
    err = bt_le_per_adv_sync_transfer_subscribe(conn, &past_params);
    if (err != 0)
    {
        ESL_PL_TRC ("[ESL PL]: PAST subscribe failed (err %d)", err);
        retval = BT_ESL_API_FAILURE;
    }

    if (NULL != conn)
    {
        /* Release the reference  */
        bt_conn_unref(conn);
    }

    return retval;
}

API_RESULT BT_esl_padv_sync_transfer_unsubscribe_pl
           (
               BT_ESL_BD_ADDR * peer_bd_address
           )
{
    API_RESULT retval;
    int err;
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

   if (peer_bd_address != NULL)
    {
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)peer_bd_address->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)peer_bd_address->type);

        /* Get conn param */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);
    }

    err = bt_le_per_adv_sync_transfer_unsubscribe(conn);
    if (err != 0)
    {
        ESL_PL_TRC ("[ESL PL]: PAST subscribe failed (err %d)", err);
        retval = BT_ESL_API_FAILURE;
    }

    if (NULL != conn)
    {
        /* Release the reference  */
        bt_conn_unref(conn);
    }

    return retval;
}
#endif /* BT_ESL_SUPPORT_TAG_ROLE */

#ifdef BT_ESL_SUPPORT_AP_ROLE
/**
 * \brief Prints the UUID of a discovered attribute.
 *
 * \par This function converts the UUID to a string and logs it for debugging purposes.
 *
 * \param uuid Pointer to the UUID to print.
 */
static void print_uuid_pl(const struct bt_uuid *uuid)
{
   char uuid_str[37];
   bt_uuid_to_str(uuid, uuid_str, sizeof(uuid_str));
   /* Ensure null termination */
   uuid_str[sizeof(uuid_str) - 1] = '\0';
   ESL_PL_TRC ("[ESL PL]: UUID [%s]", uuid_str);
}

/**
 * \brief Notifies the upper layer about the completion of the discovery process.
 *
 * \par This function informs the upper layer that the GATT discovery process has completed
 * and provides the discovered ESL attribute handles.
 *
 * \param conn Pointer to the connection object associated with the discovery.
 * \param handles Pointer to the discovered ESL attribute handles.
 */
static void discovery_complete_to_ul(struct bt_conn *conn, BT_ESL_ATTR_HANDLES * handles)
{
    BT_ESL_BD_ADDR bd_addr;

    if (conn != NULL)
    {
        /* Copy the peer address */
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

        /* Check if handles is NULL */
        if (handles == NULL)
        {
            ESL_PL_ERR("[ESL PL]: Handles parameter is NULL");
        }

        /* Callback to core */
        BT_esl_ap_discovery_complete_handler(&bd_addr, handles);
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Connection parameter is NULL");
    }
}

/**
 * \brief Updates the ESL attribute handles based on the discovered UUID.
 *
 * \par This function maps the discovered UUID to the corresponding ESL attribute handle
 * and updates the provided attribute handles structure.
 *
 * \param uuid Pointer to the UUID of the discovered attribute.
 * \param handle The handle of the discovered attribute.
 * \param attr_handles Pointer to the ESL attribute handles structure to update.
 */
void update_esl_attr_handle_pl(const struct bt_uuid *uuid, uint16_t handle, BT_ESL_ATTR_HANDLES * attr_handles)
{
    if ((NULL == uuid) || (NULL == attr_handles))
    {
        ESL_PL_ERR("[ESL PL]: Invalid parameters: uuid or attr_handles is NULL");
        return;
    }

    if (0 == bt_uuid_cmp(BT_UUID_DECLARE_16(GATT_ESL_ADDRESS_CHARACTERISTIC), uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating Address Characteristic handle: 0x%04X", handle);
        attr_handles->address_hdl = (UINT16)handle;
    }
    else if (0 == bt_uuid_cmp(BT_UUID_DECLARE_16(GATT_AP_SYNC_KEY_MATERIAL_CHARACTERISTIC), uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating AP Sync Key Material Characteristic handle: 0x%04X", handle);
        attr_handles->ap_sync_key_material_hdl = (UINT16)handle;
    }
    else if (0 == bt_uuid_cmp(BT_UUID_DECLARE_16(GATT_ESL_RESPONSE_KEY_MATERIAL_CHARACTERISTIC), uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating Response Key Material Characteristic handle: 0x%04X", handle);
        attr_handles->response_key_material_hdl = (UINT16)handle;
    }
    else if (0 == bt_uuid_cmp(BT_UUID_DECLARE_16(GATT_ESL_CURRENT_ADSOLUTE_TIME_CHARACTERISTIC), uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating Current Absolute Time Characteristic handle: 0x%04X", handle);
        attr_handles->current_absolute_time_hdl = (UINT16)handle;
    }
    else if (0 == bt_uuid_cmp(BT_UUID_DECLARE_16(GATT_ESL_DISPLAY_INFORMATION_CHARACTERISTIC), uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating Display Information Characteristic handle: 0x%04X", handle);
        attr_handles->display_info_hdl = (UINT16)handle;
    }
    else if (0 == bt_uuid_cmp(BT_UUID_DECLARE_16(GATT_ESL_IMAGE_INFORMATION_CHARACTERISTIC), uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating Image Information Characteristic handle: 0x%04X", handle);
        attr_handles->image_info_hdl = (UINT16)handle;
    }
    else if (0 == bt_uuid_cmp(BT_UUID_DECLARE_16(GATT_ESL_SENSOR_INFORMATION_CHARACTERISTIC), uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating Sensor Information Characteristic handle: 0x%04X", handle);
        attr_handles->sensor_info_hdl = (UINT16)handle;
    }
    else if (0 == bt_uuid_cmp(BT_UUID_DECLARE_16(GATT_ESL_LED_INFORMATION_CHARACTERISTIC), uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating LED Information Characteristic handle: 0x%04X", handle);
        attr_handles->led_info_hdl = (UINT16)handle;
    }
    else if (0 == bt_uuid_cmp(BT_UUID_DECLARE_16(GATT_ESL_CONTROL_POINT_CHARACTERISTIC), uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating Control Point Characteristic handle: 0x%04X", handle);
        attr_handles->cp_hdl = (UINT16)handle;
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Unhandled UUID encountered: ");
        print_uuid_pl(uuid);
    }
}

/**
 * \brief Callback for GATT discovery operation.
 *
 * \par This function is called during the GATT discovery process to handle discovered attributes.
 * It processes the discovered services, characteristics, and descriptors, and updates the
 * ESL attribute handles accordingly.
 *
 * \param conn Pointer to the connection object associated with the discovery.
 * \param attr Pointer to the discovered attribute.
 * \param params Pointer to the discovery parameters.
 *
 * \return BT_GATT_ITER_CONTINUE to continue discovery, BT_GATT_ITER_STOP to stop discovery.
 */
static uint8_t esl_discovery_cb_pl
               (
                    struct bt_conn *conn,
                    const struct bt_gatt_attr *attr,
                    struct bt_gatt_discover_params *params
               )
{
    uint8_t conn_idx;
    int err;
    uint8_t retval;

    /* Init */
    retval = BT_GATT_ITER_CONTINUE;

    /* Validate connection before any dereference (bt_conn_index requires non-NULL) */
    if (NULL == conn)
    {
        ESL_PL_ERR("[ESL PL]: Discovery callback received NULL conn");
        return BT_GATT_ITER_STOP;
    }

    /* Get connection index and validate bounds */
    conn_idx = bt_conn_index(conn);
    if (conn_idx >= CONFIG_BT_MAX_CONN)
    {
        ESL_PL_ERR("[ESL PL]: Invalid connection index %d", conn_idx);
        return BT_GATT_ITER_STOP;
    }

    /* Check if params is NULL */
    if (params == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Discovery callback received NULL params");
        attr_handles[conn_idx].discovery_in_progress = BT_ESL_FALSE;
        discovery_complete_to_ul(conn, NULL);
        return BT_GATT_ITER_STOP;
    }

    switch (params->type)
    {
    case BT_GATT_DISCOVER_PRIMARY:
    {
        struct bt_gatt_service_val *svc;

        /* Zephyr passes NULL attr when discovery is exhausted (service not found) */
        if ((NULL == attr) || (NULL == attr->user_data))
        {
            ESL_PL_TRC ("[ESL PL]: ESL primary service not found");
            attr_handles[conn_idx].discovery_in_progress = BT_ESL_FALSE;
            discovery_complete_to_ul(conn, NULL);
            retval = BT_GATT_ITER_STOP;
            break;
        }

        /* Handle service discovery */
        svc = attr->user_data;
        ESL_PL_TRC ("[ESL PL]: Found Service:");
        ESL_PL_TRC ("[ESL PL]:   Start Handle: 0x%04x", attr->handle);
        ESL_PL_TRC ("[ESL PL]:   End Handle: 0x%04x", svc->end_handle);
        print_uuid_pl(svc->uuid);

        /* Update discovery parameters to find characteristics within this service */
        attr_handles[conn_idx].discover_params.uuid = NULL;
        attr_handles[conn_idx].discover_params.start_handle = attr->handle;
        attr_handles[conn_idx].discover_params.end_handle = svc->end_handle;
        attr_handles[conn_idx].discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
        BT_ESL_AP_INIT_ESL_ATTR_HANDLES(&attr_handles[conn_idx].attr_handles);

        err = bt_gatt_discover(conn, &attr_handles[conn_idx].discover_params);
        if (err)
        {
            ESL_PL_TRC ("[ESL PL]: Failed to start characteristic discovery (err %d)", err);
            /* Reset discovery_in_progress */
            attr_handles[conn_idx].discovery_in_progress = BT_ESL_FALSE;
            /* Inform core */
            discovery_complete_to_ul(conn, NULL);
        }
        retval = BT_GATT_ITER_STOP;
        break;
    }
    case BT_GATT_DISCOVER_CHARACTERISTIC:
    {
        struct bt_gatt_chrc *chrc;
        /* Checking CHAR discovery is completed or not */
        if ((NULL != attr) && (NULL != attr->user_data)
            && (params->start_handle < params->end_handle))
        {
            /* Handle characteristic discovery */
            chrc = attr->user_data;
            ESL_PL_TRC ("[ESL PL]: Found Characteristic:");
            ESL_PL_TRC ("[ESL PL]:   Handle: 0x%04x", attr->handle);
            ESL_PL_TRC ("[ESL PL]:   Properties: 0x%02x", chrc->properties);
            if (NULL != chrc->uuid)
            {
                print_uuid_pl(chrc->uuid);

                /* Store the handles */
                update_esl_attr_handle_pl
                (
                    chrc->uuid,
                    bt_gatt_attr_value_handle(attr),
                    &attr_handles[conn_idx].attr_handles
                );
            }
        }
        else
        {
            /* Discover cccd of control point char */
            if (attr_handles[conn_idx].attr_handles.cp_hdl != BT_ESL_AP_ATTR_HANDLE_INIT_VAL)
            {
                ESL_PL_TRC ("[ESL PL]: Starting CCCD discovery for Control Point");

                /* Update discovery parameters to find CCCD for the control point */
                attr_handles[conn_idx].discover_params.uuid = NULL; /* Discover only CCCD */
                attr_handles[conn_idx].discover_params.start_handle = attr_handles[conn_idx].attr_handles.cp_hdl+ 1U;
                attr_handles[conn_idx].discover_params.end_handle = attr_handles[conn_idx].attr_handles.cp_hdl + 1U;
                attr_handles[conn_idx].discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;

                err = bt_gatt_discover(conn, &attr_handles[conn_idx].discover_params);
                if (err)
                {
                    ESL_PL_TRC ("[ESL PL]: Failed to start CCCD discovery (err %d)", err);
                    attr_handles[conn_idx].discovery_in_progress = BT_ESL_FALSE;
                    discovery_complete_to_ul(conn, NULL);
                }
            }
            else
            {
                /* Discovery process failed */
                ESL_PL_TRC("[ESL PL]: Control Point handle not found");
                attr_handles[conn_idx].discovery_in_progress = BT_ESL_FALSE;
                discovery_complete_to_ul(conn, NULL);
            }
            retval = BT_GATT_ITER_STOP;
        }
        break;
    }
    case BT_GATT_DISCOVER_DESCRIPTOR:
    {
        if (NULL != attr)
        {
            /* Handle descriptor discovery */
            ESL_PL_TRC ("[ESL PL]: Found Descriptor:");
            ESL_PL_TRC ("[ESL PL]:   Handle: 0x%04x", attr->handle);
            print_uuid_pl(attr->uuid);

            /* Store CCCD discriptor only */
            if (0 == bt_uuid_cmp(attr->uuid,BT_UUID_GATT_CCC))
            {
                ESL_PL_TRC ("[ESL PL]: Found CCCD Descriptor for Control Point");
                attr_handles[conn_idx].attr_handles.cp_cccd_hdl = (UINT16)attr->handle;
                /* Reset discovery_in_progress */
                attr_handles[conn_idx].discovery_in_progress = BT_ESL_FALSE;
                /* Inform core */
                discovery_complete_to_ul(conn, &attr_handles[conn_idx].attr_handles);
                retval = BT_GATT_ITER_STOP;
            }
        }
        else
        {
            /* Discovery process failed */
            ESL_PL_TRC("[ESL PL]: Control Point CCCD handle not found");
            attr_handles[conn_idx].discovery_in_progress = BT_ESL_FALSE;
            discovery_complete_to_ul(conn, NULL);
            retval = BT_GATT_ITER_STOP;
        }
        break;
    }
    default:
        ESL_PL_TRC ("[ESL PL]: Unknown discovery type: %d", params->type);
        /* Reset discovery_in_progress */
        attr_handles[conn_idx].discovery_in_progress = BT_ESL_FALSE;
        /* Inform core */
        discovery_complete_to_ul(conn, NULL);
        retval = BT_GATT_ITER_STOP;
    }

    return retval;
}

/**
 * \brief Discovers the ESL service on a connected device.
 *
 * \par This function initiates GATT service discovery for the ESL service on a device
 * specified by its Bluetooth address.
 *
 * \param bd_addr Pointer to the Bluetooth address of the device to discover the ESL service.
 *
 * \return BT_ESL_API_SUCCESS if service discovery starts successfully, BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_discover_esl_service_pl(BT_ESL_BD_ADDR * bd_addr)
{
    uint8_t conn_idx;
    API_RESULT retval;
    int err;
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

   if (bd_addr != NULL)
    {
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)bd_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)bd_addr->type);

        /* Get conn param */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);

        if (conn != NULL)
        {
            conn_idx = bt_conn_index(conn);

            /* Ensure the connection index is valid */
            if (conn_idx >= CONFIG_BT_MAX_CONN)
            {
                ESL_PL_ERR (
                "[ESL PL]: Invalid connection index: %d", conn_idx);
                return BT_ESL_API_FAILURE;
            }

            /* Check if discovery is already in progress for this connection */
            if (attr_handles[conn_idx].discovery_in_progress)
            {
                ESL_PL_ERR (
                "[ESL PL]: Service discovery already in progress for connection index %d", conn_idx);
                return BT_ESL_API_FAILURE;
            }

            /* Mark discovery as in progress for this connection */
            attr_handles[conn_idx].discovery_in_progress = BT_ESL_TRUE;

            /* Set up discovery parameters */
            attr_handles[conn_idx].discover_params.uuid = BT_UUID_DECLARE_16(BT_ESL_GATT_ESL_SERVICE);
            attr_handles[conn_idx].discover_params.func = esl_discovery_cb_pl;
            attr_handles[conn_idx].discover_params.start_handle = 0x0001;
            attr_handles[conn_idx].discover_params.end_handle = 0xffff;
            attr_handles[conn_idx].discover_params.type = BT_GATT_DISCOVER_PRIMARY;

            /* Start service discovery */
            err = bt_gatt_discover(conn, &attr_handles[conn_idx].discover_params);
            /* Release the reference after starting discovery */
            bt_conn_unref(conn);
            if (err)
            {
                ESL_PL_ERR ("[ESL PL]:Service discovery failed to start (err %d)", err);
                attr_handles[conn_idx].discovery_in_progress = BT_ESL_FALSE;
            }
            else
            {
                ESL_PL_TRC ("[ESL PL]: Service discovery started for connection index %d", conn_idx);
            }
        }
        else
        {
            ESL_PL_ERR (
            "[ESL PL]: Connection not found for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
            BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr));
            retval = BT_ESL_API_FAILURE;
        }
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Invalid peer address");
        retval = BT_ESL_API_FAILURE;
    }

    return retval;
}

/**
 * \brief Callback for GATT read operation.
 *
 * \par This function is called when the GATT read operation completes.
 *
 * \param conn Pointer to the connection object.
 * \param err Error code (0 if successful, negative value otherwise).
 * \param params Pointer to the read parameters used for the operation.
 * \param data Pointer to the data read from the characteristic (NULL if an error occurred).
 * \param length Length of the data read (0 if an error occurred).
 */
static uint8_t gatt_read_cb_pl
               (
                   struct bt_conn             * conn,
                   uint8_t                      err,
                   struct bt_gatt_read_params * params,
                   const void                 * data,
                   uint16_t                     length
               )
{
    BT_ESL_BD_ADDR bd_addr;
    UINT16 att_mtu;
    uint8_t conn_idx;
    BT_ESL_READ_SESSION_PL *session;
    int temp_err;
    UINT16 handle;
    UINT16 data_length;

    BT_ESL_INIT_BD_ADDR(&bd_addr);

    ESL_PL_TRC("[ESL PL]: GATT read response, (err %d) length: %d",err,  length);

    conn_idx = bt_conn_index(conn);
    if (conn_idx >= CONFIG_BT_MAX_CONN)
    {
        ESL_PL_ERR("[ESL PL]: Invalid connection index %d", conn_idx);
        BT_ESL_free_mem(params);
        return BT_GATT_ITER_STOP;
    }

    /* Get MTU size */
    att_mtu = bt_gatt_get_mtu(conn);

    BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
    BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

    ESL_PL_TRC("[ESL PL]: GATT read response, (err %d) length: %d", err, length);

    session = &read_sessions[conn_idx];

    /* fetch handle */
    handle = (UINT16)params->single.handle;

    if (data && length > 0)
    {
        if ((session->length + length) <= (BT_ESL_MAX_READ_SESSION_BUFFER_SIZE))
        {
            BT_ESL_mem_copy(&session->buffer[session->length], data, length);
            session->length += length;
            session->read_in_progress = true;
        }
        else
        {
            ESL_PL_ERR("[ESL PL]: Buffer overflow for conn_idx %d", conn_idx);
            session->length = 0;
            session->read_in_progress = false;
            BT_ESL_free_mem(session->read_params);
            (BT_ESL_IGNORE_RETURN_VALUE) BT_esl_ap_read_response_handler
                                         (
                                             &bd_addr,
                                             handle,
                                             err,
                                             NULL,
                                             0U
                                          );
            return BT_GATT_ITER_STOP;
        }
    }

    /**
     * The "length" received does not account of ATT Opcode.
     * If recieved length is less than (ATT_MTU - 1) or
     * "err" reported by stack is Invalid Offset, then
     * Read operation is deemed completed
     */
    if ((length < (att_mtu - 1)) || (err == BT_ATT_ERR_INVALID_OFFSET))
    {
        data_length = session->length;
        session->length = 0;
        session->read_in_progress = false;
        BT_ESL_free_mem(session->read_params);
        (BT_ESL_IGNORE_RETURN_VALUE) BT_esl_ap_read_response_handler
                                     (
                                         &bd_addr,
                                         handle,
                                         err,
                                         session->buffer,
                                         data_length
                                     );
    }
    else
    {
        /* Continue reading next fragment */
        params->single.offset += length;
        temp_err = bt_gatt_read(conn, params);
        if (temp_err)
        {
            ESL_PL_ERR("[ESL PL]: Failed to continue GATT read for conn_idx %d", conn_idx);
            session->length = 0;
            session->read_in_progress = false;
            BT_ESL_free_mem(session->read_params);
            (BT_ESL_IGNORE_RETURN_VALUE) BT_esl_ap_read_response_handler
                                         (
                                             &bd_addr,
                                             handle,
                                             err,
                                             NULL,
                                             0U
                                         );
        }
    }

    return BT_GATT_ITER_STOP;
}


/**
 * \brief Reads a GATT characteristic from a connected device.
 *
 * \par This function initiates a GATT read operation for a specific attribute handle
 * on a device specified by its Bluetooth address.
 *
 * \param peer_addr Pointer to the Bluetooth address of the device to read the characteristic.
 * \param attr_handle The handle of the characteristic to read.
 *
 * \return BT_ESL_API_SUCCESS if the read operation starts successfully, BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_gatt_read_characteristic_pl
           (
               BT_ESL_BD_ADDR * peer_addr,
               UINT16           attr_handle,
               UCHAR            read_mode
           )
{
    struct bt_conn * conn;
    bt_addr_le_t     conn_addr;
    int              err;
    API_RESULT       retval;
    uint8_t          conn_idx;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;
    BT_ESL_IGNORE_UNUSED_PARAM (read_mode);

    ESL_PL_TRC("[ESL PL]: -> BT_esl_gatt_read_characteristic");

    if ((peer_addr == NULL) || (attr_handle == BT_ESL_AP_ATTR_HANDLE_INIT_VAL))
    {
        ESL_PL_ERR("[ESL PL]: Invalid parameters for GATT read");
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        /* Copy peer address */
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)peer_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)peer_addr->type);

        /* Get connection object */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);

        if (conn != NULL)
        {
            conn_idx = bt_conn_index(conn);

            /* Ensure the connection index is valid */
            if (conn_idx >= CONFIG_BT_MAX_CONN)
            {
                ESL_PL_ERR (
                "[ESL PL]: Invalid connection index: %d", conn_idx);
                retval = BT_ESL_API_FAILURE;
            }
            else
            {
                /* Check if read is already in progress for this connection */
                if (BT_ESL_TRUE == read_sessions[conn_idx].read_in_progress)
                {
                    ESL_PL_ERR (
                    "[ESL PL]: read already in progress for connection index %d", conn_idx);
                    retval = BT_ESL_API_FAILURE;
                }
                else
                {
                    read_sessions[conn_idx].read_params = (struct bt_gatt_read_params *)BT_ESL_alloc_mem(sizeof(struct bt_gatt_read_params));
                    if (NULL == read_sessions[conn_idx].read_params)
                    {
                        ESL_PL_ERR("[ESL PL]: Memory allocation failed for read parameters");
                        retval = BT_ESL_API_FAILURE;
                    }
                    else
                    {
                        /* Mark read as in progress for this connection */
                        read_sessions[conn_idx].read_in_progress = BT_ESL_TRUE;

                        /* Set up read parameters */
                        read_sessions[conn_idx].read_params->func = gatt_read_cb_pl;
                        read_sessions[conn_idx].read_params->handle_count = 1U;
                        read_sessions[conn_idx].read_params->single.handle = (uint16_t)attr_handle;
                        read_sessions[conn_idx].read_params->single.offset = 0U;
                        /* Initiate GATT read operation */
                        err = bt_gatt_read(conn, read_sessions[conn_idx].read_params);
                        /* Release the reference after starting the read operation */
                        bt_conn_unref(conn);

                        if (err)
                        {
                            ESL_PL_ERR("[ESL PL]: Failed to start GATT read (err %d)", err);
                            read_sessions[conn_idx].read_in_progress = BT_ESL_FALSE;
                            BT_ESL_free_mem(read_sessions[conn_idx].read_params);
                            retval = BT_ESL_API_FAILURE;
                        }
                        else
                        {
                            ESL_PL_TRC("[ESL PL]: GATT read started for handle 0x%04x", attr_handle);
                        }
                    }
                }
            }
        }
        else
        {
            ESL_PL_ERR(
                "[ESL PL]: Connection not found for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
                BT_ESL_DEVICE_ADDR_PRINT_STR(peer_addr));
            retval = BT_ESL_API_FAILURE;
        }
    }

    ESL_PL_TRC("[ESL PL]: <- BT_esl_gatt_read_characteristic");
    return retval;
}

/**
 * \brief Callback for GATT write operation.
 *
 * \par This function is called when the GATT write operation completes, either successfully
 * or with an error. It processes the result of the write operation and informs the upper
 * layer about the outcome.
 *
 * \param conn Pointer to the connection object associated with the write operation.
 * \param err Error code indicating the result of the write operation:
 *            - 0: Write operation was successful.
 *            - Non-zero: Write operation failed.
 * \param params Pointer to the write parameters used for the operation.
 */
static void gatt_write_cb_pl(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params)
{
    BT_ESL_BD_ADDR bd_addr;
    uint8_t conn_idx;
    UINT16 handle;

    if (conn != NULL)
    {
        /* Copy the peer address */
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

        conn_idx = bt_conn_index(conn);

        if (err)
        {
            ESL_PL_ERR("[ESL PL]: GATT write failed (err %d)", err);
        }
        else
        {
            ESL_PL_TRC("[ESL PL]: GATT write successful for handle 0x%04x", params->handle);
        }

        handle = (UINT16)params->handle;

        BT_ESL_free_mem(write_sessions[conn_idx].write_params);
        BT_ESL_free_mem(write_sessions[conn_idx].buffer);
        write_sessions[conn_idx].write_in_progress = BT_ESL_FALSE;

        /* Inform upper layer with the result */
        BT_esl_ap_write_response_handler(&bd_addr, handle, (UCHAR)err);

    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Connection parameter is NULL in GATT write callback");
    }
}

/**
 * \brief Writes data to a GATT characteristic on a connected device.
 *
 * \par This function initiates a GATT write operation for a specific attribute handle
 * on a device specified by its Bluetooth address.
 *
 * \param peer_addr Pointer to the Bluetooth address of the device to write the characteristic.
 * \param attr_handle The handle of the characteristic to write.
 * \param data Pointer to the data to be written.
 * \param length Length of the data to be written.
 *
 * \return BT_ESL_API_SUCCESS if the write operation starts successfully, BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_gatt_write_characteristic_pl
           (
               BT_ESL_BD_ADDR * peer_addr,
               UINT16           attr_handle,
               UCHAR          * data,
               UINT16           length
           )
{
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;
    int err;
    API_RESULT retval;
    uint8_t  conn_idx;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

    ESL_PL_TRC("[ESL PL]: -> BT_esl_gatt_write_characteristic_pl");

    if ((peer_addr == NULL) || (attr_handle == BT_ESL_AP_ATTR_HANDLE_INIT_VAL) || (data == NULL) || (length == 0))
    {
        ESL_PL_ERR("[ESL PL]: Invalid parameters for GATT write");
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        /* Copy peer address */
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)peer_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)peer_addr->type);

        /* Get connection object */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);

        if (conn != NULL)
        {
            conn_idx = bt_conn_index(conn);

             /* Ensure the connection index is valid */
            if (conn_idx >= CONFIG_BT_MAX_CONN)
            {
                ESL_PL_ERR (
                "[ESL PL]: Invalid connection index: %d", conn_idx);
                retval = BT_ESL_API_FAILURE;
            }
            else
            {
                /* Check if write is already in progress for this connection */
                if (BT_ESL_TRUE == write_sessions[conn_idx].write_in_progress)
                {
                    ESL_PL_ERR (
                    "[ESL PL]: write already in progress for connection index %d", conn_idx);
                    retval = BT_ESL_API_FAILURE;
                }
                else
                {
                    write_sessions[conn_idx].write_params = (struct bt_gatt_write_params *)BT_ESL_alloc_mem(sizeof(struct bt_gatt_write_params));
                    write_sessions[conn_idx].buffer = (UCHAR *)BT_ESL_alloc_mem(sizeof(UCHAR) * length);
                    if ((NULL == write_sessions[conn_idx].write_params) || (NULL == write_sessions[conn_idx].buffer))
                    {
                        ESL_PL_ERR("[ESL PL]: Failed to allocate memory for write parameters");
                        if (NULL != write_sessions[conn_idx].write_params)
                        {
                            BT_ESL_free_mem(write_sessions[conn_idx].write_params);
                        }
                        if (NULL != write_sessions[conn_idx].buffer)
                        {
                            BT_ESL_free_mem(write_sessions[conn_idx].buffer);
                        }
                        retval = BT_ESL_API_FAILURE;
                    }
                    else
                    {
                        BT_ESL_mem_copy(write_sessions[conn_idx].buffer, data, length);
                        write_sessions[conn_idx].length = length;
                        write_sessions[conn_idx].write_in_progress = BT_ESL_TRUE;
                        /* Set up write parameters */
                        write_sessions[conn_idx].write_params->func = gatt_write_cb_pl;
                        write_sessions[conn_idx].write_params->handle = (uint16_t)attr_handle;
                        write_sessions[conn_idx].write_params->offset = 0U;
                        write_sessions[conn_idx].write_params->data = (uint8_t *)write_sessions[conn_idx].buffer;
                        write_sessions[conn_idx].write_params->length = (uint16_t)write_sessions[conn_idx].length;

                        /* Initiate GATT write operation */
                        err = bt_gatt_write(conn, write_sessions[conn_idx].write_params);
                        /* Release the reference after starting the write operation */
                        bt_conn_unref(conn);

                        if (err)
                        {
                            ESL_PL_ERR("[ESL PL]: Failed to start GATT write (err %d)", err);
                            BT_ESL_free_mem(write_sessions[conn_idx].write_params);
                            BT_ESL_free_mem(write_sessions[conn_idx].buffer);
                            write_sessions[conn_idx].write_in_progress = BT_ESL_FALSE;
                            retval = BT_ESL_API_FAILURE;
                        }
                        else
                        {
                            ESL_PL_TRC("[ESL PL]: GATT write started for handle 0x%04x", attr_handle);
                        }
                    }
                }
            }
        }
        else
        {
            ESL_PL_ERR(
            "[ESL PL]: Connection not found for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
            BT_ESL_DEVICE_ADDR_PRINT_STR(peer_addr));

            retval = BT_ESL_API_FAILURE;
        }
    }

    ESL_PL_TRC("[ESL PL]: <- BT_esl_gatt_write_characteristic_pl");
    return retval;
}

/**
 * \brief Writes data to a GATT characteristic on a connected device without expecting a response.
 *
 * \par This function initiates a GATT write operation for a specific attribute handle
 * on a device specified by its Bluetooth address. The operation does not expect
 * a response from the device.
 *
 * \param peer_addr Pointer to the Bluetooth address of the device to write the characteristic.
 * \param attr_handle The handle of the characteristic to write.
 * \param data Pointer to the data to be written.
 * \param length Length of the data to be written.
 *
 * \return BT_ESL_API_SUCCESS if the write operation starts successfully, BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_gatt_write_without_response_pl
           (
               BT_ESL_BD_ADDR * peer_addr,
               UINT16           attr_handle,
               UCHAR          * data,
               UINT16           length
           )
{
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;
    int err;
    API_RESULT retval;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

    ESL_PL_TRC("[ESL PL]: -> BT_esl_gatt_write_without_response_pl");

    if ((peer_addr == NULL) ||
        (attr_handle == BT_ESL_AP_ATTR_HANDLE_INIT_VAL) ||
        (data == NULL) ||
        (length == 0))
    {
        ESL_PL_ERR("[ESL PL]: Invalid input parameters");
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        /* Copy peer address */
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)peer_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)peer_addr->type);

        /* Get connection object */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);

        if (conn != NULL)
        {
            /* Perform GATT write without response */
            err = bt_gatt_write_without_response
                  (
                      conn,
                      (uint16_t)attr_handle,
                      (uint8_t *)data,
                      (uint16_t)length,
                      false
                  );
            /* Release the reference after the operation */
            bt_conn_unref(conn);

            if (err)
            {
                ESL_PL_ERR("[ESL PL]: Failed to perform GATT write without response (err %d)", err);
                retval = BT_ESL_API_FAILURE;
            }
            else
            {
                ESL_PL_TRC("[ESL PL]: GATT write without response performed for handle 0x%04x", attr_handle);
            }
        }
        else
        {
            ESL_PL_ERR(
            "[ESL PL]: Connection not found for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
            BT_ESL_DEVICE_ADDR_PRINT_STR(peer_addr));

            retval = BT_ESL_API_FAILURE;
        }
    }

    ESL_PL_TRC("[ESL PL]: <- BT_esl_gatt_write_without_response_pl");
    return retval;
}

static uint8_t cp_notify_func
               (
                   struct bt_conn                  *conn,
                   struct bt_gatt_subscribe_params *params,
                   const void                      *data,
                   uint16_t                         length
               )
{
    BT_ESL_BD_ADDR bd_addr;

    if (conn != NULL)
    {
        /* Copy the peer address */
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

        /* Inform upper layer with the result */
        BT_esl_ap_notification_handler(&bd_addr, (UINT16)params->value_handle, (UCHAR *)data, (UINT16)length);
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Connection parameter is NULL in GATT write callback");
    }

    return BT_GATT_ITER_CONTINUE;
}

static void cp_subscribe_cb
            (
                struct bt_conn                  * conn,
                uint8_t                           err,
                struct bt_gatt_subscribe_params * params
            )
{
    BT_ESL_BD_ADDR bd_addr;

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);

    if (conn != NULL)
    {
        /* Copy the peer address */
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

        if (err)
        {
            ESL_PL_ERR("[ESL PL]: CCCD write failed (err %d)", err);
        }
        else
        {
            ESL_PL_TRC("[ESL PL]: CCCD write successful for handle 0x%04x", params->value_handle);
        }

        /* Inform upper layer with the result */
        BT_esl_ap_write_response_handler(&bd_addr, (UINT16)params->ccc_handle, (UCHAR)err);
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Connection parameter is NULL in CCCD subscribe callback");
    }
}

/**
 * \brief Writes to a CCCD (Client Characteristic Configuration Descriptor).
 *
 * \par This function initiates a GATT write operation for a specific CCCD handle
 * on a device specified by its Bluetooth address.
 *
 * \param peer_addr Pointer to the Bluetooth address of the device to write the CCCD.
 * \param attr_handle The handle of the Attribute
 * \param cccd_handle The handle of the CCCD to write.
 * \param cccd_value The value to write to the CCCD (e.g., enable notifications or indications).
 *
 * \return BT_ESL_API_SUCCESS if the write operation starts successfully, BT_ESL_API_FAILURE otherwise.\
 */
API_RESULT BT_esl_gatt_write_cccd_pl
           (
               BT_ESL_BD_ADDR * peer_addr,
               UINT16           attr_handle,
               UINT16           cccd_handle,
               UINT16           cccd_value
           )
{
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;
    int err;
    API_RESULT retval;
    static struct bt_gatt_subscribe_params subscribe_params;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

    ESL_PL_TRC("[ESL PL]: -> BT_esl_gatt_write_cccd_pl");

    if ((peer_addr == NULL) || (attr_handle == BT_ESL_AP_ATTR_HANDLE_INIT_VAL))
    {
        ESL_PL_ERR("[ESL PL]: Invalid parameters for CCCD write");
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        /* Copy peer address */
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)peer_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)peer_addr->type);

        /* Get connection object */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);

        if (conn != NULL)
        {

            subscribe_params.ccc_handle = cccd_handle;
            subscribe_params.value_handle = attr_handle;
            subscribe_params.value = cccd_value;
            subscribe_params.notify = cp_notify_func;
            subscribe_params.subscribe = cp_subscribe_cb;

            err = bt_gatt_subscribe(conn, &subscribe_params);
            /* Release the reference after the operation */
            bt_conn_unref(conn);

            if (err)
            {
                ESL_PL_ERR("[ESL PL]: Failed to write CCCD (err %d)", err);
                retval = BT_ESL_API_FAILURE;
            }
            else
            {
                ESL_PL_TRC("[ESL PL]: CCCD write performed for handle 0x%04x with value 0x%04x", attr_handle, cccd_value);
            }
        }
        else
        {
            ESL_PL_ERR(
            "[ESL PL]: Connection not found for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
            BT_ESL_DEVICE_ADDR_PRINT_STR(peer_addr));

            retval = BT_ESL_API_FAILURE;
        }
    }

    ESL_PL_TRC("[ESL PL]: <- BT_esl_gatt_write_cccd_pl");
    return retval;
}

#endif /* BT_ESL_SUPPORT_AP_ROLE */

#ifdef BT_ESL_SUPPORT_TAG_ROLE
static ssize_t esl_address_write_handler_pl
               (
                    struct bt_conn            * conn,
                    const struct bt_gatt_attr * attr,
                    const void                * buf,
                    uint16_t                    len,
                    uint16_t                    offset,
                    uint8_t                     flags
               )
{
    BT_ESL_BD_ADDR bd_addr;
    ssize_t        retval;
    API_RESULT     result;

    ESL_PL_TRC ("[ESL PL]: -> esl_address_write_handler_pl");

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);
    retval = len;

    if (conn == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Connection parameter is NULL");
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }

    BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
    BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

    if (0U != (flags & BT_GATT_WRITE_FLAG_PREPARE))
    {
        ESL_PL_TRC ("[ESL PL]: Prepare write: offset %u, len %u", offset, len);
        /* Return 0 to allow long writes */
        return BT_GATT_ERR(BT_ATT_ERR_SUCCESS);
    }

    if ((0U != offset) && (0U != (flags & BT_GATT_WRITE_FLAG_EXECUTE)))
    {
        ESL_PL_ERR ("[ESL PL]: Execute write: invalid offset %u, len %u", offset, len);
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    ESL_PL_TRC ("[ESL PL]: Write: offset %u, len %u", offset, len);
    /* Inform upper layer */
    result = BT_esl_tag_write_request_handler
             (
                &bd_addr,
                BT_ESL_ESL_ADDRESS,
                (UCHAR *)buf,
                (UINT16)len
             );

    if(result != BT_ESL_ATT_SUCCESS)
    {
        BT_ESL_PL_TRANSLATE_ATT_ERROR(retval, result);
    }

    ESL_PL_TRC ("[ESL PL]: <- esl_address_write_handler_pl");

    return retval;
}

static ssize_t sync_key_write_handler_pl
               (
                    struct bt_conn            * conn,
                    const struct bt_gatt_attr * attr,
                    const void                * buf,
                    uint16_t                    len,
                    uint16_t                    offset,
                    uint8_t                     flags
               )
{
    BT_ESL_BD_ADDR bd_addr;
    ssize_t        retval;
    API_RESULT     result;

    ESL_PL_TRC ("[ESL PL]: -> sync_key_write_handler_pl");

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);
    retval = len;

    if (conn == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Connection parameter is NULL");
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }

    BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
    BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

    if (0U != (flags & BT_GATT_WRITE_FLAG_PREPARE))
    {
        ESL_PL_TRC ("[ESL PL]: Prepare write: offset %u, len %u", offset, len);
        /* Return 0 to allow long writes */
        return BT_GATT_ERR(BT_ATT_ERR_SUCCESS);
    }

    if ((0U != offset) && (0U != (flags & BT_GATT_WRITE_FLAG_EXECUTE)))
    {
        ESL_PL_ERR ("[ESL PL]: Execute write: invalid offset %u, len %u", offset, len);
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    ESL_PL_TRC ("[ESL PL]: Write: offset %u, len %u", offset, len);
    /* Inform upper layer */
    result = BT_esl_tag_write_request_handler
             (
                &bd_addr,
                BT_ESL_AP_SYNC_KEY_MATERIAL,
                (UCHAR *)buf,
                (UINT16)len
             );

    if(result != BT_ESL_ATT_SUCCESS)
    {
        BT_ESL_PL_TRANSLATE_ATT_ERROR(retval, result);
    }

    ESL_PL_TRC ("[ESL PL]: <- sync_key_write_handler_pl");

    return retval;
}

static ssize_t response_key_write_handler_pl
               (
                    struct bt_conn            * conn,
                    const struct bt_gatt_attr * attr,
                    const void                * buf,
                    uint16_t                    len,
                    uint16_t                    offset,
                    uint8_t                     flags
               )
{
    BT_ESL_BD_ADDR bd_addr;
    ssize_t        retval;
    API_RESULT     result;

    ESL_PL_TRC ("[ESL PL]: -> response_key_write_handler_pl");

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);
    retval = len;

    if (conn == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Connection parameter is NULL");
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }

    BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
    BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

    if (0U != (flags & BT_GATT_WRITE_FLAG_PREPARE))
    {
        ESL_PL_TRC ("[ESL PL]: Prepare write: offset %u, len %u", offset, len);
        /* Return 0 to allow long writes */
        return BT_GATT_ERR(BT_ATT_ERR_SUCCESS);
    }

    if ((0U != offset) && (0U != (flags & BT_GATT_WRITE_FLAG_EXECUTE)))
    {
        ESL_PL_ERR ("[ESL PL]: Execute write: invalid offset %u, len %u", offset, len);
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    ESL_PL_TRC ("[ESL PL]: Write: offset %u, len %u", offset, len);
    /* Inform upper layer */
    result = BT_esl_tag_write_request_handler
             (
                &bd_addr,
                BT_ESL_RESPONSE_KEY_MATERIAL,
                (UCHAR *)buf,
                (UINT16)len
             );

    if(result != BT_ESL_ATT_SUCCESS)
    {
        BT_ESL_PL_TRANSLATE_ATT_ERROR(retval, result);
    }

    ESL_PL_TRC ("[ESL PL]: <- response_key_write_handler_pl");

    return retval;
}

static ssize_t current_abs_time_write_handler_pl
               (
                    struct bt_conn            * conn,
                    const struct bt_gatt_attr * attr,
                    const void                * buf,
                    uint16_t                    len,
                    uint16_t                    offset,
                    uint8_t                     flags
               )
{
    BT_ESL_BD_ADDR bd_addr;
    ssize_t        retval;
    API_RESULT     result;

    ESL_PL_TRC ("[ESL PL]: -> current_abs_time_write_handler_pl");

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);
    retval = len;

    if (conn == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Connection parameter is NULL");
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }

    BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
    BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

    if (0U != (flags & BT_GATT_WRITE_FLAG_PREPARE))
    {
        ESL_PL_TRC ("[ESL PL]: Prepare write: offset %u, len %u", offset, len);
        /* Return 0 to allow long writes */
        return BT_GATT_ERR(BT_ATT_ERR_SUCCESS);
    }

    if ((0U != offset) && (0U != (flags & BT_GATT_WRITE_FLAG_EXECUTE)))
    {
        ESL_PL_ERR ("[ESL PL]: Execute write: invalid offset %u, len %u", offset, len);
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    ESL_PL_TRC ("[ESL PL]: Write: offset %u, len %u", offset, len);
    /* Inform upper layer */
    result = BT_esl_tag_write_request_handler
              (
                 &bd_addr,
                 BT_ESL_CURRENT_ABSOLUTE_TIME,
                 (UCHAR *)buf,
                 (UINT16)len
              );

    if(result != BT_ESL_ATT_SUCCESS)
    {
        BT_ESL_PL_TRANSLATE_ATT_ERROR(retval, result);
    }

    ESL_PL_TRC ("[ESL PL]: <- current_abs_time_write_handler_pl");

    return retval;
}

static ssize_t control_point_write_handler_pl
               (
                    struct bt_conn            * conn,
                    const struct bt_gatt_attr * attr,
                    const void                * buf,
                    uint16_t                    len,
                    uint16_t                    offset,
                    uint8_t                     flags
               )
{
    BT_ESL_BD_ADDR bd_addr;
    ssize_t        retval;
    API_RESULT     result;

    ESL_PL_TRC ("[ESL PL]: -> control_point_write_handler_pl");

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);
    retval = len;

    if (conn == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Connection parameter is NULL");
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }

    BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
    BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

    ESL_PL_TRC ("[ESL PL]: Write: offset %u, len %u", offset, len);
    /* Inform upper layer */
    result = BT_esl_tag_write_request_handler
              (
                 &bd_addr,
                 BT_ESL_CONTROL_POINT,
                 (UCHAR *)buf,
                 (UINT16)len
              );

    if(result != BT_ESL_ATT_SUCCESS)
    {
        BT_ESL_PL_TRANSLATE_ATT_ERROR(retval, result);
    }

    ESL_PL_TRC ("[ESL PL]: <- control_point_write_handler_pl");

    return retval;
}

static ssize_t control_point_cccd_write
               (
                   struct bt_conn *conn,
                   const struct bt_gatt_attr *attr,
                   uint16_t value
               )
{
    BT_ESL_BD_ADDR  bd_addr;
    UCHAR           att_err;

    ESL_PL_TRC ("[ESL PL]: -> control_point_cccd_write");

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);

    if (conn != NULL)
    {
        /* Copy peer address */
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: conn parameter is NULL");
    }

    ESL_PL_TRC ("[ESL PL]: CCCD value write: 0x%04x", value);

    /* Inform upper layer */
    att_err = BT_esl_tag_write_cccd_handler
              (
                 &bd_addr,
                 BT_ESL_CONTROL_POINT,
                 (UINT16)value
              );

    ESL_PL_TRC ("[ESL PL]: <- control_point_cccd_write");

    return sizeof(value);
}

static ssize_t display_info_read_handler
               (
                    struct bt_conn *conn,
                    const struct bt_gatt_attr *attr,
                    void * buf,
                    uint16_t len,
                    uint16_t offset
               )
{
    BT_ESL_BD_ADDR bd_addr;
    ssize_t        retval;
    API_RESULT     result;
    UINT16         temp_len;

    ESL_PL_TRC ("[ESL PL]: -> display_info_read_handler");

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);
    retval = 0;
    temp_len = 0;

    if (conn == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Connection parameter is NULL");
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }

    BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
    BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

    ESL_PL_TRC ("[ESL PL]: Write: offset %u, len %u", offset, len);
    /* Inform upper layer */
    result = BT_esl_tag_read_request_handler
             (
                 &bd_addr,
                 BT_ESL_GATT_ESL_SERVICE,
                 BT_ESL_DISPLAY_INFO_TYPE,
                 (UINT16)offset,
                 (UINT16)len,
                 (UCHAR *)buf,
                 &temp_len
             );

    if(result != BT_ESL_ATT_SUCCESS)
    {
        BT_ESL_PL_TRANSLATE_ATT_ERROR(retval, result);
    }
    else
    {
        /* If the read was successful, update the length */
        retval = (ssize_t)temp_len;
    }

    ESL_PL_TRC ("[ESL PL]: <- display_info_read_handler");

    return retval;
}

static ssize_t image_info_read_handler
               (
                    struct bt_conn *conn,
                    const struct bt_gatt_attr *attr,
                    void * buf,
                    uint16_t len,
                    uint16_t offset
               )
{
    BT_ESL_BD_ADDR bd_addr;
    ssize_t        retval;
    API_RESULT     result;
    UINT16         temp_len;

    ESL_PL_TRC ("[ESL PL]: -> image_info_read_handler");

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);
    retval = 0;
    temp_len = 0;

    if (conn == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Connection parameter is NULL");
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }

    BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
    BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

    ESL_PL_TRC ("[ESL PL]: Write: offset %u, len %u", offset, len);
    /* Inform upper layer */
    result = BT_esl_tag_read_request_handler
              (
                 &bd_addr,
                 BT_ESL_GATT_ESL_SERVICE,
                 BT_ESL_IMAGE_INFO_TYPE,
                 (UINT16)offset,
                 (UINT16)len,
                 (UCHAR *)buf,
                 &temp_len
              );

    if(result != BT_ESL_ATT_SUCCESS)
    {
        BT_ESL_PL_TRANSLATE_ATT_ERROR(retval, result);
    }
    else
    {
        /* If the read was successful, update the length */
        retval = (ssize_t)temp_len;
    }

    ESL_PL_TRC ("[ESL PL]: <- image_info_read_handler");

    return retval;
}

static ssize_t sensor_info_read_handler
               (
                    struct bt_conn *conn,
                    const struct bt_gatt_attr *attr,
                    void * buf,
                    uint16_t len,
                    uint16_t offset
               )
{
    BT_ESL_BD_ADDR bd_addr;
    ssize_t        retval;
    API_RESULT     result;
    UINT16         temp_len;

    ESL_PL_TRC ("[ESL PL]: -> sensor_info_read_handler");

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);
    retval = 0;
    temp_len = 0;

    if (conn == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Connection parameter is NULL");
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }

    BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
    BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

    ESL_PL_TRC ("[ESL PL]: Write: offset %u, len %u", offset, len);
    /* Inform upper layer */
    result = BT_esl_tag_read_request_handler
             (
                 &bd_addr,
                 BT_ESL_GATT_ESL_SERVICE,
                 BT_ESL_SENSOR_INFO_TYPE,
                 (UINT16)offset,
                 (UINT16)len,
                 (UCHAR *)buf,
                 &temp_len
             );

    if(result != BT_ESL_ATT_SUCCESS)
    {
        BT_ESL_PL_TRANSLATE_ATT_ERROR(retval, result);
    }
    else
    {
        /* If the read was successful, update the length */
        retval = (ssize_t)temp_len;
    }

    ESL_PL_TRC ("[ESL PL]: <- sensor_info_read_handler");

    return retval;
}

static ssize_t led_info_read_handler
               (
                    struct bt_conn *conn,
                    const struct bt_gatt_attr *attr,
                    void * buf,
                    uint16_t len,
                    uint16_t offset
               )
{
    BT_ESL_BD_ADDR bd_addr;
    ssize_t        retval;
    API_RESULT     result;
    UINT16         temp_len;

    ESL_PL_TRC ("[ESL PL]: -> led_info_read_handler");

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);
    retval = 0;
    temp_len = 0;

    if (conn == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Connection parameter is NULL");
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }

    BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
    BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

    ESL_PL_TRC ("[ESL PL]: Write: offset %u, len %u", offset, len);
    /* Inform upper layer */
    result = BT_esl_tag_read_request_handler
             (
                 &bd_addr,
                 BT_ESL_GATT_ESL_SERVICE,
                 BT_ESL_LED_INFO_TYPE,
                 (UINT16)offset,
                 (UINT16)len,
                 (UCHAR *)buf,
                 &temp_len
             );

    if(result != BT_ESL_ATT_SUCCESS)
    {
        BT_ESL_PL_TRANSLATE_ATT_ERROR(retval, result);
    }
    else
    {
        /* If the read was successful, update the length */
        retval = (ssize_t)temp_len;
    }

    ESL_PL_TRC ("[ESL PL]: <- led_info_read_handler");

    return retval;
}

void control_point_ntf_complete (struct bt_conn *conn, void *user_data)
{
    BT_ESL_BD_ADDR bd_addr;

    if (conn == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Connection parameter is NULL in control point notification complete");
        return;
    }

    /* Copy the peer address */
    BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
    BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

    ESL_PL_TRC(
    "[ESL PL]: Control Point Notification complete for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
    BT_ESL_DEVICE_ADDR_PRINT_STR(&bd_addr));

    /* Callback to core is empty */
}

static void BT_esl_ntf_timeout_handler_pl(void * t_data, UINT16 datalen)
{
    int err;
    UCHAR * ntf_data;
    BT_ESL_NTF_PARAMS_PL * ntf_params;

    ntf_params = (BT_ESL_NTF_PARAMS_PL *)t_data;
    if (NULL != ntf_params)
    {
        err = bt_gatt_notify_cb(ntf_params->conn, &ntf_params->params);
        if (err)
        {
            ESL_PL_ERR("[ESL PL]: Notification failed (err %d)", err);
        }
        else
        {
            ESL_PL_TRC(
            "[ESL PL]: Notification sent successfully");
        }

        /* Free mem */
        ntf_data = (UCHAR *)ntf_params->params.data;
        BT_ESL_free_mem(ntf_data);

        /* Un-reference the Connection */
        bt_conn_unref(ntf_params->conn);
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: NULL params received");
    }
}

API_RESULT BT_esl_gatt_notify_pl
           (
               BT_ESL_BD_ADDR * peer_addr,
               UCHAR          * data,
               UINT16           datalen
           )
{
    API_RESULT retval;
    struct bt_conn *conn;
    struct bt_gatt_notify_params params;
    bt_addr_le_t conn_addr;
    UCHAR * ntf_data;
    BT_ESL_NTF_PARAMS_PL ntf_params;

    /* Init */
    retval = BT_ESL_API_SUCCESS;
    conn = NULL;
    BT_ESL_mem_set(&params, 0, sizeof(params));
    BT_ESL_mem_set(&conn_addr, 0, sizeof(conn_addr));

    if (NULL != peer_addr)
    {
        /* Copy peer address */
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)peer_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)peer_addr->type);

        /* Get connection object */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);

        if (conn == NULL)
        {
            ESL_PL_ERR("[ESL PL]: Connection not found for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
                    BT_ESL_DEVICE_ADDR_PRINT_STR(peer_addr));
            retval = BT_ESL_API_FAILURE;
        }
        else
        {
            params.attr = bt_gatt_find_by_uuid
                          (
                                esl_svc.attrs,
                                esl_svc.attr_count,
                                BT_UUID_DECLARE_16(GATT_ESL_CONTROL_POINT_CHARACTERISTIC)
                          );
            ntf_data = (UCHAR *)BT_ESL_alloc_mem(sizeof(UCHAR) * datalen);
            if (ntf_data == NULL)
            {
                ESL_PL_ERR("[ESL PL]: Failed to allocate memory for notification data");
                retval = BT_ESL_API_FAILURE;
            }
            else
            {
                BT_ESL_mem_copy(ntf_data, data, datalen);
                params.data = ntf_data;
                params.len = (uint16_t)datalen;
                params.func = control_point_ntf_complete;

                /*err = bt_gatt_notify_cb(conn, &params);*/
                /**
                 * Start timer for configuration
                 */
                ntf_params.conn = conn;
                ntf_params.params = params;
                retval = BT_ESL_start_timer
                         (
                             &cp_ntf_timer_handle,
                             BT_ESL_CP_NTF_INTERVAL,
                             BT_esl_ntf_timeout_handler_pl,
                             &ntf_params,
                             sizeof(ntf_params)
                         );
                if (BT_ESL_API_SUCCESS != retval)
                {
                    ESL_PL_ERR("[ESL PL]: Failed to start notification timer (err %d)", retval);
                }
                else
                {
                    ESL_PL_TRC(
                    "[ESL PL]: Notification Timer started successfully");
                }
            }

            if (BT_ESL_API_SUCCESS != retval)
            {
                /* Un-reference the Connection */
                bt_conn_unref(conn);
            }
        }
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Invalid peer address");
        retval = BT_ESL_API_FAILURE;
    }

    return retval;

}

#endif /* BT_ESL_SUPPORT_TAG_ROLE */

/** Callbacks for pairing complete */
static void esl_pairing_complete(struct bt_conn *conn, bool bonded)
{
    BT_ESL_BD_ADDR bd_addr;

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);

    if(conn != NULL)
    {
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)bt_conn_get_dst(conn)->a.val);
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);

        ESL_PL_TRC(
        "[ESL PL]: Pairing complete "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER" (bonded %d)",
        BT_ESL_DEVICE_ADDR_PRINT_STR(&bd_addr), bonded);

        if(BT_ESL_TRUE == bonded)
        {
            smp_bonded_info.bonded = BT_ESL_TRUE;
            BT_ESL_COPY_BD_ADDR(smp_bonded_info.bd_addr.addr, bd_addr.addr);
            BT_ESL_COPY_TYPE(smp_bonded_info.bd_addr.type, bd_addr.type);
        }
    }
}

/** Callback for pairing failed */
static void esl_pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
    BT_ESL_BD_ADDR bd_addr;

    if(conn != NULL)
    {
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)bt_conn_get_dst(conn)->a.val);
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);
        ESL_PL_ERR(
        "[ESL PL]: Pairing failed "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"(reason %d)",
        BT_ESL_DEVICE_ADDR_PRINT_STR(&bd_addr), reason);
    }
}

/** Callback for bond deleted */
static void esl_bond_deleted(uint8_t id, const bt_addr_le_t *peer)
{
    BT_ESL_BD_ADDR bd_addr;

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);

    if (NULL != peer)
    {
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)peer->a.val);
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)peer->type);

        ESL_PL_TRC("[ESL PL]: Bond deleted for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
                BT_ESL_DEVICE_ADDR_PRINT_STR(&bd_addr));

        if (BT_ESL_COMPARE_BD_ADDR_AND_TYPE(&smp_bonded_info.bd_addr, &bd_addr) == BT_ESL_TRUE)
        {
            /* Reset the bonded info */
            smp_bonded_info.bonded = BT_ESL_FALSE;
            BT_ESL_INIT_BD_ADDR(&smp_bonded_info.bd_addr);
            ESL_PL_TRC("[ESL PL]: Bond information reset");
        }
        else
        {
            ESL_PL_ERR("[ESL PL]: Bond deleted called with different peer address");
            return;
        }

    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Bond deleted called with NULL peer address");
    }
}

/**
 * \brief Checks if a device is bonded.
 *
 * \par This function checks if the device with the given Bluetooth address is bonded.
 *
 * \param bd_addr Pointer to the Bluetooth address of the device to check.
 * \param is_bonded BT_ESL_TRUE if the device is bonded, BT_ESL_FALSE otherwise.
 */
API_RESULT BT_esl_device_is_bonded_pl
           (
               /* IN */  BT_ESL_BD_ADDR * bd_addr,
               /* OUT */ UCHAR          * is_bonded
           )
{
    API_RESULT retval;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    if (bd_addr == NULL || is_bonded == NULL)
    {
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        /** Initialized bonded flag to false */
        *is_bonded = BT_ESL_FALSE;

        /* Compare bd address */
        if (BT_ESL_TRUE == BT_ESL_COMPARE_BD_ADDR_AND_TYPE(&smp_bonded_info.bd_addr, bd_addr))
        {
            if(smp_bonded_info.bonded == BT_ESL_TRUE)
            {
                *is_bonded = BT_ESL_TRUE;
            }
        }
    }

    return retval;
}

/** Register SMP authentication info callbacks */
void esl_register_auth_info_cb(void)
{
    int err;

    /* Initialize the callback structure */
    smp_cb.pairing_complete = esl_pairing_complete;
    smp_cb.pairing_failed = esl_pairing_failed;
    smp_cb.bond_deleted = esl_bond_deleted;

    /* Register the callbacks */
    err = bt_conn_auth_info_cb_register(&smp_cb);
    if (err)
    {
        ESL_PL_ERR("[ESL PL]: Failed to register auth info callbacks (err %d)", err);
    }
    else
    {
        ESL_PL_TRC("[ESL PL]: Auth info callbacks registered successfully");
    }
}

/**
 * \brief Gets the current absolute time in milliseconds.
 *
 * \par This function fetches the current absolute time..
 *
 * \return The current absolute time in milliseconds as a UINT32.
 */
UINT32 BT_esl_get_current_time_pl(void)
{
    BT_ESL_time_type time;

    BT_ESL_get_current_time(&time);

    return (UINT32)(time - anchor_time);
}

/**
 * \brief Sets the current absolute time.
 *
 * \par This function sets the current absolute time.
 *
 * \param current_absolute_time The absolute time in milliseconds to set.
 */
void BT_esl_set_current_time_pl(UINT32 current_absolute_time)
{
    BT_ESL_time_type time;

    BT_ESL_get_current_time(&time);

    anchor_time = (INT64)time - (INT64)current_absolute_time;
}

#ifdef BT_ESL_SUPPORT_AP_ROLE
#if defined(CONFIG_BT_OTS)
/**
 * \brief Upload image to ESL by Image_Index
 *
 * \par This function uploads image data to the ESL image slot specified by image_index.
 * It reads the Max_Image_Index from the ESL Image Information characteristic, checks bounds,
 * selects the corresponding OTS object, and writes the image data.
 *
 * \param conn Pointer to the active connection.
 * \param image_index Index of the image slot to upload to (0..Max_Image_Index).
 * \param image_data Pointer to the image data buffer.
 * \param image_len Length of the image data buffer.
 * \return BT_ESL_API_SUCCESS if upload started, BT_ESL_API_FAILURE otherwise.
 */
API_RESULT BT_esl_upload_image_pl
           (
               BT_ESL_BD_ADDR * bd_addr,
               UCHAR image_index,
               const UCHAR * image_data,
               UINT16 image_len
           )
{
    int err;
    API_RESULT retval;
    uint64_t obj_id;
    bt_addr_le_t conn_addr;
    struct bt_conn *conn;
    uint8_t conn_idx;

    /* Init */
    retval = BT_ESL_API_SUCCESS;
    obj_id = BT_OTS_OBJ_ID_MIN + image_index;

    if ((NULL != bd_addr) && (NULL != image_data) && (image_len > 0))
    {
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)bd_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)bd_addr->type);

        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);
        if (conn != NULL)
        {
            conn_idx = bt_conn_index(conn);
            /* Check if already active */
            if (BT_ESL_FALSE != ots_upload_params.active)
            {
                ESL_PL_ERR("[ESL PL]: OTS upload already in progress");
                retval = BT_ESL_API_FAILURE;
            }
            else
            {
                /* Select the OTS upload params */
                ots_upload_params.image_data = image_data;
                ots_upload_params.image_len = image_len;
                ots_upload_params.active = true;
                ots_upload_params.obj_id = obj_id;

                /* Send OLCP go to command to select the object */
                err = bt_ots_client_select_id
                      (
                          &ots_client[conn_idx],
                          conn,
                          obj_id
                      );
                if (0 != err)
                {
                    ESL_PL_ERR("OTS client select obj id (%d) failed (%d)", (uint32_t)obj_id, err);
                    ESL_PL_ERR("#OTS_WRITE:0x%02X,Tag Img IDX out of range", image_index);
                    retval = BT_ESL_API_FAILURE;
                    ots_upload_params.active = false;
                }
            }

            bt_conn_unref(conn);
        }
        else
        {
            ESL_PL_ERR("Connection not found for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
            BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr));

            retval = BT_ESL_API_FAILURE;
        }
    }
    else
    {
        retval = BT_ESL_API_FAILURE;
    }

    return retval;
}

/* OTS object selected callback */
static void ots_obj_selected_cb(struct bt_ots_client *ots_inst, struct bt_conn *conn, int err)
{
    int temp_err;
    BT_ESL_BD_ADDR bd_addr;

    ESL_PL_TRC("[ESL PL]: OTS object selected, err %d", err);

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);

    if (NULL !=  conn)
    {
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);
    }

    if (err == 1)
    {
        /* After selection, read metadata (optional but robust) */
        temp_err = bt_ots_client_read_object_metadata(ots_inst, conn, BT_OTS_METADATA_REQ_ALL);
        if (0 != temp_err)
        {
            ESL_PL_ERR("[ESL PL]: OTS read_object_metadata failed (err %d)", temp_err);
            /* Still try to write if metadata read fails */
            temp_err = bt_ots_client_write_object_data
                       (
                           ots_inst,
                           conn,
                           ots_upload_params.image_data,
                           ots_upload_params.image_len,
                           0,
                           BT_OTS_OACP_WRITE_OP_MODE_NONE
                       );
            if (0 != temp_err)
            {
                ESL_PL_ERR("[ESL PL]: OTS write_object_data failed (err %d)", temp_err);
                ots_upload_params.active = false;
                /* Inform upper layer */
                if (NULL != ots_callback.image_upload_complete)
                {
                    ots_callback.image_upload_complete(&bd_addr, OTS_IMAGE_UPLOAD_WRITE_STATE, NULL);
                }
            }
        }
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: OTS object select failed (err %d)", err);
        ots_upload_params.active = false;
        /* Inform upper layer */
        if (NULL != ots_callback.image_upload_complete)
        {
            ots_callback.image_upload_complete(&bd_addr, OTS_IMAGE_UPLOAD_OBJ_SELECTION_STATE, NULL);
        }
    }
}

/* OTS object metadata read callback */
static void ots_obj_metadata_read_cb(struct bt_ots_client *ots_inst, struct bt_conn *conn, int err, uint8_t metadata_read)
{
    int temp_err;
    BT_ESL_BD_ADDR bd_addr;

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);

    ESL_PL_TRC("[ESL PL]: OTS object metadata read, err %d", err);

    if (NULL != conn)
    {
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);
    }

    if (err == 0)
    {
        /* Optionally validate metadata here */
        ESL_PL_TRC("[ESL PL]: OTS object metadata read, proceeding to write");
        bt_ots_metadata_display(&ots_inst->cur_object, 1);
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: OTS object metadata read failed (err %d)", err);
    }

    temp_err = bt_ots_client_write_object_data(ots_inst, conn, ots_upload_params.image_data, ots_upload_params.image_len, 0, BT_OTS_OACP_WRITE_OP_MODE_NONE);
    if (0 != temp_err)
    {
        ESL_PL_ERR("[ESL PL]: OTS write_object_data failed (err %d)", temp_err);
        ots_upload_params.active = false;
        /* Inform upper layer */
        if (NULL != ots_callback.image_upload_complete)
        {
            ots_callback.image_upload_complete(&bd_addr, OTS_IMAGE_UPLOAD_WRITE_STATE, NULL);
        }
    }
}

/* OTS object data written callback */
static void ots_obj_data_written_cb(struct bt_ots_client *ots_inst, struct bt_conn *conn, size_t len)
{
    BT_ESL_BD_ADDR bd_addr;

    /* Init */
    BT_ESL_INIT_BD_ADDR(&bd_addr);

    ESL_PL_TRC("[ESL PL]: Image upload complete, bytes written: %u", (unsigned)len);

    if (NULL != conn)
    {
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);
    }

    ots_upload_params.active = false;
    /* Inform upper layer */
    if (NULL != ots_callback.image_upload_complete)
    {
        ots_callback.image_upload_complete(&bd_addr, OTS_IMAGE_UPLOAD_COMPLETED, NULL);
    }
}

void BT_esl_ots_client_init_pl(void)
{
    int i;
    BT_ESL_mem_set(&ots_upload_params, 0, sizeof(ots_upload_params));
    BT_ESL_mem_set(ots_client, 0, sizeof(ots_client));

    /* OTS Client Callbacks */
    ots_client_cbs.obj_selected = ots_obj_selected_cb;
    ots_client_cbs.obj_metadata_read = ots_obj_metadata_read_cb;
    ots_client_cbs.obj_data_written = ots_obj_data_written_cb;
    for (i = 0; i < CONFIG_BT_MAX_CONN; i++)
    {
        ots_client[i].cb = &ots_client_cbs;
        bt_ots_client_register(&ots_client[i]);
    }
}

static bool is_discovery_complete(uint8_t conn_idx)
{
    return (atomic_test_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_FEATURE_BIT) &&
        atomic_test_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_NAME_BIT) &&
        atomic_test_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_TYPE_BIT) &&
        atomic_test_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_SIZE_BIT) &&
        atomic_test_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_ID_BIT) &&
        atomic_test_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_PROPERTIES_BIT) &&
        atomic_test_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_ACTION_CP_BIT));
}

void update_ots_attr_handle_pl(const struct bt_uuid *uuid, uint16_t handle, uint8_t conn_idx)
{
    if (NULL == uuid)
    {
        ESL_PL_ERR("[ESL PL]: Invalid parameters: uuid is NULL");
        return;
    }

    if (0 == bt_uuid_cmp(BT_UUID_OTS_FEATURE, uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating OTS Feature handle: 0x%04X", handle);
        ots_client[conn_idx].feature_handle = (UINT16)handle;
        atomic_set_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_FEATURE_BIT);
    }
    else if (0 == bt_uuid_cmp(BT_UUID_OTS_NAME, uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating OTS Name handle: 0x%04X", handle);
        ots_client[conn_idx].obj_name_handle = (UINT16)handle;
        atomic_set_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_NAME_BIT);
    }
    else if (0 == bt_uuid_cmp(BT_UUID_OTS_TYPE, uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating OTS Type handle: 0x%04X", handle);
        ots_client[conn_idx].obj_type_handle = (UINT16)handle;
        atomic_set_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_TYPE_BIT);
    }
    else if (0 == bt_uuid_cmp(BT_UUID_OTS_SIZE, uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating OTS Size handle: 0x%04X", handle);
        ots_client[conn_idx].obj_size_handle = (UINT16)handle;
        atomic_set_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_SIZE_BIT);
    }
    else if (0 == bt_uuid_cmp(BT_UUID_OTS_ID, uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating OTS ID handle: 0x%04X", handle);
        ots_client[conn_idx].obj_id_handle = (UINT16)handle;
        atomic_set_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_ID_BIT);
    }
    else if (0 == bt_uuid_cmp(BT_UUID_OTS_PROPERTIES, uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating OTS Properties handle: 0x%04X", handle);
        ots_client[conn_idx].obj_properties_handle = (UINT16)handle;
        atomic_set_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_PROPERTIES_BIT);
    }
    else if (0 == bt_uuid_cmp(BT_UUID_OTS_ACTION_CP, uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating OTS Action Control Point handle: 0x%04X", handle);
        ots_client[conn_idx].oacp_handle = (UINT16)handle;
        atomic_set_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_ACTION_CP_BIT);
    }
    else if (0 == bt_uuid_cmp(BT_UUID_OTS_LIST_CP, uuid))
    {
        ESL_PL_TRC ("[ESL PL]: Updating OTS List Control Point handle: 0x%04X", handle);
        ots_client[conn_idx].olcp_handle = (UINT16)handle;
        atomic_set_bit(&ots_discovery_session[conn_idx].state, DISC_OTS_LIST_CP_BIT);
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Unhandled UUID encountered: ");
        print_uuid_pl(uuid);
    }
}
static uint8_t ots_discovery_cb_pl
               (
                    struct bt_conn *conn,
                    const struct bt_gatt_attr *attr,
                    struct bt_gatt_discover_params *params
               )
{
    uint8_t conn_idx;
    int err;
    uint8_t retval;
    BT_ESL_BD_ADDR bd_addr;

    /* Init */
    retval = BT_GATT_ITER_CONTINUE;
    BT_ESL_mem_set(&bd_addr, 0x00U, sizeof(bd_addr));

    /* Validate connection before any dereference (bt_conn_index requires non-NULL) */
    if (NULL == conn)
    {
        ESL_PL_ERR("[ESL PL]: Discovery callback received NULL conn");
        return BT_GATT_ITER_STOP;
    }

    /* Get connection index and validate bounds */
    conn_idx = bt_conn_index(conn);
    if (conn_idx >= CONFIG_BT_MAX_CONN)
    {
        ESL_PL_ERR("[ESL PL]: Invalid connection index %d", conn_idx);
        return BT_GATT_ITER_STOP;
    }

    /* Copy the peer address (guard bt_conn_get_dst() return) */
    if (NULL != bt_conn_get_dst(conn))
    {
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);
    }

    /* Check if params is NULL */
    if (params == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Discovery callback received NULL params");
        /* Reset discovery_in_progress */
        ots_discovery_session[conn_idx].discovery_in_progress = BT_ESL_FALSE;
        if (NULL != ots_callback.discovery_complete)
        {
            ots_callback.discovery_complete(&bd_addr, BT_ESL_API_FAILURE);
        }
        return BT_GATT_ITER_STOP;
    }

    switch (params->type)
    {
    case BT_GATT_DISCOVER_PRIMARY:
    {
        struct bt_gatt_service_val *svc;

        /* Zephyr passes NULL attr when discovery is exhausted (service not found) */
        if ((NULL == attr) || (NULL == attr->user_data))
        {
            ESL_PL_TRC ("[ESL PL]: OTS primary service not found");
            ots_discovery_session[conn_idx].discovery_in_progress = BT_ESL_FALSE;
            if (NULL != ots_callback.discovery_complete)
            {
                ots_callback.discovery_complete(&bd_addr, BT_ESL_API_FAILURE);
            }
            retval = BT_GATT_ITER_STOP;
            break;
        }

        /* Handle service discovery */
        svc = attr->user_data;
        ESL_PL_TRC ("[ESL PL]: Found Service:");
        ESL_PL_TRC ("[ESL PL]:   Start Handle: 0x%04x", attr->handle);
        ESL_PL_TRC ("[ESL PL]:   End Handle: 0x%04x", svc->end_handle);
        print_uuid_pl(svc->uuid);

        ots_client[conn_idx].start_handle = attr->handle;
        ots_client[conn_idx].end_handle = svc->end_handle;
        /* Update discovery parameters to find characteristics within this service */
        ots_discovery_session[conn_idx].discover_params.uuid = NULL;
        ots_discovery_session[conn_idx].discover_params.start_handle = attr->handle;
        ots_discovery_session[conn_idx].discover_params.end_handle = svc->end_handle;
        ots_discovery_session[conn_idx].discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &ots_discovery_session[conn_idx].discover_params);
        if (err)
        {
            ESL_PL_TRC ("[ESL PL]: Failed to start characteristic discovery (err %d)", err);
            /* Reset discovery_in_progress */
            ots_discovery_session[conn_idx].discovery_in_progress = BT_ESL_FALSE;
            if (NULL != ots_callback.discovery_complete)
            {
                ots_callback.discovery_complete(&bd_addr, BT_ESL_API_FAILURE);
            }
        }
        retval = BT_GATT_ITER_STOP;
        break;
    }
    case BT_GATT_DISCOVER_CHARACTERISTIC:
    {
        struct bt_gatt_chrc *chrc;
        /* Checking CHAR discovery is completed or not */
        if ((NULL != attr) && (NULL != attr->user_data)
            && (params->start_handle < params->end_handle))
        {
            /* Handle characteristic discovery */
            chrc = attr->user_data;
            ESL_PL_TRC ("[ESL PL]: Found Characteristic:");
            ESL_PL_TRC ("[ESL PL]:   Handle: 0x%04x", attr->handle);
            ESL_PL_TRC ("[ESL PL]:   Properties: 0x%02x", chrc->properties);
            if (NULL != chrc->uuid)
            {
                print_uuid_pl(chrc->uuid);

                /* Store the handles */
                update_ots_attr_handle_pl
                (
                    chrc->uuid,
                    bt_gatt_attr_value_handle(attr),
                    conn_idx
                );
            }
        }
        else
        {
            ots_discovery_session[conn_idx].discovery_in_progress = BT_ESL_FALSE;
            /* Discover cccd of control point char */
            if (is_discovery_complete(conn_idx))
            {
                /* Discovery completed */
                ESL_PL_TRC("[ESL PL]: OTS Discovery Completed");
                if (NULL != ots_callback.discovery_complete)
                {
                    ots_callback.discovery_complete(&bd_addr, BT_ESL_API_SUCCESS);
                }
            }
            else
            {
                /* Discovery process failed */
                ESL_PL_TRC("[ESL PL]: OTS Discovery failed");
                if (NULL != ots_callback.discovery_complete)
                {
                    ots_callback.discovery_complete(&bd_addr, BT_ESL_API_FAILURE);
                }
            }
            retval = BT_GATT_ITER_STOP;
        }
        break;
    }
    default:
    {
        ESL_PL_TRC ("[ESL PL]: Unknown discovery type: %d", params->type);
        /* Reset discovery_in_progress */
        ots_discovery_session[conn_idx].discovery_in_progress = BT_ESL_FALSE;
        if (NULL != ots_callback.discovery_complete)
        {
            ots_callback.discovery_complete(&bd_addr, BT_ESL_API_FAILURE);
        }
        retval = BT_GATT_ITER_STOP;
    }
    }

    return retval;
}

API_RESULT BT_esl_discover_ots_pl(BT_ESL_BD_ADDR * bd_addr)
{
    uint8_t conn_idx;
    API_RESULT retval;
    int err;
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

    if (bd_addr != NULL)
    {
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)bd_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)bd_addr->type);

        /* Get conn param */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);

        if (conn != NULL)
        {
            conn_idx = bt_conn_index(conn);

            /* Ensure the connection index is valid */
            if (conn_idx >= CONFIG_BT_MAX_CONN)
            {
                ESL_PL_ERR (
                "[ESL PL]: Invalid connection index: %d", conn_idx);
                return BT_ESL_API_FAILURE;
            }

            /* Check if discovery is already in progress for this connection */
            if (ots_discovery_session[conn_idx].discovery_in_progress)
            {
                ESL_PL_ERR (
                "[ESL PL]: Service discovery already in progress for connection index %d", conn_idx);
                return BT_ESL_API_FAILURE;
            }

            /* Mark discovery as in progress for this connection */
            ots_discovery_session[conn_idx].discovery_in_progress = BT_ESL_TRUE;

            /* Clear atomic state */
            atomic_clear(&ots_discovery_session[conn_idx].state);

            /* Set up discovery parameters */
            ots_discovery_session[conn_idx].discover_params.uuid = BT_UUID_OTS;
            ots_discovery_session[conn_idx].discover_params.func = ots_discovery_cb_pl;
            ots_discovery_session[conn_idx].discover_params.start_handle = 0x0001;
            ots_discovery_session[conn_idx].discover_params.end_handle = 0xffff;
            ots_discovery_session[conn_idx].discover_params.type = BT_GATT_DISCOVER_PRIMARY;

            /* Start service discovery */
            err = bt_gatt_discover(conn, &ots_discovery_session[conn_idx].discover_params);
            bt_conn_unref(conn);
            if (err)
            {
                ESL_PL_ERR ("[ESL PL]: OTS Service discovery failed to start (err %d)", err);
                ots_discovery_session[conn_idx].discovery_in_progress = BT_ESL_FALSE;
            }
            else
            {
                ESL_PL_TRC("[ESL PL]: OTS discovery started for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
                BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr));
            }
        }
        else
        {
            ESL_PL_ERR ("[ESL PL]: Connection not found for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
            BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr));
            retval = BT_ESL_API_FAILURE;
        }
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Invalid peer address");
        retval = BT_ESL_API_FAILURE;
    }

    return retval;
}

API_RESULT BT_esl_config_ots_pl(BT_ESL_BD_ADDR * bd_addr)
{

    API_RESULT retval;
    int err;
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;
    uint8_t conn_idx;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

    if (bd_addr != NULL)
    {
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)bd_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)bd_addr->type);

        /* Get conn param */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);

        if (conn != NULL)
        {
            conn_idx = bt_conn_index(conn);

            /* Ensure the connection index is valid */
            if (conn_idx >= CONFIG_BT_MAX_CONN)
            {
                ESL_PL_ERR (
                "[ESL PL]: Invalid connection index: %d", conn_idx);
                return BT_ESL_API_FAILURE;
            }
            /* Configure OTS for the specified ESL device */
            ots_client[conn_idx].oacp_sub_params.disc_params = &ots_client[conn_idx].oacp_sub_disc_params;
            ots_client[conn_idx].oacp_sub_params.ccc_handle = 0;
            ots_client[conn_idx].oacp_sub_params.end_handle = ots_client[conn_idx].end_handle;
            ots_client[conn_idx].oacp_sub_params.value = BT_GATT_CCC_INDICATE;
            ots_client[conn_idx].oacp_sub_params.value_handle = ots_client[conn_idx].oacp_handle;
            ots_client[conn_idx].oacp_sub_params.notify = bt_ots_client_indicate_handler;
            err = bt_gatt_subscribe(conn, &ots_client[conn_idx].oacp_sub_params);

            if (err != 0)
            {
                ESL_PL_ERR ("Subscribe OACP failed %d\n", err);
                return BT_ESL_API_FAILURE;
            }

            ots_client[conn_idx].olcp_sub_params.disc_params = &ots_client[conn_idx].olcp_sub_disc_params;
            ots_client[conn_idx].olcp_sub_params.ccc_handle = 0;
            ots_client[conn_idx].olcp_sub_params.end_handle = ots_client[conn_idx].end_handle;
            ots_client[conn_idx].olcp_sub_params.value = BT_GATT_CCC_INDICATE;
            ots_client[conn_idx].olcp_sub_params.value_handle = ots_client[conn_idx].olcp_handle;
            ots_client[conn_idx].olcp_sub_params.notify = bt_ots_client_indicate_handler;
            err = bt_gatt_subscribe(conn, &ots_client[conn_idx].olcp_sub_params);

            if (err != 0)
            {
                ESL_PL_ERR ("Subscribe OLCP failed %d\n", err);
                return BT_ESL_API_FAILURE;
            }
            bt_conn_unref(conn);
        }
    }
    else
    {
        retval = BT_ESL_API_FAILURE;
    }

    return retval;
}

API_RESULT BT_esl_register_ots_callback_pl(BT_ESL_OTS_CALLBACK * callback)
{
    API_RESULT retval;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    if (NULL != callback)
    {
        ots_callback = *callback;
    }
    else
    {
        retval = BT_ESL_API_FAILURE;
    }

    return retval;
}
#endif /* CONFIG_BT_OTS */

static uint8_t dis_discovery_cb_pl
               (
                    struct bt_conn *conn,
                    const struct bt_gatt_attr *attr,
                    struct bt_gatt_discover_params *params
               )
{
    uint8_t conn_idx;
    int err;
    uint8_t retval;
    BT_ESL_BD_ADDR bd_addr;

    /* Init */
    retval = BT_GATT_ITER_CONTINUE;
    BT_ESL_mem_set(&bd_addr, 0x00U, sizeof(bd_addr));

    /* Validate connection before any dereference (bt_conn_index requires non-NULL) */
    if (NULL == conn)
    {
        ESL_PL_ERR("[ESL PL]: DIS discovery callback received NULL conn");
        return BT_GATT_ITER_STOP;
    }

    /* Get connection index and validate bounds */
    conn_idx = bt_conn_index(conn);
    if (conn_idx >= CONFIG_BT_MAX_CONN)
    {
        ESL_PL_ERR("[ESL PL]: Invalid connection index %d", conn_idx);
        return BT_GATT_ITER_STOP;
    }

    /* Copy the peer address (guard bt_conn_get_dst() return) */
    if (NULL != bt_conn_get_dst(conn))
    {
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);
    }

    /* Check if params is NULL */
    if (params == NULL)
    {
        ESL_PL_ERR("[ESL PL]: Discovery callback received NULL params");
        /* Reset discovery_in_progress */
        dis_discovery_session[conn_idx].discovery_in_progress = BT_ESL_FALSE;
        return BT_GATT_ITER_STOP;
    }

    switch (params->type)
    {
    case BT_GATT_DISCOVER_PRIMARY:
    {
        struct bt_gatt_service_val *svc;

        /* Zephyr passes NULL attr when discovery is exhausted (service not found) */
        if ((NULL == attr) || (NULL == attr->user_data))
        {
            ESL_PL_TRC ("[ESL PL]: DIS primary service not found");
            dis_discovery_session[conn_idx].discovery_in_progress = BT_ESL_FALSE;
            retval = BT_GATT_ITER_STOP;
            break;
        }

        /* Handle service discovery */
        svc = attr->user_data;
        ESL_PL_TRC ("[ESL PL]: Found DIS Service:");
        ESL_PL_TRC ("[ESL PL]:   Start Handle: 0x%04x", attr->handle);
        ESL_PL_TRC ("[ESL PL]:   End Handle: 0x%04x", svc->end_handle);
        print_uuid_pl(svc->uuid);

        /* Update discovery parameters to find characteristics within this service */
        dis_discovery_session[conn_idx].discover_params.uuid = NULL;
        dis_discovery_session[conn_idx].discover_params.start_handle = attr->handle;
        dis_discovery_session[conn_idx].discover_params.end_handle = svc->end_handle;
        dis_discovery_session[conn_idx].discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &dis_discovery_session[conn_idx].discover_params);
        if (err)
        {
            ESL_PL_TRC ("[ESL PL]: Failed to start characteristic discovery (err %d)", err);
            /* Reset discovery_in_progress */
            dis_discovery_session[conn_idx].discovery_in_progress = BT_ESL_FALSE;
        }
        retval = BT_GATT_ITER_STOP;
        break;
    }
    case BT_GATT_DISCOVER_CHARACTERISTIC:
    {
        struct bt_gatt_chrc *chrc;
        /* Checking CHAR discovery is completed or not */
        if ((NULL != attr) && (NULL != attr->user_data)
            && (params->start_handle < params->end_handle))
        {
            /* Handle characteristic discovery */
            chrc = attr->user_data;
            ESL_PL_TRC ("[ESL PL]: Found DIS Characteristic:");
            ESL_PL_TRC ("[ESL PL]:   Handle: 0x%04x", attr->handle);
            ESL_PL_TRC ("[ESL PL]:   Properties: 0x%02x", chrc->properties);
            if (NULL != chrc->uuid)
            {
                print_uuid_pl(chrc->uuid);
            }
        }
        else
        {
            dis_discovery_session[conn_idx].discovery_in_progress = BT_ESL_FALSE;
            /* Discover cccd of control point char */
            if (is_discovery_complete(conn_idx))
            {
                /* Discovery completed */
                ESL_PL_TRC("[ESL PL]: DIS Discovery Completed");
            }
            else
            {
                /* Discovery process failed */
                ESL_PL_TRC("[ESL PL]: DIS Discovery failed");
            }
            retval = BT_GATT_ITER_STOP;
        }
        break;
    }
    default:
    {
        ESL_PL_TRC ("[ESL PL]: Unknown discovery type: %d", params->type);
        /* Reset discovery_in_progress */
        dis_discovery_session[conn_idx].discovery_in_progress = BT_ESL_FALSE;
        retval = BT_GATT_ITER_STOP;
    }
    }

    return retval;
}

API_RESULT BT_esl_discover_dis_pl(BT_ESL_BD_ADDR * bd_addr)
{
    uint8_t conn_idx;
    API_RESULT retval;
    int err;
    struct bt_conn *conn;
    bt_addr_le_t conn_addr;

    /* Init */
    conn = NULL;
    retval = BT_ESL_API_SUCCESS;

    if (bd_addr != NULL)
    {
        BT_ESL_COPY_BD_ADDR(conn_addr.a.val, (uint8_t *)bd_addr->addr);
        BT_ESL_COPY_TYPE(conn_addr.type, (uint8_t)bd_addr->type);

        /* Get conn param */
        conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &conn_addr);

        if (conn != NULL)
        {
            conn_idx = bt_conn_index(conn);

            /* Ensure the connection index is valid */
            if (conn_idx >= CONFIG_BT_MAX_CONN)
            {
                ESL_PL_ERR (
                "[ESL PL]: Invalid connection index: %d", conn_idx);
                return BT_ESL_API_FAILURE;
            }

            /* Check if discovery is already in progress for this connection */
            if (dis_discovery_session[conn_idx].discovery_in_progress)
            {
                ESL_PL_ERR (
                "[ESL PL]: Service discovery already in progress for connection index %d", conn_idx);
                return BT_ESL_API_FAILURE;
            }

            /* Mark discovery as in progress for this connection */
            dis_discovery_session[conn_idx].discovery_in_progress = BT_ESL_TRUE;

            /* Clear atomic state */
            atomic_clear(&dis_discovery_session[conn_idx].state);

            /* Set up discovery parameters */
            dis_discovery_session[conn_idx].discover_params.uuid = BT_UUID_DIS;
            dis_discovery_session[conn_idx].discover_params.func = dis_discovery_cb_pl;
            dis_discovery_session[conn_idx].discover_params.start_handle = 0x0001;
            dis_discovery_session[conn_idx].discover_params.end_handle = 0xffff;
            dis_discovery_session[conn_idx].discover_params.type = BT_GATT_DISCOVER_PRIMARY;

            /* Start service discovery */
            err = bt_gatt_discover(conn, &dis_discovery_session[conn_idx].discover_params);
            /* Release the reference after starting discovery */
            bt_conn_unref(conn);

            if (err)
            {
                ESL_PL_ERR ("[ESL PL]: DIS Service discovery failed to start (err %d)", err);
                dis_discovery_session[conn_idx].discovery_in_progress = BT_ESL_FALSE;
            }
            else
            {
                ESL_PL_TRC("[ESL PL]: DIS discovery started for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
                BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr));
            }
        }
        else
        {
            ESL_PL_ERR (
            "[ESL PL]: Connection not found for "BT_ESL_DEVICE_ADDR_FRMT_SPECIFIER"",
            BT_ESL_DEVICE_ADDR_PRINT_STR(bd_addr));
            retval = BT_ESL_API_FAILURE;
        }
    }
    else
    {
        ESL_PL_ERR("[ESL PL]: Invalid peer address");
        retval = BT_ESL_API_FAILURE;
    }

    return retval;
}

#endif /* BT_ESL_SUPPORT_AP_ROLE */

#ifdef BT_ESL_SUPPORT_TAG_ROLE
API_RESULT BT_esl_register_ots_server_callback_pl(BT_ESL_OTS_SERVER_CALLBACK *callback)
{
    API_RESULT retval;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    if (NULL != callback)
    {
        ots_server_callback = *callback;
    }
    else
    {
        retval = BT_ESL_API_FAILURE;
    }

    return retval;
}

static int ots_obj_created
           (
                struct bt_ots                     * ots,
                struct bt_conn                    * conn,
                uint64_t                            id,
                const struct bt_ots_obj_add_param * add_param,
                struct bt_ots_obj_created_desc    * created_desc
            )
{

    if ((id - BT_OTS_OBJ_ID_MIN) >= number_of_objects)
    {
        ESL_PL_ERR(
        "[ESL PL]: Invalid Object ID requested: 0x%08X", (UINT32)id);
        return -ENOMEM;
    }

    if (add_param->size > object_being_created->size.alloc)
    {
        ESL_PL_ERR("[ESL PL]: Not enough memory (0x%08X ID)", (UINT32)id);
        return -ENOMEM;
    }

    if (NULL != object_being_created)
    {
        created_desc->name = object_being_created->name;
        created_desc->size = object_being_created->size;
        created_desc->props = object_being_created->props;
        /*BT_OTS_OBJ_SET_PROP_READ(created_desc->props);*/ /* TODO: Check from the ESL Spec which section mandates this */
        BT_OTS_OBJ_SET_PROP_WRITE(created_desc->props);
        BT_OTS_OBJ_SET_PROP_PATCH(created_desc->props);
    }
    else
    {
        return -ENOMEM;
    }

    ESL_PL_INF("[ESL PL]: Object with 0x%08X ID has been created", (UINT32)id);

    return 0;
}



/**
 * \brief Creates OTS objects for ESL Tag
 *
 * \par This function creates OTS objects for ESL Tag based on the provided metadata.
 *
 * \param no_of_objects Number of objects to create.
 * \param obj_meta_data Reference to the array of metadata for the objects.
*/
API_RESULT BT_esl_ots_create_obj_pl
           (
                UCHAR                 no_of_objects,
                BT_ESL_OTS_METADATA * obj_meta_data
            )
{
    int err;
    API_RESULT retval;
    OBJECT_CREATION_DATA obj_data;
    int idx;
    struct bt_ots_obj_add_param param;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    if (NULL == obj_meta_data)
    {
        ESL_PL_ERR("[ESL PL]: Invalid object metadata");
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        number_of_objects = no_of_objects;

        for (idx = 0; idx < no_of_objects; idx++)
        {
            obj_data.name = obj_meta_data[idx].name;
            obj_data.size.cur = obj_meta_data[idx].cur_size;
            obj_data.size.alloc = obj_meta_data[idx].alloc_size;
            obj_data.props = 0;
            /*BT_OTS_OBJ_SET_PROP_READ(obj_data.props);*/
            BT_OTS_OBJ_SET_PROP_WRITE(obj_data.props);
            BT_OTS_OBJ_SET_PROP_PATCH(obj_data.props);

            /* Set the object being created */
            object_being_created = &obj_data;

            param.size = obj_data.size.alloc;
            param.type.uuid.type = BT_UUID_TYPE_16;
            param.type.uuid_16.val = BT_UUID_OTS_TYPE_UNSPECIFIED_VAL;

            /* Add object (it immediately calls the created obj call in the same thread)*/
            err = bt_ots_obj_add(ots_server, &param);

            /* Clear the object being created */
            object_being_created = NULL;
            if (err < 0)
            {
                ESL_PL_ERR("[ESL PL]: Failed to add an object to OTS (err: %d)", err);
                retval = BT_ESL_API_FAILURE;
            }
        }
    }

    return retval;
}

static void ots_obj_selected
            (
                 struct bt_ots  * ots,
                 struct bt_conn * conn,
                 uint64_t         id
            )
{
    UCHAR img_index;
    BT_ESL_BD_ADDR bd_addr;

    /* Init  */
    BT_ESL_INIT_BD_ADDR(&bd_addr);
    img_index = id - BT_OTS_OBJ_ID_MIN;

    ESL_PL_INF ("[ESL PL]: Object with 0x%08X ID has been selected", (UINT32)id);

    if (NULL != conn)
    {
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);
    }
    /* Inform upper layer */
    if (NULL != ots_server_callback.image_selected)
    {
        ots_server_callback.image_selected(&bd_addr, img_index);
    }
}

#if 0
static ssize_t ots_obj_read
               (
                    struct bt_ots *ots,
                    struct bt_conn *conn,
                    uint64_t id,
                    void **data,
                    size_t len,
                    off_t offset
               )
{
    char id_str[BT_OTS_OBJ_ID_STR_LEN];
    uint16_t obj_index = (id - BT_OTS_OBJ_ID_MIN);

    bt_ots_obj_id_to_str(id, id_str, sizeof(id_str));

    if (NULL == data)
    {
        ESL_PL_INF ("[ESL PL]: Object with %s ID has been successfully read", id_str);
        return 0;
    }

    ESL_PL_ERR ("[ESL PL]: no read_img_from_storage cb");

    ESL_PL_TRC(
    "[ESL PL]: Object with %s ID is being read"
    "Offset = %lu, Length = %zu",
    id_str, (long)offset, len);

    return 0;
}
#endif

static ssize_t ots_obj_write
               (
                    struct bt_ots  * ots,
                    struct bt_conn * conn,
                    uint64_t         id,
                    const void     * data,
                    size_t           len,
                    off_t            offset,
                    size_t           rem
                )
{
    API_RESULT retval;
    uint8_t image_index;
    BT_ESL_BD_ADDR bd_addr;

    /* Init */
    retval = BT_ESL_API_SUCCESS;
    image_index = id - BT_OTS_OBJ_ID_MIN;
    BT_ESL_INIT_BD_ADDR(&bd_addr);

    if (NULL != conn)
    {
        BT_ESL_COPY_BD_ADDR(bd_addr.addr, (UCHAR *)(bt_conn_get_dst(conn)->a.val));
        BT_ESL_COPY_TYPE(bd_addr.type, (UCHAR)bt_conn_get_dst(conn)->type);
    }

    ESL_PL_TRC(
    "[ESL PL]: Write object CB \n\tID: 0x%08X\n\timage index: %d"
    "\n\tOffset: %d\n\tLen: %d\n\tRem: %d",
    (UINT32)id, image_index, (UINT32)offset, (UINT32)len, (UINT32)rem);

    if (NULL != ots_server_callback.image_write)
    {
        retval = ots_server_callback.image_write
                 (
                     &bd_addr,
                     image_index,
                     (void *)data,
                     (UINT32) len,
                     (UINT32) offset,
                     (UINT32) rem
                 );
        if (BT_ESL_OTS_NO_MEM == retval)
        {
            return -ENOMEM;
        }
    }
    else
    {
        ESL_PL_ERR ("[ESL PL]: no write_img_to_storage cb");
    }

    return len;
}

/**
 * \brief Initializes the OTS server for ESL Tag
 *
 * \par This function initializes the OTS server instance and sets up the OTS callbacks.
 *
 * \return return BT_ESL_API_SUCCESS on success, BT_ESL_API_FAILURE on failure.
 */
API_RESULT BT_esl_ots_init_pl(void)
{
    API_RESULT retval;
    int err;
    struct bt_ots_init_param ots_init;

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    ots_server = bt_ots_free_instance_get();
    if (NULL == ots_server)
    {
        ESL_PL_ERR ("[ESL PL]: Failed to retrieve OTS instance");
        retval = BT_ESL_API_FAILURE;
    }
    else
    {
        /* Init OTS cbs */
        ots_callbacks.obj_created = ots_obj_created;
        ots_callbacks.obj_selected = ots_obj_selected;
        /*ots_callbacks.obj_read = ots_obj_read;*/
        ots_callbacks.obj_write = ots_obj_write;
        /* Configure OTS initialization. */
        BT_ESL_mem_set(&ots_init, 0, sizeof(ots_init));
        /*BT_OTS_OACP_SET_FEAT_READ(ots_init.features.oacp);*/
        BT_OTS_OACP_SET_FEAT_WRITE(ots_init.features.oacp);
        BT_OTS_OACP_SET_FEAT_PATCH(ots_init.features.oacp);
        BT_OTS_OLCP_SET_FEAT_GO_TO(ots_init.features.olcp);
        ots_init.cb = &ots_callbacks;

        /* Initialize OTS instance. */
        err = bt_ots_init(ots_server, &ots_init);
        if (0 != err)
        {
            ESL_PL_ERR("[ESL PL]: Failed to init OTS (err:%d)", err);
            retval = BT_ESL_API_FAILURE;
        }
    }

    return retval;
}

#endif /* BT_ESL_SUPPORT_TAG_ROLE */
