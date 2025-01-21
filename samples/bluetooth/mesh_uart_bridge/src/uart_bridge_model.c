/*
 * Copyright (c) 2025, Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/bluetooth/mesh.h>
#include "uart_bridge_model.h"

#define LOG_LEVEL CONFIG_BT_MESH_MODEL_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt_mesh_uart_bridge);

BUILD_ASSERT(CONFIG_BT_MESH_UART_BRIDGE_PAYLOAD_LEN <=
            (BT_MESH_UART_BRIDGE_PAYLOAD_MAXLEN),
            "Payload size is too long");

/* Forward declarations */
static int payload(const struct bt_mesh_model *model,
                   struct bt_mesh_msg_ctx *ctx,
                   struct net_buf_simple *buf);
static int uart_bridge_init(const struct bt_mesh_model *model);

/* Model Operation Codes and callbacks */
const struct bt_mesh_model_op _bt_mesh_uart_bridge_opcode_list[] = {
    {
        BT_MESH_UART_BRIDGE_OP_PAYLOAD,
        BT_MESH_UART_BRIDGE_PAYLOAD_MINLEN,
        payload
    },
    BT_MESH_MODEL_OP_END,
};

/* Model Callbacks */
const struct bt_mesh_model_cb _bt_mesh_uart_bridge_cb = {
    .init = uart_bridge_init,
    .reset = NULL,
    .start = NULL,
};

/*
 * OpCodes Callbacks
 *
 * These callbacks are called when the corresponding OpCode is received.
*/
static int payload(const struct bt_mesh_model *model,
                   struct bt_mesh_msg_ctx *ctx,
                   struct net_buf_simple *buf)
{
    const struct bt_mesh_uart_bridge *uart_bridge = model->rt->user_data;

    if (buf->len < BT_MESH_UART_BRIDGE_PAYLOAD_MINLEN) {
        return -EINVAL;
    }

    if (buf->len > BT_MESH_UART_BRIDGE_PAYLOAD_MAXLEN) {
        return -EMSGSIZE;
    }

    uart_bridge->cb->payload(uart_bridge, ctx, buf->data);

    return 0;
}

/*
 * Define Model Callbacks
*/
static int uart_bridge_init(const struct bt_mesh_model *model)
{
    struct bt_mesh_uart_bridge *uart_bridge = model->rt->user_data;

    if (!uart_bridge) {
		LOG_ERR("No UART Bridge context provided");
		return -EINVAL;
	}

    if (!model->pub) {
		LOG_ERR("UART Bridge has no publication support");
		return -EINVAL;
	}

    uart_bridge->model = model;
    net_buf_simple_init_with_data(&(uart_bridge->pub_msg),
                                  uart_bridge->buf,
                                  sizeof(uart_bridge->buf));
    uart_bridge->pub.msg = &(uart_bridge->pub_msg);

    return 0;
}

int bt_mesh_uart_bridge_payload_send(const struct bt_mesh_model *model,
                                     const uint8_t *payload)
{
	/* Set the message's opcode */
	bt_mesh_model_msg_init(model->pub->msg, BT_MESH_UART_BRIDGE_OP_PAYLOAD);
	/* Add the payload to the message buffer */
	net_buf_simple_add_mem(model->pub->msg, payload, CONFIG_BT_MESH_UART_BRIDGE_PAYLOAD_LEN);

    model->pub->msg->data[CONFIG_BT_MESH_UART_BRIDGE_PAYLOAD_LEN - 1] = '\0';

	return bt_mesh_model_publish(model);
}
