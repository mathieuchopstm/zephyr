/*
 * Copyright 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stm32_clock_management_common.h"

#define DT_DRV_COMPAT st_stm32_clock_multiplexer

struct stm32_clock_mux_config {
	struct stm32_reg_field mux_reg;
	const struct clk *parents[];
};

static int stm32_clock_mux_get_rate(const struct clk *clk_hw)
{
	const struct stm32_clock_mux_config *config = clk_hw->hw_data;

	return clock_get_rate(config->parents[stm32_clk_read_field(config->mux_reg)]);
}

static int stm32_clock_mux_configure(const struct clk *clk_hw, const void *data)
{
	const struct stm32_clock_mux_config *config = clk_hw->hw_data;
	uint8_t new_parent_index = (uint8_t)(uintptr_t)data;

	if (config->parents[new_parent_index] == NULL) {
		return -ENODEV;
	}

	/* compared to clock-mux, we are slightly unsafe
	 * because we don't check whether the new parent
	 * index is valid
	 */

	stm32_clk_write_field(config->mux_reg, new_parent_index);

	return 0;
}

#if defined(CONFIG_CLOCK_MANAGEMENT_RUNTIME)
/* TBD */
#endif

#if defined(CONFIG_CLOCK_MANAGEMENT_SET_RATE)
/* TBD */
#endif

const struct clock_management_driver_api stm32_clock_mux_api = {
	.get_rate = stm32_clock_mux_get_rate,
	.configure = stm32_clock_mux_configure,
#if defined(CONFIG_CLOCK_MANAGEMENT_RUNTIME)
	.notify = /* TBD */,
#endif
#if defined(CONFIG_CLOCK_MANAGEMENT_SET_RATE)
	.round_rate = /* TBD */,
	.set_rate = /* TBD */,
#endif
};

#define GET_MUX_INPUT(node_id, prop, idx)					\
	CLOCK_DT_GET_OR_NULL(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

#define STM32_CLOCK_MUX_DEFINE(inst)						\
	const struct stm32_clock_mux_config stm32_clock_mux_##inst = {		\
		.mux_reg = STM32_INST_REG_FIELD(inst),				\
		.parents = {							\
			DT_INST_FOREACH_PROP_ELEM(inst, inputs, GET_MUX_INPUT)	\
		},								\
	};									\
										\
	CLOCK_DT_INST_DEFINE(inst,						\
			     &stm32_clock_mux_##inst,				\
			     &stm32_clock_mux_api)

DT_INST_FOREACH_STATUS_OKAY(STM32_CLOCK_MUX_DEFINE)
