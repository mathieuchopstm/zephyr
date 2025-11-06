/**
 * Copyright (c) 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Clocks manager for global/system IPs
 */

#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/util.h>

#include <zephyr/drivers/clock_control/stm32_clock_control.h>

#include <stm32_ll_bus.h>

#include "stm32_global_periph_clocks.h"

/**
 * TODO: use Clock Control API
 *
 * NOTE: &exti for SYSCFG clock
 *       &pwr for PWR clock
 * to be reviewed
 */

/*
 * This routine is used by the enable path when runtime gating is enabled,
 * and the enable-clocks-of-everyone when runtime gating is disabled, so
 * it is compiled unconditionally.
 */
static void enable_periph_clock_gate(enum stm32_global_peripheral_id periph_id)
{
	switch (periph_id) {
	case STM32_GLOBAL_PERIPH_PWR:
#if defined(CONFIG_SOC_SERIES_STM32N6X) || defined(CONFIG_SOC_SERIES_STM32WBAX)
	LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_PWR);
#elif defined(CONFIG_SOC_SERIES_STM32U3X)
	LL_AHB1_GRP2_EnableClock(LL_AHB1_GRP2_PERIPH_PWR);
#elif defined(CONFIG_SOC_SERIES_STM32U5X)
	LL_AHB3_GRP1_EnableClock(LL_AHB3_GRP1_PERIPH_PWR);
#else
	/* TODO: some series don't need this! */
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
#endif /* ... */

	case STM32_GLOBAL_PERIPH_SYSCFG:
#if defined(CONFIG_SOC_SERIES_STM32H7X)
	LL_APB4_GRP1_EnableClock(LL_APB4_GRP1_PERIPH_SYSCFG);
#elif defined(CONFIG_SOC_SERIES_STM32C0X) || defined(CONFIG_SOC_SERIES_STM32F0X) || defined(CONFIG_SOC_SERIES_STM32U0X)
	LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG);
#elif defined(CONFIG_SOC_SERIES_STM32MP1) || defined(CONFIG_SOC_SERIES_STM32MP13X) || defined(CONFIG_SOC_SERIES_STM32U3X) || defined(CONFIG_SOC_SERIES_STM32U5X)
	LL_APB3_GRP1_EnableClock(LL_APB3_GRP1_PERIPH_SYSCFG);
#elif defined(CONFIG_SOC_SERIES_STM32WBAX)
	LL_APB7_GRP1_EnableClock(LL_APB7_GRP1_PERIPH_SYSCFG);
#elif defined(CONFIG_SOC_SERIES_STM32WB0X)
	LL_APB0_GRP1_EnableClock(LL_APB0_GRP1_PERIPH_SYSCFG);
#elif defined(CONFIG_SOC_SERIES_STM32N6X)
	LL_APB4_GRP2_EnableClock(LL_APB4_GRP2_PERIPH_SYSCFG);
#elif defined(CONFIG_SOC_SERIES_STM32H5X)
	LL_APB3_GRP1_EnableClock(LL_APB3_GRP1_PERIPH_SBS);
#elif defined(CONFIG_SOC_SERIES_STM32H7RSX)
	LL_APB4_GRP1_EnableClock(LL_APB4_GRP1_PERIPH_SBS);
#else
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
#endif /* ... */

	default:
		__ASSERT_NO_MSG(0);
	}
}

#if !defined(CONFIG_STM32_GLOBAL_CLOCKS_RUNTIME_GATING)

int stm32_global_periph_enable_all_clocks(void)
{
	for (int i = 0; i < STM32_GLOBAL_PERIPH_NUM; i++) {
		enable_periph_clock_gate((enum stm32_global_peripheral_id)i);
	}
}

/** TODO: invoke from RCC for proper ordering. for now this is a SYS_INIT() */
//TODO: sys_init

#else /* CONFIG_STM32_GLOBAL_CLOCKS_RUNTIME_GATING */

static uint8_t periphs_refcount[STM32_GLOBAL_PERIPH_NUM];
static struct k_spinlock refcounts_lock;

static void disable_periph_clock_gate(enum stm32_global_peripheral_id periph_id)
{
	/** TODO: */
}

void _stm32_global_periph_refer(enum stm32_global_peripheral_id periph_id)
{
	__ASSERT(IN_RANGE(periph_id, 0, STM32_GLOBAL_PERIPH_NUM - 1),
		"Invalid peripheral ID %d", periph_id);

	K_SPINLOCK(&refcounts_lock) {
		__ASSERT(periphs_refcount[periph_id] < UINT8_MAX,
			"Too many enable() calls for peripheral with ID=%d", periph_id);

		uint8_t old_refcount = periphs_refcount[periph_id];

		periphs_refcount[periph_id] += 1;

		if (old_refcount == 0U) {
			enable_periph_clock_gate(periph_id);
		}
	}
}

void _stm32_global_periph_release(enum stm32_global_peripheral_id periph_id)
{
	__ASSERT(IN_RANGE(periph_id, 0, STM32_GLOBAL_PERIPH_NUM - 1),
		"Invalid peripheral ID %d", periph_id);

	K_SPINLOCK(&refcounts_lock) {
		__ASSERT(periphs_refcount[periph_id] > 0,
			"Too many disable() calls for peripheral with ID=%d", periph_id);

		uint8_t new_refcount = --periphs_refcount[periph_id];

		if (new_refcount == 0U) {
			disable_periph_clock_gate(periph_id);
		}
	}
}

#endif /* !CONFIG_STM32_GLOBAL_CLOCKS_RUNTIME_GATING */
