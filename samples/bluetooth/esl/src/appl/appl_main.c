/**
 *  \file appl_main.c
 *
 */

/*
 *  Copyright (C) 2025. LTIMindtree Ltd.
 *  All rights reserved.
 */

/* --------------------------------------------- Header File Inclusion */
#include "appl_main.h"

#ifdef BT_ESL_SUPPORT_TAG_ROLE
#include "appl_esl_tag.h"
#endif /* BT_ESL_SUPPORT_TAG_ROLE */

#ifdef BT_ESL_SUPPORT_AP_ROLE
#include "appl_esl_ap.h"
#endif /* BT_ESL_SUPPORT_AP_ROLE */

#ifdef BT_ESL_SUPPORT_VERSION_INFO
#include "BT_esl_version.h"
#endif /* BT_ESL_SUPPORT_VERSION_INFO */

/* --------------------------------------------- Global Definitions */

/* --------------------------------------------- External Global Variables */

/* --------------------------------------------- Exported Global Variables */

/* --------------------------------------------- Static Global Variables */

/* --------------------------------------------- Function Prototype */

/* --------------------------------------------- Functions */

/** PL callback */
void (esl_init_complete_cb)(UINT16 err, void* blob);

void esl_init_complete_cb(UINT16 err, void* blob)
{
    API_RESULT retval;
#ifdef BT_ESL_SUPPORT_VERSION_INFO
    BT_ESL_VERSION_NUMBER esl_version;
#endif /* BT_ESL_SUPPORT_VERSION_INFO */

    BT_ESL_IGNORE_UNUSED_PARAM(blob);

    /* Init */
    retval = BT_ESL_API_SUCCESS;

    if (BT_ESL_API_SUCCESS == err)
    {
        APPL_ESL_TRC("[APPL]: ESL PL initialized successfully\n");

#ifdef BT_ESL_SUPPORT_VERSION_INFO
        APPL_ESL_INF(
        "==========================================\n");
        /* Fetch and Print the Mesh Stack Version Number */
        BT_esl_get_version_number(&esl_version);

        APPL_ESL_INF(
        "  ESL Module Version %03d:%03d:%03d\n"
        "==========================================\n",
        esl_version.major, esl_version.minor, esl_version.subminor);
#endif /* BT_ESL_SUPPORT_VERSION_INFO */

        /* Initialize the tag instance */
#ifdef BT_ESL_SUPPORT_TAG_ROLE
        appl_esl_tag_init();
#endif /* BT_ESL_SUPPORT_TAG_ROLE */

#ifdef BT_ESL_SUPPORT_AP_ROLE
        /* Initialize the AP instance */
        retval = appl_esl_ap_init();
#endif /* BT_ESL_SUPPORT_AP_ROLE */

    }
    else
    {
        APPL_ESL_ERR("[APPL]: ESL PL initialization failed\n");
    }
}

void appl_init_pl(void)
{
    API_RESULT retval;

    retval = BT_esl_init_pl(esl_init_complete_cb);

    if (BT_ESL_API_SUCCESS != retval)
    {
        APPL_ESL_ERR("[APPL]: BT_esl_init_pl - retval 0x%04X\n", retval);
    }
}

void appl_init_esl(void)
{
    appl_init_pl();
}

int main(int argc, char ** argv)
{
    BT_ESL_IGNORE_UNUSED_PARAM(argc);
    BT_ESL_IGNORE_UNUSED_PARAM(argv);

    while(1)
    {
        BT_ESL_sleep(1);
    }

    return 1;
}
