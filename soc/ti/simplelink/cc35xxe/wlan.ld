/*
 * Copyright (c) 2025 Conclusive Engineering Sp. z o. o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/linker/linker-tool.h>

/* The last 256 bytes of DMEM are designated as shared memory between
 * application and RF cores.
 */
SECTION_PROLOGUE(.connectivity_shared_status_section, 0x2803ff00 (NOLOAD),)
{
	gFwStatusInfo = .;
	. = ALIGN(256);
} GROUP_NOLOAD_LINK_IN(RAMABLE_REGION, RAMABLE_REGION)
