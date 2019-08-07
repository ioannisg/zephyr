/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief System/hardware module for Nordic Semiconductor nRF53 family processor
 *
 * This module provides routines to initialize and support board-level hardware
 * for the Nordic Semiconductor nRF53 family processor.
 */

#include <kernel.h>
#include <init.h>
#include <cortex_m/exc.h>
#include <nrfx.h>
#include <soc/nrfx_coredep.h>
#include <logging/log.h>

#ifdef CONFIG_RUNTIME_NMI
extern void z_NmiInit(void);
#define NMI_INIT() z_NmiInit()
#else
#define NMI_INIT()
#endif

#if defined(CONFIG_SOC_NRF5340_CPU0)
#include <system_nrf5340_application.h>
#elif defined(CONFIG_SOC_NRF5340_CPU1)
#include <system_nrf5340_network.h>
#else
#error "Unknown nRF53 SoC."
#endif

#define LOG_LEVEL CONFIG_SOC_LOG_LEVEL
LOG_MODULE_REGISTER(soc);

static int nordicsemi_nrf53_init(struct device *arg)
{
	u32_t key;

	ARG_UNUSED(arg);

	key = irq_lock();

#ifdef CONFIG_NRF_ENABLE_CACHE
#ifdef CONFIG_SOC_NRF5340_CPU0
	/* Enable the instruction & data cache */
	NRF_CACHE_S->ENABLE = CACHE_ENABLE_ENABLE_Msk;
#endif /* CONFIG_SOC_NRF5340_CPU0 */
#ifdef CONFIG_SOC_NRF5340_CPU1
	NRF_NVMC_NS->ICACHECNF |= NVMC_ICACHECNF_CACHEEN_Enabled;
#endif /* CONFIG_SOC_NRF5340_CPU1 */
#endif

	/* Install default handler that simply resets the CPU
	* if configured in the kernel, NOP otherwise
	*/
	NMI_INIT();

	irq_unlock(key);

	return 0;
}

void z_arch_busy_wait(u32_t time_us)
{
	nrfx_coredep_delay_us(time_us);
}

void z_platform_init(void)
{
	SystemInit();
}


SYS_INIT(nordicsemi_nrf53_init, PRE_KERNEL_1, 0);
