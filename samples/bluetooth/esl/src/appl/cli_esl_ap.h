
/**
 *  \file cli_esl_ap.h
 *
 *  This Header File contains Application Layer Declarations for Zephyr.
 */

/*
 *  Copyright (C) 2025. LTIMindtree Ltd.
 *  All rights reserved.
 */

#ifndef _H_CLI_ESL_AP_
#define _H_CLI_ESL_AP_

/* ----------------------------------------------- Header File Inclusion */
#include "appl_main.h"
#include "appl_esl_ap.h"

/* ----------------------------------------------- Macros */

/* ----------------------------------------------- Global Definitions */

/* ----------------------------------------------- Structures/Data Types */

/* ----------------------------------------------- Function Declarations */

int cmd_ap_init(const struct shell *shell, size_t argc, char **argv);
int cmd_start_scan(const struct shell *shell, size_t argc, char **argv);
int cmd_stop_scan(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_discover_service(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_discover_ots_service(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_discover_dis_service(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_start_periodic_adv(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_stop_periodic_adv(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_configure_multiple_command_mode(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_push_to_per_sync(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_clear_command_buffer(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_add_esl_dev(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_connect_esl(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_disconnect_esl(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_configure_esl_device(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_sync_esl(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_reset_esl_device(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_remove_esl_device(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_dispay_esl_device_info(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_dispay_esl_advertising_list(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_read_display_info(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_read_led_info(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_read_image_info(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_read_sensor_info(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_ping_command(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_unassociate_command(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_service_reset_command(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_factory_reset_command(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_read_sensor_data_command(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_refresh_display_command(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_display_image_command(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_led_control_command(const struct shell *shell, size_t argc, char **argv);
int cmd_reset(const struct shell *shell, size_t argc, char **argv);
int cmd_eslp_upload_image(const struct shell *shell, size_t argc, char **argv);

#ifdef APPL_ESL_DO_NOT_USE_DEFAULT_CONN_PARAMS
int cmd_eslp_set_conn_interval(const struct shell *shell, size_t argc, char **argv);
#endif /* APPL_ESL_DO_NOT_USE_DEFAULT_CONN_PARAMS */

#endif /* _H_CLI_ESL_AP_ */

