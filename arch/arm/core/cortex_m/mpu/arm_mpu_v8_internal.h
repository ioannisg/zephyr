/*
 * Copyright (c) 2017 Linaro Limited.
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_ARCH_ARM_CORE_CORTEX_M_MPU_ARM_MPU_V8_INTERNAL_H_
#define ZEPHYR_ARCH_ARM_CORE_CORTEX_M_MPU_ARM_MPU_V8_INTERNAL_H_

#include <cmse.h>
#define LOG_LEVEL CONFIG_MPU_LOG_LEVEL
#include <logging/log.h>

#include <arm_mpu_common_internal.h>

/* Global MPU configuration at system initialization. */
static void _mpu_init(void)
{
	/* Configure the cache-ability attributes for all the
	 * different types of memory regions.
	 */

	/* Flash region(s): Attribute-0
	 * SRAM region(s): Attribute-1
	 * SRAM no cache-able regions(s): Attribute-2
	 */
	MPU->MAIR0 =
		((MPU_MAIR_ATTR_FLASH << MPU_MAIR0_Attr0_Pos) &
			MPU_MAIR0_Attr0_Msk)
		|
		((MPU_MAIR_ATTR_SRAM << MPU_MAIR0_Attr1_Pos) &
			MPU_MAIR0_Attr1_Msk)
		|
		((MPU_MAIR_ATTR_SRAM_NOCACHE << MPU_MAIR0_Attr2_Pos) &
			MPU_MAIR0_Attr2_Msk);
}

/* This internal function performs MPU region initialization.
 *
 * Note:
 *   The caller must provide a valid region index.
 */
static void _region_init(u32_t index, const struct arm_mpu_region *region_conf)
{
	ARM_MPU_SetRegion(
		/* RNR */
		index,
		/* RBAR */
		(region_conf->base & MPU_RBAR_BASE_Msk)
		| (region_conf->attr.rbar &
			(MPU_RBAR_XN_Msk | MPU_RBAR_AP_Msk | MPU_RBAR_SH_Msk)),
		/* RLAR */
		(region_conf->attr.r_limit & MPU_RLAR_LIMIT_Msk)
		| ((region_conf->attr.mair_idx << MPU_RLAR_AttrIndx_Pos)
			& MPU_RLAR_AttrIndx_Msk)
		| MPU_RLAR_EN_Msk
	);

	LOG_DBG("[%d] 0x%08x 0x%08x 0x%08x 0x%08x",
			index, region_conf->base, region_conf->attr.rbar,
			region_conf->attr.mair_idx, region_conf->attr.r_limit);
}

/**
 * This internal function is utilized by the MPU driver to combine a given
 * MPU RAM attribute configuration and region size and fill-in a structure with
 * the correct parameter set.
 */
static inline void _get_ram_region_attr_by_conf(arm_mpu_region_attr_t *p_attr,
	u32_t ap_attr, u32_t base, u32_t size)
{
	p_attr->rbar = ap_attr  & (MPU_RBAR_XN_Msk | MPU_RBAR_AP_Msk);
	p_attr->mair_idx = MPU_MAIR_INDEX_SRAM;
	p_attr->r_limit = REGION_LIMIT_ADDR(base, size);
}

/* This internal function programs an MPU region
 * of a given configuration at a given MPU index.
 */
static inline void _mpu_configure_region(u8_t index,
	struct k_mem_partition new_region)
{
	struct arm_mpu_region region_conf;

	/* Populate internal ARM MPU region configuration structure. */
	region_conf.base = new_region.start;
	_get_ram_region_attr_by_conf(&region_conf.attr,
			new_region.attr, new_region.start, new_region.size);
	/* Program region */
	_region_init(index, &region_conf);
}

static void _mpu_configure_static_mpu_regions(const struct k_mem_partition
	static_regions[], u8_t regions_num,
	u32_t background_area_base,
	u32_t background_area_end)
{
	/* In ARMv8-M architecture the static regions are programmed on SRAM,
	 * forming a full partition of the background area, specified by the
	 * given boundaries.
	 */
	int i;

	/* Set the previous region end to the beginning of
	 * the background area.
	 */
	u32_t _prev_end = background_area_base;

	for (i = 0; i < regions_num; i++) {
		if (static_regions[i].size == 0) {
			continue;
		}
		/* Non-empty static region. */

		if (static_regions[i].base > _prev_end) {
			/* Configure the area before the start of
			 * the first region.
			 */

			/* Attempt to allocate new region index. */
			if (static_regions_num > (_get_num_regions() - 1)) {
				/* No available MPU region index. */
				__ASSERT(0,
					"Failed to allocate MPU region %u\n",
				static_regions_num);
				return;
			}

			LOG_DBG("Configure bkgrnd static region at index 0x%x",
				static_regions_num);

			struct k_mem_partition part = {_prev_end, static_regions[i].base};

			_mpu_configure_region(static_regions_num,
				static_regions[i],
				K_MEM_PARTITION_P_RW_U_NA);

			/* Increment number of programmed MPU indices. */
			static_regions_num++;
		}
		/* Attempt to allocate new region index. */
		if (static_regions_num > (_get_num_regions() - 1)) {
			/* No available MPU region index. */
			__ASSERT(0, "Failed to allocate new MPU region %u\n",
			static_regions_num);
			return;
		}

		LOG_DBG("Configure new static region at index 0x%x",
			static_regions_num);

		_mpu_configure_region(static_regions_num, static_regions[i]);

		/* Increment number of programmed MPU indices. */
		static_regions_num++;

		_prev_end = static_region[i].start + static_region[i].size;
	}

	/* If there is un-covered area after the end of the last
	 * region and the end of the background area, we will
	 * require one more region.
	 */
	if (_prev_end < background_area_end) {
		/* Attempt to allocate new region index. */
		if (static_regions_num > (_get_num_regions() - 1)) {
			/* No available MPU region index. */
			__ASSERT(0,
				"Failed to allocate MPU region %u\n",
			static_regions_num);
			return;
		}

		LOG_DBG("Configure bkgrnd static region at index 0x%x",
			static_regions_num);

		struct k_mem_partition part = {_prev_end,
			background_area_end - _prev_end,
			K_MEM_PARTITION_P_RW_U_NA};

		_mpu_configure_region(static_regions_num,
			static_regions[i]);

		/* Increment number of programmed MPU indices. */
		static_regions_num++;

	}
}




#if defined(CONFIG_USERSPACE) || defined(CONFIG_MPU_STACK_GUARD) || \
	defined(CONFIG_APPLICATION_MEMORY)

/**
 * This internal function allocates default RAM cache-ability, share-ability,
 * and execution allowance attributes along with the requested access
 * permissions and size.
 */
static inline void _get_mpu_ram_region_attr(arm_mpu_region_attr_t *p_attr,
	u32_t ap, u32_t base, u32_t size)
{
	p_attr->rbar = ((1UL << MPU_RBAR_XN_Pos) & MPU_RBAR_XN_Msk)
		| ((ap << MPU_RBAR_AP_Pos) & MPU_RBAR_AP_Msk);
	p_attr->mair_idx = MPU_MAIR_INDEX_SRAM;
	p_attr->r_limit = REGION_LIMIT_ADDR(base, size);
}

/**
 * This internal function is utilized by the MPU driver to combine a given
 * MPU RAM attribute configuration and region size and fill-in a structure with
 * the correct parameter set.
 */
static inline void _get_ram_region_attr_by_conf(arm_mpu_region_attr_t *p_attr,
	k_mem_partition_attr_t attr, u32_t base, u32_t size)
{
	p_attr->rbar = attr.rbar & (MPU_RBAR_XN_Msk | MPU_RBAR_AP_Msk);
	p_attr->mair_idx = attr.mair_idx;
	p_attr->r_limit = REGION_LIMIT_ADDR(base, size);
}

/**
 * This internal function checks if region is enabled or not.
 *
 * Note:
 *   The caller must provide a valid region number.
 */
static inline int _is_enabled_region(u32_t r_index)
{
	MPU->RNR = r_index;

	return MPU->RLAR & MPU_RLAR_EN_Msk;
}

/**
 * This internal function checks if the given buffer is in the region.
 *
 * Note:
 *   The caller must provide a valid region number.
 */
static inline int _is_in_region(u32_t r_index, u32_t start, u32_t size)
{
	u32_t region_start_addr = arm_cmse_mpu_region_get(start);
	u32_t region_end_addr = arm_cmse_mpu_region_get(start + size - 1);

	/* MPU regions are contiguous so return true if both start and end address
	 * are in the same region and this region is indexed by r_index.
	 */
	if ((region_start_addr == r_index) && (region_end_addr == r_index)) {
		return 1;
	}

	return 0;
}

/**
 * This internal function validates whether a given memory buffer
 * is user accessible or not.
 */
static inline int _mpu_buffer_validate(void *addr, size_t size, int write)
{
	u32_t _addr = (u32_t)addr;
	u32_t _size = (u32_t)size;

	if (write) {
		if (arm_cmse_addr_range_readwrite_ok(_addr, _size, 1)) {
			return 0;
		}
	} else {
		if (arm_cmse_addr_range_read_ok(_addr, _size, 1)) {
			return 0;
		}
	}

	return -EPERM;
}
#endif /* USERSPACE || MPU_STACK_GUARD || APPLICATION_MEMORY */

#endif	/* ZEPHYR_ARCH_ARM_CORE_CORTEX_M_MPU_ARM_MPU_V8_INTERNAL_H_ */
