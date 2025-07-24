/*
 * Copyright 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stm32_clock_management_common.h"

#define DT_DRV_COMPAT st_stm32_timer_freqmul

/**
 * Macros to strip down driver when possible
 *
 * NOTE: driver is "multi-instance-ish" - all or none must have TIMPRE
 * (this should always be single instance anyways...)
 */
#if DT_ANY_COMPAT_HAS_PROP_STATUS_OKAY(DT_DRV_COMPAT, timpre_loc)
#define HAS_TIMPRE 1
#else
#define HAS_TIMPRE 0
#endif

struct stm32_timer_freqmul_config {
	const struct clk *const parent;
	const struct stm32_reg_field apbpre_rf;
#if HAS_TIMPRE
	const struct stm32_reg_field timpre_rf;
#endif
};

static int stm32_timer_freqmul_get_rate(const struct clk *clk)
{
	const struct stm32_timer_freqmul_config *config = clk->hw_data;
	const uint32_t apbpre = stm32_clk_read_field(config->apbpre_rf);
	const uint32_t pclk_rate = clock_get_rate(config->parent);
	uint32_t timpclk_rate;

#if HAS_TIMPRE
	/**
	 * TODO: support "extended" TIMPRE (as found on N6)
	 */
	const uint32_t timpre = stm32_clk_read_field(config->timpre_rf);
#else
	const uint32_t timpre = 0;
#endif
	if (timpre != 0U && apbpre >= 0x5) {
		/**
		 * TIMPRE enabled and APB prescaler at least /4
		 *	--> TIMPCLK = 4x PCLK
		 */
		timpclk_rate = 4 * pclk_rate;
	} else if (apbpre >= 0x4) {
		/**
		 * TIMPRE enabled and APB prescaler is /2, or
		 * TIMPRE disabled and APB prescaler at least /2
		 *	--> TIMPCLK = 2x PCLK
		 */
		timpclk_rate = 2 * pclk_rate;
	} else {
		/**
		 * APB prescaler is /1
		 *	--> TIMPCLK = PCLK (= HCLK)
		 */
		timpclk_rate = pclk_rate;
	}

	return timpclk_rate;
}

const struct clock_management_driver_api stm32_timer_freqmul_api = {
	.get_rate = stm32_timer_freqmul_get_rate
};

#define CFGNAME(inst) CONCAT(stm32_timer_freqmul_config_, DT_INST_DEP_ORD(inst))

/* TODO: .timpre_rf should have fallback null value! */
#define STM32_TIMER_FREQMUL_DEFINE(inst)					\
	const struct stm32_timer_freqmul_config CFGNAME(inst) = {		\
		.parent = CLOCK_DT_GET(DT_INST_PARENT(inst)),			\
		.apbpre_rf = STM32_INST_REG_FIELD_FROM_PROP(inst, apbpre_loc),	\
		IF_ENABLED(HAS_TIMPRE, (					\
		.timpre_rf = STM32_INST_REG_FIELD_FROM_PROP(inst, timpre_loc),	\
		))								\
	};									\
										\
	CLOCK_DT_INST_DEFINE(inst, &CFGNAME(inst),				\
			     &stm32_timer_freqmul_api);

DT_INST_FOREACH_STATUS_OKAY(STM32_TIMER_FREQMUL_DEFINE)
