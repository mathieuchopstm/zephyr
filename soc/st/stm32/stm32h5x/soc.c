/*
 * Copyright (c) 2023 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief System/hardware module for STM32H5 processor
 */

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/cache.h>
#include <stm32_ll_bus.h>
#include <stm32_ll_pwr.h>
#include <zephyr/logging/log.h>

#include <stm32_global_periph_clocks.h>

#include <cmsis_core.h>

#define LOG_LEVEL CONFIG_SOC_LOG_LEVEL
LOG_MODULE_REGISTER(soc);

extern void stm32_power_init(void);
/**
 * @brief Perform basic hardware initialization at boot.
 *
 * This needs to be run from the very beginning.
 */
void soc_early_init_hook(void)
{
	sys_cache_instr_enable();

	/* Update CMSIS SystemCoreClock variable (HCLK) */
	/* At reset, system core clock is set to 32 MHz from HSI with a HSIDIV = 2 */
	SystemCoreClock = 32000000;

#if defined(PWR_UCPDR_UCPD_DBDIS)
	if (IS_ENABLED(CONFIG_DT_HAS_ST_STM32_UCPD_ENABLED) ||
		!IS_ENABLED(CONFIG_USB_DEVICE_DRIVER)) {
		/* Disable USB Type-C dead battery pull-down behavior */
		stm32_global_periph_refer(STM32_GLOBAL_PERIPH_PWR);
		LL_PWR_DisableUCPDDeadBattery();
		stm32_global_periph_release(STM32_GLOBAL_PERIPH_PWR);
	}

#endif /* PWR_UCPDR_UCPD_DBDIS */

#if CONFIG_PM
	stm32_power_init();
#endif
}
