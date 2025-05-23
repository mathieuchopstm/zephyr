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

#define Z_CLOCK_MANAGEMENT_ST_STM32_BUS_PRESCALER_DATA_DEFINE(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_ST_STM32_BUS_PRESCALER_DATA_GET(node_id, prop, idx)		\
	DT_PHA_BY_IDX(node_id, prop, idx, prescaler)

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_CLOCK_MANAGEMENT_STM32_CLOCK_MANAGEMENT_STM32_H_ */
