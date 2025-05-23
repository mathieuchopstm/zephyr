/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_CLOCK_MANAGEMENT_DRIVERS_H_
#define ZEPHYR_DRIVERS_CLOCK_MANAGEMENT_DRIVERS_H_

#include <zephyr/devicetree.h>
#include <zephyr/sys/util_macro.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @cond INTERNAL_HIDDEN */

/* Macro definitions for common clock drivers */

#define Z_CLOCK_MANAGEMENT_CLOCK_SOURCE_DATA_DEFINE(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_CLOCK_SOURCE_DATA_GET(node_id, prop, idx)      \
	DT_PHA_BY_IDX(node_id, prop, idx, gate)

#define Z_CLOCK_MANAGEMENT_CLOCK_GATE_DATA_DEFINE(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_CLOCK_GATE_DATA_GET(node_id, prop, idx)		\
	DT_PHA_BY_IDX(node_id, prop, idx, gate_enable)

/* pow2-prescaler: data is value to program in register field (= LOG2(prescaler)) */
#define Z_CLOCK_MANAGEMENT_POW2_PRESCALER_DATA_DEFINE(node_id, prop, idx)	\
		BUILD_ASSERT(IS_POWER_OF_TWO(DT_PHA_BY_IDX(node_id, prop, idx, prescaler)))
#define Z_CLOCK_MANAGEMENT_POW2_PRESCALER_DATA_GET(node_id, prop, idx)		\
		LOG2(DT_PHA_BY_IDX(node_id, prop, idx, prescaler))
/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_CLOCK_MANAGEMENT_DRIVERS_H_ */
