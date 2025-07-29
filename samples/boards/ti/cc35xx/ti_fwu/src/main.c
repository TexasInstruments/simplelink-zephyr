/*
 * Copyright (c) 2026 Conclusive Engineering Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/http/parser_url.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/shell/shell.h>

#include <kernel/zephyr/dpl/ti_flash_map_config.h>
#include <psa_fwu.h>
#include <ti/drivers/xmem/XMEMWFF3.h>

#define TI_FWU_GPE_MAGIC 0x690c47c2U
#define TI_FWU_HTTP_RECV_BUF_SIZE 4096
#define TI_FWU_HTTP_DEFAULT_PORT 80
#define TI_FWU_HTTP_TIMEOUT_MS 15000

struct ti_fwu_url {
	char host[64];
	char path[160];
	char port_str[6];
	uint16_t port;
};

struct ti_fwu_stream {
	const struct shell *sh;
	psa_fwu_component_t component;
	uint8_t manifest[TI_FWU_MANIFEST_SIZE];
	size_t manifest_len;
	size_t image_offset;
	size_t image_bytes;
	int ret;
	bool started;
};

struct ti_fwu_slot {
	psa_fwu_component_t id;
	const char *name;
	uint32_t *physical_address;
	uint32_t *logical_address;
	uint32_t *size;
};

static const struct ti_fwu_slot slots[] = {
	{ BL2_Slot_1, "bl2_1", &bl2_physical_slot_1_address,
	  &bl2_logical_slot_1_address, &bl2_slot_1_region_size },
	{ BL2_Slot_2, "bl2_2", &bl2_physical_slot_2_address,
	  &bl2_logical_slot_2_address, &bl2_slot_2_region_size },
	{ WSOC_OR_RFTool_Slot_1, "wsoc_1", &wifi_connectivity_physical_slot_1_address,
	  &wifi_connectivity_logical_slot_1_address, &wifi_connectivity_slot_1_region_size },
	{ WSOC_OR_RFTool_Slot_2, "wsoc_2", &wifi_connectivity_physical_slot_2_address,
	  &wifi_connectivity_logical_slot_2_address, &wifi_connectivity_slot_2_region_size },
	{ Vendor_Image_Slot_1, "vendor_1", &vendor_image_physical_slot_1_address,
	  &vendor_image_logical_slot_1_address, &vendor_image_slot_1_region_size },
	{ Vendor_Image_Slot_2, "vendor_2", &vendor_image_physical_slot_2_address,
	  &vendor_image_logical_slot_2_address, &vendor_image_slot_2_region_size },
};

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback dhcp_cb;

static const struct ti_fwu_slot *ti_fwu_slot_get(psa_fwu_component_t component)
{
	for (size_t i = 0; i < ARRAY_SIZE(slots); i++) {
		if (slots[i].id == component) {
			return &slots[i];
		}
	}

	return NULL;
}

static const struct ti_fwu_slot *ti_fwu_slot_from_name(const char *name)
{
	char *end;
	unsigned long id;

	for (size_t i = 0; i < ARRAY_SIZE(slots); i++) {
		if (strcmp(name, slots[i].name) == 0) {
			return &slots[i];
		}
	}

	id = strtoul(name, &end, 0);
	if (*name != '\0' && *end == '\0') {
		return ti_fwu_slot_get((psa_fwu_component_t)id);
	}

	return NULL;
}

static const char *ti_fwu_state_name(uint8_t state)
{
	switch (state) {
	case PSA_FWU_READY:
		return "ready";
	case PSA_FWU_WRITING:
		return "writing";
	case PSA_FWU_CANDIDATE:
		return "candidate";
	case PSA_FWU_STAGED:
		return "staged";
	case PSA_FWU_FAILED:
		return "failed";
	case PSA_FWU_TRIAL:
		return "trial";
	case PSA_FWU_REJECTED:
		return "rejected";
	case PSA_FWU_UPDATED:
		return "updated";
	default:
		return "unknown";
	}
}

static bool ti_fwu_ota_layout_present(void)
{
	return vendor_image_slot_2_region_size != 0U ||
	       wifi_connectivity_slot_2_region_size != 0U ||
	       bl2_slot_2_region_size != 0U;
}

static void ti_fwu_print(const struct shell *sh, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (sh != NULL) {
		shell_vfprintf(sh, SHELL_NORMAL, fmt, args);
		shell_fprintf(sh, SHELL_NORMAL, "\n");
	} else {
		vprintf(fmt, args);
		printf("\n");
	}
	va_end(args);
}

static void ti_fwu_wifi_event_handler(struct net_mgmt_event_callback *cb,
				      uint64_t mgmt_event, struct net_if *iface)
{
	const struct wifi_status *status = cb->info;

	if (mgmt_event != NET_EVENT_WIFI_CONNECT_RESULT) {
		return;
	}

	if (status != NULL && status->status != WIFI_STATUS_CONN_SUCCESS) {
		printk("ti_fwu: wifi connect failed status=%d\n", status->status);
		return;
	}

	printk("ti_fwu: wifi connected, starting DHCPv4\n");
	net_dhcpv4_start(iface);
}

static void ti_fwu_dhcp_event_handler(struct net_mgmt_event_callback *cb,
				      uint64_t mgmt_event, struct net_if *iface)
{
	char addr[NET_IPV4_ADDR_LEN];

	ARG_UNUSED(cb);

	if (mgmt_event != NET_EVENT_IPV4_DHCP_BOUND) {
		return;
	}

	net_addr_ntop(AF_INET, &iface->config.dhcpv4.requested_ip,
		      addr, sizeof(addr));
	printk("ti_fwu: DHCPv4 address %s\n", addr);
}

static void ti_fwu_net_init(void)
{
	net_mgmt_init_event_callback(&wifi_cb, ti_fwu_wifi_event_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	net_mgmt_init_event_callback(&dhcp_cb, ti_fwu_dhcp_event_handler,
				     NET_EVENT_IPV4_DHCP_BOUND);
	net_mgmt_add_event_callback(&dhcp_cb);
}

#if defined(CONFIG_TI_FWU_WIFI_AUTO_CONNECT)
static int ti_fwu_wifi_connect(void)
{
	struct net_if *iface = net_if_get_default();
	struct wifi_connect_req_params params = {
		.ssid = CONFIG_TI_FWU_WIFI_SSID,
		.ssid_length = sizeof(CONFIG_TI_FWU_WIFI_SSID) - 1,
		.psk = CONFIG_TI_FWU_WIFI_PASSWORD,
		.psk_length = sizeof(CONFIG_TI_FWU_WIFI_PASSWORD) - 1,
		.sae_password = CONFIG_TI_FWU_WIFI_PASSWORD,
		.sae_password_length = sizeof(CONFIG_TI_FWU_WIFI_PASSWORD) - 1,
		.channel = WIFI_CHANNEL_ANY,
		.security = IS_ENABLED(CONFIG_TI_FWU_WIFI_SECURITY_WPA3) ?
			    WIFI_SECURITY_TYPE_SAE : WIFI_SECURITY_TYPE_PSK,
		.mfp = IS_ENABLED(CONFIG_TI_FWU_WIFI_SECURITY_WPA3) ?
		       WIFI_MFP_REQUIRED : WIFI_MFP_DISABLE,
		.timeout = CONFIG_TI_FWU_WIFI_CONNECT_TIMEOUT_SEC,
	};
	int ret;

	if (params.ssid_length == 0U || params.psk_length == 0U) {
		printk("ti_fwu: auto-connect requires configured SSID and password\n");
		return -EINVAL;
	}

	printk("ti_fwu: connecting to %s (%s)\n", CONFIG_TI_FWU_WIFI_SSID,
	       IS_ENABLED(CONFIG_TI_FWU_WIFI_SECURITY_WPA3) ? "WPA3-SAE" : "WPA2-PSK");
	ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
	if (ret != 0) {
		printk("ti_fwu: connect request failed ret=%d\n", ret);
	}

	return ret;
}
#endif /* CONFIG_TI_FWU_WIFI_AUTO_CONNECT */

static int ti_fwu_read_raw_version(const struct ti_fwu_slot *slot,
				   PSA_FWU_GPEVersion_t *version,
				   uint32_t *magic)
{
	XMEM_Params params;
	XMEM_Handle handle;
	int_fast16_t ret;

	if (*slot->size == 0U || *slot->logical_address == 0U) {
		return -ENOENT;
	}

	params.regionBase = *slot->physical_address;
	params.regionStartAddr = *slot->logical_address;
	params.regionSize = *slot->size;
	params.deviceNum = XMEM_FLASH;

	handle = XMEMWFF3_open(&params);
	if (handle == NULL) {
		return -ENODEV;
	}

	ret = XMEMWFF3_read(handle, 0xffc, magic, sizeof(*magic), XMEM_READ_STIG);
	if (ret != XMEM_STATUS_SUCCESS) {
		XMEMWFF3_close(handle);
		return -EIO;
	}

	if (*magic != TI_FWU_GPE_MAGIC) {
		XMEMWFF3_close(handle);
		return -ENOEXEC;
	}

	ret = XMEMWFF3_read(handle, 0x1010, version, sizeof(*version), XMEM_READ);
	XMEMWFF3_close(handle);

	return ret == XMEM_STATUS_SUCCESS ? 0 : -EIO;
}

static void ti_fwu_print_slot(const struct shell *sh, const struct ti_fwu_slot *slot)
{
	PSA_FWU_GPEVersion_t raw_version;
	psa_fwu_component_info_t info;
	psa_status_t status;
	uint32_t magic = 0;
	int ret;

	ti_fwu_print(sh, "%-8s id=%u physical=0x%08x logical=0x%08x size=0x%08x",
		     slot->name, slot->id, *slot->physical_address,
		     *slot->logical_address, *slot->size);

	ret = ti_fwu_read_raw_version(slot, &raw_version, &magic);
	if (ret == 0) {
		ti_fwu_print(sh, "  raw_magic=0x%08x raw_version=%u.%u.%u.%u",
			     magic, raw_version.iv_major, raw_version.iv_minor,
			     raw_version.iv_revision, raw_version.iv_build_num);
	} else if (ret == -ENOEXEC) {
		ti_fwu_print(sh, "  raw_magic=0x%08x raw=empty/invalid", magic);
	} else {
		ti_fwu_print(sh, "  raw=unavailable ret=%d", ret);
	}

	status = psa_fwu_query(slot->id, &info);
	if (status != PSA_SUCCESS) {
		ti_fwu_print(sh, "  query failed status=%d", status);
		return;
	}

	ti_fwu_print(sh,
		     "  state=%s(%u) error=%d primary=%u running=%u request=%u gpe=%u version=%u.%u.%u.%u max=0x%08x",
		     ti_fwu_state_name(info.state), info.state, info.error,
		     info.impl.Primary, info.impl.running_status,
		     info.impl.request_type, info.impl.GPE_state,
		     info.version.major, info.version.minor, info.version.patch,
		     info.version.build, info.max_size);
}

static int ti_fwu_prepare_target(psa_fwu_component_t component)
{
	psa_fwu_component_info_t info;
	psa_status_t status;

	status = psa_fwu_query(component, &info);
	if (status != PSA_SUCCESS) {
		return status;
	}

	if (info.impl.Primary) {
		return -EPERM;
	}

	switch (info.state) {
	case PSA_FWU_READY:
		return 0;
	case PSA_FWU_WRITING:
	case PSA_FWU_CANDIDATE:
		status = psa_fwu_cancel(component);
		if (status != PSA_SUCCESS) {
			return status;
		}
		break;
	case PSA_FWU_FAILED:
	case PSA_FWU_REJECTED:
	case PSA_FWU_UPDATED:
		break;
	default:
		return -EALREADY;
	}

	status = psa_fwu_clean(component);

	return status == PSA_SUCCESS ? 0 : status;
}

static int ti_fwu_stream_write(struct ti_fwu_stream *stream, const uint8_t *data,
			       size_t len)
{
	psa_status_t status;
	size_t copy_len;

	if (!stream->started) {
		copy_len = MIN(len, sizeof(stream->manifest) - stream->manifest_len);
		memcpy(&stream->manifest[stream->manifest_len], data, copy_len);
		stream->manifest_len += copy_len;
		data += copy_len;
		len -= copy_len;

		if (stream->manifest_len == sizeof(stream->manifest)) {
			ti_fwu_print(stream->sh, "manifest received (%u bytes)",
				     (unsigned int)sizeof(stream->manifest));
			status = psa_fwu_start(stream->component, stream->manifest,
					       sizeof(stream->manifest));
			if (status != PSA_SUCCESS) {
				ti_fwu_print(stream->sh,
					     "psa_fwu_start failed status=%d",
					     status);
				return status;
			}

			stream->started = true;
			stream->image_offset = sizeof(stream->manifest);
		}
	}

	if (len == 0 || !stream->started) {
		return 0;
	}

	status = psa_fwu_write(stream->component, stream->image_offset, data, len);
	if (status != PSA_SUCCESS) {
		ti_fwu_print(stream->sh,
			     "psa_fwu_write failed offset=%u len=%u status=%d",
			     (unsigned int)stream->image_offset, (unsigned int)len,
			     status);
		return status;
	}

	stream->image_offset += len;
	stream->image_bytes += len;

	if ((stream->image_bytes % (64U * 1024U)) < len) {
		ti_fwu_print(stream->sh, "written %u bytes",
			     (unsigned int)(stream->image_bytes +
					    sizeof(stream->manifest)));
	}

	return 0;
}

static int ti_fwu_parse_http_url(const char *url, struct ti_fwu_url *parsed)
{
	struct http_parser_url parser;
	const char http_scheme[] = "http";
	uint16_t off;
	uint16_t len;
	int ret;

	http_parser_url_init(&parser);
	ret = http_parser_parse_url(url, strlen(url), 0, &parser);
	if (ret != 0) {
		return -EINVAL;
	}

	if ((parser.field_set & BIT(UF_SCHEMA)) == 0 ||
	    (parser.field_set & BIT(UF_HOST)) == 0 ||
	    (parser.field_set & BIT(UF_PATH)) == 0) {
		return -EINVAL;
	}

	off = parser.field_data[UF_SCHEMA].off;
	len = parser.field_data[UF_SCHEMA].len;
	if (len != strlen(http_scheme) ||
	    strncmp(url + off, http_scheme, len) != 0) {
		return -EINVAL;
	}

	if ((parser.field_set & BIT(UF_PORT)) != 0) {
		parsed->port = parser.port;
	} else {
		parsed->port = TI_FWU_HTTP_DEFAULT_PORT;
	}

	ret = snprintk(parsed->port_str, sizeof(parsed->port_str), "%u",
		       parsed->port);
	if (ret <= 0 || ret >= sizeof(parsed->port_str)) {
		return -ENOBUFS;
	}

	off = parser.field_data[UF_HOST].off;
	len = parser.field_data[UF_HOST].len;
	if (len == 0 || len >= sizeof(parsed->host)) {
		return -ENOBUFS;
	}

	memcpy(parsed->host, url + off, len);
	parsed->host[len] = '\0';

	off = parser.field_data[UF_PATH].off;
	len = parser.field_data[UF_PATH].len;
	if (len <= 1 || len >= sizeof(parsed->path)) {
		return -ENOBUFS;
	}

	memcpy(parsed->path, url + off, len);
	parsed->path[len] = '\0';

	return 0;
}

static int ti_fwu_http_response_cb(struct http_response *rsp,
				   enum http_final_call final_data,
				   void *user_data)
{
	struct ti_fwu_stream *stream = user_data;
	int ret;

	ARG_UNUSED(final_data);

	if (stream->ret != 0) {
		return 0;
	}

	if (rsp->http_status_code != 0U && rsp->http_status_code != 200U) {
		ti_fwu_print(stream->sh, "unexpected HTTP status: %u %s",
			     rsp->http_status_code, rsp->http_status);
		stream->ret = -EIO;
		return 0;
	}

	if (rsp->body_frag_len == 0U) {
		return 0;
	}

	ret = ti_fwu_stream_write(stream, rsp->body_frag_start,
				  rsp->body_frag_len);
	if (ret != 0) {
		stream->ret = ret;
	}

	return 0;
}

static int ti_fwu_download_http(const struct shell *sh,
				const struct ti_fwu_url *url,
				struct ti_fwu_stream *stream)
{
	static uint8_t recv_buf[TI_FWU_HTTP_RECV_BUF_SIZE];
	struct http_request req = {
		.method = HTTP_GET,
		.response = ti_fwu_http_response_cb,
		.recv_buf = recv_buf,
		.recv_buf_len = sizeof(recv_buf),
		.url = url->path,
		.protocol = "HTTP/1.1",
		.host = url->host,
		.port = url->port_str,
	};
	struct sockaddr_in server = {
		.sin_family = AF_INET,
		.sin_port = htons(url->port),
	};
	struct timeval timeout = {
		.tv_sec = TI_FWU_HTTP_TIMEOUT_MS / MSEC_PER_SEC,
	};
	int sock;
	int ret;

	ret = zsock_inet_pton(AF_INET, url->host, &server.sin_addr);
	if (ret != 1) {
		ti_fwu_print(sh, "only IPv4 literal hosts are supported: %s",
			     url->host);
		return -EINVAL;
	}

	sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		return -errno;
	}

	zsock_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	zsock_setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	ti_fwu_print(sh, "connecting to %s:%u", url->host, url->port);
	ret = zsock_connect(sock, (struct sockaddr *)&server, sizeof(server));
	if (ret != 0) {
		ret = -errno;
		goto out;
	}

	stream->ret = 0;
	ret = http_client_req(sock, &req, TI_FWU_HTTP_TIMEOUT_MS, stream);
	if (ret < 0) {
		goto out;
	}

	if (stream->ret != 0) {
		ret = stream->ret;
		goto out;
	}

	if (!stream->started) {
		ret = -EINVAL;
		goto out;
	}

	ret = 0;

out:
	zsock_close(sock);
	return ret;
}

static int cmd_fwu_list(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	for (size_t i = 0; i < ARRAY_SIZE(slots); i++) {
		ti_fwu_print_slot(sh, &slots[i]);
	}

	return 0;
}

static int cmd_fwu_query(const struct shell *sh, size_t argc, char **argv)
{
	const struct ti_fwu_slot *slot = ti_fwu_slot_from_name(argv[1]);

	ARG_UNUSED(argc);

	if (slot == NULL) {
		shell_error(sh, "unknown component: %s", argv[1]);
		return -EINVAL;
	}

	ti_fwu_print_slot(sh, slot);

	return 0;
}

static int cmd_fwu_download(const struct shell *sh, size_t argc, char **argv)
{
	struct ti_fwu_url url;
	struct ti_fwu_stream stream = {
		.sh = sh,
	};
	const struct ti_fwu_slot *slot = ti_fwu_slot_from_name(argv[2]);
	psa_status_t status;
	int ret;

	ARG_UNUSED(argc);

	if (slot == NULL) {
		shell_error(sh, "unknown component: %s", argv[2]);
		return -EINVAL;
	}

	ret = ti_fwu_parse_http_url(argv[1], &url);
	if (ret != 0) {
		shell_error(sh, "invalid URL; use http://IPv4[:port]/path");
		return ret;
	}

	ret = ti_fwu_prepare_target(slot->id);
	if (ret != 0) {
		shell_error(sh, "target prepare failed ret=%d", ret);
		return ret;
	}

	stream.component = slot->id;
	ret = ti_fwu_download_http(sh, &url, &stream);
	if (ret != 0) {
		if (stream.started) {
			psa_fwu_cancel(slot->id);
		}
		shell_error(sh, "download failed ret=%d", ret);
		return ret;
	}

	ti_fwu_print(sh, "download complete image_size=%u",
		     (unsigned int)(stream.image_bytes + sizeof(stream.manifest)));

	status = psa_fwu_finish(slot->id);
	if (status != PSA_SUCCESS) {
		psa_fwu_cancel(slot->id);
		shell_error(sh, "psa_fwu_finish failed status=%d", status);
		return status;
	}

	shell_print(sh, "candidate written; run 'fwu install' and then 'fwu reboot'");

	return 0;
}

static int cmd_fwu_install(const struct shell *sh, size_t argc, char **argv)
{
	psa_status_t status;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	status = psa_fwu_install();
	if (status != PSA_SUCCESS_REBOOT && status != PSA_SUCCESS) {
		shell_error(sh, "psa_fwu_install failed status=%d", status);
		return status;
	}

	shell_print(sh, "install accepted status=%d", status);

	return 0;
}

static int cmd_fwu_reboot(const struct shell *sh, size_t argc, char **argv)
{
	psa_status_t status;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	status = psa_fwu_request_reboot();
	shell_print(sh, "psa_fwu_request_reboot status=%d", status);

	return status == PSA_SUCCESS_REBOOT || status == PSA_SUCCESS ? 0 : status;
}

static int cmd_fwu_accept(const struct shell *sh, size_t argc, char **argv)
{
	psa_status_t status;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	status = psa_fwu_accept();
	shell_print(sh, "psa_fwu_accept status=%d", status);

	return status == PSA_SUCCESS_REBOOT || status == PSA_SUCCESS ? 0 : status;
}

static int cmd_fwu_reject(const struct shell *sh, size_t argc, char **argv)
{
	psa_status_t error = PSA_ERROR_GENERIC_ERROR;
	psa_status_t status;
	char *end;

	if (argc > 1) {
		error = strtol(argv[1], &end, 0);
		if (*argv[1] == '\0' || *end != '\0') {
			return -EINVAL;
		}
	}

	status = psa_fwu_reject(error);
	shell_print(sh, "psa_fwu_reject status=%d", status);

	return status == PSA_SUCCESS_REBOOT || status == PSA_SUCCESS ? 0 : status;
}

static int cmd_fwu_cancel(const struct shell *sh, size_t argc, char **argv)
{
	const struct ti_fwu_slot *slot = ti_fwu_slot_from_name(argv[1]);
	psa_status_t status;

	ARG_UNUSED(argc);

	if (slot == NULL) {
		shell_error(sh, "unknown component: %s", argv[1]);
		return -EINVAL;
	}

	status = psa_fwu_cancel(slot->id);
	shell_print(sh, "psa_fwu_cancel %s status=%d", slot->name, status);

	return status == PSA_SUCCESS ? 0 : status;
}

static int cmd_fwu_clean(const struct shell *sh, size_t argc, char **argv)
{
	const struct ti_fwu_slot *slot = ti_fwu_slot_from_name(argv[1]);
	psa_status_t status;

	ARG_UNUSED(argc);

	if (slot == NULL) {
		shell_error(sh, "unknown component: %s", argv[1]);
		return -EINVAL;
	}

	status = psa_fwu_clean(slot->id);
	shell_print(sh, "psa_fwu_clean %s status=%d", slot->name, status);

	return status == PSA_SUCCESS ? 0 : status;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_fwu,
	SHELL_CMD_ARG(list, NULL, "List FWU slots and states", cmd_fwu_list, 1, 0),
	SHELL_CMD_ARG(query, NULL, "Query one slot: <slot>", cmd_fwu_query, 2, 0),
	SHELL_CMD_ARG(download, NULL, "Download update: <url> <slot>",
		      cmd_fwu_download, 3, 0),
	SHELL_CMD_ARG(install, NULL, "Stage written candidates", cmd_fwu_install, 1, 0),
	SHELL_CMD_ARG(reboot, NULL, "Request FWU reboot", cmd_fwu_reboot, 1, 0),
	SHELL_CMD_ARG(accept, NULL, "Accept running trial image", cmd_fwu_accept, 1, 0),
	SHELL_CMD_ARG(reject, NULL, "Reject staged/trial image: [error]",
		      cmd_fwu_reject, 1, 1),
	SHELL_CMD_ARG(cancel, NULL, "Cancel writing/candidate slot: <slot>",
		      cmd_fwu_cancel, 2, 0),
	SHELL_CMD_ARG(clean, NULL, "Erase failed/updated slot: <slot>",
		      cmd_fwu_clean, 2, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(fwu, &sub_fwu, "TI CC35xx FWU commands", NULL);

int main(void)
{
	printf("ti_fwu: CC35xx TI FWU shell sample\n");

	if (!ti_fwu_ota_layout_present()) {
		printf("ti_fwu: OTA layout not present; update slots are empty\n");
		return 0;
	}

	psa_fwu_init();
	ti_fwu_net_init();
#if defined(CONFIG_TI_FWU_WIFI_AUTO_CONNECT)
	ti_fwu_wifi_connect();
#endif
	cmd_fwu_list(NULL, 0, NULL);
	printf("ti_fwu: connect Wi-Fi with shell; DHCP starts after connect\n");
	printf("ti_fwu: use 'fwu download http://<ip>:<port>/<image> <slot>'\n");

	return 0;
}
