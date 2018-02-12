/*
 * Copyright (c) 2016 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief System/hardware module for Nordic Semiconductor nRF5CM33X family processor (CM-33 emulator)
 *
 * This module provides routines to initialize and support board-level hardware
 * for the Nordic Semiconductor nRF5CM33 family processor (CM-33 emulator).
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <soc.h>
#include <cortex_m/exc.h>

#ifdef CONFIG_RUNTIME_NMI
extern void _NmiInit(void);
#define NMI_INIT() _NmiInit()
#else
#define NMI_INIT()
#endif

#include "nrf.h"

#define __SYSTEM_CLOCK_128M (128000000UL)

uint32_t SystemCoreClock __used = __SYSTEM_CLOCK_128M;

static void clock_init(void)
{
	SystemCoreClock = __SYSTEM_CLOCK_128M;
}

static int nordicsemi_nrf52_init(struct device *arg)
{
	u32_t key;

	ARG_UNUSED(arg);

	key = irq_lock();

	_ClearFaults();

	/* Setup master clock */
	clock_init();

	/* Install default handler that simply resets the CPU
	* if configured in the kernel, NOP otherwise
	*/
	NMI_INIT();

	irq_unlock(key);

	return 0;
}

SYS_INIT(nordicsemi_nrf52_init, PRE_KERNEL_1, 0);
