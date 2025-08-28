/*
 * Copyright (c) 2024 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/buf.h>
#include <soc.h>
#include <zephyr/init.h>
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/sys/atomic.h>

#include <zephyr/drivers/bluetooth.h>

#include "hci_api.h"
#include "ble_init.h"
#include "assert.h"
#include "comdef.h"

#define LOG_LEVEL CONFIG_BT_HCI_DRIVER_LOG_LEVEL
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bt_ctlr_hci_driver);

#define DT_DRV_COMPAT ti_bt_hci

struct hci_driver_data {
	bt_hci_recv_t recv;
};
/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * CONSTANTS
 */
#define BLE_SYNC_INIT_TIMEOUT_SEC   10
#define BLE_SYNC_INIT_TIMEOUT_TICKS 10000 /* K_SECONDS(BLE_SYNC_INIT_TIMEOUT_SEC) */
/*******************************************************************************
 * LOCAL FUNCTIONS PROTOTYPES
 */
static int hci_driver_ll_send_to_host_cb(uint8 *pHciPkt, uint16 pktLen);
/* static void vs_set_bd_addr(); */

/*******************************************************************************
 * EXTERNS
 */

typedef int_fast16_t ICall_Errno;
typedef uint_least8_t ICall_EntityID;
typedef void *ICall_SyncHandle;
ICall_Errno ICall_registerApp(ICall_EntityID *entity, ICall_SyncHandle *msgSyncHdl);

/*******************************************************************************
 * GLOBAL VARIABLES
 */

static bleServicesParams_t bleServicesParams;

/*******************************************************************************
 * API FUNCTIONS
 */

/*******************************************************************************
 * @fn          AssertHandler
 *
 * @brief       This is the Application's callback handler for asserts raised
 *              in the stack.  When EXT_HAL_ASSERT is defined in the Stack Wrapper
 *              project this function will be called when an assert is raised,
 *              and can be used to observe or trap a violation from expected
 *              behavior.
 *
 *              As an example, for Heap allocation failures the Stack will raise
 *              HAL_ASSERT_CAUSE_OUT_OF_MEMORY as the assertCause and
 *              HAL_ASSERT_SUBCAUSE_NONE as the assertSubcause.  An application
 *              developer could trap any malloc failure on the stack by calling
 *              HAL_ASSERT_SPINLOCK under the matching case.
 *
 *              An application developer is encouraged to extend this function
 *              for use by their own application.  To do this, add hal_assert.c
 *              to your project workspace, the path to hal_assert.h (this can
 *              be found on the stack side). Asserts are raised by including
 *              hal_assert.h and using macro HAL_ASSERT(cause) to raise an
 *              assert with argument assertCause.  the assertSubcause may be
 *              optionally set by macro HAL_ASSERT_SET_SUBCAUSE(subCause) prior
 *              to asserting the cause it describes. More information is
 *              available in hal_assert.h.
 *
 * input parameters
 *
 * @param       assertCause    - Assert cause as defined in hal_assert.h.
 * @param       assertSubCause - Optional assert subcause (see hal_assert.h).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void AssertHandler(uint8 assertCause, uint8 assertSubCause)
{
	/* check the assert cause */
	switch (assertCause) {
	/* This assert is raised from the BLE Stack when a malloc failure occurs. */
	case HAL_ASSERT_CAUSE_OUT_OF_MEMORY:
		/* ERROR: OUT OF MEMORY */
		HAL_ASSERT_SPINLOCK;
		break;

	case HAL_ASSERT_CAUSE_INTERNAL_ERROR:
		/* check the subcause */
		if (assertSubCause == HAL_ASSERT_SUBCAUSE_FW_INERNAL_ERROR) {
			/* ERROR: INTERNAL FW ERROR */
			HAL_ASSERT_SPINLOCK;
		} else {
			/* ERROR: INTERNAL ERROR */
			HAL_ASSERT_SPINLOCK;
		}
		break;

	/* An assert originating from an ICall failure. */
	case HAL_ASSERT_CAUSE_ICALL_ABORT:
		/* ERROR: ICALL ABORT */
		HAL_ASSERT_SPINLOCK;
		break;

	default:
		/* ERROR: DEFAULT SPINLOCK */
		HAL_ASSERT_SPINLOCK;
		break;
	}
}

#define HCI_TYPE_INVALID (0xFF)

static const enum bt_buf_type bt_buf_type_in[6] = {
	HCI_TYPE_INVALID,
	/* BT_HCI_H4_NONE */ /* NOT SUPPORTED */
	HCI_TYPE_INVALID,
	/* BT_HCI_H4_CMD  */ /* NOT SUPPORTED */
	BT_BUF_ACL_IN,       /* BT_HCI_H4_ACL  */
	HCI_TYPE_INVALID,
	/* BT_HCI_H4_SCO  */                  /* NOT SUPPORTED */
	BT_BUF_EVT,                           /* BT_HCI_H4_EVT  */
	HCI_TYPE_INVALID /* BT_HCI_H4_ISO  */ /* NOT SUPPORTED */
};

static const uint8_t bt_buf_type_out[6] = {
	BT_HCI_H4_CMD, /* BT_BUF_CMD      */
	HCI_TYPE_INVALID,
	/* BT_BUF_EVT      */ /* NOT SUPPORTED */
	BT_HCI_H4_ACL,        /* BT_BUF_ACL_OUT  */
	HCI_TYPE_INVALID,
	/* BT_BUF_ISO_OUT  */ /* NOT SUPPORTED */
	HCI_TYPE_INVALID,
	/* BT_BUF_ISO_IN   */                  /* NOT SUPPORTED */
	HCI_TYPE_INVALID /* BT_BUF_H4       */ /* NOT SUPPORTED */
};

static bool is_hci_event_discardable(uint8 *pHciPkt)
{
	bool ret = false;
	uint8_t evt_type = pHciPkt[1];

	/* ADV reporting events are discardable */
	if (evt_type == BT_HCI_EVT_LE_META_EVENT) {
		uint8_t subevt_type = pHciPkt[sizeof(struct bt_hci_evt_hdr) + 1];

		if (subevt_type == BT_HCI_EVT_LE_ADVERTISING_REPORT ||
		    subevt_type == BT_HCI_EVT_LE_EXT_ADVERTISING_REPORT) {
			ret = true;
		}
	}

	return ret;
}

static int hci_driver_add_pkt_type(struct net_buf *buf)
{
	/* Read the net_buf buffer packet type */
	enum bt_buf_type type = bt_buf_get_type(buf);

	/* Translate the net_buf buffer packet type into the HCI Packet type */
	uint8_t h4_type = bt_buf_type_out[type];

	if (h4_type == HCI_TYPE_INVALID) {
		LOG_ERR("Received Invalid pkt type from the Host: %u", type);
		return -EINVAL;
	}

	/* Add the HCI Packet type to the buffer */
	net_buf_push_u8(buf, h4_type);

	return 0; /* Assuming 0 indicates success */
}

static int hci_driver_send(const struct device *dev, struct net_buf *buf)
{
	static bool first_entry = true;
	int err = 0;

	if (first_entry) {
		first_entry = false;
		/* Register the calling context in the icall */
		ICall_SyncHandle syncEvent_dummy;
		ICall_EntityID icall_entity_dummy;

		ICall_registerApp(&icall_entity_dummy, &syncEvent_dummy);
	}

	/* Add the HCI Packet type to the buffer */
	err = hci_driver_add_pkt_type(buf);
	if (err) {
		net_buf_unref(buf);
		LOG_ERR("Failed to add HCI packet type");
		return err;
	}

	/* Send the buffer to the device */
	err = HCI_HostToControllerSend(buf->data, buf->len);
	if (err) {
		net_buf_unref(buf);
		LOG_ERR("Failed to send buffer to device");
		return err;
	}
	net_buf_unref(buf);

	return 0; /* Assuming 0 indicates success */
}

struct net_buf *hci_evt_create(uint8 *pHciPkt, uint16 pktLen)
{
	struct net_buf *buf;

	enum bt_buf_type buf_type = bt_buf_type_in[pHciPkt[0] /* pktType */];

	if (buf_type == HCI_TYPE_INVALID) {
		LOG_ERR("Received Invalid pkt type from the Controller: %u", pHciPkt[0]);
		return NULL;
	}

	if (buf_type == BT_BUF_EVT) {
		bool discardable_evt = is_hci_event_discardable(pHciPkt);

		buf = bt_buf_get_evt(pHciPkt[1], discardable_evt, K_NO_WAIT);
	} else {
		buf = bt_buf_get_rx(buf_type, K_NO_WAIT);
	}

	if (buf != NULL) {
		net_buf_add_mem(buf, pHciPkt, pktLen);

		/* Skip the native H4_EVT OpCode as
		 * - HCI_RAW adds it in bt_recv
		 * - HCI_CORE doesn't expect it at all
		 */
		net_buf_pull_u8(buf);
	}
	return buf;
}

static int hci_driver_ll_send_to_host_cb(uint8 *pHciPkt, uint16 pktLen)
{
	struct net_buf *buf = NULL;

	buf = hci_evt_create(pHciPkt, pktLen);

	if (buf) {
		const struct device *dev = DEVICE_DT_GET(DT_DRV_INST(0));
		struct hci_driver_data *data = dev->data;

		data->recv(dev, buf);
		return 0; /* Assuming 0 indicates success */
	}

	LOG_ERR("Failed to send pkt type %u from the Controller to the Host, len %u", pHciPkt[0],
		pktLen);
	return -1; /* Assuming -1 indicates failure */
}

static int hci_driver_open(const struct device *dev, bt_hci_recv_t recv)
{
	uint32 status = 0;
	struct hci_driver_data *data = dev->data;

	data->recv = recv;

	/* Init BLE Services params structure */
	status = BLE_ServicesParamsInit(&bleServicesParams, sizeof(bleServicesParams_t));
	if (status == 0) {
		/* Set HCI Driver callbacks to provide hci_driver interface to the LL */
		bleServicesParams.hciCbs.send = hci_driver_ll_send_to_host_cb;
		/* Set User Defined Assert handling callback */
		bleServicesParams.assertCallback = AssertHandler;
		/* Set User Defined Assert handling callback */
		bleServicesParams.syncInitTimeoutTicks = BLE_SYNC_INIT_TIMEOUT_TICKS;

		/* Init the BLE services */
		LOG_DBG("BLE Init Start");
		status = BLE_ServicesInit(&bleServicesParams);
		LOG_DBG("BLE Init End");
	}
	return status;
}

static int hci_driver_close(const struct device *dev)
{
	struct hci_driver_data *data = dev->data;

	/* Clear the (host) receive callback */
	data->recv = NULL;
	return 0; /* Assuming 0 indicates success */
}

static const struct bt_hci_driver_api hci_driver_api = {
	.open = hci_driver_open,
	.close = hci_driver_close,
	.send = hci_driver_send,
};

#define BT_HCI_CONTROLLER_INIT(inst)                                                               \
	static struct hci_driver_data data_##inst = {};                                            \
	DEVICE_DT_INST_DEFINE(inst, NULL, NULL, &data_##inst, NULL, POST_KERNEL,                   \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &hci_driver_api)

/* Only a single instance is supported */
BT_HCI_CONTROLLER_INIT(0)
