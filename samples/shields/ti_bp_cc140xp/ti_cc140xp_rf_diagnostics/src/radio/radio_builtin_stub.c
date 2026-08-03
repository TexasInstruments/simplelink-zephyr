/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 * SPDX-License-Identifier: Apache-2.0
 *
 * Stub for the builtin radio slot. This port has no builtin radio;
 * all function pointers are set to NULL so that the DUAL_RADIOS vtable
 * array can be populated without linker errors. The AT command layer
 * selects radio ID 2 (TRX) by default, so these NULL entries are never
 * called.
 */
#include <string.h>
#include "radio.h"
#include "radio_pvt.h"

void Radio_Builtin_registerFxns(Radio_Fxns *fxns)
{
    memset(fxns, 0, sizeof(Radio_Fxns));
}
