/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <misc/printk.h>

typedef void ( * null_fn_t ) ( void );

k_tid_t thisThread;

volatile int expected_reason = -1;

void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t *esf)
{
	printk("My System Fatal Error Handler\n");
	switch (reason) {
	case K_ERR_CPU_EXCEPTION:
		printk("CPU exception");
		break;
	case K_ERR_SPURIOUS_IRQ:
		printk("Unhandled interrupt");
		break;
	case K_ERR_STACK_CHK_FAIL:
		printk("Stack overflow");
		break;
	case K_ERR_KERNEL_OOPS:
		printk("Kernel oops");
		break;
	case K_ERR_KERNEL_PANIC:
		printk("Kernel panic");
		break;
	default:
		printk("Unknown error");
		break;
	}

  NVIC_SystemReset();
}


void main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);

	null_fn_t fn = NULL;

	printk("While sample started\r\n");
	k_sleep(1000);
	//volatile int counter = 10 / 0;
	//k_oops();
	fn();
}
