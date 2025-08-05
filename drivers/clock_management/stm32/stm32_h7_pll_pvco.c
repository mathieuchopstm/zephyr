/*
 * Copyright (c) 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stm32_clock_management_common.h"
#include "clock_management_stm32.h"
#include <zephyr/sys/__assert.h>

#define DT_DRV_COMPAT st_stm32_h7_pll_pvco

#define Z_STM32_gpcp_sysname PLL_PVCO

struct stm32_pll_pvco_config {
	const struct clk *parent;
	struct stm32_reg_field divm_reg;

	struct stm32_reg_field enable_reg;
	struct stm32_reg_field status_reg;
	struct stm32_reg_field vcosel_reg;
	struct stm32_reg_field range_reg;

	struct stm32_reg_field fracen_reg;
	struct stm32_reg_field divn_reg;
	struct stm32_reg_field fracn_reg;
};

static int stm32_pll_pvco_get_rate(const struct clk *hw)
{
	const struct stm32_pll_pvco_config *config = hw->hw_data;

	if (stm32_clk_read_field(config->status_reg) == 0) {
		return 0;
	}

	const uint32_t divm = stm32_clk_read_field(config->divm_reg);

	if (divm == 0) {
		/**
		 * Prescaler is disabled.
		 * This should never be reached as a PLL should always
		 * be disabled if its prescaler is off, but catch this
		 * situation anyways to prevent a division-by-0 later.
		 */
		return 0;
	}

	/**
	 * Adjust DIVN as the field contains the multiplier value minus 1.
	 */
	const uint32_t pllmux_ck = clock_get_rate(config->parent);
	const uint32_t divn = stm32_clk_read_field(config->divn_reg) + 1;

	/**
	 * refx_ck = source clock / DIVM
	 * vcox_ck = refx_ck * (DIVN + (FRACN / 2^13))
	 *         = source_clock / DIVM * (DIVN + (FRACN / 2^13))
	 *	   = vcox_ck_int + vcox_ck_frac
	 * where
	 *	   vcox_ck_int  = refx_ck * DIVN
	 *	   vcox_ck_frac = refx_ck * (FRACN / 2^13)
	 *
	 * For best accuracy (within integer limitations), we should be computing
	 * the multiplication then the division. However, this is not safe as the
	 * result may be larger than 32 bits, and we don't want to use 64-bit math
	 * as that is much slower. Since, in practice, DIVM is often chosen such
	 * that refx_ck is an "exact" value, performing the division first should
	 * not cause inaccuracies in most scenarios.
	 *
	 * Note: worst case error = 0.0063%?
	 */
	const uint32_t refx_ck = pllmux_ck / divm;
	const uint32_t vcox_ck_int = refx_ck * divn;
	uint32_t vcox_ck_frac = 0;

	__ASSERT(IN_RANGE(refx_ck, MHZ(1), MHZ(16)), "refx_ck out of hardware limits!");

	/**
	 * TODO: gate fractional support behind Kconfig?
	 * Rationale: this is expensive for very little benefit...
	 */
	if (stm32_clk_read_field(config->fracen_reg) != 0) {
		/*
		 * We are only computing the fractional part here:
		 *	vcox_ck_frac = Fref_ck * (FRACN / 2^13)
		 *
		 * refx_ck is a 24-bit value (max. 16 MHz) and fracn is
		 * a 13-bit value so the multiplication result is (24+13=)
		 * 37 bits wide and may overflow. Pre-divide refx_ck by 2^5
		 * which drops the 5 low-order bits to make the operation
		 * overflow-safe as the result then fits in (19+13=)32 bits.
		 *
		 * Note: worst-case error from dropping 5 low-order bits is
		 * (((2^5) - 1) / 10^6) = 0.0031%, which should be invisible
		 * because of integer truncation.
		 *
		 * TBD: check that this numerical analysis is correct.
		 */
		const uint32_t fracn = stm32_clk_read_field(config->fracn_reg);

		vcox_ck_frac = ((refx_ck >> 5) * fracn) >> (13 - 5);
	}

	return vcox_ck_int + vcox_ck_frac;
}

static int stm32_pll_pvco_configure(const struct clk *hw, const void *configuration)
{
	const struct stm32_pll_pvco_config *config = hw->hw_data;
	const uint32_t pll_cfg = (uint32_t)configuration;

	/* TODO: -EIO if generator is active */

	const uint32_t divm = Z_STM32_generic_propcell_unpack(pll_cfg, divm);
	const uint32_t vcosel = Z_STM32_generic_propcell_unpack(pll_cfg, vcosel);
	const uint32_t range = Z_STM32_generic_propcell_unpack(pll_cfg, range);
	const uint32_t divn = Z_STM32_generic_propcell_unpack(pll_cfg, divn);
	const uint32_t fracn = Z_STM32_generic_propcell_unpack(pll_cfg, fracn);

	stm32_clk_write_field(config->divm_reg, divm);
	stm32_clk_write_field(config->vcosel_reg, vcosel);
	stm32_clk_write_field(config->range_reg, range);
	stm32_clk_write_field(config->divn_reg, divn);

	if (fracn == 0) {
		/* Disable fractional mode */
		stm32_clk_write_field(config->fracen_reg, 0);
	} else {
		stm32_clk_write_field(config->fracen_reg, 1);
		stm32_clk_write_field(config->fracn_reg, fracn);
	}

	return 0;
}

static int stm32_pll_pvco_off_on(const struct clk *hw, bool enable)
{
	const struct stm32_pll_pvco_config *config = hw->hw_data;

	stm32_clk_write_field(config->enable_reg, !!enable);
	stm32_clk_poll_field(config->status_reg, !!enable);

	return 0;
}

const struct clock_management_driver_api stm32_pll_pvco_api = {
	/* OFF_ON support required */
	.get_rate = stm32_pll_pvco_get_rate,
	.configure = stm32_pll_pvco_configure,
	.off_on = stm32_pll_pvco_off_on,
};

#define CFGNAME(inst) CONCAT(stm32_pll_pvco_config_, DT_INST_DEP_ORD(inst))

#define STM32_PLL_PVCO_DEFINE(inst)						\
	const struct stm32_pll_pvco_config CFGNAME(inst) = {			\
		.parent = CLOCK_DT_GET(DT_INST_PARENT(inst)),			\
		.divm_reg = STM32_INST_REG_FIELD_FROM_PROP(inst, reg_divm),	\
		.enable_reg = STM32_INST_REG_FIELD_FROM_PROP(inst, reg_on),	\
		.status_reg = STM32_INST_REG_FIELD_FROM_PROP(inst, reg_rdy),	\
		.vcosel_reg = STM32_INST_REG_FIELD_FROM_PROP(inst, reg_vcosel),	\
		.range_reg = STM32_INST_REG_FIELD_FROM_PROP(inst, reg_range),	\
		.fracen_reg = STM32_INST_REG_FIELD_FROM_PROP(inst, reg_fracen),	\
		.divn_reg = STM32_INST_REG_FIELD_FROM_PROP(inst, reg_divn),	\
		.fracn_reg = STM32_INST_REG_FIELD_FROM_PROP(inst, reg_fracn),	\
	};									\
										\
	CLOCK_DT_INST_DEFINE(inst, &CFGNAME(inst), &stm32_pll_pvco_api);

DT_INST_FOREACH_STATUS_OKAY(STM32_PLL_PVCO_DEFINE)
