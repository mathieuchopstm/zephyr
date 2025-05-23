/*
 * Copyright 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stm32_clock_management_common.h"

#define DT_DRV_COMPAT st_stm32_sysclk_prescaler

struct stm32_sysclk_prescaler_config {
	const struct clk *const parent;
	const struct stm32_reg_field presc_reg_field;
};

static int stm32_sysclk_prescaler_get_rate(const struct clk *clk)
{
	const struct stm32_sysclk_prescaler_config *config = clk->hw_data;
	const uint32_t presc_fval = stm32_clk_read_field(config->presc_reg_field);

	/* Prescaler field contains (<division factor> - 1) */
	return clock_get_rate(config->parent) / (presc_fval + 1);
}

static int stm32_sysclk_prescaler_configure(const struct clk *clk, const void *data)
{
	const struct stm32_sysclk_prescaler_config *config = clk->hw_data;
	const uint32_t prescaler_div_factor = (uint32_t)data;

	/* TODO: runtime support */
	/* TODO: assert factor is valid for HW */
	/* Prescaler field contains (<division factor> - 1)
	 * However, serialization macros already did the correction,
	 * so we can write the value as-is.
	 */
	stm32_clk_write_field(config->presc_reg_field, prescaler_div_factor);

	return 0;
}

#if defined(CONFIG_CLOCK_MANAGEMENT_RUNTIME)
/* TBD */
#endif

#if defined(CONFIG_CLOCK_MANAGEMENT_SET_RATE)
/* TBD */
#endif

const struct clock_management_driver_api stm32_sysclk_prescaler_api = {
	.get_rate = stm32_sysclk_prescaler_get_rate,
	.configure = stm32_sysclk_prescaler_configure,
#if defined(CONFIG_CLOCK_MANAGEMENT_RUNTIME)
	.notify = /* TBD */,
#endif
#if defined(CONFIG_CLOCK_MANAGEMENT_SET_RATE)
	.round_rate = /* TBD */,
	.set_rate = /* TBD */,
#endif
};

#define CFGNAME(inst) CONCAT(stm32_sysclk_prescaler_config_, DT_INST_DEP_ORD(inst))

#define ST_SYSCLK_PRESCALER_DEFINE(inst)					\
	const struct stm32_sysclk_prescaler_config CFGNAME(inst) = {		\
		.parent = CLOCK_DT_GET(DT_INST_PARENT(inst)),			\
		.presc_reg_field = STM32_INST_REG_FIELD(inst)			\
	};									\
										\
	CLOCK_DT_INST_DEFINE(inst, &CFGNAME(inst),				\
			     &stm32_sysclk_prescaler_api);

DT_INST_FOREACH_STATUS_OKAY(ST_SYSCLK_PRESCALER_DEFINE)
