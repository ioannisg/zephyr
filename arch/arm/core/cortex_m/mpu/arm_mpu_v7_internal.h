/*
 * Copyright (c) 2017 Linaro Limited.
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* This internal function performs MPU region initialization.
 *
 * Note:
 *   The caller must provide a valid region index.
 */
static void _region_init(u32_t index, struct arm_mpu_region *region_conf)
{
	/* Select the region you want to access */
	MPU->RNR = index;
	/* Configure the region */
	MPU->RBAR = (region_conf->base & MPU_RBAR_ADDR_Msk)
				| MPU_RBAR_VALID_Msk | index;
	MPU->RASR = region_conf->attr | MPU_RASR_ENABLE_Msk;
	SYS_LOG_DBG("[%d] 0x%08x 0x%08x",
		index, region_conf->base, region_conf->attr);
}

#if defined(CONFIG_USERSPACE) || defined(CONFIG_MPU_STACK_GUARD) || \
	defined(CONFIG_APPLICATION_MEMORY)

/**
 * Generate the value of the MPU Region Attribute and Size Register
 * (MPU_RASR) that corresponds to the supplied MPU region attributes.
 * This function is internal to the driver.
 */
static inline u32_t _get_region_attr(u32_t xn, u32_t ap, u32_t tex,
				     u32_t c, u32_t b, u32_t s,
				     u32_t srd, u32_t size)
{
	return (((xn << MPU_RASR_XN_Pos) & MPU_RASR_XN_Msk)
		| ((ap << MPU_RASR_AP_Pos) & MPU_RASR_AP_Msk)
		| ((tex << MPU_RASR_TEX_Pos) & MPU_RASR_TEX_Msk)
		| ((s << MPU_RASR_S_Pos) & MPU_RASR_S_Msk)
		| ((c << MPU_RASR_C_Pos) & MPU_RASR_C_Msk)
		| ((b << MPU_RASR_B_Pos) & MPU_RASR_B_Msk)
		| ((srd << MPU_RASR_SRD_Pos) & MPU_RASR_SRD_Msk)
		| (size));
}

/**
 * This internal function converts the region size to
 * the SIZE field value of MPU_RASR.
 */
static inline u32_t _size_to_mpu_rasr_size(u32_t size)
{
	/* The minimal supported region size is 32 bytes */
	if (size <= 32) {
		return REGION_32B;
	}

	/*
	 * A size value greater than 2^31 could not be handled by
	 * round_up_to_next_power_of_two() properly. We handle
	 * it separately here.
	 */
	if (size > (1 << 31)) {
		return REGION_4G;
	}

	size = 1 << (32 - __builtin_clz(size - 1));
	return (32 - __builtin_clz(size) - 2) << 1;
}

/**
 * This internal function is utilized by the MPU driver to parse the intent
 * type (i.e. THREAD_STACK_REGION) and return the correct parameter set.
 */
static inline u32_t _get_region_attr_by_type(u32_t type, u32_t size)
{
	int region_size = _size_to_mpu_rasr_size(size);

	switch (type) {
#ifdef CONFIG_USERSPACE
	case THREAD_STACK_REGION:
		return _get_region_attr(1, P_RW_U_RW, 0, 1, 0,
					1, 0, region_size);
#endif
#ifdef CONFIG_MPU_STACK_GUARD
	case THREAD_STACK_GUARD_REGION:
		return _get_region_attr(1, P_RO_U_NA, 0, 1, 0,
					1, 0, region_size);
#endif
#ifdef CONFIG_APPLICATION_MEMORY
	case THREAD_APP_DATA_REGION:
		return _get_region_attr(1, P_RW_U_RW, 0, 1, 0,
					1, 0, region_size);
#endif
	default:
		/* Size 0 region */
		return 0;
	}
}

/**
 * This internal function is utilized by the MPU driver to combine a given
 * MPU attribute configuration and region size and return the correct
 * parameter set.
 */
static inline u32_t _get_region_attr_by_conf(u32_t attr, u32_t size)
{
	return attr | _size_to_mpu_rasr_size(size);
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

	return MPU->RASR & MPU_RASR_ENABLE_Msk;
}

/**
 * This internal function checks if the given buffer is in the region.
 *
 * Note:
 *   The caller must provide a valid region number.
 */
static inline int _is_in_region(u32_t r_index, u32_t start, u32_t size)
{
	u32_t r_addr_start;
	u32_t r_size_lshift;
	u32_t r_addr_end;

	MPU->RNR = r_index;
	r_addr_start = MPU->RBAR & MPU_RBAR_ADDR_Msk;
	r_size_lshift = ((MPU->RASR & MPU_RASR_SIZE_Msk) >>
			MPU_RASR_SIZE_Pos) + 1;
	r_addr_end = r_addr_start + (1 << r_size_lshift) - 1;

	if (start >= r_addr_start && (start + size - 1) <= r_addr_end) {
		return 1;
	}

	return 0;
}

/**
 * This internal function returns the access permissions of an MPU region
 * specified by its region index.
 *
 * Note:
 *   The caller must provide a valid region number.
 */
static inline u32_t _get_region_ap(u32_t r_index)
{
	MPU->RNR = r_index;
	return MPU->RASR & MPU_RASR_AP_Msk >> MPU_RASR_AP_Pos;
}

/* Only a single bit is set for all user accessible permissions.
 * In ARMv7-M MPU this is bit AP[1].
 */
#define MPU_USER_READ_ACCESSIBLE_Msk (P_RW_U_RO & P_RW_U_RW & P_RO_U_RO & RO)

#endif /* USERSPACE || MPU_STACK_GUARD || APPLICATION_MEMORY */
