/*
 * Copyright (c) 2024 Kickmaker
 * Copyright (c) 2025 Tomas Jurena
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/toolchain.h>
#include <zephyr/drivers/misc/stm32_wkup_pins/stm32_wkup_pins.h>

#include <stm32_ll_cortex.h>
#include <stm32_ll_pwr.h>
#include <stm32_ll_system.h>

#include <stm32_global_periph_clocks.h>

void z_sys_poweroff(void)
{
#ifdef CONFIG_STM32_WKUP_PINS
	stm32_pwr_wkup_pin_cfg_pupd();
#endif /* CONFIG_STM32_WKUP_PINS */

	/* reference will be released upon platform reset */
	stm32_global_periph_refer(STM32_GLOBAL_PERIPH_PWR);

	LL_PWR_ClearFlag_WU();

	LL_PWR_SetPowerMode(LL_PWR_MODE_SHUTDOWN);
	LL_LPM_EnableDeepSleep();
	LL_DBGMCU_DisableDBGStandbyMode();

	k_cpu_idle();

	CODE_UNREACHABLE;
}
