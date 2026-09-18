/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 STMicroelectronics
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/entropy.h>
#include <zephyr/ztest.h>

extern const char *z_stm32_entropy_reentrancy_case;

static const struct device *trng_dev = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_entropy));

static uint8_t random_buffer_nested[CONFIG_TEST_RANDOM_REQUEST_SIZE];
static uint8_t random_buffer_outer[CONFIG_TEST_RANDOM_REQUEST_SIZE];

static void dump_buffer(const char *header, const uint8_t *buf)
{
	TC_PRINT("%s\n", header);
	for (int i = 0; i < CONFIG_TEST_RANDOM_REQUEST_SIZE; i++) {
		TC_PRINT("%02x ", buf[i]);
		if ((i + 1) % 16 == 0) {
			TC_PRINT("\n");
		}
	}
	TC_PRINT("\n");
}

static void nmi_handler(void)
{
	TC_PRINT("NMI triggered from %s\n", z_stm32_entropy_reentrancy_case);

	int res = entropy_get_entropy_isr(
		trng_dev,
		random_buffer_nested,
		CONFIG_TEST_RANDOM_REQUEST_SIZE,
		ENTROPY_BUSYWAIT);

	zassert_equal(res, CONFIG_TEST_RANDOM_REQUEST_SIZE,
		"Failed to get entropy in NMI handler: %d", res);

	dump_buffer("Entropy obtained from nested NMI:", random_buffer_nested);
}

static void *install_nmi_handler(void)
{
	z_arm_nmi_set_handler(nmi_handler);
	return NULL;
}

ZTEST(stm32_trng_reentrancy, test_trng_reentrancy)
{
	zassert_not_null(trng_dev, "TRNG device not found\n");

	memset(random_buffer_outer, 0, CONFIG_TEST_RANDOM_REQUEST_SIZE);
	memset(random_buffer_nested, 0, CONFIG_TEST_RANDOM_REQUEST_SIZE);
	dump_buffer("Initial outer random buffer:", random_buffer_outer);
	dump_buffer("Initial nested random buffer:", random_buffer_nested);

	int res = entropy_get_entropy_isr(
		trng_dev,
		random_buffer_outer,
		CONFIG_TEST_RANDOM_REQUEST_SIZE,
		ENTROPY_BUSYWAIT);

	zassert_equal(res, CONFIG_TEST_RANDOM_REQUEST_SIZE,
		"Failed to get entropy in test: %d", res);

	dump_buffer("Outer entropy obtained:", random_buffer_outer);
}

ZTEST_SUITE(stm32_trng_reentrancy, NULL, install_nmi_handler, NULL, NULL, NULL);
