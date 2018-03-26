/*
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/arm/cortex_m/cmsis.h>
#include <cortex_m/tz.h>

static void configure_nonsecure_vtor_offset(u32_t vtor_ns)
{
	SCB_NS->VTOR = vtor_ns;
}

static void configure_nonsecure_msp(u32_t msp_ns)
{
	__TZ_set_MSP_NS(msp_ns);
}

static void configure_nonsecure_psp(u32_t psp_ns)
{
	__TZ_set_PSP_NS(psp_ns);
}

static void configure_nonsecure_control(u32_t spsel_ns, u32_t npriv_ns)
{
	u32_t control_ns = __TZ_get_CONTROL_NS();

	control_ns &= ~(CONTROL_SPSEL_Msk | CONTROL_nPRIV_Msk);

	if (spsel_ns) {
		control_ns |= CONTROL_SPSEL_Msk;
	}
	if (npriv_ns) {
		control_ns |= CONTROL_nPRIV_Msk;
	}

	__TZ_set_CONTROL_NS(control_ns);
}

#if defined(CONFIG_ARMV8_M_MAINLINE)
void tz_nonsecure_splim_set(u32_t is_msp, u32_t lim)
{
	if (is_msp) {
		__TZ_set_MSPLIM_NS(lim);
	} else {
		__TZ_set_PSPLIM_NS(lim);
	}
	__ISB();
	__DSB();
}
#endif /* CONFIG_ARMV8_M_MAINLINE */

void tz_nonsecure_state_setup(const tz_nonsecure_setup_conf_t *p_ns_conf)
{
	configure_nonsecure_vtor_offset(p_ns_conf->vtor_ns);
	configure_nonsecure_msp(p_ns_conf->msp_ns);
	configure_nonsecure_psp(p_ns_conf->psp_ns);
	configure_nonsecure_control(p_ns_conf->control_ns.spsel,
		p_ns_conf->control_ns.npriv);
}

void tz_nbanked_exception_target_state_set(int secure_state)
{
	u32_t aircr_payload = SCB->AIRCR & (~(SCB_AIRCR_VECTKEY_Msk));
	if (secure_state) {
		aircr_payload &= ~(SCB_AIRCR_BFHFNMINS_Msk);
	} else {
		aircr_payload |= SCB_AIRCR_BFHFNMINS_Msk;
	}
	SCB->AIRCR = ((0x5FAUL << SCB_AIRCR_VECTKEY_Pos)
			& SCB_AIRCR_VECTKEY_Msk)
		| aircr_payload;
}

void tz_sau_configure(int enable, int allns)
{
	if (enable) {
		TZ_SAU_Enable();
	} else {
		TZ_SAU_Disable();
		if (allns) {
			SAU->CTRL |= SAU_CTRL_ALLNS_Msk;
		} else {
			SAU->CTRL &= ~(SAU_CTRL_ALLNS_Msk);
		}
	}
}
