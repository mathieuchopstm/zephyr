/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/sys/iterable_sections.h>

#define SAMPLE_NO (109 + 0)

/* The data represent a sine wave (440 Hz when sampled @ 48 kHz) */
static const int16_t data[SAMPLE_NO] = {
         0,   1888,   3769,   5638,   7489,   9314,  11108,  12866,
     14581,  16247,  17859,  19412,  20901,  22320,  23665,  24932,
     26115,  27212,  28218,  29131,  29947,  30663,  31278,  31789,
     32194,  32492,  32682,  32764,  32736,  32600,  32356,  32004,
     31546,  30984,  30318,  29551,  28687,  27727,  26675,  25534,
     24308,  23002,  21620,  20165,  18644,  17060,  15420,  13729,
     11992,  10215,   8405,   6566,   4706,   2830,    944,   -944,
     -2830,  -4706,  -6566,  -8405, -10215, -11992, -13729, -15420,
    -17060, -18644, -20165, -21620, -23002, -24308, -25534, -26675,
    -27727, -28687, -29551, -30318, -30984, -31546, -32004, -32356,
    -32600, -32736, -32764, -32682, -32492, -32194, -31789, -31278,
    -30663, -29947, -29131, -28218, -27212, -26115, -24932, -23665,
    -22320, -20901, -19412, -17859, -16247, -14581, -12866, -11108,
     -9314,  -7489,  -5638,  -3769,  -1888,
};

/* Fill buffer with sine wave on left channel, and sine wave shifted by
 * 90 degrees on right channel. "att" represents a power of two to attenuate
 * the samples by
 */
static void fill_buf(int16_t *tx_block, int att)
{
	int r_idx;

	for (int i = 0; i < SAMPLE_NO; i++) {
		/* Left channel is sine wave */
		tx_block[2 * i] = data[i] / (1u << 2);
		/* Right channel is same sine wave, shifted by 90 degrees */
		tx_block[2 * i + 1] = tx_block[2 * i];
//		r_idx = (i + (ARRAY_SIZE(data) / 4)) % ARRAY_SIZE(data);
//		tx_block[2 * i + 1] = data[r_idx] / (1 << att);
	}
}

#define NUM_BLOCKS 20
#define BLOCK_SIZE (2 * sizeof(data))

#ifdef CONFIG_NOCACHE_MEMORY
	#define MEM_SLAB_CACHE_ATTR __nocache
#else
	#define MEM_SLAB_CACHE_ATTR
#endif /* CONFIG_NOCACHE_MEMORY */

static char MEM_SLAB_CACHE_ATTR __aligned(WB_UP(32))
	_k_mem_slab_buf_tx_0_mem_slab[(NUM_BLOCKS) * WB_UP(BLOCK_SIZE)];

static STRUCT_SECTION_ITERABLE(k_mem_slab, tx_0_mem_slab) =
	Z_MEM_SLAB_INITIALIZER(tx_0_mem_slab, _k_mem_slab_buf_tx_0_mem_slab,
				WB_UP(BLOCK_SIZE), NUM_BLOCKS);

int main(void)
{
	void *tx_block[NUM_BLOCKS];
	struct i2s_config i2s_cfg;
	int ret;
	uint32_t tx_idx;
	const struct device *dev_i2s = DEVICE_DT_GET(DT_ALIAS(i2s_tx));

	if (!device_is_ready(dev_i2s)) {
		printf("I2S device not ready\n");
		return -ENODEV;
	}
	/* Configure I2S stream */
	i2s_cfg.word_size = 16U;
	i2s_cfg.channels = 2U;
	i2s_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
	i2s_cfg.frame_clk_freq = 48000;
	i2s_cfg.block_size = BLOCK_SIZE;
	i2s_cfg.timeout = 2000;
	/* Configure the Transmit port as Controller */
	i2s_cfg.options = I2S_OPT_FRAME_CLK_CONTROLLER
			| I2S_OPT_BIT_CLK_CONTROLLER;
	i2s_cfg.mem_slab = &tx_0_mem_slab;
	ret = i2s_configure(dev_i2s, I2S_DIR_TX, &i2s_cfg);
	if (ret < 0) {
		printf("Failed to configure I2S stream\n");
		return ret;
	}

	/* Prepare all TX blocks */
	for (tx_idx = 0; tx_idx < NUM_BLOCKS; tx_idx++) {
		ret = k_mem_slab_alloc(&tx_0_mem_slab, &tx_block[tx_idx],
				       K_FOREVER);
		if (ret < 0) {
			printf("Failed to allocate TX block\n");
			return ret;
		}
		fill_buf((uint16_t *)tx_block[tx_idx], tx_idx % 3);
	}

	tx_idx = 0;
	/* Send first block */
	ret = i2s_write(dev_i2s, tx_block[tx_idx++], BLOCK_SIZE);
	if (ret < 0) {
		printf("Could not write TX buffer %d\n", tx_idx);
		return ret;
	}
	/* Trigger the I2S transmission */
	ret = i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_START);
	if (ret < 0) {
		printf("Could not trigger I2S tx\n");
		return ret;
	}

	for (; tx_idx < NUM_BLOCKS; ) {
		ret = i2s_write(dev_i2s, tx_block[tx_idx++], BLOCK_SIZE);
		if (ret < 0) {
			printf("Could not write TX buffer %d\n", tx_idx);
			return ret;
		}

		if (tx_idx == NUM_BLOCKS) {
			tx_idx = 0;
		}
	}
	/* Drain TX queue */
	ret = i2s_trigger(dev_i2s, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
	if (ret < 0) {
		printf("Could not trigger I2S tx\n");
		return ret;
	}
	printf("All I2S blocks written\n");
	return 0;
}
