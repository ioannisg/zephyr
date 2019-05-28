/*
 * Copyright (c) 2019 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Common FPU routines for ARM Cortex-M
 *
 * This module provides the FPU exception handling for ARM Cortex-M.
 */

#include <kernel.h>
#include <kernel_structs.h>
#include <logging/log_ctrl.h>

#if defined(CONFIG_FLOAT)

/* FPSCR exception bit fields */
#define FPSCR_IOC_Pos                    0U
#define FPSCR_IOC_Msk                   (1UL << FPSCR_IOC_Pos)
#define FPSCR_DZC_Pos                    1U
#define FPSCR_DZC_Msk                   (1UL << FPSCR_DZC_Pos)
#define FPSCR_OFC_Pos                    2U
#define FPSCR_OFC_Msk                   (1UL << FPSCR_OFC_Pos)
#define FPSCR_UFC_Pos                    3U
#define FPSCR_UFC_Msk                   (1UL << FPSCR_UFC_Pos)
#define FPSCR_IXC_Pos                    4U
#define FPSCR_IXC_Msk                   (1UL << FPSCR_IXC_Pos)
#define FPSCR_IDC_Pos                    7U
#define FPSCR_IDC_Msk                   (1UL << FPSCR_IDC_Pos)

#ifdef CONFIG_PRINTK
#include <misc/printk.h>
#define PRINT(...) printk(__VA_ARGS__)
#else
#define PRINT(...)
#endif

#if (CONFIG_FAULT_DUMP > 0)
#define PR_FAULT_INFO(...) PRINT(__VA_ARGS__)
#else
#define PR_FAULT_INFO(...)
#endif

u32_t z_fpu_exception_handler(void)
{
#ifdef CONFIG_THREAD_NAME
	const char *thread_name = k_thread_name_get(k_current_get());
#endif

	u32_t fpscr = __get_FPSCR();

	LOG_PANIC();

	/* Dump FPU error information. */
	PR_FAULT_INFO("***** FPU Exception *****\n");

	if (fpscr & FPSCR_IOC_Msk) {
		PR_FAULT_INFO("  Invalid Operation\n");
	}
	if (fpscr & FPSCR_DZC_Msk) {
		PR_FAULT_INFO("  Division by Zero\n");
	}
	if (fpscr & FPSCR_OFC_Msk) {
		PR_FAULT_INFO("  Overflow\n");
	}
	if (fpscr & FPSCR_UFC_Msk) {
		PR_FAULT_INFO("  Underflow\n");
	}
	if (fpscr & FPSCR_IXC_Msk) {
		PR_FAULT_INFO("  Inexact operation\n");
	}
	if (fpscr & FPSCR_IDC_Msk) {
		PR_FAULT_INFO("  Input Denormal\n");
	}

	printk("Current thread ID = %p"
#ifdef CONFIG_THREAD_NAME
	       " (%s)"
#endif
	       "\n"
	       k_current_get()
#ifdef CONFIG_THREAD_NAME
	       ,
		   thread_name ? thread_name : "unknown"
#endif
		   );

	/* FPSCR exception flags are sticky and can be
	 * cleared by writing 0 to these bits FPSCR [7:0].
	 */
	__set_FPSCR(__get_FPSCR() & (~(0xf)));

	/* Call the implemented policty to respond to the error. */
	z_SysFatalErrorHandler(re_NANO_ERR_HW_EXCEPTION, NULL);
}
#endif /* CONFIG_FLOAT */
