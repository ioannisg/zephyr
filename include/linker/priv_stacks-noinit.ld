/*
 * Copyright (c) 2017 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Determine the size and alignment requirements for the MPU
 * guard region in the thread privilege stack area.
 */
#if defined(CONFIG_MPU_STACK_GUARD)
#if defined(CONFIG_FLOAT) && defined(CONFIG_FP_SHARING)
#define PRIV_STACK_GUARD_SIZE CONFIG_MPU_STACK_GUARD_MIN_SIZE_FLOAT
#else
#define PRIV_STACK_GUARD_SIZE CONFIG_ARM_MPU_REGION_MIN_ALIGN_AND_SIZE
#endif /*  CONFIG_FOAT && CONFIG_FP_SHARING */
#if defined(CONFIG_MPU_REQUIRES_POWER_OF_TWO_ALIGNMENT)
#define PRIV_STACK_GUARD_ALIGN PRIV_STACK_GUARD_SIZE
#else
#define PRIV_STACK_GUARD_ALIGN CONFIG_ARM_MPU_REGION_MIN_ALIGN_AND_SIZE
#endif /* CONFIG_ARM_MPU_REGION_MIN_ALIGN_AND_SIZE */
#else
#define PRIV_STACK_GUARD_SIZE 0
#define PRIV_STACK_GUARD_ALIGN 0
#endif /* CONFIG_MPU_STACK_GUARD */


     SECTION_DATA_PROLOGUE(priv_stacks_noinit,,)
        {
        MPU_ALIGN(_priv_stacks_ram_end - _priv_stacks_ram_start);
        . = ALIGN(PRIV_STACK_GUARD_ALIGN);
        _priv_stacks_ram_start = .;
        . += PRIV_STACK_GUARD_SIZE;
        *(".priv_stacks.noinit")
        MPU_ALIGN(_priv_stacks_ram_end - _priv_stacks_ram_start);
        _priv_stacks_ram_end = .;
        } GROUP_DATA_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)
