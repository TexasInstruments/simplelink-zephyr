/**
 *  \file cli_esl_ap.c
 */

/*
 *  Copyright (C) 2025. LTI Mindtree Ltd.
 *  All rights reserved.
 */

/* --------------------------------------------- Header File Inclusion */
#include "cli_esl_ap.h"
/*#include <zephyr/sys/util.h>*/

#ifdef BT_ESL_SUPPORT_AP_ROLE

/* --------------------------------------------- Global Definitions */

/* --------------------------------------------- External Global Variables */

/* --------------------------------------------- Exported Global Variables */

/* --------------------------------------------- Static Global Variables */

/* --------------------------------------------- Function Prototype */

/* --------------------------------------------- Functions */

SHELL_STATIC_SUBCMD_SET_CREATE(cli_esl_ap,
    SHELL_CMD(init, NULL, "Initialize ESL AP", cmd_ap_init),
    SHELL_CMD(start_scan, NULL, "Start Scan", cmd_start_scan),
    SHELL_CMD(stop_scan, NULL, "Stop Scan", cmd_stop_scan),
    SHELL_CMD_ARG(discover, NULL, "Discover eslp Service", cmd_eslp_discover_service, 0, 0),
    SHELL_CMD_ARG(discover_ots, NULL, "Discover OTS Service", cmd_eslp_discover_ots_service, 0, 0),
    SHELL_CMD_ARG(discover_dis, NULL, "Discover DIS Service", cmd_eslp_discover_dis_service, 0, 0),
    SHELL_CMD(start_padv, NULL, "Start periodic Advertisements", cmd_eslp_start_periodic_adv),
    SHELL_CMD(stop_padv, NULL, "Stop periodic Advertisements", cmd_eslp_stop_periodic_adv),
    SHELL_CMD_ARG(multiple_cmd, NULL, "Enable/Disable multiple command mode", cmd_eslp_configure_multiple_command_mode, 0, 0),
    SHELL_CMD(push_cmds, NULL, "Push commands to per sync", cmd_eslp_push_to_per_sync),
    SHELL_CMD(clear_cmds, NULL, "Clear command buffer", cmd_eslp_clear_command_buffer),
    SHELL_CMD_ARG(add_esl, NULL, "Add ESL device", cmd_eslp_add_esl_dev, 0, 0),
    SHELL_CMD_ARG(connect_esl, NULL, "Connect to ESL device", cmd_eslp_connect_esl, 0, 0),
    SHELL_CMD_ARG(disconnect_esl, NULL, "Disconnect ESL device", cmd_eslp_disconnect_esl, 0, 0),
    SHELL_CMD_ARG(config_esl, NULL, "Configure ESL device", cmd_eslp_configure_esl_device, 0, 0),
    SHELL_CMD_ARG(sync_esl, NULL, "Sync ESL", cmd_eslp_sync_esl, 0, 0),
    SHELL_CMD_ARG(reset_esl, NULL, "Reset ESL device", cmd_eslp_reset_esl_device, 0, 0),
    SHELL_CMD_ARG(remove_esl, NULL, "Remove ESL device", cmd_eslp_remove_esl_device, 0, 0),
    SHELL_CMD(esl_dev_list, NULL, "Display ESL device info list", cmd_eslp_dispay_esl_device_info),
    SHELL_CMD(adv_dev_list, NULL, "Display ESL advertising list", cmd_eslp_dispay_esl_advertising_list),
    SHELL_CMD_ARG(disp_info, NULL, "Read display info", cmd_eslp_read_display_info, 0, 0),
    SHELL_CMD_ARG(led_info, NULL, "Read LED info", cmd_eslp_read_led_info, 0, 0),
    SHELL_CMD_ARG(img_info, NULL, "Read image info", cmd_eslp_read_image_info, 0, 0),
    SHELL_CMD_ARG(sensor_info, NULL, "Read sensor info", cmd_eslp_read_sensor_info, 0, 0),
    SHELL_CMD_ARG(ping, NULL, "Ping ESL", cmd_eslp_ping_command, 0, 0),
    SHELL_CMD_ARG(unassociate, NULL, "Unassociate ESL", cmd_eslp_unassociate_command, 0, 0),
    SHELL_CMD_ARG(service_reset, NULL, "Service reset ESL", cmd_eslp_service_reset_command, 0, 0),
    SHELL_CMD_ARG(factory_reset, NULL, "Factory reset ESL", cmd_eslp_factory_reset_command, 0, 0),
    SHELL_CMD_ARG(read_sensor, NULL, "Read sensor data ESL", cmd_eslp_read_sensor_data_command, 0, 0),
    SHELL_CMD_ARG(refresh_display, NULL, "Refresh display ESL", cmd_eslp_refresh_display_command, 0, 0),
    SHELL_CMD_ARG(display_image, NULL, "display image ESL", cmd_eslp_display_image_command, 0, 0),
    SHELL_CMD_ARG(control_led, NULL, "Control ESL LED", cmd_eslp_led_control_command, 0, 0),
    SHELL_CMD_ARG(upload_image, NULL, "Upload image to TAG", cmd_eslp_upload_image, 0, 0),
#ifdef APPL_ESL_DO_NOT_USE_DEFAULT_CONN_PARAMS
    SHELL_CMD_ARG(set_conn_interval, NULL, "Set preferred conn interval", cmd_eslp_set_conn_interval, 3, 0),
#endif /* APPL_ESL_DO_NOT_USE_DEFAULT_CONN_PARAMS */
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(esl_ap, &cli_esl_ap, "ESL AP commands", NULL);

/* Helper function to convert hex string to little-endian byte array */
static void str_to_array_le
            (
                 const struct shell * shell,
                 const CHAR         * str,
                 UINT16               str_len,
                 UCHAR              * array,
                 UINT16               size
            )
{
    UINT16 bytes_to_convert;
    UCHAR temp;
    UINT16 converted;
    INT32 i;

    if (str_len % 2 != 0)
    {
        CONSOLE_OUT("Invalid hex string length");
        return;
    }

    /* Init */
    bytes_to_convert = (str_len / 2) > size ? size : (str_len / 2);

    /* Convert hex string to bytes using hex2bin */
    converted = hex2bin(str, str_len, array, bytes_to_convert);

    /* Reverse the array in-place for little endian */
    for (i = 0; i < converted / 2; i++)
    {
        temp = array[i];
        array[i] = array[converted - 1 - i];
        array[converted - 1 - i] = temp;
    }
}

int cmd_ap_init(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;

    /* Init */
    retval = BT_ESL_AP_SUCCESS;

    /* Initialize the ESL tag */
    appl_init_esl();

    CONSOLE_OUT("ESL AP Initialized!");

    return retval;
}

int cmd_start_scan(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;

    CONSOLE_OUT("ESLP: Started Scanning\n");

    retval = appl_esl_ap_scan_esl_device(BT_ESL_TRUE);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to start scanning: 0x%04X", retval);
    }

    return retval;
}

int cmd_stop_scan(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;

    CONSOLE_OUT("ESLP: Stop Scanning\n");

    retval = appl_esl_ap_scan_esl_device(BT_ESL_FALSE);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to stop scanning: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_discover_service(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: discover <Grp ID> <ESL ID>");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Discovering ESLP service");

    retval = appl_esl_ap_discover_esl_service(&esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Service discovery failed: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_discover_ots_service(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: discover_ots <Grp ID> <ESL ID>");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Discovering OTS service");

    retval = appl_esl_ap_discover_ots(&esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Service discovery failed: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_discover_dis_service(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: discover_dis <Grp ID> <ESL ID>");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Discovering DIS service");

    retval = appl_esl_ap_discover_dis(&esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("DIS Service discovery failed: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_start_periodic_adv(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;

    /* Init */
    retval = 0;

    CONSOLE_OUT("ESLP: Start Periodic Adv\n");
    /* Start the periodic advertisement */
    retval = appl_esl_ap_start_periodic_adv();
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to start periodic advertisement: 0x%04X", retval);
    }
    return retval;
}

int cmd_eslp_stop_periodic_adv(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;

    /* Init */
    retval = 0;

    CONSOLE_OUT("ESLP: Stop Periodic Adv");
    /* Stop the periodic advertisement */
    retval = appl_esl_ap_stop_periodic_adv();
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to stop periodic advertisement: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_configure_multiple_command_mode(const struct shell *shell, size_t argc, char **argv)
{
    UCHAR flag;

    if (2 != argc)
    {
        CONSOLE_OUT("Usage: multiple_cmd <Flag>\n");
        CONSOLE_OUT("Flag <0- disable 1-enable>\n");
        return -ENOEXEC;
    }

    flag = (UCHAR)strtol(argv[1], NULL, 16);
    if(flag != 0 && flag != 1)
    {
        CONSOLE_OUT("Invalid Flag value. Use 0 or 1.\n");
        return -ENOEXEC;
    }

    CONSOLE_OUT("Configuring Multiple Command Mode");
    /* Configure the multiple command mode */
    appl_esl_ap_set_multiple_commands(flag);

    return 0;
}

int cmd_eslp_push_to_per_sync(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;

    CONSOLE_OUT("Pushing commands to per sync");

    retval = appl_esl_ap_send_esl_command();
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to push commands: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_clear_command_buffer(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;

    CONSOLE_OUT("Clearing command buffer");

    retval = appl_esl_ap_clear_esl_command_buf();
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to clear command buffer: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_add_esl_dev(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;
    BT_ESL_BD_ADDR peer_addr;

    if (5 != argc)
    {
        CONSOLE_OUT("Usage: add_esl <Peer addr type> <Peer address>"
                    " <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    peer_addr.type = (UCHAR)strtol(argv[1], NULL, 16);
    str_to_array_le
    (
         shell,
         argv[2],
         strlen(argv[2]),
         peer_addr.addr,
         sizeof(peer_addr.addr)
    );

    esl_addr.group_id = (UCHAR)strtol(argv[3], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[4], NULL, 16);

    CONSOLE_OUT("Adding ESL device");

    retval = appl_esl_ap_add_esl_tag(&esl_addr, &peer_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to add ESL device: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_connect_esl(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: connect_esl <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Connection to esl device");

    retval = appl_esl_ap_connect_esl(&esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to connect to ESL device: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_disconnect_esl(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: disconnect_esl <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Disconnection to esl device");
    retval = appl_esl_ap_disconnect_esl(&esl_addr);

    return retval;
}

int cmd_eslp_configure_esl_device(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: config_esl <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Configure a new ESL Device");

    retval = appl_esl_ap_config(&esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to configure ESL device: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_sync_esl(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: sync_esl <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("SYNC ESL");

    retval = appl_esl_ap_sync_with_esl(&esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to sync with ESL: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_reset_esl_device(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: reset_esl <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Reset ESL Device");

    retval = appl_esl_ap_reset_esl_tag(&esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to reset ESL device: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_remove_esl_device(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: remove_esl <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Remove ESL Device");

    retval = appl_esl_ap_remove_esl_tag(&esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to remove ESL device: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_dispay_esl_device_info(const struct shell *shell, size_t argc, char **argv)
{
    CONSOLE_OUT("ESLP: Display ESL Device Info\n");
    /* displaying esl display info */
    return 0;
}

int cmd_eslp_dispay_esl_advertising_list(const struct shell *shell, size_t argc, char **argv)
{
    CONSOLE_OUT("ESLP: Display ESL Advertising list\n");
    /* displaying esl advertising list */
    appl_esl_ap_display_advertising_list();
    return 0;
}

int cmd_eslp_read_display_info(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: disp_info <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Read Display Information");

    retval = appl_esl_ap_get_display_information(&esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to read display information: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_read_led_info(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: led_info <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Read LED Information");

    retval = appl_esl_ap_get_led_information(&esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to read LED information: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_read_image_info(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: img_info <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Read Image Information");

    retval = appl_esl_ap_get_image_information(&esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to read image information: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_read_sensor_info(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: sensor_info <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Read Sensor Information");

    retval = appl_esl_ap_get_sensor_information(&esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to read sensor information: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_ping_command(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;
    UCHAR multiple_cmd_flag;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: ping <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Ping ESL");
    multiple_cmd_flag = appl_esl_ap_is_multiple_commands_enabled();

    retval = appl_esl_ap_send_ping(&esl_addr, multiple_cmd_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to ping ESL: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_unassociate_command(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;
    UCHAR multiple_cmd_flag;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: unassociate <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Unassociate ESL");
    multiple_cmd_flag = appl_esl_ap_is_multiple_commands_enabled();

    retval = appl_esl_ap_send_unassociate(&esl_addr, multiple_cmd_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to unassociate ESL: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_service_reset_command(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;
    UCHAR multiple_cmd_flag;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: service_reset <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Service Reset ESL");
    multiple_cmd_flag = appl_esl_ap_is_multiple_commands_enabled();

    retval = appl_esl_ap_send_service_reset(&esl_addr, multiple_cmd_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to service reset ESL: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_factory_reset_command(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;

    if (3 != argc)
    {
        CONSOLE_OUT("Usage: factory_reset <Grp ID> <ESL ID>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);

    CONSOLE_OUT("Factory reset ESL");

    retval = appl_esl_ap_send_factory_reset(&esl_addr);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to factory reset ESL: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_read_sensor_data_command(const struct shell *shell, size_t argc, char **argv)
{
    UCHAR sensor_index;
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;
    UCHAR multiple_cmd_flag;

    if (4 != argc)
    {
        CONSOLE_OUT("Usage: read_sensor <Grp ID> <ESL ID> <sensor index>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);
    sensor_index = (UCHAR)strtol(argv[3], NULL, 10);

    CONSOLE_OUT("Read Sensor Data");
    multiple_cmd_flag = appl_esl_ap_is_multiple_commands_enabled();

    retval = appl_esl_ap_send_read_sensor_data(&esl_addr, sensor_index, multiple_cmd_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to read sensor data: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_refresh_display_command(const struct shell *shell, size_t argc, char **argv)
{
    UCHAR display_index;
    API_RESULT retval;
    BT_ESL_ADDR esl_addr;
    UCHAR multiple_cmd_flag;

    if (4 != argc)
    {
        CONSOLE_OUT("Usage: refresh_display <Grp ID> <ESL ID> <Display index>\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);
    display_index = (UCHAR)strtol(argv[3], NULL, 10);

    CONSOLE_OUT("Refresh ESL Display");
    multiple_cmd_flag = appl_esl_ap_is_multiple_commands_enabled();

    retval = appl_esl_ap_send_refresh_display(&esl_addr, display_index, multiple_cmd_flag);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to refresh display: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_display_image_command(const struct shell *shell, size_t argc, char **argv)
{
    UCHAR       display_index;
    UCHAR       image_index;
    UINT32      delay;
    API_RESULT  retval;
    BT_ESL_ADDR esl_addr;
    UCHAR       abs_time_flag;
    UINT32      absolute_time;
    UCHAR       multiple_cmd_flag;

    /* Initialize */
    retval        = BT_ESL_API_SUCCESS;
    abs_time_flag = BT_ESL_FALSE;
    delay         = 0;
    absolute_time = 0;

    if (argc < 5)
    {
        CONSOLE_OUT(
            "Usage: display_image <Grp Id> <ESL ID> <disp idx> <image index> "
            "[Abs time flag] [delay in s/absolute time]\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);
    display_index = (UCHAR)strtol(argv[3], NULL, 10);
    image_index = (UCHAR)strtol(argv[4], NULL, 10);

    /* Checking if it is Display timed control */
    if (argc > 5)
    {
        /* Check if 7 arguments exists */
        if (argc != 7)
        {
            CONSOLE_OUT(
            "Usage: display_image <Grp Id> <ESL ID> <disp idx> <image index> "
            "[Abs time flag] [delay in s/absolute time]\n");
            return -ENOEXEC;
        }
        abs_time_flag = (UINT32)strtol(argv[5], NULL, 10);
        if (BT_ESL_FALSE != abs_time_flag)
        {
            absolute_time = (UINT32)strtol(argv[6], NULL, 16);
        }
        else
        {
            delay = (UINT32)strtol(argv[6], NULL, 10);
            absolute_time = appl_esl_ap_get_current_abs_time() + (delay * 1000U);
        }
    }

    multiple_cmd_flag = appl_esl_ap_is_multiple_commands_enabled();

    if ((delay > 0) || (abs_time_flag == BT_ESL_TRUE))
    {
        CONSOLE_OUT("Display ESL Timed Image");
        /* Use timed image display */
        retval = appl_esl_ap_send_display_timed_image
                 (
                     &esl_addr,
                     display_index,
                     image_index,
                     absolute_time,
                     multiple_cmd_flag
                 );
    }
    else
    {
        CONSOLE_OUT("Display ESL Image");
        /* Use immediate image display */
        retval = appl_esl_ap_send_display_image
                 (
                     &esl_addr,
                     display_index,
                     image_index,
                     multiple_cmd_flag
                 );
    }

    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to display image: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_led_control_command(const struct shell *shell, size_t argc, char **argv)
{
    API_RESULT  retval;
    BT_ESL_ADDR esl_addr;
    UCHAR       led_index;
    UINT32      delay;
    UCHAR       color_brightness;
    UCHAR       flashing_pattern[7U];
    UINT16      repeat_type;
    UINT32      absolute_time;
    UCHAR       abs_time_flag;
    UCHAR       multiple_cmd_flag;

    /* Initialize */
    retval        = BT_ESL_API_SUCCESS;
    abs_time_flag = BT_ESL_FALSE;
    delay         = 0;
    absolute_time = 0;
    BT_ESL_mem_set(flashing_pattern, 0U, sizeof(flashing_pattern));

    if (argc < 7)
    {
        CONSOLE_OUT("Usage: control_led <Grp Id> <ESL ID> <LED idx>"
                    " <clr brightness> <flsh pattern> <rpt type> [Abs time flag] [delay in s/absolute time]\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);
    led_index = (UCHAR)strtol(argv[3], NULL, 10);
    color_brightness = (UCHAR)strtol(argv[4], NULL, 16);

    /* Parse flashing pattern using str_to_array_le */
    str_to_array_le
    (
         shell,
         argv[5],
         strlen(argv[5]),
         flashing_pattern,
         sizeof(flashing_pattern)
    );

    repeat_type = (UINT16)strtol(argv[6], NULL, 16);

    /* Checking if it is LED timed control */
    if (argc > 7)
    {
        /* Check if 9 arguments exists */
        if (argc != 9)
        {
            CONSOLE_OUT("Usage: control_led <Grp Id> <ESL ID> <LED idx>"
                    " <clr brightness> <flsh pattern> <rpt type> [Abs time flag] [delay in s/absolute time]\n");
            return -ENOEXEC;
        }
        abs_time_flag = (UINT32)strtol(argv[7], NULL, 10);
        if (BT_ESL_FALSE != abs_time_flag)
        {
            absolute_time = (UINT32)strtol(argv[8], NULL, 16);
        }
        else
        {
            delay = (UINT32)strtol(argv[8], NULL, 10);
            absolute_time = appl_esl_ap_get_current_abs_time() + (delay * 1000U);
        }
    }

    multiple_cmd_flag = appl_esl_ap_is_multiple_commands_enabled();

    if ((delay > 0) || (abs_time_flag == BT_ESL_TRUE))
    {
        CONSOLE_OUT("Control ESL LED Timed");
        /* Use timed LED control */
        retval = appl_esl_ap_send_led_timed_control
                 (
                     &esl_addr,
                     led_index,
                     color_brightness,
                     flashing_pattern,
                     repeat_type,
                     absolute_time,
                     multiple_cmd_flag
                 );
    }
    else
    {
        CONSOLE_OUT("Control ESL LED");
        /* Use immediate LED control */
        retval = appl_esl_ap_send_led_control
                 (
                     &esl_addr,
                     led_index,
                     color_brightness,
                     flashing_pattern,
                     repeat_type,
                     multiple_cmd_flag
                 );
    }

    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to control LED: 0x%04X", retval);
    }

    return retval;
}

int cmd_eslp_upload_image(const struct shell *shell, size_t argc, char **argv)
{
    UCHAR       image_index;
    UINT16      image_size;
    API_RESULT  retval;
    BT_ESL_ADDR esl_addr;

    /* Init */
    image_size = 0;

    if (argc < 4 || argc > 5)
    {
        CONSOLE_OUT("Usage: upload_image <Grp Id> <ESL ID> <Image idx> [Image size(20-512)]\n");
        return -ENOEXEC;
    }

    esl_addr.group_id = (UCHAR)strtol(argv[1], NULL, 16);
    esl_addr.esl_id = (UCHAR)strtol(argv[2], NULL, 16);
    image_index = (UCHAR)strtol(argv[3], NULL, 10);
    if (argc == 5)
    {
        image_size = (UINT16)strtol(argv[4], NULL, 10);
    }
    retval = appl_esl_ap_upload_image(&esl_addr, image_index, image_size);
    if (BT_ESL_AP_SUCCESS != retval)
    {
        CONSOLE_OUT("Failed to upload image: 0x%04X", retval);
    }

    return retval;
}

#ifdef APPL_ESL_DO_NOT_USE_DEFAULT_CONN_PARAMS
int cmd_eslp_set_conn_interval(const struct shell *shell, size_t argc, char **argv)
{
    UINT16 interval_min;
    UINT16 interval_max;

    if (argc != 3)
    {
        shell_print(shell, "Usage: set_conn_interval <min> <max> (in units of 1.25ms, hex)");
        return -ENOEXEC;
    }

    interval_min = (UINT16)strtol(argv[1], NULL, 16);
    interval_max = (UINT16)strtol(argv[2], NULL, 16);

    if (interval_min < 0x0006 || interval_max > 0x0C80 || interval_min > interval_max)
    {
        shell_error(shell, "Invalid interval range. Min: 0x0006-0x0C80, Min <= Max");
        return -EINVAL;
    }

    BT_esl_set_preferred_conn_interval_pl(interval_min, interval_max);

    shell_print(shell, "Preferred conn interval set: min=0x%04X (%.2f ms), max=0x%04X (%.2f ms)",
                interval_min, (double)(interval_min * 1.25),
                interval_max, (double)(interval_max * 1.25));

    return 0;
}
#endif /* APPL_ESL_DO_NOT_USE_DEFAULT_CONN_PARAMS */

#endif /* BT_ESL_SUPPORT_AP_ROLE */
