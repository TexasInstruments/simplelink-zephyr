/**
 *  \file appl_main.h
 *
 *  This Header File contains common application wrappers for ESL.
 */

/*
 *  Copyright (C) 2025. LTIMindtree Ltd.
 *  All rights reserved.
 */

#ifndef _H_APPL_MAIN_
#define _H_APPL_MAIN_

/* ----------------------------------------------- Header File Inclusion */
#include "BT_esl_pl.h"
#include "BT_esl_api.h"

/* ----------------------------------------------- Macros */
#define APPL_ESL_ERR(...) CONSOLE_ERR(__VA_ARGS__)
#define APPL_ESL_TRC(...) CONSOLE_TRC(__VA_ARGS__)
#define APPL_ESL_INF(...) CONSOLE_INF(__VA_ARGS__)

/* ----------------------------------------------- Global Definitions */

/* ----------------------------------------------- Structures/Data Types */

/* -------------------------------------------- Function Declarations */
/** Initialize pl */
void appl_init_pl(void);

/** Init ESL */
void appl_init_esl(void);

#endif /* _H_APPL_MAIN_ */
