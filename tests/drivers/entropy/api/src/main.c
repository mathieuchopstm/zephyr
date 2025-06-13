/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/drivers/entropy.h>
#include <zephyr/ztest.h>

/*
 * @addtogroup t_entropy_api
 * @{
 * @defgroup t_entropy_get_entropy test_entropy_get_entropy
 * @brief TestPurpose: verify Get entropy works
 * @details
 * - Test Steps
 *   -# Read random numbers from Entropy driver.
 *   -# Verify whether buffer overflow occurred or not.
 *   -# Verify whether buffer completely filled or not.
 * - Expected Results
 *   -# Random number should be generated.
 * @}
 */

#define BUFFER_LENGTH           1024
#define RECHECK_RANDOM_ENTROPY  0x10

#ifdef CONFIG_RANDOM_BUFFER_NOCACHED
__attribute__((__section__(".nocache")))
static uint8_t entropy_buffer[BUFFER_LENGTH] = {0};
#else
static uint8_t entropy_buffer[BUFFER_LENGTH] = {0};
#endif

#define USE_ISR 1

#if 0
void __trng_backdoor(void) {
	static bool ran = false;
	if (!ran) {
		ran = true;

		/* TODO: trigger NMI? */
		const struct device *rng = DEVICE_DT_GET(DT_CHOSEN(zephyr_entropy));
		uint8_t xxx[32];
		int howmany = entropy_get_entropy_isr(rng, xxx, sizeof(xxx), ENTROPY_BUSYWAIT);

		if (howmany < 0) {
			/* get mad */
			__ASSERT_NO_MSG(0);
		}

		printk("recursive call returns %d\n", howmany);
		for (int i = 0; i < howmany; i++) {
			printk("0x%02hhX ", xxx[i]);
		}
		printk("\n");
	}
}
#endif

int ege_wrp(const struct device *dev, void *buffer, uint32_t len)
{
	if (USE_ISR) {
		return entropy_get_entropy_isr(dev, buffer, len, ENTROPY_BUSYWAIT);
	} else {
		return entropy_get_entropy(dev, buffer, len);
	}
}

static int random_entropy(const struct device *dev, char *buffer, char num)
{
	int ret, i;
	int count = 0;

	(void)memset(buffer, num, BUFFER_LENGTH);

	uint64_t pre = k_cycle_get_64();

	/* The BUFFER_LENGTH-1 is used so the driver will not
	 * write the last byte of the buffer. If that last
	 * byte is not 0 on return it means the driver wrote
	 * outside the passed buffer, and that should never
	 * happen.
	 */
	ret = ege_wrp(dev, buffer, BUFFER_LENGTH - 1);

	uint64_t post = k_cycle_get_64();

	if (ret < 0) {
		TC_PRINT("Error: entropy_get_entropy%s failed: %d\n",
			USE_ISR ? "_isr" : "", ret);
		return TC_FAIL;
	}
	if (buffer[BUFFER_LENGTH - 1] != num) {
		TC_PRINT("Error: entropy_get_entropy buffer overflow\n");
		return TC_FAIL;
	}

	uint64_t time = k_cyc_to_ns_floor64(post - pre);

	TC_PRINT("Buffer of size %u filled in %llu ns (%llu ns/byte)\n",
		BUFFER_LENGTH - 1, time, time / (BUFFER_LENGTH - 1));

	for (i = 0; i < BUFFER_LENGTH - 1; i++) {
		TC_PRINT("  0x%02x", buffer[i]);
		if ((i+1) % 16 == 0) {
			TC_PRINT("\n");
		}
		if (buffer[i] == num) {
			count++;
		}
	}
	TC_PRINT("\n");

	if (count >= BUFFER_LENGTH / 10) {
		return RECHECK_RANDOM_ENTROPY;
	} else {
		return TC_PASS;
	}
}

/*
 * Function invokes the get_entropy callback in driver
 * to get the random data and fill to passed buffer
 */
static int get_entropy(void)
{
	const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_entropy));
	int ret;

	if (!device_is_ready(dev)) {
		TC_PRINT("error: random device not ready\n");
		return TC_FAIL;
	}

	TC_PRINT("random device is %p, name is %s\n",
		 dev, dev->name);

	ret = random_entropy(dev, entropy_buffer, 0);

	/* Check whether 20% or more of buffer still filled with default
	 * value(0), if yes then recheck again by filling nonzero value(0xa5)
	 * to buffer. Recheck random_entropy and verify whether 20% or more
	 * of buffer filled with value(0xa5) or not.
	 */
	if (ret == RECHECK_RANDOM_ENTROPY) {
		ret = random_entropy(dev, entropy_buffer, 0xa5);
		if (ret == RECHECK_RANDOM_ENTROPY) {
			return TC_FAIL;
		} else {
			return ret;
		}
	}
	return ret;
}

ZTEST(entropy_api, test_entropy_get_entropy)
{
	TC_PRINT("Using API %s\n", USE_ISR ? "entropy_get_entropy_isr" : "entropy_get_entropy");
	zassert_true(get_entropy() == TC_PASS);
}

void *entropy_api_setup(void)
{
#ifdef CONFIG_BT
	bt_enable(NULL);
#endif /* CONFIG_BT */

	return NULL;
}

ZTEST_SUITE(entropy_api, NULL, entropy_api_setup, NULL, NULL, NULL);
