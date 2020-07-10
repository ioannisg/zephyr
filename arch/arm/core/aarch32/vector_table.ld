/*
 * Copyright (c) 2019 - 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_CPU_CORTEX_M_HAS_VTOR)
/*
 * When setting TBLOFF in VTOR we must align the offset to the number of
 * exception entries in the vector table. Minimum alignment is 32 words,
 * sufficient for the 16 ARM Core exceptions and up to 16 HW interrupts.
 * For more than 16 HW interrupts, we adjust the alignment by rounding up
 * to the next power of two; this restriction guarantees a functional VTOR
 * setting in any Cortex-M implementation (might not be required in every
 * Cortex-M processor).
 *
 * In an MCU with VTOR the VTOR.TBLOFF is set to the start address of the
 * exc_vector_table, which must respect the above alignment requirements.
 */
. = ALIGN( 1 << LOG2CEIL(4 * 32) );
. = ALIGN( 1 << LOG2CEIL(4 * (16 + CONFIG_NUM_IRQS)) );
. = ALIGN( 640 );
#endif

_vector_start = .;
KEEP(*(.exc_vector_table))
KEEP(*(".exc_vector_table.*"))

KEEP(*(_IRQ_VECTOR_TABLE_SECTION_NAME))

KEEP(*(.vectors))

_vector_end = .;

KEEP(*(.openocd_dbg))
KEEP(*(".openocd_dbg.*"))
