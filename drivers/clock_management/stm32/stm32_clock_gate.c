/*
 * Copyright 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stm32_clock_management_common.h"

#define DT_DRV_COMPAT st_stm32_clock_gate

struct stm32_clock_gate_config {
	const struct clk *parent;
	struct stm32_reg_field gate_reg;
};

static int stm32_clock_gate_get_rate(const struct clk *hw)
{
	const struct stm32_clock_gate_config *config = hw->hw_data;

	if (stm32_clk_read_field(config->gate_reg) != 0) {
		return clock_get_rate(config->parent);
	} else { /* Gate not active */
		return 0;
	}
}

static int stm32_clock_gate_configure(const struct clk *hw, const void *configuration)
{
	const struct stm32_clock_gate_config *config = hw->hw_data;
	const uint32_t enable = (uint32_t)configuration;

	stm32_clk_write_field(config->gate_reg, enable);

	return 0;
}

#if defined(CONFIG_CLOCK_MANAGEMENT_RUNTIME)
/* TBD */
#endif

#if defined(CONFIG_CLOCK_MANAGEMENT_SET_RATE)
/* TBD */
#endif

const struct clock_management_driver_api stm32_clock_gate_api = {
	.get_rate = stm32_clock_gate_get_rate,
	.configure = stm32_clock_gate_configure,
#if defined(CONFIG_CLOCK_MANAGEMENT_RUNTIME)
	.notify = /* TBD */,
#endif
#if defined(CONFIG_CLOCK_MANAGEMENT_SET_RATE)
	.round_rate = /* TBD */,
	.set_rate = /* TBD */,
#endif
};

#define STM32_CLOCK_GATE_DEFINE(inst)						\
	const struct stm32_clock_gate_config stm32_clock_gate_config_##inst = {	\
		.parent = CLOCK_DT_GET(DT_INST_PARENT(inst)),			\
		.gate_reg = STM32_INST_REG_FIELD(inst, gate_offset),		\
	};									\
	CLOCK_DT_INST_DEFINE(inst,						\
			     &stm32_clock_gate_config_##inst,			\
			     &stm32_clock_gate_api);

DT_INST_FOREACH_STATUS_OKAY(STM32_CLOCK_GATE_DEFINE)
