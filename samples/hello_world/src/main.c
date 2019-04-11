/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <misc/printk.h>

int systick_exc_trigger_count;

/* Exception handler will increment counter. */
void z_clock_isr(void *arg)
{
	ARG_UNUSED(arg);

	systick_exc_trigger_count++;

	/* Stop Clock */
	SysTick->CTRL &= ~(SysTick_CTRL_ENABLE_Msk);
}

void main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);

	/* Set priority of SysTick to default. */
	NVIC_SetPriority(SysTick_IRQn, _IRQ_PRIO_OFFSET);

	/* Clear (possibly) pending SysTick exception */
	SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;

	/*--------------------------- TEST 1 --------------------------------
	 * - SysTick not running.
	 * - Pend the SysTick in Software and confirm the ISR runs once.
	 */

	/* Assert on PENDSTSET being 0 */
	__ASSERT((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) == 0,
		"PENDSETSET is not clear");

	/* Pend the SysTick exception */
	SCB->ICSR |= SCB_ICSR_PENDSTSET_Msk;

	/* Synchronization barriers, just in case. */
	__DSB();
	__ISB();

	/* The SysTick exception should have been triggered once.
	 * Assert on PENDSTSET being 0
	 * Assert that the ISR was executed
	 */
	__ASSERT((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) == 0,
		"PENDSETSET is not clear");
	__ASSERT(systick_exc_trigger_count == 1,
		"SysTick exception not triggered\n");

	/*--------------------------- TEST 2 --------------------------------
	 * - SysTick not running.
	 * - Disable interrupts.
	 * - Pend the SysTick in Software and confirm the PENDSVSET is set.
	 * - Enable interrupts and confirm that the ISR runs once and PENDSVSET
	 *   is cleared.
	 */

	/* Disable interrupts */
	irq_lock();

	/* Assert on PENDSTSET being 0 */
	__ASSERT((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) == 0,
		"PENDSETSET is not clear");

	/* Pend the SysTick exception */
	SCB->ICSR |= SCB_ICSR_PENDSTSET_Msk;

	/* Synchronization barriers, just in case. */
	__DSB();
	__ISB();

	/* Assert on PENDSTSET being 1 and the exception is not triggered */
	__ASSERT((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) == SCB_ICSR_PENDSTSET_Msk,
		"PENDSETSET is not set");
	__ASSERT(systick_exc_trigger_count == 1,
		"SysTick exception was triggered\n");

	/* Enabled interrupts. */
	irq_unlock(0);

	/* The SysTick exception should have been triggered once.
	 * Assert on PENDSTSET being 0
	 * Assert that the ISR was executed
	 */
	__ASSERT((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) == 0,
		"PENDSETSET is not clear");
	__ASSERT(systick_exc_trigger_count == 2,
		"SysTick exception not triggered\n");

	/*--------------------------- TEST 3 --------------------------------
	 * - Disable interrupts.
	 * - Configure the SysTick to trigger an event in 10ms.
	 * - Busy Wait with interrupts disabled.
	 * - Confirm that the exception is pending and has not been triggered.
	 * - Enable interrupts and confirm that the ISR runs once and PENDSVSET
	 *   is cleared.
	 */

	volatile u32_t countflag = 0;

	/* Disable interrupts */
	irq_lock();

	/* Configure SysTick to trigger an event in 100 ms (1/10 sec)
	 * CPU Clock is 64 MHz.
	 */
	SysTick->LOAD = (64000000 - 1) / 10;
	SysTick->VAL = 0;
	SysTick->CTRL |= (SysTick_CTRL_ENABLE_Msk |
			  SysTick_CTRL_TICKINT_Msk |
			  SysTick_CTRL_CLKSOURCE_Msk);

	/* Reading the CTRL is expected to clear the COUNTFLAG */

	/* Read immediately the COUNTFLAG and confirm that it is zero. */
	countflag = SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk;
	__ASSERT(countflag == 0, "COUNTFLAG is not clear");

	/* 100ms busy wait.
	 * Note: extra 2ms are added because nRF52840
	 * k_busy_wait() sleeps less than expected!
	 */
	k_busy_wait(100000 + 2000);

	/* Read the COUNTFLAG and confirm that it is not zero anymore. */
	countflag = SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk;
		__ASSERT(countflag != 0, "COUNTFLAG is not set");

	/* Assert on PENDSTSET being 1 and the exception is not triggered */
	__ASSERT((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) == SCB_ICSR_PENDSTSET_Msk,
		"PENDSETSET is not set");
	__ASSERT(systick_exc_trigger_count == 2,
		"SysTick exception was triggered\n");

	/* Disabling interrupts. */
	irq_unlock(0);

	/* The SysTick exception should have been triggered once.
	 * Assert on PENDSTSET being 0
	 * Assert that the ISR was executed
	 */
	__ASSERT((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) == 0,
		"PENDSETSET is not clear");
	__ASSERT(systick_exc_trigger_count == 3,
		"SysTick exception not triggered\n");

	/*--------------------------- TEST 4 --------------------------------
	 * - Disable interrupts.
	 * - Configure the SysTick to trigger an event in 10ms.
	 * - Busy Wait with interrupts disabled.
	 * - Confirm that the exception is pending and has not been triggered.
	 * - Re-configure the SysTick to trigger an event in 100ms.
	 * - Enable interrupts and confirm that the ISR runs once, IMMEDIATELY,
	 *   and PENDSVSET is cleared. BUT IT SHOULD NOT BE THE CASE.
	 */

	/* Disable interrupts */
	irq_lock();

	/* Configure SysTick to trigger an event in 10 ms (1/100 sec)
	 * CPU Clock is 64 MHz.
	 */
	SysTick->LOAD = (64000000 - 1) / 100;
	SysTick->VAL = 0;
	SysTick->CTRL |= (SysTick_CTRL_ENABLE_Msk |
			  SysTick_CTRL_TICKINT_Msk |
			  SysTick_CTRL_CLKSOURCE_Msk);

	/* Reading the CTRL and writing to VAL are expected to clear the COUNTFLAG */

	/* Read immediately the COUNTFLAG and confirm that it is zero. */
	countflag = SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk;
	__ASSERT(countflag == 0, "COUNTFLAG is not clear");

	/* 10ms busy wait.
	 * Note: extra 0.2ms are added because nRF52840
	 * k_busy_wait() sleeps less than expected!
	 */
	k_busy_wait(10000 + 200);

	/* Read the COUNTFLAG and confirm that it is not zero. */
	countflag = SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk;
		__ASSERT(countflag != 0, "COUNTFLAG is not set");

	/* Assert on PENDSTSET being 1 and the exception is not triggered */
	__ASSERT((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) == SCB_ICSR_PENDSTSET_Msk,
		"PENDSETSET is not set");
	__ASSERT(systick_exc_trigger_count == 3,
		"SysTick exception was triggered\n");

	/* Re-configure SysTick to trigger an event in 100 ms (1/10 sec)
	 * CPU Clock is 64 MHz.
	 */
	SysTick->LOAD = (64000000 - 1) / 10;
	SysTick->VAL = 0;
	SysTick->CTRL |= (SysTick_CTRL_ENABLE_Msk |
			  SysTick_CTRL_TICKINT_Msk |
			  SysTick_CTRL_CLKSOURCE_Msk);

	/* Disabling interrupts. */
	irq_unlock(0);

	/* Assert on PENDSTSET being 0 and the exception is triggered
	 * IMMEDIATELY (BUT THIS SHOULD NOT BE THE CASE)
	 */
	__ASSERT((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) == 0,
		"PENDSETSET is not set");

	__ASSERT(systick_exc_trigger_count == 4,
		"SysTick exception was not triggered\n");

	/*--------------------------- TEST 5 --------------------------------
	 * - Disable interrupts.
	 * - Configure the SysTick to trigger an event in 10ms.
	 * - Busy Wait with interrupts disabled.
	 * - Confirm that the exception is pending and has not been triggered.
	 * - Re-configure the SysTick to trigger an event in 100ms.
	 * - Clear the pending status of the exception.
	 * - Enable interrupts and confirm that the ISR does not run IMMEDIATELY.
	 * - Busy wait for some time and confirm that the ISR has finally run.
	 */

	/* Disable interrupts */
	irq_lock();

	/* Configure SysTick to trigger an event in 10 ms (1/100 sec)
	 * CPU Clock is 64 MHz.
	 */
	SysTick->LOAD = (64000000 - 1) / 100;
	SysTick->VAL = 0;
	SysTick->CTRL |= (SysTick_CTRL_ENABLE_Msk |
			  SysTick_CTRL_TICKINT_Msk |
			  SysTick_CTRL_CLKSOURCE_Msk);

	/* Reading the CTRL and writing to VAL are expected to clear the COUNTFLAG */

	/* Read immediately the COUNTFLAG and confirm that it is zero. */
	countflag = SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk;
	__ASSERT(countflag == 0, "COUNTFLAG is not clear");

	/* 10ms busy wait.
	 * Note: extra 0.2ms are added because nRF52840
	 * k_busy_wait() sleeps less than expected!
	 */
	k_busy_wait(10000 + 200);

	/* Read the COUNTFLAG and confirm that it is not zero. */
	countflag = SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk;
		__ASSERT(countflag != 0, "COUNTFLAG is not set");

	/* Assert on PENDSTSET being 1 and the exception is not triggered */
	__ASSERT((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) == SCB_ICSR_PENDSTSET_Msk,
		"PENDSETSET is not set");
	__ASSERT(systick_exc_trigger_count == 4,
		"SysTick exception was triggered\n");

	/* Re-configure SysTick to trigger an event in 100 ms (1/10 sec)
	 * CPU Clock is 64 MHz.
	 */
	SysTick->LOAD = (64000000 - 1) / 10;
	SysTick->VAL = 0;
	SysTick->CTRL |= (SysTick_CTRL_ENABLE_Msk |
			  SysTick_CTRL_TICKINT_Msk |
			  SysTick_CTRL_CLKSOURCE_Msk);

	/* Clear the pended SysTick exception */
	SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;

	/* Disabling interrupts. */
	irq_unlock(0);

	/* Assert on PENDSTSET being 0 and the exception is not triggered */
	__ASSERT((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) == 0,
		"PENDSETSET is not set");
	__ASSERT(systick_exc_trigger_count == 4,
		"SysTick exception was not triggered\n");

	/* Wait for some time */
	k_busy_wait(200000);
	__ASSERT(systick_exc_trigger_count == 5,
		"SysTick exception was not triggered\n");

}
