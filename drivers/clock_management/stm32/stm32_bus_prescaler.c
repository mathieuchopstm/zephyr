/*
 * Copyright 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stm32_clock_management_common.h"

#define DT_DRV_COMPAT st_stm32_bus_prescaler

struct stm32_bus_prescaler_config {
	const struct clk *parent;
	struct stm32_reg_field prescaler_reg;
	const uint8_t *prescaler_shift_table;
};

/* TODO: valid for all series? */
static const uint8_t ahbpre_to_shift_table[] = {
	/* 0b1000:   2 = 2^1 */ 1,
	/* 0b1001:   4 = 2^2 */ 2,
	/* 0b1010:   8 = 2^3 */ 3,
	/* 0b1011:  16 = 2^4 */ 4,
	/* 0b1100:  64 = 2^6 */ 6,
	/* 0b1101: 128 = 2^7 */ 7,
	/* 0b1110: 256 = 2^8 */ 8,
	/* 0b1111: 512 = 2^9 */ 9
};

static const uint8_t apbpre_to_shift_table[] = {
	/* 0b100:  2 = 2^1 */ 1,
	/* 0b101:  4 = 2^2 */ 2,
	/* 0b110:  8 = 2^3 */ 3,
	/* 0b111: 16 = 2^4 */ 4
};

static int stm32_bus_prescaler_get_rate(const struct clk *clk)
{
	const struct stm32_bus_prescaler_config *config = clk->hw_data;
	const uint32_t field_val = stm32_clk_read_field(config->prescaler_reg);

	/**
	 * NOTE: Prescaler field should be set to all zeroes when disabled!
	 * This allows checking enablement using this cheaper condition.
	 * (Otherwise, we would need to test top bit explicitly).
	 */
	if (field_val == 0) {
		/* Prescaler is disabled (division factor = 1) */
		return clock_get_rate(config->parent);
	}

	/* Index in table is all bits except the top one */
	uint32_t idx = field_val & ((config->prescaler_reg.mask) >> 1);
	uint32_t pre = config->prescaler_shift_table[idx];

	return clock_get_rate(config->parent) >> pre;
}

static int stm32_bus_prescaler_configure(const struct clk *clk, const void *data)
{
	const struct stm32_bus_prescaler_config *config = clk->hw_data;
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

const struct clock_management_driver_api stm32_bus_prescaler_api = {
	.get_rate = stm32_bus_prescaler_get_rate,
	.configure = stm32_bus_prescaler_configure,
#if defined(CONFIG_CLOCK_MANAGEMENT_RUNTIME)
	.notify = /* TBD */,
#endif
#if defined(CONFIG_CLOCK_MANAGEMENT_SET_RATE)
	.round_rate = /* TBD */,
	.set_rate = /* TBD */,
#endif
};

#define CFGNAME(inst) CONCAT(stm32_bus_prescaler_config_, DT_INST_DEP_ORD(inst))

#define ST_BUS_PRESCALER_DEFINE(inst)						\
	const struct stm32_bus_prescaler_config CFGNAME(inst) = {		\
		.parent = CLOCK_DT_GET(DT_INST_PARENT(inst)),			\
		.prescaler_reg = STM32_INST_REG_FIELD(inst, field_offset),	\
		.prescaler_shift_table = COND_CODE_1(DT_INST_PROP(inst, ahbpre),		\
			(ahbpre_to_shift_table), (apbpre_to_shift_table)),	\
	};									\
										\
	CLOCK_DT_INST_DEFINE(inst, &CFGNAME(inst),				\
			     &stm32_bus_prescaler_api);

DT_INST_FOREACH_STATUS_OKAY(ST_BUS_PRESCALER_DEFINE)
