/*
 * Copyright 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_CLOCK_MANAGEMENT_GENERIC_H_
#define ZEPHYR_DRIVERS_CLOCK_MANAGEMENT_GENERIC_H_

#include <zephyr/devicetree.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @cond INTERNAL_HIDDEN */

/* Macro definitions for generic clock drivers */
	/* fixed-prescaler has clock-cells = <0> */

#define Z_CLOCK_MANAGEMENT_CLOCK_GATE_DATA_DEFINE(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_CLOCK_GATE_DATA_GET(node_id, prop, idx)		\
	DT_PHA_BY_IDX(node_id, prop, idx, gate_enable)

#define Z_CLOCK_MANAGEMENT_CLOCK_MUX_DATA_DEFINE(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_CLOCK_MUX_DATA_GET(node_id, prop, idx)		\
	DT_PHA_BY_IDX(node_id, prop, idx, input_selection)

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_CLOCK_MANAGEMENT_GENERIC_H_ */
