/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/pm/pm.h>

#include <stm32_ll_pwr.h>
#include <stm32_ll_rcc.h>
#include <stm32_ll_rtc.h>

const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

__attribute__((naked)) void __debugbreak(void)
{
	/**
	 * __BKPT() triggers a HardFault if the debugger
	 * is not connected which is annoying. Instead,
	 * use this to stop execution until debugger is
	 * attached and used to step over the `b .`
	 *
	 * Far from perfect, but works well enough...
	 */
	__asm__ volatile(
		"	b	.\n"
		"	bx	lr"
	);
}

void configure_led(const struct gpio_dt_spec *led, unsigned int i)
{
	int res;

	if (!gpio_is_ready_dt(led)) {
		printf("LED %zu: GPIO not ready\n", i);
		return;
	}

	res = gpio_pin_configure_dt(led, GPIO_OUTPUT);
	if (res < 0) {
		printf("failed to configure LED %zu as OUTPUT\n", i);
		return;
	}
}

void configure_gpio(void)
{
	configure_led(&led0, 0);
	configure_led(&led1, 1);
	configure_led(&led2, 2);
}

int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

	configure_gpio();

	for (int i = 0; i < 3; i++) {
		/**
		 * Enable wakeup from Deepstop via SW1 press.
		 * Useful for debugging when something goes wrong.
		 */
		LL_PWR_EnableWakeUpPin(LL_PWR_WAKEUP_PA0);
		LL_PWR_SetWakeUpPinPolarityLow(LL_PWR_WAKEUP_PA0);

		gpio_pin_set_dt(&led0, 1);
		gpio_pin_set_dt(&led1, 0);
		gpio_pin_set_dt(&led2, 0);

		printf("(%d) Entering Deepstop...", i);

		/* Allow UART flushing(?) w/o entry in Deepstop */
		k_yield();

		/* Enable RTC for power layer and wait a bit */
		LL_APB0_GRP1_EnableClock(LL_APB0_GRP1_PERIPH_RTC);
		k_busy_wait(1 * 1000 * 1000);

		/* Enter WAITING state, go idle and enter Deepstop mode */
		k_msleep(5 * 1000);

		/**
		 * GPIO configuration was lost, have to reapply again...
		 */
		configure_gpio();
		gpio_pin_set_dt(&led0, 0);
		gpio_pin_set_dt(&led1, 1);
		gpio_pin_set_dt(&led2, LL_PWR_GetDeepstopSeqFlag() != 0);

		printf(" done! We survived :)\n");
		k_busy_wait(1 * 1000 * 1000);
	}

	printf("--- END OF DEEPSTOP CYCLES TEST ---\n");
	__debugbreak();

	return 0;
}
