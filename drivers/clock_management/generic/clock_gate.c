/*
 * Copyright 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/arch/common/sys_io.h>
#include <zephyr/drivers/clock_management/clock_driver.h>

#define DT_DRV_COMPAT clock_gate

struct clock_gate_config {
	const struct clk *parent;
	mem_addr_t reg;
	uint32_t mask;
};

static int clock_gate_get_rate(const struct clk *clk_hw)
{
	const struct clock_gate_config *config = clk_hw->hw_data;

	return (sys_read32(config->reg) & config->mask)
		? clock_get_rate(config->parent)
		: 0;
}

static int clock_gate_configure(const struct clk *clk_hw, const void *data)
{
	const struct clock_gate_config *config = clk_hw->hw_data;
	bool enable = (bool)data;


	if (enable) {
		sys_write32(
			sys_read32(config->reg) | config->mask,
			config->reg);
	} else {
		sys_write32(
			sys_read32(config->reg) & ~config->mask,
			config->reg);
	}

#if defined(CONFIG_CLOCK_MANAGEMENT_GENERIC_DRIVERS_READ_AFTER_WRITE)
	(void)sys_read32(config->reg);
#endif

	return 0;
}

const struct clock_management_driver_api clock_gate_api = {
	.get_rate = clock_gate_get_rate,
	.configure = clock_gate_configure,
	/* RUNTIME/GET_RATE not supported yet */
};

#define CLOCK_GATE_DEFINE(inst)							\
	const struct clock_gate_config clock_gate_##inst = {			\
		.parent = CLOCK_DT_GET(DT_INST_PARENT(inst)),			\
		.reg = DT_INST_REG_ADDR(inst),		\
		.mask = BIT(DT_INST_PROP(inst, gate_offset)),			\
	};									\
										\
	CLOCK_DT_INST_DEFINE(inst,						\
			     &clock_gate_##inst,				\
			     &clock_gate_api);

DT_INST_FOREACH_STATUS_OKAY(CLOCK_GATE_DEFINE)
