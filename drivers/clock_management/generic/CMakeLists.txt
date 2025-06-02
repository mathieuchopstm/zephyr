# Copyright 2025 STMicroelectronics
# SPDX-License-Identifier: Apache-2.0

add_clock_management_header(clock_management_generic.h)

zephyr_library_sources_ifdef(CONFIG_CLOCK_MANAGEMENT_GATE clock_gate.c)
