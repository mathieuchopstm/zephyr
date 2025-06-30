/*
 * Copyright 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stm32_clock_management_common.h"

#define DT_DRV_COMPAT st_stm32c0_hsisys_div

struct stm32c0_hsisys_div_config {
	const struct clk *parent;
	struct stm32_reg_field prescaler_reg;
};

static int stm32c0_hsisys_div_get_rate(const struct clk *clk)
{
	/* Register field contains log2(prescaler) */
	const struct stm32c0_hsisys_div_config *config = clk->hw_data;
	const uint32_t log2_presc = stm32_clk_read_field(config->prescaler_reg);

	return clock_get_rate(config->parent) >> log2_presc;
}

static int stm32c0_hsisys_div_configure(const struct clk *clk, const void *data)
{
	const struct stm32c0_hsisys_div_config *config = clk->hw_data;
	const uint32_t field_val = (uint32_t)data;

	stm32_clk_write_field(config->prescaler_reg, field_val);

	return 0;
}

#if defined(CONFIG_CLOCK_MANAGEMENT_RUNTIME)
/* TBD */
#endif

#if defined(CONFIG_CLOCK_MANAGEMENT_SET_RATE)
/* TBD */
#endif

const struct clock_management_driver_api stm32c0_hsisys_div_api = {
	.get_rate = stm32c0_hsisys_div_get_rate,
	.configure = stm32c0_hsisys_div_configure,
#if defined(CONFIG_CLOCK_MANAGEMENT_RUNTIME)
	.notify = /* TBD */,
#endif
#if defined(CONFIG_CLOCK_MANAGEMENT_SET_RATE)
	.round_rate = /* TBD */,
	.set_rate = /* TBD */,
#endif
};

#define CFGNAME(inst) CONCAT(stm32c0_hsisys_div_config_, DT_INST_DEP_ORD(inst))

#define STM32C0_HSISYS_DIV_DEFINE(inst)						\
	const struct stm32c0_hsisys_div_config CFGNAME(inst) = {		\
		.parent = CLOCK_DT_GET(DT_INST_PARENT(inst)),			\
		.prescaler_reg = STM32_INST_REG_FIELD(inst),			\
	};									\
										\
	CLOCK_DT_INST_DEFINE(inst, &CFGNAME(inst),				\
			     &stm32c0_hsisys_div_api);

DT_INST_FOREACH_STATUS_OKAY(STM32C0_HSISYS_DIV_DEFINE)
