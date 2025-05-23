/*
 * Copyright 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/clock_management/clock_driver.h>

#define DT_DRV_COMPAT fixed_prescaler

struct fixed_prescaler_config {
	const struct clk *parent;
	uint32_t div_factor;
};

static int fixed_prescaler_get_rate(const struct clk *clk_hw)
{
	const struct fixed_prescaler_config *config = clk_hw->hw_data;

	return clock_get_rate(config->parent) / config->div_factor;
}

const struct clock_management_driver_api fixed_prescaler_api = {
	.get_rate = fixed_prescaler_get_rate,
	/* RUNTIME/SET_RATE not supported yet */
};

#define FIXED_PRESCALER_DEFINE(inst)					\
	const struct fixed_prescaler_config fixed_prescaler_##inst = {	\
		.parent = CLOCK_DT_GET(DT_INST_PARENT(inst)),		\
		.div_factor = DT_INST_PROP(inst, division_factor),	\
	};								\
									\
	CLOCK_DT_INST_DEFINE(inst,					\
			     &fixed_prescaler_##inst,			\
			     &fixed_prescaler_api);

DT_INST_FOREACH_STATUS_OKAY(FIXED_PRESCALER_DEFINE)
