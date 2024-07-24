/*
 * Copyright (c) 2025, Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UART_BRIDGE_MODEL_H
#define UART_BRIDGE_MODEL_H

#include <zephyr/bluetooth/mesh.h>

#ifdef __cplusplus
extern "C" {
#endif

/** UART Bridge Model ID */
#define BT_MESH_UART_BRIDGE_VENDOR_MODEL_ID     0x0010

/** UART Bridge Model Op Code Size */
#define BT_MESH_UART_BRIDGE_OP_CODE_SIZE        3
/** UART Bridge Model Payload Op Code */
#define BT_MESH_UART_BRIDGE_OP_PAYLOAD          BT_MESH_MODEL_OP_3(0x01, \
                                                CONFIG_BT_COMPANY_ID)


/** UART Bridge Model Payload minimum length in bytes */
#define BT_MESH_UART_BRIDGE_PAYLOAD_MINLEN      1
/** UART Bridge Model Payload maximum length in bytes */
#define BT_MESH_UART_BRIDGE_PAYLOAD_MAXLEN      MIN(BT_MESH_TX_SDU_MAX, \
                                                BT_MESH_RX_SDU_MAX)     \
                                                - BT_MESH_MIC_SHORT     \
                                                - BT_MESH_UART_BRIDGE_OP_CODE_SIZE

/**
 * @brief UART Bridge Model composition data entry
 *
 * @param uart_bridge Pointer to the bt_mesh_uart_bridge instance.
*/
#define BT_MESH_MODEL_UART_BRIDGE(uart_bridge)              \
		BT_MESH_MODEL_VND_CB(CONFIG_BT_COMPANY_ID,          \
                     BT_MESH_UART_BRIDGE_VENDOR_MODEL_ID,   \
                     _bt_mesh_uart_bridge_opcode_list,      \
                     &(uart_bridge)->pub,                   \
                     (void*)uart_bridge,                    \
                     &_bt_mesh_uart_bridge_cb)

struct bt_mesh_uart_bridge {
    /** Pointer to the UART Bridge Model instance */
    const struct bt_mesh_model *model;
    /** UART Bridge Model Publication context*/
    struct bt_mesh_model_pub pub;
    /** Simple network buffer for UART Bridge Model publication data */
    struct net_buf_simple pub_msg;
    uint8_t buf[BT_MESH_UART_BRIDGE_OP_CODE_SIZE                \
                + CONFIG_BT_MESH_UART_BRIDGE_PAYLOAD_LEN        \
                + BT_MESH_MIC_SHORT];
    /** UART Bridge Model callbacks */
    const struct bt_mesh_uart_bridge_cb *cb;
};

struct bt_mesh_uart_bridge_cb {
    /**
     * @brief Callback for handling received payload.
     *
     * @param uart_bridge Pointer to the bt_mesh_uart_bridge instance.
     * @param ctx         Message context for the incoming message.
     * @param payload     Payload of the incoming message.
    */
    int (*const payload)(const struct bt_mesh_uart_bridge *uart_bridge,
                         struct bt_mesh_msg_ctx *ctx,
                         const uint8_t *payload);
};

/**
 * @brief Publish a payload to the UART Bridge Model.
 *
 * @param model   Pointer to the UART Bridge Model instance.
 * @param payload Pointer to the payload data.
 *
 * @return 0 on success, or (negative) error code on failure.
 */
int bt_mesh_uart_bridge_payload_send(const struct bt_mesh_model *model,
                                     const uint8_t *payload);


/** @cond INTERNAL_HIDDEN */
extern const struct bt_mesh_model_op _bt_mesh_uart_bridge_opcode_list[];
extern const struct bt_mesh_model_cb _bt_mesh_uart_bridge_cb;

#ifdef __cplusplus
}
#endif

#endif /* UART_BRIDGE_MODEL_H */
