/*
 * Copyright (c) 2026 Conclusive Engineering sp. z o. o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <errno.h>

#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_types.h>

static int cc3xxx_send_hci_cmd(uint16_t opcode, const void *data, size_t len)
{
	struct net_buf *buf;

	buf = bt_hci_cmd_create(opcode, len);
	if (!buf) {
		return -ENOBUFS;
	}

	if (len) {
		net_buf_add_mem(buf, data, len);
	}

	return bt_hci_cmd_send_sync(opcode, buf, NULL);
}

int bt_hci_ti_cc3xxx_privacy_workaround(const uint8_t local_irk[16])
{
	static bool applied;
	struct bt_hci_cp_le_set_addr_res_enable addr_res;
	struct bt_hci_cp_le_add_dev_to_rl rl = { 0 };
	struct bt_hci_cp_le_set_privacy_mode privacy = { 0 };
	int err;

	if (applied) {
		return 0;
	}

	addr_res.enable = BT_HCI_ADDR_RES_DISABLE;
	err = cc3xxx_send_hci_cmd(BT_HCI_OP_LE_SET_ADDR_RES_ENABLE,
				  &addr_res, sizeof(addr_res));
	if (err) {
		return err;
	}

	err = cc3xxx_send_hci_cmd(BT_HCI_OP_LE_CLEAR_RL, NULL, 0);
	if (err) {
		return err;
	}

	addr_res.enable = BT_HCI_ADDR_RES_ENABLE;
	err = cc3xxx_send_hci_cmd(BT_HCI_OP_LE_SET_ADDR_RES_ENABLE,
				  &addr_res, sizeof(addr_res));
	if (err) {
		return err;
	}

	memcpy(rl.local_irk, local_irk, sizeof(rl.local_irk));
	err = cc3xxx_send_hci_cmd(BT_HCI_OP_LE_ADD_DEV_TO_RL, &rl, sizeof(rl));
	if (err) {
		return err;
	}

	privacy.mode = BT_HCI_LE_PRIVACY_MODE_DEVICE;
	err = cc3xxx_send_hci_cmd(BT_HCI_OP_LE_SET_PRIVACY_MODE, &privacy, sizeof(privacy));
	if (err) {
		return err;
	}

	applied = true;

	return 0;
}
