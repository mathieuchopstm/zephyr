/*
 * Copyright 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * TODO: if all div-factor are pow2, we can use a shift to avoid costy divisions
 */
#include <zephyr/drivers/clock_management/clock_driver.h>

#define DT_DRV_COMPAT clock_mux

struct clock_mux_config {
	mem_addr_t mux_reg;
	uint32_t mux_mask;
	uint8_t mux_shift;
	uint8_t num_parents;
	uint16_t padding;
	const struct clk *parents[];
};

static int clock_mux_get_rate(const struct clk *clk_hw)
{
	struct clock_mux_config *config = clk_hw->hw_data;
	uint32_t regval = sys_read32(config->mux_reg);
	uint32_t parent_idx = (regval & config->mux_mask) >> config->mux_shift;

	return clock_get_rate(config->parents[parent_idx]);
}

static int clock_mux_configure(const struct clk *clk_hw, const void *data)
{
	struct clock_mux_config *config = clk_hw->hw_data;
	uint8_t new_parent_index = (uint8_t)(uintptr_t)data;
	uint32_t regval;

	if (new_parent_index >= config->num_parents) {
		return -EINVAL;
	}

	regval = sys_read32(config->mux_reg);
	regval &= ~config->mux_mask;
	regval |= (new_parent_index << config->mux_shift);
	sys_write32(regval, config->mux_reg);

#if defined(CONFIG_CLOCK_MANAGEMENT_GENERIC_DRIVERS_READ_AFTER_WRITE)
	(void)sys_read32(config->mux_reg);
#endif

	return 0;
}

const struct clock_management_driver_api clock_mux_api = {
	.get_rate = clock_mux_get_rate,
	.configure = clock_mux_configure,
	/* RUNTIME/SET_RATE not supported yet */
};

#define GET_MUX_INPUT(node_id, prop, idx)				\
	CLOCK_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

#define MUX_MASK(inst)								\
	GENMASK(DT_INST_PROP(inst, mux_offset) + DT_INST_REG_SIZE(inst) - 1,	\
		DT_INST_PROP(inst, mux_offset))

#define CLOCK_MUX_DEFINE(inst)							\
	struct clock_mux_config clock_mux_##inst = {				\
		.num_parents = DT_INST_PROP_LEN(inst, inputs),			\
		.mux_reg = DT_INST_REG_ADDR(inst),				\
		.mux_mask = MUX_MASK(inst),					\
		.mux_shift = DT_INST_PROP(inst, mux_offset),			\
		.parents = {							\
			DT_INST_FOREACH_PROP_ELEM(inst, inputs, GET_MUX_INPUT)	\
		},								\
	};									\
										\
	CLOCK_DT_INST_DEFINE(inst,						\
			     &clock_mux_##inst,					\
			     &clock_mux_api)

DT_INST_FOREACH_STATUS_OKAY(CLOCK_MUX_DEFINE)
