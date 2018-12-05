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
#include <linker/linker-defs.h>

#define LOG_LEVEL CONFIG_MPU_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(mpu);

/*
 * Maximum number of dynamic memory partitions that may be supplied to the MPU
 * driver for programming during run-time. Note that the actual number of the
 * available MPU regions for dynamic programming depends on the number of the
 * static MPU regions currently being programmed, and the total number of HW-
 * available MPU regions. This macro is only used internally in function
 * _arch_configurre_dynamic_mpu_regions(), to reserve sufficient area for the
 * array of dynamic regions passed to the underlying driver.
 */
#if defined(CONFIG_USERSPACE)
#if defined(CONFIG_MPU_STACK_GUARD)
#define _MAX_DYNAMIC_MPU_REGIONS_NUM \
	(CONFIG_MAX_DOMAIN_PARTITIONS + 2)
#else
#define _MAX_DYNAMIC_MPU_REGIONS_NUM \
	(CONFIG_MAX_DOMAIN_PARTITIONS + 1)
#endif /* CONFIG_MPU_STACK_GUARD */
#else /* CONFIG_USERSPACE */
#if defined(CONFIG_MPU_STACK_GUARD)
#define _MAX_DYNAMIC_MPU_REGIONS_NUM 1
#else
#define _MAX_DYNAMIC_MPU_REGIONS_NUM 0
#endif /* CONFIG_MPU_STACK_GUARD */
#endif /* CONFIG_USERSPACE */

/* Convenience macros to denote the start address and the size of the system
 * memory area, where dynamic memory regions may be programmed at run-time.
 */
#if defined(CONFIG_APP_SHARED_MEM)
#define _MPU_DYNAMIC_REGIONS_AREA_START ((u32_t)&_app_smem_start)
#else
#define _MPU_DYNAMIC_REGIONS_AREA_START ((u32_t)&__kernel_ram_start)
#endif /* CONFIG_APP_SHARED_MEM */
#define _MPU_DYNAMIC_REGIONS_AREA_SIZE ((u32_t)&__kernel_ram_end - \
		_MPU_DYNAMIC_REGIONS_AREA_START)

/**
 * @brief Use the HW-specific MPU driver to program
 *        the static MPU regions.
 *
 * Program the static MPU regions using the HW-specific MPU driver. The
 * function is meant to be invoked only once upon system initialization.
 *
 * If the function attempts to configure a number of regions beyond the
 * MPU HW limitations, the system behavior will be undefined.
 *
 * For some MPU architectures, such as the unmodified ARMv8-M MPU,
 * the function must execute with MPU enabled.
 */
void _arch_configure_static_mpu_regions(void)
{
	/* Define a constant array of k_mem_partition objects
	 * to hold the configuration of the respective static
	 * MPU regions.
	 */
	const struct k_mem_partition static_regions[] = {
#if defined(CONFIG_APPLICATION_MEMORY)
		{
		.start = (u32_t)&__app_ram_start,
		.size = (u32_t)&__app_ram_end - (u32_t)&__app_ram_start,
		.attr = K_MEM_PARTITION_P_RW_U_RW,
		},
#endif /* CONFIG_APPLICATION_MEMORY */
#if defined(CONFIG_COVERAGE_GCOV) && defined(CONFIG_USERSPACE)
		{
		.start = (u32_t)&__gcov_bss_start,
		.size = (u32_t)&__gcov_bss_end - (u32_t)&__gcov_bss_start,
		.attr = K_MEM_PARTITION_P_RW_U_RW,
		},
#endif /* CONFIG_COVERAGE_GCOV && CONFIG_USERSPACE */
#if defined(CONFIG_NOCACHE_MEMORY)
		{
		.start = (u32_t)&_nocache_ram_start,
		.size = (u32_t)&_nocache_ram_end - (u32_t)&_nocache_ram_start,
		.attr = K_MEM_PARTITION_P_RW_U_RW_NOCACHE,
		}
#endif /* CONFIG_NOCACHE_MEMORY */
	};

	/* Configure the static MPU regions within firmware SRAM boundaries.
	 * Start address of the image is given by _image_ram_start. The end
	 * of the firmware SRAM area is marked by __kernel_ram_end, taking
	 * into account the unused SRAM area, as well.
	 */
	arm_core_mpu_configure_static_mpu_regions(static_regions,
		ARRAY_SIZE(static_regions),
		(u32_t)&_image_ram_start,
		(u32_t)&__kernel_ram_end);

#if defined(CONFIG_MPU_REQUIRES_NON_OVERLAPPING_REGIONS)
	/* Define a constant array of k_mem_partition objects that holds the
	 * boundaries of the areas, inside which dynamic region programming
	 * is allowed. The information is passed to the underlying driver at
	 * initialization.
	 */
	const struct k_mem_partition dyn_region_areas[] = {
#if defined(CONFIG_APPLICATION_MEMORY)
	/* Dynamic areas are also allowed in Application Memory. */
		{
		.start = (u32_t)&__app_ram_start,
		.size = (u32_t)&__app_ram_end - (u32_t)&__app_ram_start,
		},
#endif /* CONFIG_APPLICATION_MEMORY */
		{
		.start = _MPU_DYNAMIC_REGIONS_AREA_START,
		.size =  _MPU_DYNAMIC_REGIONS_AREA_SIZE,
		}
	};
	arm_core_mpu_mark_areas_for_dynamic_regions(dyn_region_areas,
		ARRAY_SIZE(dyn_region_areas));
#endif /* CONFIG_MPU_REQUIRES_NON_OVERLAPPING_REGIONS */
}

/**
 * @brief Use the HW-specific MPU driver to program
 *        the dynamic MPU regions.
 *
 * Program the dynamic MPU regions using the HW-specific MPU
 * driver. This function is meant to be invoked every time the
 * memory map is to be re-programmed, e.g during thread context
 * switch, entering user mode, reconfiguring memory domain, etc.
 *
 * For some MPU architectures, such as the unmodified ARMv8-M MPU,
 * the function must execute with MPU enabled.
 */
void _arch_configure_dynamic_mpu_regions(struct k_thread *thread)
{
	/* Define an array of k_mem_partition objects to hold the configuration
	 * of the respective dynamic MPU regions to be programmed for
	 * the given thread. The array of partitions (along with its
	 * actual size) will be supplied to the underlying MPU driver.
	 */
	struct k_mem_partition dynamic_regions[_MAX_DYNAMIC_MPU_REGIONS_NUM];

	u8_t region_num = 0;

#if defined(CONFIG_USERSPACE)
	/* Memory domain */
	LOG_DBG("configure thread %p's domain", thread);
	struct k_mem_domain *mem_domain = thread->mem_domain_info.mem_domain;
	if (mem_domain) {
		LOG_DBG("configure domain: %p", mem_domain);
		u32_t num_partitions = mem_domain->num_partitions;
		struct k_mem_partition partition;
		int i;

		LOG_DBG("configure domain: %p", mem_domain);

		for (i = 0; i < CONFIG_MAX_DOMAIN_PARTITIONS; i++) {
			partition = mem_domain->partitions[i];
			if (partition.size == 0) {
				/* Zero size indicates a non-existing
				 * memory partition.
				 */
				continue;
			}
			LOG_DBG("set region 0x%x 0x%x",
				partition.start, partition.size);
			dynamic_regions[region_num].start = partition.start;
			dynamic_regions[region_num].size = partition.size;
			dynamic_regions[region_num].attr = partition.attr;

			region_num++;
			num_partitions--;
			if (num_partitions == 0) {
				break;
			}
		}
	}
	/* Thread user stack */
	LOG_DBG("configure user thread %p's context", thread);
	if (thread->arch.priv_stack_start) {
		u32_t base = (u32_t)thread->stack_obj;
		u32_t size = thread->stack_info.size;
#if !defined(CONFIG_MPU_REQUIRES_POWER_OF_TWO_ALIGNMENT)
		/* In user-mode the thread stack will include the (optional)
		 * guard area. For MPUs with arbitrary base address and limit
		 * it is essential to include this size increase, to avoid
		 * MPU faults.
		 */
		size += thread->stack_info.start - base;
#endif
		dynamic_regions[region_num].start = base;
		dynamic_regions[region_num].size = size;
		dynamic_regions[region_num].attr = K_MEM_PARTITION_P_RW_U_RW;

		region_num++;
	}
#endif /* CONFIG_USERSPACE */

#if defined(CONFIG_MPU_STACK_GUARD)
	/* Privileged stack guard */
#if defined(CONFIG_USERSPACE)
	u32_t guard_start = thread->arch.priv_stack_start ?
	    (u32_t)thread->arch.priv_stack_start :
	    (u32_t)thread->stack_obj;
#else
	u32_t guard_start = thread->stack_info.start;
#endif
	dynamic_regions[region_num].start = guard_start;
	dynamic_regions[region_num].size = MPU_GUARD_ALIGN_AND_SIZE;
	dynamic_regions[region_num].attr = K_MEM_PARTITION_P_RO_U_NA;

	region_num++;
#endif /* CONFIG_MPU_STACK_GUARD */

	/* Configure the dynamic MPU regions */
	arm_core_mpu_configure_dynamic_mpu_regions(dynamic_regions,
		region_num);
}

#if defined(CONFIG_USERSPACE)

/**
 * @brief Get the maximum number of partitions for a memory domain
 *        that is supported by the MPU hardware.
 */
int _arch_mem_domain_max_partitions_get(void)
{
	return arm_core_mpu_get_max_domain_partition_regions();
}

/**
 * @brief Configure the memory domain of the thread.
 */
void _arch_mem_domain_configure(struct k_thread *thread)
{
	/* Request to configure memory domain for a thread.
	 * This triggers re-programming of the entire dynamic
	 * memory map.
	 */
	_arch_configure_dynamic_mpu_regions(thread);
}

/*
 * @brief Reset the MPU configuration related to the memory domain
 *        partitions
 *
 * @param domain pointer to the memory domain (must be valid)
 */
void _arch_mem_domain_destroy(struct k_mem_domain *domain)
{
	/* This function will reset the access permission configuration
	 * of the active partitions of the memory domain.
	 */
	int i;
	struct k_mem_partition partition;
	/* Partitions belonging to the memory domain will be reset
	 * to default (Privileged RW, Unprivileged NA) permissions.
	 */
	k_mem_partition_attr_t reset_attr = K_MEM_PARTITION_P_RW_U_NA;

	for (i = 0; i < CONFIG_MAX_DOMAIN_PARTITIONS; i++) {
		partition = domain->partitions[i];
		if (partition.size == 0) {
			/* Zero size indicates a non-existing
			 * memory partition.
			 */
			continue;
		}
		arm_core_mpu_mem_partition_configure(&partition, &reset_attr);
	}
}

/*
 * @brief Remove a partition from the memory domain
 *
 * @param domain pointer to the memory domain (must be valid
 * @param partition_id the ID (sequence) number of the memory domain
 *        partition (must be a valid partition).
 */
void _arch_mem_domain_partition_remove(struct k_mem_domain *domain,
				       u32_t  partition_id)
{
	/* Request to remove a partition from a memory domain.
	 * This resets the access permissions of the partition
	 * to default (Privileged RW, Unprivileged NA).
	 */
	k_mem_partition_attr_t reset_attr = K_MEM_PARTITION_P_RW_U_NA;

	arm_core_mpu_mem_partition_configure(
		&domain->partitions[partition_id], &reset_attr);
}

/*
 * Validate the given buffer is user accessible or not
 */
int _arch_buffer_validate(void *addr, size_t size, int write)
{
	return arm_core_mpu_buffer_validate(addr, size, write);
}

#endif /* CONFIG_USERSPACE */
