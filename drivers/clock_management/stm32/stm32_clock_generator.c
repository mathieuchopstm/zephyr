/*
 * Copyright 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "stm32_clock_management_common.h"

#define DT_DRV_COMPAT st_stm32_clock_generator

struct stm32_clock_generator_config {
	struct stm32_reg_field enable_reg; /* Bit controlling if generator is enabled */
	struct stm32_reg_field status_reg; /* Bit indicating the generator's status */
	uint32_t clock_rate;
};

static int stm32_clock_generator_get_rate(const struct clk *hw)
{
	const struct stm32_clock_generator_config *config = hw->hw_data;

	if (stm32_clk_read_field(config->status_reg) != 0) {
		return config->clock_rate;
	} else { /* generator not active */
		return 0;
	}
}

static int stm32_clock_generator_configure(const struct clk *hw, const void *configuration)
{
	const struct stm32_clock_generator_config *config = hw->hw_data;
	const uint32_t enable = (uint32_t)configuration;

	stm32_clk_write_field(config->enable_reg, enable);
	stm32_clk_poll_field(config->status_reg, enable);

	return 0;
}

#if defined(CONFIG_CLOCK_MANAGEMENT_RUNTIME)
static int stm32_clock_generator_notify(const struct clk *hw, const struct clk *parent,
			       const struct clock_management_event *event)
{
	const struct stm32_clock_generator_data *data = hw->hw_data;
	const uint32_t my_rate = stm32_clock_generator_get_rate(hw);
	const struct clock_management_event notify_event = {
		/*
		 * Use QUERY type, no need to forward this notification to
		 * consumers
		 */
		.type = CLOCK_MANAGEMENT_QUERY_RATE_CHANGE,
		.old_rate = my_rate,
		.new_rate = my_rate,
	};

	ARG_UNUSED(event);
	return clock_notify_children(hw, &notify_event);
}
#endif

#if defined(CONFIG_CLOCK_MANAGEMENT_SET_RATE)

static int stm32_clock_generator_round_rate(const struct clk *hw, uint32_t rate_req)
{
	const struct stm32_clock_generator_data *data = hw->hw_data;

	return data->clock_rate;
}

static int stm32_clock_generator_set_rate(const struct clk *hw, uint32_t rate_req)
{
	const struct stm32_clock_generator_config *config = hw->hw_data;

	/* enable if rate_req is non-zero, disable if zero */
	(void)stm32_clock_generator_configure(hw, (void *)(rate_req != 0));

	return (rate_req != 0) ? config->clock_rate : 0;
}

#endif

const struct clock_management_driver_api stm32_clock_generator_api = {
	.get_rate = stm32_clock_generator_get_rate,
	.configure = stm32_clock_generator_configure,
#if defined(CONFIG_CLOCK_MANAGEMENT_RUNTIME)
	.notify = stm32_clock_generator_notify,
#endif
#if defined(CONFIG_CLOCK_MANAGEMENT_SET_RATE)
	.round_rate = stm32_clock_generator_round_rate,
	.set_rate = stm32_clock_generator_set_rate,
#endif
};

#define STM32_CLOCK_GENERATOR_DEFINE(inst)							\
	const struct stm32_clock_generator_config stm32_clock_generator_config_##inst = {	\
		.clock_rate = DT_INST_PROP(inst, clock_frequency),				\
		.enable_reg = STM32_INST_REG_FIELD(inst, enable_offset),			\
		.status_reg = STM32_INST_REG_FIELD(inst, status_offset),			\
	};											\
	ROOT_CLOCK_DT_INST_DEFINE(inst,								\
				  &stm32_clock_generator_config_##inst,				\
				  &stm32_clock_generator_api);

DT_INST_FOREACH_STATUS_OKAY(STM32_CLOCK_GENERATOR_DEFINE)
