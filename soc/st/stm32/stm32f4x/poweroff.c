/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 * Copyright (c) 2024 STMicroelectronics
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

#include <stm32_global_periph_clocks.h>

void z_sys_poweroff(void)
{
	stm32_global_periph_refer(STM32_GLOBAL_PERIPH_PWR);

	LL_PWR_ClearFlag_WU();

	LL_PWR_SetPowerMode(LL_PWR_MODE_STANDBY);
	LL_LPM_EnableDeepSleep();

	k_cpu_idle();

	CODE_UNREACHABLE;
}
