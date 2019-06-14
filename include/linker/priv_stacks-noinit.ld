/*
 * Copyright (c) 2017 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

     SECTION_DATA_PROLOGUE(priv_stacks_noinit,,)
        {
        MPU_ALIGN(_priv_stacks_ram_end - _priv_stacks_ram_start);
        _priv_stacks_ram_start = .;
        *(".priv_stacks.noinit")
        MPU_ALIGN(_priv_stacks_ram_end - _priv_stacks_ram_start);
        _priv_stacks_ram_end = .;
        } GROUP_DATA_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)
