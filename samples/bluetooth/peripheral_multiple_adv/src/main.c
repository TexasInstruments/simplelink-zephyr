/*
 * Copyright (c) 2025, Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>

#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/shell/shell.h>

#define CONNECTABLE_ADV_SID		0
#define NON_CONNECTABLE_ADV_SID	1

#define MANUFACTURER_DATA_LEN	249

/* Set the first two bytes as the Company ID */
static uint8_t mfg_data[MANUFACTURER_DATA_LEN] = { 0x0D, 0x00 };

static struct bt_le_ext_adv *conn_ext_adv;
static struct bt_le_ext_adv *nconn_ext_adv;

static uint8_t led_state = 0x00;
static const struct gpio_dt_spec gled = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec rled = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

static int configure_leds(void);
static ssize_t read_led(struct bt_conn *conn,
						const struct bt_gatt_attr *attr,
						void *buf, uint16_t len,
						uint16_t offset);
static ssize_t write_led(struct bt_conn *conn,
						 const struct bt_gatt_attr *attr,
						 const void *buf, uint16_t len,
						 uint16_t offset, uint8_t flags);

static void adv_work_handler(struct k_work *work);
static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);

static K_WORK_DEFINE(adv_work, adv_work_handler);
BT_GATT_SERVICE_DEFINE(led_service,
					   BT_GATT_PRIMARY_SERVICE(BT_UUID_UDS),
					   BT_GATT_CHARACTERISTIC(BT_UUID_GATT_DI,
									BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
									BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
									read_led, write_led, &led_state)
);

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

/* LE Advertising Parameters for Connectable Advertising Set */
static const struct bt_le_adv_param conn_adv_param = {
	.id = BT_ID_DEFAULT,
	.sid = CONNECTABLE_ADV_SID,
	.secondary_max_skip = 0U,
	.options = BT_LE_ADV_OPT_CONNECTABLE,
	.interval_min = BT_GAP_ADV_SLOW_INT_MIN,
	.interval_max = BT_GAP_ADV_SLOW_INT_MAX,
	.peer = NULL,
};

/* LE Advertising Parameters for Non-Connectable Advertising Set */
static const struct bt_le_adv_param nconn_adv_param = {
	.id = BT_ID_DEFAULT,
	.sid = NON_CONNECTABLE_ADV_SID,
	.secondary_max_skip = 0U,
	.options = BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_EXT_ADV,
	.interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
	.interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
	.peer = NULL,
};

/* LE Advertising Data for Connectable Advertising Set */
static const struct bt_data conn_ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_UDS_VAL)),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* LE Advertising Data for Non-Connectable Advertising Set */
static const struct bt_data nconn_ad[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data))
};

static int configure_leds(void)
{
	int err = 0;

	if (!gpio_is_ready_dt(&gled)) {
		return -EAGAIN;
	}

	if (!gpio_is_ready_dt(&rled)) {
		return -EAGAIN;
	}

	err = gpio_pin_configure_dt(&gled, GPIO_OUTPUT_INACTIVE);
	if (err) {
		return err;
	}

	err = gpio_pin_configure_dt(&rled, GPIO_OUTPUT_INACTIVE);
	if (err) {
		return err;
	}

	return err;
}

static ssize_t read_led(struct bt_conn *conn,
						const struct bt_gatt_attr *attr,
						void *buf, uint16_t len,
						uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &led_state,
							 sizeof(led_state));
}

static ssize_t write_led(struct bt_conn *conn,
						 const struct bt_gatt_attr *attr,
						 const void *buf, uint16_t len,
						 uint16_t offset, uint8_t flags)
{
	if (offset + len > sizeof(led_state)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	bytecpy(&led_state + offset, buf, len);
	gpio_pin_set_dt(&rled, (led_state & 0x01));

	return len;
}

static void adv_work_handler(struct k_work *work)
{
	int err = bt_le_ext_adv_start(conn_ext_adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		printk("Failed to start connectable advertising set (err %d)\n",
		       err);
	}
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
	} else {
		printk("Connected\n");

		gpio_pin_set_dt(&gled, 0x01);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);

	gpio_pin_set_dt(&gled, 0x00);

	k_work_submit(&adv_work);
}

static int connectable_adv_start(void)
{
	int err;

	/* Create a connectable advertising set */
	err = bt_le_ext_adv_create(&conn_adv_param, NULL, &conn_ext_adv);
	if (err) {
		printk("Failed to create connectable advertising set (err %d)\n", err);
		return err;
	}

	/* Set extended advertising data */
	err = bt_le_ext_adv_set_data(conn_ext_adv, conn_ad, ARRAY_SIZE(conn_ad), NULL, 0);
	if (err) {
		printk("Failed to set advertising data for connectable advertising set \
			   (err %d)\n", err);
		return err;
	}

	/* Start extended advertising set */
	err = bt_le_ext_adv_start(conn_ext_adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		printk("Failed to start connectable advertising set (err %d)\n",
		       err);
		return err;
	}

	return 0;
}

static int non_connectable_adv_start(void)
{
	int err;

	/* Create a connectable advertising set */
	err = bt_le_ext_adv_create(&nconn_adv_param, NULL, &nconn_ext_adv);
	if (err) {
		printk("Failed to create non connectable advertising set (err %d)\n", err);
		return err;
	}

	/* Set extended advertising data */
	err = bt_le_ext_adv_set_data(nconn_ext_adv, nconn_ad, ARRAY_SIZE(nconn_ad),
								 NULL, 0);
	if (err) {
		printk("Failed to set advertising data for non connectable advertising set \
			   (err %d)\n", err);
		return err;
	}

	/* Start extended advertising set */
	err = bt_le_ext_adv_start(nconn_ext_adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		printk("Failed to start non connectable advertising set (err %d)\n",
		       err);
		return err;
	}

	return 0;
}

static int manufacturer_data_update(uint8_t* data, size_t len)
{
	int err;

	/* We reserve the first two bytes for the Manufacturer ID */
	if (len > MANUFACTURER_DATA_LEN - 2) {
		printk("Manufacturer data length exceeds maximum length\n");
		return -EINVAL;
	}

	memset(&mfg_data[2], 0, MANUFACTURER_DATA_LEN - 2);
	bytecpy(&mfg_data[2], data, len);

	/* Set extended advertising data */
	err = bt_le_ext_adv_set_data(nconn_ext_adv, nconn_ad, ARRAY_SIZE(nconn_ad),
								 NULL, 0);
	if (err) {
		printk("Failed to set manufacturer data in non connectable advertising set \
			   (err %d)\n", err);
		return err;
	}

	return 0;
}

int main(void)
{
	int err;

	printk("Starting Peripheral Multiple Advertiser Demo\n");

	err = configure_leds();
	if (err) {
		printk("Failed to configure LEDs\n");
		return 0;
	}

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	err = connectable_adv_start();
	if (err) {
		return 0;
	}
	printk("Connectable advertising has started\n");

	err = non_connectable_adv_start();
	if (err) {
		return 0;
	}
	printk("Non-connectable advertising has started\n");

	return 0;
}

/* UART shell command callbacks */
static int cmd_bt_help(const struct shell *sh, size_t argc, char **argv)
{
	if (argc == 1) {
		shell_help(sh);
		return 1;
	}
	return -EINVAL;
}

static int cmd_bt_adv_data_set(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_print(sh, "Usage: bt adv set_mfg <data>");
		return -EINVAL;
	}

	shell_print(sh, "%s", argv[1]);

	return manufacturer_data_update((uint8_t*)argv[1], strlen(argv[1]));
}

/* UART shell commands */
SHELL_STATIC_SUBCMD_SET_CREATE(adv_cmds,
	SHELL_CMD_ARG(set-mfg, NULL, "Set manufacturer data", cmd_bt_adv_data_set, 1, 1),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(bt_cmds,
	SHELL_CMD(adv, &adv_cmds, "Advertising Data Commands", cmd_bt_help),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_ARG_REGISTER(bt, &bt_cmds,
					   "Bluetooth commands",
					   cmd_bt_help, 1, 1);
