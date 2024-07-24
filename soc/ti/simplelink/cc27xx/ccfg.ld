/*
 * Copyright (c) 2025 Texas Instruments Incorporated
 * Copyright (c) 2024 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

GROUP_START(FLASH_CCFG)

CRC_CCFG_BOOT_CFG_begin     = 0x4e020000;
CRC_CCFG_BOOT_CFG_end       = 0x4e02000b;
CRC_CCFG_begin              = 0x4e020010;
CRC_CCFG_end                = 0x4e02074b;
CRC_CCFG_USER_RECORD_begin  = 0x4e020750;
CRC_CCFG_USER_RECORD_end    = 0x4e0207cb;
CRC_CCFG_DEBUG_begin        = 0x4e0207d0;
CRC_CCFG_DEBUG_end          = 0x4e0207fb;

SECTION_PROLOGUE(.ti_ccfg,,)
{
    KEEP(*(_TI_CCFG_SECTION_NAME))
} GROUP_LINK_IN(LINKER_DT_NODE_REGION_NAME(DT_NODELABEL(ti_ccfg_partition)))

GROUP_END(FLASH_CCFG)
