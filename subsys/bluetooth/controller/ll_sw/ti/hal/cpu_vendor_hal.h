/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
//#include <DeviceFamily_constructPath(cmsis/core/cmsis_compiler.h)>
//static inline void cpu_sleep(void)
//{
//	__WFE();
//	/* __SEV(); */
//	__WFE();
//}

static inline void cpu_dmb(void)
{
	/* FIXME: Add necessary host machine required Data Memory Barrier
	 *        instruction along with the below defined compiler memory
	 *        clobber.
	 */
	__asm__ volatile ("" : : : "memory");
}
