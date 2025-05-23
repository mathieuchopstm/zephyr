# Copyright 2025 STMicroelectronics
# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources_ifdef(CONFIG_CLOCK_MANAGEMENT_STM32 stm32_clock_management_common.c)

# Header
add_clock_management_header_ifdef(CONFIG_CLOCK_MANAGEMENT_STM32 clock_management_stm32.h)
