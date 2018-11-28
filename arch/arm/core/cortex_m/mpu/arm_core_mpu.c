/*
 * Copyright (c) 2017 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <init.h>
#include <kernel.h>
#include <soc.h>

#include <kernel_structs.h>

#include <arch/arm/cortex_m/mpu/arm_core_mpu_dev.h>
#include <linker/linker-defs.h>

#define LOG_LEVEL CONFIG_MPU_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(mpu);


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

/**
 * @brief Use the HW-specific MPU driver to program
 *        the static MPU regions.
 *
 * Program the static MPU regions using the HW-specific MPU driver. The
 * function is meant to be invoked only once upon system initialization.
 */
void _arch_configure_static_mpu_regions(void)
{
	/* Define a constant array of partitions, to
	 * hold the configuration of the respective
	 * static MPU regions.
	 */
	const struct k_mem_partition static_regions[] = {
#if defined(CONFIG_APPLICATION_MEMORY)
		{
		.start = (u32_t)&__app_ram_start,
		.size = (u32_t)&__app_ram_end - (u32_t)&__app_ram_start,
		.attr = K_MEM_PARTITION_P_RW_U_RW,
		},
#endif /* CONFIG_APPLICATION_MEMORY */
#if defined(CONFIG_NOCACHE_MEMORY)
		{
		.start = (u32_t)&_nocache_ram_start,
		.size = (u32_t)&_nocache_ram_end - (u32_t)&_nocache_ram_start,
		.attr = K_MEM_PARTITION_P_RW_U_RW_NOCACHE,
		}
#endif /* CONFIG_NOCACHE_MEMORY */
	};

	/* Configure the static MPU regions within image SRAM boundaries. */
	arm_core_mpu_disable();
	arm_core_mpu_configure_static_mpu_regions(static_regions,
		sizeof(static_regions)/sizeof(static_regions[0]),
		(u32_t)_image_ram_start,
		(u32_t)_image_ram_end);
	arm_core_mpu_enable();
}

/**
 * @brief Use the HW-specific MPU driver to program
 *        the dynamic MPU regions.
 *
 * Program the dynamic MPU regions using the HW-specific
 * MPU driver. The function is meant to be invoked every
 * time the memory map is to be re-programmed, e.g upon
 * thread context-switch, entering user mode, etc.
 */
void _arch_configure_dynamic_mpu_regions(struct k_thread *thread)
{
	/* Define an array of partitions, to hold the configuration
	 * of the respective dynamic MPU regions to be programmed
	 * for this thread. The array of partitions will be supplied
	 * to the underlying MPU driver.
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
		size += thread->stack_info.start - (u32_t)thread->stack_obj;
#endif
		dynamic_regions[region_num].start = base;
		dynamic_regions[region_num].size = size;
		dynamic_regions[region_num].attr = (k_mem_partition_attr_t)K_MEM_PARTITION_P_RW_U_RW;

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
	dynamic_regions[region_num].attr = (k_mem_partition_attr_t)K_MEM_PARTITION_P_RO_U_NA;

	region_num++;
#endif /* CONFIG_MPU_STACK_GUARD */

	if (region_num > 0) {
		/* Configure the dynamic MPU regions */
		arm_core_mpu_disable();
		arm_core_mpu_configure_dynamic_mpu_regions(dynamic_regions,
			region_num);
		arm_core_mpu_enable();
	}
}

#if defined(CONFIG_USERSPACE)

int _arch_mem_domain_max_partitions_get(void)
{
	return arm_core_mpu_get_max_domain_partition_regions();
}

void _arch_mem_domain_configure(struct k_thread *thread)
{
	/* Request to configure memory domain for a thread.
	 * This triggers re-programming of the entire dynamic
	 * memory map.
	 */
	_arch_configure_dynamic_mpu_regions(thread);
}

/*
 * Destroy MPU regions for the mem domain
 */
void _arch_mem_domain_destroy(struct k_mem_domain *domain)
{
	/* Request to destroy a memory domain.
	 * This triggers re-programming of the entire dynamic
	 * memory map for the current thread.
	 */
	if (_current != NULL) {
		_arch_configure_dynamic_mpu_regions(_current);
	}
}

/*
 * Reset MPU region for a single memory partition
 */
void _arch_mem_domain_partition_remove(struct k_mem_domain *domain,
				       u32_t  partition_id)
{
	/* Request to destroy a partition of a memory domain.
	 * This triggers re-programming of the entire dynamic
	 * memory map for the current thread.
	 */
	if (_current != NULL) {
		_arch_configure_dynamic_mpu_regions(_current);
	}
}

/*
 * Validate the given buffer is user accessible or not
 */
int _arch_buffer_validate(void *addr, size_t size, int write)
{
	return arm_core_mpu_buffer_validate(addr, size, write);
}

#endif /* CONFIG_USERSPACE */

#if defined(CONFIG_CPU_HAS_NXP_MPU)
#if defined(CONFIG_MPU_STACK_GUARD)
/*
 * @brief Configure MPU stack guard
 *
 * This function configures per thread stack guards reprogramming the MPU.
 * The functionality is meant to be used during context switch.
 *
 * @param thread thread info data structure.
 */
void configure_mpu_stack_guard(struct k_thread *thread)
{
	u32_t guard_size = MPU_GUARD_ALIGN_AND_SIZE;
#if defined(CONFIG_USERSPACE)
	u32_t guard_start = thread->arch.priv_stack_start ?
			    (u32_t)thread->arch.priv_stack_start :
			    (u32_t)thread->stack_obj;
#else
	u32_t guard_start = thread->stack_info.start;
#endif

	arm_core_mpu_disable();
	arm_core_mpu_configure(THREAD_STACK_GUARD_REGION, guard_start,
			       guard_size);
	arm_core_mpu_enable();
}
#endif
#endif

#if defined(CONFIG_USERSPACE)
#if defined(CONFIG_NXP_MPU)
/*
 * @brief Configure MPU user context
 *
 * This function configures the thread's user context.
 * The functionality is meant to be used during context switch.
 *
 * @param thread thread info data structure.
 */
void configure_mpu_user_context(struct k_thread *thread)
{
	LOG_DBG("configure user thread %p's context", thread);
	arm_core_mpu_disable();
	arm_core_mpu_configure_user_context(thread);
	arm_core_mpu_enable();
}

/*
 * @brief Configure MPU memory domain
 *
 * This function configures per thread memory domain reprogramming the MPU.
 * The functionality is meant to be used during context switch.
 *
 * @param thread thread info data structure.
 */
void configure_mpu_mem_domain(struct k_thread *thread)
{
	LOG_DBG("configure thread %p's domain", thread);
	arm_core_mpu_disable();
	arm_core_mpu_configure_mem_domain(thread->mem_domain_info.mem_domain);
	arm_core_mpu_enable();
}
#endif


#if CONFIG_NXP_MPU

/*
 * Reset MPU region for a single memory partition
 */
void _arch_mem_domain_partition_remove(struct k_mem_domain *domain,
				       u32_t  partition_id)
{
	ARG_UNUSED(domain);

	arm_core_mpu_disable();
	arm_core_mpu_mem_partition_remove(partition_id);
	arm_core_mpu_enable();

}

/*
 * Destroy MPU regions for the mem domain
 */
void _arch_mem_domain_destroy(struct k_mem_domain *domain)
{
	ARG_UNUSED(domain);

	arm_core_mpu_disable();
	arm_core_mpu_configure_mem_domain(NULL);
	arm_core_mpu_enable();
}
#endif



#endif
