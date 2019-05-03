/*
 * Copyright (c) 2016 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief System/hardware module for Nuvoton M2351 family processor
 *
 * This module provides routines to initialize and support board-level hardware
 * for the Nuvoton M2351 family processor.
 */

#include <kernel.h>
#include <init.h>
#include <soc.h>

#ifdef CONFIG_RUNTIME_NMI
extern void z_NmiInit(void);
#define NMI_INIT() z_NmiInit()
#else
#define NMI_INIT()
#endif

/* Overrides the weak ARM implementation:
   Set general purpose retention register and reboot */
void sys_arch_reboot(int type)
{
	NVIC_SystemReset();
}

static int nuvoton_m2351_init(struct device *arg)
{

	return 0;
}

SYS_INIT(nuvoton_m2351_init, PRE_KERNEL_1, 0);
