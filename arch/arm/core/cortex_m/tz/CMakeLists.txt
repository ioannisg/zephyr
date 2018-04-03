# '-mcmse' enables the generation of code for the Secure state of the ARMv8-M
# Security Extensions. This option is required when building a Secure firmware.
zephyr_compile_options(-mcmse)
# '--section-start,.gnu.sgstubs' enables entry veneers (for Secure Entry
# functions) to be placed inside the special output section .gnu.sgstubs.
zephyr_ld_option_ifdef(CONFIG_ARM_FIRMWARE_HAS_SECURE_ENTRY_FUNCS
	${LINKERFLAGPREFIX},--section-start,.gnu.sgstubs=${CONFIG_ARM_NSC_REGION_BASE_ADDRESS}
)

zephyr_sources(arm_core_tz.c)
