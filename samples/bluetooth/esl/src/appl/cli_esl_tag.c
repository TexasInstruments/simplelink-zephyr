/**
 *  \file cli_esl_tag.c
 */

/*
 *  Copyright (C) 2025. LTIMindtree Ltd.
 *  All rights reserved.
 */

/* --------------------------------------------- Header File Inclusion */
#include "cli_esl_tag.h"

#ifdef BT_ESL_SUPPORT_TAG_ROLE

/* --------------------------------------------- Global Definitions */

/* --------------------------------------------- External Global Variables */

/* --------------------------------------------- Exported Global Variables */

/* --------------------------------------------- Static Global Variables */

/* --------------------------------------------- Function Prototype */

/* --------------------------------------------- Functions */

SHELL_STATIC_SUBCMD_SET_CREATE(cli_esl_tag,
    SHELL_CMD_ARG(init, NULL, "Initialize ESL Tag", cmd_tag_init, 0, 0),
    SHELL_CMD(start, NULL, "Start Advertisement", cmd_start_adv),
    SHELL_CMD(stop, NULL, "Stop Advertisement", cmd_stop_adv),
    SHELL_CMD_ARG(config_image, NULL, "config_image <Flag>", cmd_config_image, 0, 0),
    SHELL_CMD(config_bs, NULL, "Configure Service Needed Bit", cmd_config_bs),
    SHELL_CMD(get_time, NULL, "Get Current Time", cmd_get_time),
    SHELL_CMD(reset, NULL, "Reset ESL", cmd_reset),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(esl_tag, &cli_esl_tag, "ESL Tag commands", NULL);

int cmd_tag_init(const struct shell *shell, size_t argc, char **argv)
{
    int result;
    UCHAR flag;

    /* Init */
    result = 0;
    flag = BT_ESL_FALSE;

    if (1 < argc)
    {
        flag = atoi(argv[1]);
    }

    /** Setting single IO mode (disabled by default) */
    appl_esl_set_single_io_mode(flag);

    /* Initialize the ESL tag */
    appl_init_esl();

    CONSOLE_OUT("ESL Tag Initialized!");

    return result;
}

int cmd_start_adv(const struct shell *shell, size_t argc, char **argv)
{
    int result = 0;
    CONSOLE_OUT("Starting Advertisement");
    /* Start the advertisement */
    result = appl_esl_tag_start_advertise();
    return result;
}

int cmd_stop_adv(const struct shell *shell, size_t argc, char **argv)
{
    int result = 0;
    CONSOLE_OUT("Stopping Advertisement");
    /* Stop the advertisement */
    result = appl_esl_tag_stop_advertise();
    return result;
}

int cmd_config_image(const struct shell *shell, size_t argc, char **argv)
{
    UCHAR flag;
    int result = 0;

    if (2 != argc)
    {
        CONSOLE_OUT("Usage: config_image <Flag>\n");
        CONSOLE_OUT("Flag <0- No image displayed 1-Image being displayed>\n");
        return -ENOEXEC;
    }

    flag = atoi(argv[1]);
    if(flag != 0 && flag != 1)
    {
        CONSOLE_OUT("Invalid Flag value. Use 0 or 1.\n");
        return -ENOEXEC;
    }

    /* Configure the image to display */
    CONSOLE_OUT("Configuring Image");
    appl_esl_config_image_on_display(flag);

    return result;
}

int cmd_config_bs(const struct shell *shell, size_t argc, char **argv)
{
    int result = 0;

    /* Configure serviceneeded bit to true in basic state */
    CONSOLE_OUT("Configuring Service Needed Bit");
    appl_esl_set_service_needed_flag();

    return result;
}

int cmd_get_time(const struct shell *shell, size_t argc, char **argv)
{
    int result = 0;
    UINT32 curr_time;

    CONSOLE_OUT("Getting Current Time");

    /* Get Current time */
    curr_time = BT_esl_get_current_time_pl();
    CONSOLE_OUT("Current Time: 0x%08X seconds since power-cycle", curr_time);

    return result;
}

int cmd_reset(const struct shell *shell, size_t argc, char **argv)
{
    int result = 0;
    CONSOLE_OUT("Resetting ESL");
    /* Reset ESL Tag */
    appl_esl_reset_tag();

    return result;
}

#endif /* BT_ESL_SUPPORT_TAG_ROLE */
