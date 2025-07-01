/*
 * Copyright 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Driver for STM32 internal clock generators: HSE, HSI, LSE, LSI
 */


#include "stm32_clock_management_common.h"

#define DT_DRV_COMPAT st_stm32_internal_clkgen

struct stm32_internal_clkgen_config {
	uint32_t clock_rate;	/* Frequency of generator when enabled */
	uint16_t rcc_offset;	/* Register offset in RCC MMIO range */
	uint8_t enable_offset;	/* Offset of genON bit */
	uint8_t status_offset;	/* Offset of genRDY bit */
	uint8_t bypass_offset;	/* Offset of genBYP bit */
	uint8_t drive_offset;	/* Offset of genDRV field */
	uint8_t drive_mask;	/* Mask for genDRV field */
};

#define BITREG(cfg, bit_name)					\
	{							\
		.reg_offset = (cfg)->rcc_offset,		\
		.offset = (cfg)->CONCAT(bit_name, _offset),	\
		.mask = 0x1,					\
	}

#define FLDREG(cfg, field_name)					\
	{							\
		.reg_offset = (cfg)->rcc_offset,		\
		.offset = (cfg)->CONCAT(field_name, _offset),	\
		.mask = (cfg)->CONCAT(field_name, _mask),	\
	}

static int stm32_internal_clkgen_get_rate(const struct clk *hw)
{
	const struct stm32_internal_clkgen_config *config = hw->hw_data;
	const struct stm32_reg_field status_reg = BITREG(config, status);

	uint32_t regval = stm32_clk_read_field(status_reg);

	return (regval != 0) ? config->clock_rate : 0;
}

static int stm32_internal_clkgen_configure(const struct clk *hw, const void *configuration)
{
	const struct stm32_internal_clkgen_config *config = hw->hw_data;
	const uint32_t clk_config = (uint32_t)configuration;

	/* MCH: sign check is more expensive that compare with 0xFF on CM0+ */
	if (config->bypass_offset != 0xFF) {
		const struct stm32_reg_field bypass_reg = BITREG(config, bypass);
		const uint32_t bypass_en = !!(clk_config & BIT(Z_STM32_CLKGENEX_bypass_SHIFT));

		stm32_clk_write_field(bypass_reg, bypass_en);
	}

	if (config->drive_offset != 0xFF) {
		const struct stm32_reg_field drive_reg = FLDREG(config, drive);
		const uint32_t drive_cfg =
			(clk_config >> Z_STM32_CLKGENEX_driving_capability_SHIFT)
			& drive_reg.mask;

		stm32_clk_write_field(drive_reg, drive_cfg);
	}

	const struct stm32_reg_field enable_reg = BITREG(config, enable);
	const struct stm32_reg_field status_reg = BITREG(config, status);
	const uint32_t enable = !!(clk_config & BIT(Z_STM32_CLKGENEX_enable_SHIFT));

	stm32_clk_write_field(enable_reg, enable);
	stm32_clk_poll_field(status_reg, enable);

	return 0;
}

const struct clock_management_driver_api stm32_intclkgen_api = {
	.get_rate = stm32_internal_clkgen_get_rate,
	.configure = stm32_internal_clkgen_configure,
};

#define CFGNAME(inst)										\
	CONCAT(stm32_internal_clkgen_config_, DT_DEP_ORD(DT_DRV_INST(inst)))

#define STM32_INTCLKGEN_DEFINE(inst)								\
	const struct stm32_internal_clkgen_config CFGNAME(inst) = {				\
		.clock_rate = DT_INST_PROP(inst, clock_frequency),				\
		.rcc_offset = (uint16_t)(DT_INST_PROP(inst, rcc_reg) - RCC_ADDR),		\
		.enable_offset = DT_INST_PROP(inst, enable_offset),				\
		.status_offset = DT_INST_PROP(inst, status_offset),				\
		.bypass_offset = DT_INST_PROP_OR(inst, bypass_offset, 0xFF),			\
		COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, drive_capability_field),		\
		  (.drive_offset = DT_INST_PROP_BY_IDX(inst, drive_capability_field, 0),	\
		   .drive_mask = SIZE2MASK(							\
			DT_INST_PROP_BY_IDX(inst, drive_capability_field, 1))),			\
		  (.drive_offset = 0xFF, .drive_mask = 0)),					\
	};											\
	ROOT_CLOCK_DT_INST_DEFINE(inst,	&CFGNAME(inst), &stm32_intclkgen_api);

DT_INST_FOREACH_STATUS_OKAY(STM32_INTCLKGEN_DEFINE)
