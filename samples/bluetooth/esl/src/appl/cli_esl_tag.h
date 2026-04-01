/**
 *  \file cli_esl_tag.h
 *
 *  This Header File contains Application Layer Declarations for Zephyr.
 */

/*
 *  Copyright (C) 2025. LTIMindtree Ltd.
 *  All rights reserved.
 */

#ifndef _H_CLI_ESL_TAG_
#define _H_CLI_ESL_TAG_

/* ----------------------------------------------- Header File Inclusion */
#include "appl_main.h"
#include "appl_esl_tag.h"

/* ----------------------------------------------- Macros */

/* ----------------------------------------------- Global Definitions */

/* ----------------------------------------------- Structures/Data Types */

/* ----------------------------------------------- Function Declarations */

int cmd_tag_init(const struct shell *shell, size_t argc, char **argv);
int cmd_start_adv(const struct shell *shell, size_t argc, char **argv);
int cmd_stop_adv(const struct shell *shell, size_t argc, char **argv);
int cmd_config_image(const struct shell *shell, size_t argc, char **argv);
int cmd_config_bs(const struct shell *shell, size_t argc, char **argv);
int cmd_get_time(const struct shell *shell, size_t argc, char **argv);
int cmd_reset(const struct shell *shell, size_t argc, char **argv);

#endif /* _H_CLI_ESL_TAG_ */

