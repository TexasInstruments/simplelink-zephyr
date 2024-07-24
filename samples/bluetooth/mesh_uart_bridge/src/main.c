/* main.c - Application main entry point */

/*
 * Copyright (c) 2017 Intel Corporation
 * Copyright (c) 2025 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/printk.h>

#include <zephyr/settings/settings.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/mesh.h>

#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>

#include "board.h"
#include "uart_bridge_model.h"

static const struct shell *uart_shell;

static void attention_on(const struct bt_mesh_model *mod)
{
	board_led_set(true);
}

static void attention_off(const struct bt_mesh_model *mod)
{
	board_led_set(false);
}

static int handle_payload(const struct bt_mesh_uart_bridge *uart_bridge,
						  struct bt_mesh_msg_ctx *ctx,
						  const uint8_t *payload)
{
	uint16_t addr = bt_mesh_model_elem(uart_bridge->model)->rt->addr;

	/* Do not print the payload if it came from this device */
	if (addr == ctx->addr)
    {
        return 0;
    }

	shell_print(uart_shell, "[0x%04x]: %s", ctx->addr, payload);

	return 0;
}

static const struct bt_mesh_health_srv_cb health_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_cb,
};

static const struct bt_mesh_uart_bridge_cb uart_bridge_cb = {
	.payload = handle_payload,
};

static struct bt_mesh_uart_bridge uart_bridge = {
	.cb = &uart_bridge_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);


/* This application only needs one element to contain its models */
static const struct bt_mesh_model models[] = {
	BT_MESH_MODEL_CFG_SRV,
	BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
	BT_MESH_MODEL_RPR_SRV,
};

/* Vendor models */
static const struct bt_mesh_model vnd_models[] = {
	BT_MESH_MODEL_UART_BRIDGE(&uart_bridge)
};

static const struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(0, models, vnd_models)
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

/* Provisioning */

static int output_number(bt_mesh_output_action_t action, uint32_t number)
{
	printk("OOB Number: %u\n", number);

	board_output_number(action, number);

	return 0;
}

static void prov_complete(uint16_t net_idx, uint16_t addr)
{
	board_prov_complete();
}

static void prov_reset(void)
{
	bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
}

static uint8_t dev_uuid[16];

static const struct bt_mesh_prov prov = {
	.uuid = dev_uuid,
	.output_size = 4,
	.output_actions = BT_MESH_DISPLAY_NUMBER,
	.output_number = output_number,
	.complete = prov_complete,
	.reset = prov_reset,
};

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	err = bt_mesh_init(&prov, &comp);
	if (err) {
		printk("Initializing mesh failed (err %d)\n", err);
		return;
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	/* This will be a no-op if settings_load() loaded provisioning info */
	bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);

	printk("Mesh initialized\n");
}

/* UART shell command callbacks */
static int cmd_bridge(const struct shell *sh, size_t argc, char **argv)
{
	if (argc == 1) {
		shell_help(sh);
		return 1;
	}
	return -EINVAL;
}

static int cmd_payload_send(const struct shell *sh, size_t argc, char **argv)
{
	uint16_t addr = bt_mesh_model_elem(uart_bridge.model)->rt->addr;
	shell_print(sh, "[0x%04x]: %s", addr, argv[1]);

	return bt_mesh_uart_bridge_payload_send(uart_bridge.model, argv[1]);
}

/* UART shell commands */
SHELL_STATIC_SUBCMD_SET_CREATE(bridge_cmds,
	SHELL_CMD_ARG(payload, NULL, "Send payload", cmd_payload_send, 1, 1),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_ARG_REGISTER(bridge, &bridge_cmds,
					   "Bluetooth Mesh UART Bridge commands",
					   cmd_bridge, 1, 1);

int main(void)
{
	int err = -1;

	printk("Initializing...\n");

	if (IS_ENABLED(CONFIG_HWINFO)) {
		err = hwinfo_get_device_id(dev_uuid, sizeof(dev_uuid));
	}

	if (err < 0) {
		dev_uuid[0] = 0xdd;
		dev_uuid[1] = 0xdd;
	}

	err = board_init();
	if (err) {
		printk("Board init failed (err: %d)\n", err);
		return 0;
	}

	uart_shell = shell_backend_uart_get_ptr();
	if (!uart_shell) {
		printk("UART shell backend not available\n");
		return 0;
	}

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}

	return 0;
}
