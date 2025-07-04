# Copyright (c) 2024 STMicroelectronics
# SPDX-License-Identifier: Apache-2.0

if (CONFIG_BUILD_WITH_TFM)
	set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
		#Execute post build script postbuild.sh
		COMMAND $<TARGET_PROPERTY:tfm,TFM_BINARY_DIR>/api_ns/postbuild.sh ${CROSS_COMPILE}${CC}
	)
endif()

zephyr_library_sources_ifdef(CONFIG_CLOCK_STM32_MCO ${ZEPHYR_BASE}/drivers/clock_control/clock_stm32_mco.c)

add_subdirectory(common)
add_subdirectory(${SOC_SERIES})
