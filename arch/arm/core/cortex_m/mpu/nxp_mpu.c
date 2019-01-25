/*
 * Copyright (c) 2017 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <init.h>
#include <kernel.h>
#include <soc.h>
#include <arch/arm/cortex_m/mpu/arm_core_mpu_dev.h>
#include <arch/arm/cortex_m/mpu/arm_core_mpu.h>
#include <misc/__assert.h>
#include <linker/linker-defs.h>

#define LOG_LEVEL CONFIG_MPU_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_DECLARE(mpu);

/*
 * Global status variable holding the number of HW MPU region indices, which
 * have been reserved by the MPU driver to program the static (fixed) memory
 * regions. The variable is meant to be used at run-time to derive the number
 * of HW MPU region indices available for programming dynamic memory regions.
 */
static u8_t static_regions_num = 0;

/* Global MPU configuration at system initialization. */
static void _mpu_init(void)
{
	/* Enable clock for the Memory Protection Unit (MPU). */
	CLOCK_EnableClock(kCLOCK_Sysmpu0);
}

/**
 *  Get the number of supported MPU regions.
 */
static inline u8_t _get_num_regions(void)
{
	return FSL_FEATURE_SYSMPU_DESCRIPTOR_COUNT;
}

/* @brief Partition sanity check
 *
 * This internal function performs run-time sanity check for
 * MPU region start address and size.
 *
 * @param part Pointer to the data structure holding the partition
 *             information (must be valid).
 * */
static int _mpu_partition_is_sane(const struct k_mem_partition *part)
{
	/* Partition size must be a multiple of the minimum MPU region
	 * size. Start address of the partition must align with the
	 * minimum MPU region size.
	 */
	int partition_is_sane =
		(part->size != 0)
		&&
		((part->size % CONFIG_ARM_MPU_REGION_MIN_ALIGN_AND_SIZE) == 0)
		&&
		((part->start &
			(CONFIG_ARM_MPU_REGION_MIN_ALIGN_AND_SIZE - 1)) == 0);

	return partition_is_sane;
}

/* This internal function performs MPU region initialization.
 *
 * Note:
 *   The caller must provide a valid region index.
 */
static void _region_init(const u32_t index,
	const struct nxp_mpu_region *region_conf)
{
	u32_t region_base = region_conf->base;
	u32_t region_end = region_conf->end;
	u32_t region_attr = region_conf->attr.attr;

	if (index == 0) {
		/* The MPU does not allow writes from the core to affect the
		 * RGD0 start or end addresses nor the permissions associated
		 * with the debugger; it can only write the permission fields
		 * associated with the other masters. These protections
		 * guarantee that the debugger always has access to the entire
		 * address space.
		 */
		__ASSERT(region_base == SYSMPU->WORD[index][0],
			 "Region %d base address got 0x%08x expected 0x%08x",
			 index, region_base, (u32_t)SYSMPU->WORD[index][0]);

		__ASSERT(region_end == SYSMPU->WORD[index][1],
			 "Region %d end address got 0x%08x expected 0x%08x",
			 index, region_end, (u32_t)SYSMPU->WORD[index][1]);

		/* Changes to the RGD0_WORD2 alterable fields should be done
		 * via a write to RGDAAC0.
		 */
		SYSMPU->RGDAAC[index] = region_attr;

	} else {
		SYSMPU->WORD[index][0] = region_base;
		SYSMPU->WORD[index][1] = region_end;
		SYSMPU->WORD[index][2] = region_attr;
		SYSMPU->WORD[index][3] = SYSMPU_WORD_VLD_MASK;
	}

	LOG_DBG("[%d] 0x%08x 0x%08x 0x%08x 0x%08x", index,
		    (u32_t)SYSMPU->WORD[index][0],
		    (u32_t)SYSMPU->WORD[index][1],
		    (u32_t)SYSMPU->WORD[index][2],
		    (u32_t)SYSMPU->WORD[index][3]);

}

static void log_mpu_configuration(void)
{
#if 0
	int index;
	for (index = 0; index < _get_num_regions(); index++) {
		if (((u32_t)SYSMPU->WORD[index][3]) == 1)
			printk("[%d] 0x%08x  0x%08x  0x%08x\n", index,
				(u32_t)SYSMPU->WORD[index][0],
				(u32_t)SYSMPU->WORD[index][1],
				(u32_t)SYSMPU->WORD[index][2]);
	}
#endif
}

static int _region_allocate_and_init(const u8_t index,
	const struct nxp_mpu_region *region_conf)
{
	/* Attempt to allocate new region index. */
	if (index > (_get_num_regions() - 1)) {

		/* No available MPU region index. */
		__ASSERT(0, "Failed to allocate new MPU region %u\n", index);
		return -EINVAL;
	}

	LOG_DBG("Program MPU region at index 0x%x", index);

	/* Program region */
	_region_init(index, region_conf);

	return index;
}

/**
 * This internal function is utilized by the MPU driver to combine a given
 * MPU RAM attribute configuration and region size and return the correct
 * parameter set.
 */
static inline void _get_ram_region_attr_by_conf(nxp_mpu_region_attr_t *p_attr,
	const k_mem_partition_attr_t *attr, u32_t base, u32_t size)
{
	/* in NXP MPU the base address and size are not required
	 * to determine region attributes
	 */
	(void) base;
	(void) size;

	p_attr->attr = attr->ap_attr;
}

/* This internal function programs an MPU region
 * of a given configuration at a given MPU index.
 */
static int _mpu_configure_region(const u8_t index,
	const struct k_mem_partition *new_region)
{
	struct nxp_mpu_region region_conf;

	LOG_DBG("Configure MPU region at index 0x%x", index);

	/* Populate internal NXP MPU region configuration structure. */
	region_conf.base = new_region->start;
	region_conf.end = (new_region->start + new_region->size - 1);
	_get_ram_region_attr_by_conf(&region_conf.attr,
		&new_region->attr, new_region->start, new_region->size);

	/* Allocate and program region */
	return _region_allocate_and_init(index,
		(const struct nxp_mpu_region *)&region_conf);
}

/* This internal function programs a set of given MPU regions
 * over a background memory area, optionally performing a
 * sanity check of the memory regions to be programmed.
 */
static int _mpu_configure_regions(const struct k_mem_partition
	regions[], u8_t regions_num, u8_t start_reg_index,
	bool do_sanity_check)
{
	//printk("Configuring %u MPU regions\n", regions_num);
	int i;
	u8_t reg_index = start_reg_index;

	for (i = 0; i < regions_num; i++) {
		if (regions[i].size == 0) {
			continue;
		}
		/* Non-empty region. */

		if (do_sanity_check &&
				(!_mpu_partition_is_sane(&regions[i]))) {
			__ASSERT(0, "Partition %u: sanity check failed.", i);
			return -EINVAL;
		}
		//printk("Configuring region with base 0x%x size 0x%x attr 0x%x\n",
//			regions[i].start, regions[i].size, regions[i].attr.ap_attr);
#if defined(CONFIG_MPU_STACK_GUARD)
		if (regions[i].attr.ap_attr == MPU_REGION_SU_RX) {
			arm_core_mpu_disable();

			//printk("Guard region %u base: 0x%x, size: 0x%x attr: 0x%x\n",
		//		i,
		//		regions[i].start,
		//		regions[i].size,
		//		regions[i].attr.ap_attr);

			//printk("SRAM region index %u\n", mpu_config.sram_region);

			struct nxp_mpu_region adj_region;
			adj_region.base =
				mpu_config.mpu_regions[mpu_config.sram_region].base;
			adj_region.end = regions[i].start - 1;
			adj_region.attr.attr =
				mpu_config.mpu_regions[mpu_config.sram_region].attr.attr;

			//printk("SRAM region base: 0x%x end: 0x%x atrr: 0x%x\n",
		//		mpu_config.mpu_regions[mpu_config.sram_region].base,
		//		mpu_config.mpu_regions[mpu_config.sram_region].end,
		//		mpu_config.mpu_regions[mpu_config.sram_region].attr.attr);

			//printk("SRAM region modified base: 0x%x end: 0x%x atrr: 0x%x\n",
		//		adj_region.base, adj_region.end, adj_region.attr.attr);

			/* Adjust the SRAM background region's end address */
			_region_init(mpu_config.sram_region,
				(const struct nxp_mpu_region *)&adj_region);

			/* Additional region for SRAM, above the guard stack */
			struct nxp_mpu_region fill_region;

			fill_region.base = regions[i].start + regions[i].size;
			fill_region.end =
				mpu_config.mpu_regions[mpu_config.sram_region].end;
			fill_region.attr.attr =
				mpu_config.mpu_regions[mpu_config.sram_region].attr.attr;

			//printk("Fill region base: 0x%x end: 0x%x attr: 0x%x\n",
		//		fill_region.base,
		//		fill_region.end,
		//		fill_region.attr.attr);

			reg_index =
				_region_allocate_and_init(reg_index,
					(const struct nxp_mpu_region *)&fill_region);
			//printk("Configured fill region at index: %u\n", reg_index);

			reg_index++;

			log_mpu_configuration();

		}
#endif /* CONFIG_MPU_STACK_GUARD */

		reg_index = _mpu_configure_region(reg_index, &regions[i]);
		arm_core_mpu_enable();
		if (reg_index == -EINVAL) {
			return reg_index;
		}

		/* Increment number of programmed MPU indices. */
		reg_index++;
	}

	return reg_index;
}

/* This internal function programs the static MPU regions. */
static void _mpu_configure_static_mpu_regions(const struct k_mem_partition
	static_regions[], const u8_t regions_num,
	const u32_t background_area_base,
	const u32_t background_area_end)
{
	int mpu_reg_index = static_regions_num;

	//printk("Configuring static regions. Initial static regions: %u\n", static_regions_num);
	log_mpu_configuration();

	/* In NXP MPU architecture the static regions are
	 * programmed on top of SRAM region configuration.
	 */
	ARG_UNUSED(background_area_base);
	ARG_UNUSED(background_area_end);

	mpu_reg_index = _mpu_configure_regions(static_regions,
		regions_num, mpu_reg_index, true);

	static_regions_num = mpu_reg_index;
	//printk("Configured static regions. Total now: %u\n", mpu_reg_index);
}

/* This internal function programs the dynamic MPU regions */
static void _mpu_configure_dynamic_mpu_regions(const struct k_mem_partition
		dynamic_regions[], u8_t regions_num)
{
	/* Reset MPU regions inside which dynamic memory regions may
	 * be programmed.
	 */
	_region_init(mpu_config.sram_region,
		(const struct nxp_mpu_region *)&mpu_config.mpu_regions[mpu_config.sram_region]);


	u32_t mpu_reg_index = static_regions_num;

	//printk("Configuring %u dynamic regions. Current static regions: %u\n",
	//	regions_num, static_regions_num);
	log_mpu_configuration();

	/* In NXP MPU architecture the dynamic regions are
	 * programmed on top of existing SRAM region configuration.
	 */

	mpu_reg_index = _mpu_configure_regions(dynamic_regions,
		regions_num, mpu_reg_index, false);

	//printk("Configured dynamic regions. Total now: %u\n", mpu_reg_index);
	log_mpu_configuration();

	if (mpu_reg_index != -EINVAL) {

		/* Disable the non-programmed MPU regions. */
		for (int i = mpu_reg_index; i < _get_num_regions(); i++) {
			LOG_DBG("disable region 0x%x", i);
			//printk("Disabling unused region 0x%x\n", i);
			/* Disable region */
			SYSMPU->WORD[i][0] = 0;
			SYSMPU->WORD[i][1] = 0;
			SYSMPU->WORD[i][2] = 0;
			SYSMPU->WORD[i][3] = 0;
		}
	} else {
		printk("ERROR configuring dynamic regions\n");
	}

	log_mpu_configuration();
}

#if 0
static inline u8_t _get_num_usable_regions(void)
{
	u8_t max = _get_num_regions();
#ifdef CONFIG_MPU_STACK_GUARD
	/* Last region reserved for stack guard.
	 * See comments in nxp_mpu_setup_sram_region()
	 */
	return max - 1;
#else
	return max;
#endif
}
#endif

#if 0
/**
 * This internal function is utilized by the MPU driver to parse the intent
 * type (i.e. THREAD_STACK_REGION) and return the correct region index.
 */
static inline u32_t _get_region_index_by_type(u32_t type)
{
	u32_t region_index;

	__ASSERT(type < THREAD_MPU_REGION_LAST,
		 "unsupported region type");


	region_index = mpu_config.num_regions + type;

	__ASSERT(region_index < _get_num_usable_regions(),
		 "out of MPU regions, requested %u max is %u",
		 region_index, _get_num_usable_regions() - 1);

	return region_index;
}
#endif


#ifdef CONFIG_MPU_STACK_GUARD
#if 0
static void nxp_mpu_setup_sram_region(u32_t base, u32_t size)
{
	u32_t last_region = _get_num_regions() - 1;

	/*
	 * The NXP MPU manages the permissions of the overlapping regions
	 * doing the logical OR in between them, hence they can't be used
	 * for stack/stack guard protection. For this reason the last region of
	 * the MPU will be reserved.
	 *
	 * A consequence of this is that the SRAM is split in different
	 * regions. In example if THREAD_STACK_GUARD_REGION is selected:
	 * - SRAM before THREAD_STACK_GUARD_REGION: RW
	 * - SRAM THREAD_STACK_GUARD_REGION: RO
	 * - SRAM after THREAD_STACK_GUARD_REGION: RW
	 */

	/* Configure SRAM_0 region
	 *
	 * The mpu_config.sram_region contains the index of the region in
	 * the mpu_config.mpu_regions array but the region 0 on the NXP MPU
	 * is the background region, so on this MPU the regions are mapped
	 * starting from 1, hence the mpu_config.sram_region on the data
	 * structure is mapped on the mpu_config.sram_region + 1 region of
	 * the MPU.
	 */
	_region_init(mpu_config.sram_region,
		     mpu_config.mpu_regions[mpu_config.sram_region].base,
		     ENDADDR_ROUND(base),
		     mpu_config.mpu_regions[mpu_config.sram_region].attr);

	/* Configure SRAM_1 region */
	_region_init(last_region, base + size,
	   ENDADDR_ROUND(mpu_config.mpu_regions[mpu_config.sram_region].end),
			mpu_config.mpu_regions[mpu_config.sram_region].attr);

}
#endif
#endif /* CONFIG_MPU_STACK_GUARD */

/* ARM Core MPU Driver API Implementation for NXP MPU */

/**
 * @brief enable the MPU
 */
void arm_core_mpu_enable(void)
{
	/* Enable MPU */
	SYSMPU->CESR |= SYSMPU_CESR_VLD_MASK;
}

/**
 * @brief disable the MPU
 */
void arm_core_mpu_disable(void)
{
	/* Disable MPU */
	SYSMPU->CESR &= ~SYSMPU_CESR_VLD_MASK;
	/* Clear Interrupts */
	SYSMPU->CESR |=  SYSMPU_CESR_SPERR_MASK;
}

#if 0
/**
 * @brief configure the base address and size for an MPU region
 *
 * @param   type    MPU region type
 * @param   base    base address in RAM
 * @param   size    size of the region
 */
void arm_core_mpu_configure(u8_t type, u32_t base, u32_t size)
{
	LOG_DBG("Region info: 0x%x 0x%x", base, size);
	u32_t region_index = _get_region_index_by_type(type);
	u32_t region_attr = _get_region_attr_by_type(type);

	_region_init(region_index, base,
		     ENDADDR_ROUND(base + size),
		     region_attr);
#ifdef CONFIG_MPU_STACK_GUARD
	if (type == THREAD_STACK_GUARD_REGION) {
		nxp_mpu_setup_sram_region(base, size);
	}
#endif
}
#endif

#if defined(CONFIG_USERSPACE)
#if 0
void arm_core_mpu_configure_user_context(struct k_thread *thread)
{
	u32_t base = (u32_t)thread->stack_info.start;
	u32_t size = thread->stack_info.size;
	u32_t index = _get_region_index_by_type(THREAD_STACK_REGION);
	u32_t region_attr = _get_region_attr_by_type(THREAD_STACK_REGION);

	/* configure stack */
	_region_init(index, base, ENDADDR_ROUND(base + size), region_attr);
}

/**
 * @brief configure MPU regions for the memory partitions of the memory domain
 *
 * @param   mem_domain    memory domain that thread belongs to
 */
void arm_core_mpu_configure_mem_domain(struct k_mem_domain *mem_domain)
{
	u32_t region_index =
		_get_region_index_by_type(THREAD_DOMAIN_PARTITION_REGION);
	u32_t region_attr;
	u32_t num_partitions;
	struct k_mem_partition *pparts;

	if (mem_domain) {
		LOG_DBG("configure domain: %p", mem_domain);
		num_partitions = mem_domain->num_partitions;
		pparts = mem_domain->partitions;
	} else {
		LOG_DBG("disable domain partition regions");
		num_partitions = 0U;
		pparts = NULL;
	}

	/*
	 * Don't touch the last region, it is reserved for SRAM_1 region.
	 * See comments in arm_core_mpu_configure().
	 */
	for (; region_index < _get_num_usable_regions(); region_index++) {
		if (num_partitions && pparts->size) {
			LOG_DBG("set region 0x%x 0x%x 0x%x",
				    region_index, pparts->start, pparts->size);
			region_attr = pparts->attr;
			_region_init(region_index, pparts->start,
				     ENDADDR_ROUND(pparts->start+pparts->size),
				     region_attr);
			num_partitions--;
		} else {
			LOG_DBG("disable region 0x%x", region_index);
			/* Disable region */
			SYSMPU->WORD[region_index][0] = 0;
			SYSMPU->WORD[region_index][1] = 0;
			SYSMPU->WORD[region_index][2] = 0;
			SYSMPU->WORD[region_index][3] = 0;
		}
		pparts++;
	}
}

/**
 * @brief configure MPU region for a single memory partition
 *
 * @param   part_index  memory partition index
 * @param   part        memory partition info
 */
void arm_core_mpu_configure_mem_partition(u32_t part_index,
					  struct k_mem_partition *part)
{
	u32_t region_index =
		_get_region_index_by_type(THREAD_DOMAIN_PARTITION_REGION);
	u32_t region_attr;

	LOG_DBG("configure partition index: %u", part_index);

	if (part) {
		LOG_DBG("set region 0x%x 0x%x 0x%x",
			    region_index + part_index, part->start, part->size);
		region_attr = part->attr;
		_region_init(region_index + part_index, part->start,
			     ENDADDR_ROUND(part->start + part->size),
			     region_attr);
	} else {
		LOG_DBG("disable region 0x%x", region_index);
		/* Disable region */
		SYSMPU->WORD[region_index + part_index][0] = 0;
		SYSMPU->WORD[region_index + part_index][1] = 0;
		SYSMPU->WORD[region_index + part_index][2] = 0;
		SYSMPU->WORD[region_index + part_index][3] = 0;
	}
}

/**
 * @brief Reset MPU region for a single memory partition
 *
 * @param   part_index  memory partition index
 */
void arm_core_mpu_mem_partition_remove(u32_t part_index)
{
	u32_t region_index =
		_get_region_index_by_type(THREAD_DOMAIN_PARTITION_REGION);

	LOG_DBG("disable region 0x%x", region_index);
	/* Disable region */
	SYSMPU->WORD[region_index + part_index][0] = 0;
	SYSMPU->WORD[region_index + part_index][1] = 0;
	SYSMPU->WORD[region_index + part_index][2] = 0;
	SYSMPU->WORD[region_index + part_index][3] = 0;
}

#endif

static inline u32_t _mpu_region_get_base(u32_t r_index)
{
	return SYSMPU->WORD[r_index][0];
}

static inline u32_t _mpu_region_get_size(u32_t r_index)
{
	/* <END> + 1 - <BASE> */
	return (SYSMPU->WORD[r_index][1] + 1) - SYSMPU->WORD[r_index][0];
}

/**
 * This internal function checks if region is enabled or not.
 *
 * Note:
 *   The caller must provide a valid region number.
 */
static inline int _is_enabled_region(u32_t r_index)
{
	return SYSMPU->WORD[r_index][3] & SYSMPU_WORD_VLD_MASK;
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
	u32_t r_addr_end;

	r_addr_start = SYSMPU->WORD[r_index][0];
	r_addr_end = SYSMPU->WORD[r_index][1];

	if (start >= r_addr_start && (start + size - 1) <= r_addr_end) {
		return 1;
	}

	return 0;
}

/**
 * @brief configure an active memory partition
 */
void arm_core_mpu_mem_partition_configure(struct k_mem_partition *partition,
	k_mem_partition_attr_t *new_attr)
{
	printk("REMOVING PARTITION\n");
	/* Find the partition. ASSERT if not found. */
	u8_t i;
	u8_t reg_index = _get_num_regions();

	for (i = static_regions_num; i < _get_num_regions(); i++) {
		if (!_is_enabled_region(i)) {
			continue;
		}

		u32_t base = _mpu_region_get_base(i);

		if (base != partition->start) {
			continue;
		}

		u32_t size = _mpu_region_get_size(i);

		if (size != partition->size) {
			continue;
		}

		/* Region found */
		reg_index = i;
		break;
	}
	__ASSERT(reg_index != _get_num_regions(),
		"Memory domain partition not found\n");

	/* Modify the permissions */
	partition->attr = *new_attr;
	_mpu_configure_region(reg_index, partition);
}

/**
 * @brief Maximum number of memory domain partitions
 *
 * This internal macro returns the maximum number of memory partitions, which
 * may be defined in a memory domain, given the amount of available HW MPU
 * regions.
 *
 * For the NXP MPU architecture, where the domain partitions are defined
 * on top of the statically configured memory regions, the maximum number
 * of memory domain partitions is equal to the number of available MPU regions.
 *
 * @param mpu_regions_num the number of available HW MPU regions.
 */
#define _MPU_MAX_DOMAIN_PARTITIONS_GET(mpu_regions_num) (mpu_regions_num)

/**
 * @brief get the maximum number of free regions for memory domain partitions
 */
int arm_core_mpu_get_max_domain_partition_regions(void)
{
	int available_regions_num =	_get_num_regions() - static_regions_num;

	/* Additional regions required for the thread stack */
	available_regions_num -= 1;

#if defined(CONFIG_MPU_STACK_GUARD)
	/* Additional regions required for the current thread's privileged
	 * stack guard. Due to the OR-based decision policy, the MPU stack
	 * guard splits the (background) SRAM region. Therefore, 2 regions
	 * are required in total.
	 */
	available_regions_num -= 2;
#endif

	return _MPU_MAX_DOMAIN_PARTITIONS_GET(available_regions_num);
}

/**
 * This internal function checks if the region is user accessible or not
 *
 * Note:
 *   The caller must provide a valid region number.
 */
static inline int _is_user_accessible_region(u32_t r_index, int write)
{
	u32_t r_ap = SYSMPU->WORD[r_index][2];

	if (write) {
		return (r_ap & MPU_REGION_WRITE) == MPU_REGION_WRITE;
	}

	return (r_ap & MPU_REGION_READ) == MPU_REGION_READ;
}

/**
 * @brief validate the given buffer is user accessible or not
 */
int arm_core_mpu_buffer_validate(void *addr, size_t size, int write)
{
	u8_t r_index;

	/* Iterate through all MPU regions */
	for (r_index = 0U; r_index < _get_num_regions(); r_index++) {
		if (!_is_enabled_region(r_index) ||
				!_is_in_region(r_index, (u32_t)addr, size)) {
			continue;
		}

		/* For NXP MPU, priority is given to granting permission over
		 * denying access for overlapping region.
		 * So we can stop the iteration immediately once we find the
		 * matched region that grants permission.
		 */
		if (_is_user_accessible_region(r_index, write)) {
			return 0;
		}
	}

	return -EPERM;
}

#endif /* CONFIG_USERSPACE */

/**
 * @brief configure fixed (static) MPU regions.
 */
void arm_core_mpu_configure_static_mpu_regions(const struct k_mem_partition
	static_regions[], const u8_t regions_num,
	const u32_t background_area_start, const u32_t background_area_end)
{
	_mpu_configure_static_mpu_regions(static_regions, regions_num,
		background_area_start, background_area_end);
}

/**
 * @brief configure dynamic MPU regions.
 */
void arm_core_mpu_configure_dynamic_mpu_regions(const struct k_mem_partition
	dynamic_regions[], u8_t regions_num)
{
	_mpu_configure_dynamic_mpu_regions(dynamic_regions, regions_num);
}

/* NXP MPU Driver Initial Setup */

/*
 * @brief MPU default configuration
 *
 * This function provides the default configuration mechanism for the Memory
 * Protection Unit (MPU).
 */
static int nxp_mpu_init(struct device *arg)
{
	ARG_UNUSED(arg);

	u32_t r_index;

	if (mpu_config.num_regions > _get_num_regions()) {
		/* Attempt to configure more MPU regions than
		 * what is supported by hardware. As this operation
		 * may be executed during system (pre-kernel) initialization,
		 * we want to ensure we can detect an attempt to
		 * perform invalid configuration.
		 */
		__ASSERT(0,
			"Request to configure: %u regions (supported: %u)\n",
			mpu_config.num_regions,
			_get_num_regions()
		);
		return -1;
	}

	LOG_DBG("total region count: %d", _get_num_regions());

	arm_core_mpu_disable();

	/* Architecture-specific configuration */
	_mpu_init();

	/* Program fixed regions configured at SOC definition. */
	for (r_index = 0U; r_index < mpu_config.num_regions; r_index++) {
		_region_init(r_index, &mpu_config.mpu_regions[r_index]);
	}

	/* Update the number of programmed MPU regions. */
	static_regions_num = mpu_config.num_regions;


	arm_core_mpu_enable();

	/* Make sure that all the registers are set before proceeding */
	__DSB();
	__ISB();

	return 0;
}

#if defined(CONFIG_LOG)
/* To have logging the driver needs to be initialized later */
SYS_INIT(nxp_mpu_init, POST_KERNEL,
	 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#else
SYS_INIT(nxp_mpu_init, PRE_KERNEL_1,
	 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif
