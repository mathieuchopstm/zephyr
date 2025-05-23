/*
 * Copyright 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/arch/common/sys_io.h>
#include <zephyr/drivers/clock_management/clock.h>
#include <zephyr/drivers/clock_management/clock_driver.h>

#define DT_DRV_COMPAT pow2_prescaler

/**
 * TODO:
 *	- support RUNTIME
 *	- support 64-bit registers
 */

struct pow2_prescaler_config {
	const struct clk *parent;
	mem_addr_t reg_addr;
	/**
	 * uint8_t allows for a prescaler value up to 2^255 located
	 * in a 256-bit configuration register - more than enough
	 */
	uint8_t field_offset;
	uint8_t field_mask;
};

static int pow2_prescaler_get_rate(const struct clk *clk)
{
	const struct pow2_prescaler_config *config = clk->hw_data;
	uint32_t regval = sys_read32(config->reg_addr);
	uint32_t log2_div_factor = (regval >> config->field_offset) & config->field_mask;

	/* NOTE: (x >> y) = x / (2^y) */
	return clock_get_rate(config->parent) >> log2_div_factor;
}

static int pow2_prescaler_configure(const struct clk *clk, const void *data)
{
	const struct pow2_prescaler_config *config = clk->hw_data;
	const uint32_t log2_div_factor = (uint32_t)data;
	uint32_t regval = sys_read32(config->reg_addr);

	regval &= ~((uint32_t)config->field_mask << config->field_offset);
	regval |= (log2_div_factor << config->field_offset);
	sys_write32(regval, config->reg_addr);

	return 0;
}

#if defined(CONFIG_CLOCK_MANAGEMENT_RUNTIME)
/* TBD */
#endif

#if defined(CONFIG_CLOCK_MANAGEMENT_SET_RATE)
/* TBD */
#endif

const struct clock_management_driver_api pow2_prescaler_api = {
	.get_rate = pow2_prescaler_get_rate,
	.configure = pow2_prescaler_configure,
#if defined(CONFIG_CLOCK_MANAGEMENT_RUNTIME)
	.notify = /* TBD */,
#endif
#if defined(CONFIG_CLOCK_MANAGEMENT_SET_RATE)
	.round_rate = /* TBD */,
	.set_rate = /* TBD */,
#endif
};

#define POW2_PRESCALER_DEFINE(inst)					\
	const struct pow2_prescaler_config pow2_prescaler_##inst = {	\
		.parent = CLOCK_DT_GET(DT_INST_PARENT(inst)),		\
		.reg_addr = DT_INST_REG_ADDR(inst),			\
		.field_mask = GENMASK(DT_INST_REG_SIZE(inst) - 1, 0),	\
		.field_offset = DT_INST_PROP(inst, field_offset),	\
	};								\
									\
	CLOCK_DT_INST_DEFINE(inst,					\
			     &pow2_prescaler_##inst,			\
			     &pow2_prescaler_api)

DT_INST_FOREACH_STATUS_OKAY(POW2_PRESCALER_DEFINE)
