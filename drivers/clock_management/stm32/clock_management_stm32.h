/*
 * Copyright 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_CLOCK_MANAGEMENT_STM32_CLOCK_MANAGEMENT_STM32_H_
#define ZEPHYR_DRIVERS_CLOCK_MANAGEMENT_STM32_CLOCK_MANAGEMENT_STM32_H_

#include <zephyr/devicetree.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @cond INTERNAL_HIDDEN */

/**
 * Bits for "st,stm32-clkgenex" configuration
 * [   00] Enable
 *	0: Disable generator
 *	1: Enable generator
 * [   01] Bypass enable
 *   Used for external oscillators only (HSE/LSE).
 *	0: Regular mode (Xtal)
 *	1: Bypass mode
 * [27:02] <not used>
 * [31:28] LSE drive capability
 *	Refer to target SoC's Reference Manual for details
 *	about the meaning of this field. 4 bits are reserved
 *	at software level, but quantity used depends on HW.
 */
#define Z_STM32_CLKGENEX_enable_SHIFT			0
#define Z_STM32_CLKGENEX_enable_MASK			0x1
#define Z_STM32_CLKGENEX_bypass_SHIFT			1
#define Z_STM32_CLKGENEX_bypass_MASK			0x1
#define Z_STM32_CLKGENEX_driving_capability_SHIFT	28
#define Z_STM32_CLKGENEX_driving_capability_MASK	0xF

#define Z_STM32_CLKGENEX_PROP_FMT(prop_val, cell_name)				\
	(((prop_val) & CONCAT(Z_STM32_CLKGENEX_, cell_name, _MASK))		\
		<< CONCAT(Z_STM32_CLKGENEX_, cell_name, _SHIFT))

#define Z_STM32_CLKGENEX_PROP_EXTRACT(node_id, prop, idx, cell_name)		\
	Z_STM32_CLKGENEX_PROP_FMT(						\
		DT_PHA_BY_IDX_OR(node_id, prop, idx, cell_name, 0), cell_name)

#define Z_STM32_CLKGENEX_NODEPROP_EXTRACT(node_id, prop)			\
	Z_STM32_CLKGENEX_PROP_FMT(DT_PROP_OR(node_id, prop, 0), prop)

#define Z_CLOCK_MANAGEMENT_ST_STM32_INTERNAL_CLKGEN_DATA_DEFINE(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_ST_STM32_INTERNAL_CLKGEN_DATA_GET(node_id, prop, idx)	\
	(Z_STM32_CLKGENEX_PROP_EXTRACT(node_id, prop, idx, enable) |		\
	Z_STM32_CLKGENEX_PROP_EXTRACT(node_id, prop, idx, bypass) |		\
	Z_STM32_CLKGENEX_PROP_EXTRACT(node_id, prop, idx, driving_capability))

#define Z_CLOCK_MANAGEMENT_ST_STM32_INTERNAL_CLKGEN_INIT_DATA_GET(node_id)	\
	(Z_STM32_CLKGENEX_NODEPROP_EXTRACT(node_id, enable) |			\
	 Z_STM32_CLKGENEX_NODEPROP_EXTRACT(node_id, bypass) |			\
	 Z_STM32_CLKGENEX_NODEPROP_EXTRACT(node_id, driving_capability))

#define Z_CLOCK_MANAGEMENT_ST_STM32_HSE_DATA_DEFINE(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_ST_STM32_HSE_DATA_GET(node_id, prop, idx)		\
	Z_CLOCK_MANAGEMENT_ST_STM32_INTERNAL_CLKGEN_DATA_GET(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_ST_STM32_HSE_INIT_DATA_GET(node_id)			\
	Z_CLOCK_MANAGEMENT_ST_STM32_INTERNAL_CLKGEN_INIT_DATA_GET(node_id)

#define Z_CLOCK_MANAGEMENT_ST_STM32_LSE_DATA_DEFINE(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_ST_STM32_LSE_DATA_GET(node_id, prop, idx)		\
	Z_CLOCK_MANAGEMENT_ST_STM32_INTERNAL_CLKGEN_DATA_GET(node_id, prop, idx)

#define Z_CLOCK_MANAGEMENT_ST_STM32_BUS_PRESCALER_DATA_DEFINE(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_ST_STM32_BUS_PRESCALER_DATA_GET(node_id, prop, idx)		\
	DT_PHA_BY_IDX(node_id, prop, idx, prescaler)
#define Z_CLOCK_MANAGEMENT_ST_STM32_BUS_PRESCALER_INIT_DATA_GET(node_id)		\
	DT_PROP(node_id, prescaler)

#define Z_CLOCK_MANAGEMENT_ST_STM32_CLOCK_GATE_DATA_DEFINE(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_ST_STM32_CLOCK_GATE_DATA_GET(node_id, prop, idx)		\
	DT_PHA_BY_IDX(node_id, prop, idx, enable)

#define Z_CLOCK_MANAGEMENT_ST_STM32_CLOCK_GENERATOR_DATA_DEFINE(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_ST_STM32_CLOCK_GENERATOR_DATA_GET(node_id, prop, idx)	\
		DT_PHA_BY_IDX(node_id, prop, idx, enable)

#define PHANDLE_IDX_PLUS_ONE_IF_TARGET(node_id, prop, idx, target)	\
	(DT_SAME_NODE(DT_PROP_BY_IDX(node_id, prop, idx), target) ? (idx + 1) : 0)

#define PHANDLE_IDX_BY_NODE(node_id, prop, target)		\
	((DT_FOREACH_PROP_ELEM_SEP_VARGS(node_id, prop,	\
		PHANDLE_IDX_PLUS_ONE_IF_TARGET, (+), target)) - 1)

#define Z_CLOCK_MANAGEMENT_ST_STM32_CLOCK_MULTIPLEXER_DATA_DEFINE(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_ST_STM32_CLOCK_MULTIPLEXER_DATA_GET(node_id, prop, idx)		\
		DT_PHA_BY_IDX(node_id, prop, idx, input_selection)
#define Z_CLOCK_MANAGEMENT_ST_STM32_CLOCK_MULTIPLEXER_INIT_DATA_GET(node_id)		\
		PHANDLE_IDX_BY_NODE(node_id, inputs, DT_PROP(node_id, input_selection))

/* Prescaler field contains one less than the desired division factor */
#define Z_CLOCK_MANAGEMENT_ST_STM32_SYSCLK_PRESCALER_DATA_DEFINE(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_ST_STM32_SYSCLK_PRESCALER_DATA_GET(node_id, prop, idx)	\
		(DT_PHA_BY_IDX(node_id, prop, idx, prescaler) - 1)
#define Z_CLOCK_MANAGEMENT_ST_STM32_SYSCLK_PRESCALER_INIT_DATA_GET(node_id)	\
		(DT_PROP(node_id, prescaler) - 1)

#define Z_CLOCK_MANAGEMENT_ST_STM32C0_HSISYS_DIV_DATA_DEFINE(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_ST_STM32C0_HSISYS_DIV_DATA_GET(node_id, prop, idx)		\
		(DT_PHA_BY_IDX(node_id, prop, idx, prescaler))

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_CLOCK_MANAGEMENT_STM32_CLOCK_MANAGEMENT_STM32_H_ */
