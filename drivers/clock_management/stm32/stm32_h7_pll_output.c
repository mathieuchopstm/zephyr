/*
 * Copyright (c) 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stm32_clock_management_common.h"
#include <zephyr/sys/__assert.h>

#define DT_DRV_COMPAT st_stm32_h7_pll_output

struct stm32_pll_output_config {
	const struct clk *parent;
	struct stm32_reg_field enable_reg;
	struct stm32_reg_field div_reg;
};

static int stm32_pll_output_get_rate(const struct clk *hw)
{
	const struct stm32_pll_output_config *config = hw->hw_data;

	if (stm32_clk_read_field(config->enable_reg) == 0) {
		return 0;
	}

	/* Adjust DIVyx as the field contains the division factor minus 1 */
	const uint32_t div = stm32_clk_read_field(config->div_reg) + 1;

	/* All zeroes value is not allowed (per RefMan) */
	__ASSERT(div != 1, "Illegal divider programmed in register");

	return clock_get_rate(config->parent) / div;
}

static int stm32_pll_output_configure(const struct clk *hw, const void *configuration)
{
	const struct stm32_pll_output_config *config = hw->hw_data;
	const uint32_t div_factor = (uint32_t)configuration;

	/* TODO: -EIO if active */

	__ASSERT(div_factor != 0, "Illegal configuration");

	stm32_clk_write_field(config->div_reg, div_factor);

	return 0;
}

static int stm32_pll_output_off_on(const struct clk *hw, bool enable)
{
	const struct stm32_pll_output_config *config = hw->hw_data;

	stm32_clk_write_field(config->enable_reg, !!enable);

	return 0;
}

const struct clock_management_driver_api stm32_pll_output_api = {
	/* OFF_ON support required */
	.get_rate = stm32_pll_output_get_rate,
	.configure = stm32_pll_output_configure,
	.off_on = stm32_pll_output_off_on,
};

#define CFGNAME(inst) CONCAT(stm32_pll_output_config_, DT_INST_DEP_ORD(inst))

#define STM32_PLL_OUTPUT_DEFINE(inst)						\
	const struct stm32_pll_output_config CFGNAME(inst) = {			\
		.parent = CLOCK_DT_GET(DT_INST_PARENT(inst)),			\
		.enable_reg = STM32_INST_REG_FIELD_FROM_PROP(inst, reg_en),	\
		.div_reg = STM32_INST_REG_FIELD_FROM_PROP(inst, reg_div),	\
	};									\
										\
	CLOCK_DT_INST_DEFINE(inst, &CFGNAME(inst), &stm32_pll_output_api);

DT_INST_FOREACH_STATUS_OKAY(STM32_PLL_OUTPUT_DEFINE)
