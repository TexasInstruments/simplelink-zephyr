/*
 * Copyright (c) 2026 Conclusive Engineering Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#include <zephyr/sys/printk.h>

#define WIFI_SSID CONFIG_PM_WIFI_SSID
#define WIFI_PSK  CONFIG_PM_WIFI_PSK
#define WIFI_ON_TIME_SECONDS  30
#define WIFI_OFF_TIME_SECONDS 5

static K_SEM_DEFINE(wifi_connected_sem, 0, 1);
static K_SEM_DEFINE(ipv4_acquired_sem, 0, 1);

static bool wifi_link_up;

/* Keep standby locked until the DHCP address is acquired. */
static int pm_wifi_boot_lock(void)
{
	pm_policy_state_lock_get(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
	return 0;
}
SYS_INIT(pm_wifi_boot_lock, PRE_KERNEL_1, 0);

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

static void wifi_event_handler(struct net_mgmt_event_callback *cb,
			       uint32_t mgmt_event, struct net_if *iface)
{
	const struct wifi_status *status = cb->info;

	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		if (status && status->status == 0) {
			wifi_link_up = true;
			printk("wifi: connected\n");
			k_sem_give(&wifi_connected_sem);
		} else {
			printk("wifi: connect failed %d\n",
			       status ? status->status : -1);
		}
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		wifi_link_up = false;
		printk("wifi: disconnected\n");
		break;
	default:
		break;
	}
}

static void ipv4_event_handler(struct net_mgmt_event_callback *cb,
			       uint32_t mgmt_event, struct net_if *iface)
{
	char addr[NET_IPV4_ADDR_LEN];

	if (mgmt_event != NET_EVENT_IPV4_DHCP_BOUND) {
		return;
	}

	for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		struct net_if_addr_ipv4 *if_addr =
			&iface->config.ip.ipv4->unicast[i];

		if (if_addr->ipv4.addr_type != NET_ADDR_DHCP ||
		    if_addr->ipv4.address.in_addr.s_addr == 0) {
			continue;
		}

		net_addr_ntop(AF_INET, &if_addr->ipv4.address.in_addr,
			      addr, sizeof(addr));
		printk("wifi: ipv4 acquired %s\n", addr);
		k_sem_give(&ipv4_acquired_sem);
		return;
	}
}

static void wifi_events_init(void)
{
	net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT |
					     NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	net_mgmt_init_event_callback(&ipv4_cb, ipv4_event_handler,
				     NET_EVENT_IPV4_DHCP_BOUND);
	net_mgmt_add_event_callback(&ipv4_cb);
}

static int wifi_connect(void)
{
	struct net_if *iface = net_if_get_first_wifi();
	struct wifi_connect_req_params params = {
		.ssid = WIFI_SSID,
		.ssid_length = sizeof(WIFI_SSID) - 1,
		.sae_password = WIFI_PSK,
		.sae_password_length = sizeof(WIFI_PSK) - 1,
		.channel = WIFI_CHANNEL_ANY,
		.security = WIFI_SECURITY_TYPE_SAE,
		.mfp = WIFI_MFP_REQUIRED,
	};

	if (!iface) {
		printk("wifi: no iface\n");
		return -ENODEV;
	}

	k_sem_reset(&wifi_connected_sem);

	printk("wifi: connecting to \"%s\"\n", WIFI_SSID);
	return net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params,
			sizeof(params));
}

static int wifi_connect_wait(struct net_if *iface)
{
	for (int attempt = 0; attempt < 3; attempt++) {
		if (wifi_connect() < 0) {
			printk("wifi: connect request failed\n");
			return -EIO;
		}

		if (k_sem_take(&wifi_connected_sem, K_SECONDS(30)) == 0) {
			goto connected;
		}

		printk("wifi: connect timeout\n");
	}

	return -ETIMEDOUT;

connected:
	return 0;
}

static int wifi_wait_for_ipv4(struct net_if *iface)
{
	for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		struct net_if_addr_ipv4 *if_addr =
			&iface->config.ip.ipv4->unicast[i];

		if (if_addr->ipv4.addr_type == NET_ADDR_DHCP &&
		    if_addr->ipv4.addr_state == NET_ADDR_PREFERRED &&
		    if_addr->ipv4.address.in_addr.s_addr != 0) {
			return 0;
		}
	}

	k_sem_reset(&ipv4_acquired_sem);
	net_dhcpv4_stop(iface);
	net_dhcpv4_start(iface);

	if (k_sem_take(&ipv4_acquired_sem, K_SECONDS(30)) == 0) {
		return 0;
	}

	printk("wifi: ipv4 timeout\n");
	return -ETIMEDOUT;
}

static int wifi_reconnect(struct net_if *iface)
{
	int ret;

	ret = wifi_connect_wait(iface);
	if (ret < 0) {
		return ret;
	}

	return wifi_wait_for_ipv4(iface);
}

static void wifi_configure_power_save(struct net_if *iface)
{
	struct wifi_ps_params ps_mode = {
		.type = WIFI_PS_PARAM_WAKEUP_MODE,
#if defined(CONFIG_PM_WIFI_STRATEGY_BEACON)
		.wakeup_mode = WIFI_PS_WAKEUP_MODE_LISTEN_INTERVAL,
#else
		.wakeup_mode = WIFI_PS_WAKEUP_MODE_DTIM,
#endif
	};

	int mode = net_mgmt(NET_REQUEST_WIFI_PS, iface, &ps_mode, sizeof(ps_mode));

	printk("wifi: PM strategy=%s listen=%u mode_rc=%d\n",
	       IS_ENABLED(CONFIG_PM_WIFI_STRATEGY_BEACON) ? "BEACON" :
	       IS_ENABLED(CONFIG_PM_WIFI_STRATEGY_LONG_DOZE) ? "LONG_DOZE" :
							      "DTIM",
	       1U, mode);
#if defined(CONFIG_PM_WIFI_STRATEGY_NONE)
	struct wifi_ps_params ps_state = {
		.type = WIFI_PS_PARAM_STATE,
		.enabled = WIFI_PS_DISABLED,
	};

	int ps = net_mgmt(NET_REQUEST_WIFI_PS, iface, &ps_state, sizeof(ps_state));

	printk("wifi: PM strategy=NONE rc=%d\n", ps);
#endif
}

static int wifi_apply_power_save(struct net_if *iface, bool enable)
{
	struct wifi_ps_params ps_state = {
		.type = WIFI_PS_PARAM_STATE,
		.enabled = enable ? WIFI_PS_ENABLED : WIFI_PS_DISABLED,
	};
	int ret = net_mgmt(NET_REQUEST_WIFI_PS, iface, &ps_state, sizeof(ps_state));

	printk("wifi: PS %s rc=%d\n", enable ? "enabled" : "disabled", ret);
	return ret;
}

static int pm_wifi_sleep_window(struct net_if *iface)
{
	if (IS_ENABLED(CONFIG_PM_WIFI_LOW_POWER_SUSPEND)) {
#if defined(CONFIG_NET_POWER_MANAGEMENT)
		int ret;

		ret = net_if_suspend(iface);
		if (ret < 0 && ret != -EALREADY) {
			return ret;
		}
#endif

		pm_policy_state_lock_put(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
		k_msleep(WIFI_OFF_TIME_SECONDS * MSEC_PER_SEC);
		pm_policy_state_lock_get(PM_STATE_STANDBY, PM_ALL_SUBSTATES);

#if defined(CONFIG_NET_POWER_MANAGEMENT)
		ret = net_if_resume(iface);
		if (ret < 0 && ret != -EALREADY) {
			return ret;
		}
#endif
	} else {
		k_msleep(WIFI_OFF_TIME_SECONDS * MSEC_PER_SEC);
	}

	return 0;
}

int main(void)
{
	struct net_if *iface = net_if_get_first_wifi();
	uint32_t cycle = 0;
	int ret;

	printk("pm_wifi sample: boot\n");

	if (!iface) {
		printk("wifi: iface not ready\n");
		return -ENODEV;
	}

	wifi_events_init();

	ret = wifi_reconnect(iface);
	if (ret < 0) {
		return ret;
	}

	wifi_configure_power_save(iface);

	printk("pm_wifi: low-power mode=%s\n",
	       IS_ENABLED(CONFIG_PM_WIFI_LOW_POWER_SUSPEND) ? "suspend" : "wifi-ps");

	if (IS_ENABLED(CONFIG_PM_WIFI_LOW_POWER_PS)) {
		ret = wifi_apply_power_save(iface, true);
		if (ret < 0 && ret != -EALREADY) {
			return ret;
		}

		printk("pm_wifi: PS enabled, idle\n");
		while (1) {
			k_sleep(K_FOREVER);
		}
	}

	printk("pm_wifi: entering %ds active / %ds sleep loop\n",
	       WIFI_ON_TIME_SECONDS, WIFI_OFF_TIME_SECONDS);

	while (1) {
		printk("pm_wifi[%u]: wifi sleep for %ds\n",
		       cycle, WIFI_OFF_TIME_SECONDS);
		if (!wifi_link_up) {
			printk("pm_wifi[%u]: reconnecting after disconnect\n", cycle);
			ret = wifi_reconnect(iface);
			if (ret < 0) {
				printk("wifi: reconnect failed %d\n", ret);
				return ret;
			}
			wifi_configure_power_save(iface);
			continue;
		}

		ret = wifi_apply_power_save(iface, true);
		if (ret < 0 && ret != -EALREADY) {
			cycle++;
			continue;
		}

		ret = pm_wifi_sleep_window(iface);
		if (ret < 0) {
			printk("wifi: sleep window failed %d\n", ret);
		}

		ret = wifi_apply_power_save(iface, false);
		if (ret < 0 && ret != -EALREADY) {
			printk("wifi: PS restore failed %d\n", ret);
		}

		if (!wifi_link_up) {
			printk("pm_wifi[%u]: reconnecting after wake\n", cycle);
			ret = wifi_reconnect(iface);
			if (ret < 0) {
				printk("wifi: reconnect failed %d\n", ret);
				return ret;
			}
		}

		printk("pm_wifi[%u]: active for %ds\n",
		       cycle, WIFI_ON_TIME_SECONDS);
		k_msleep(WIFI_ON_TIME_SECONDS * MSEC_PER_SEC);

		cycle++;
	}
	return 0;
}
