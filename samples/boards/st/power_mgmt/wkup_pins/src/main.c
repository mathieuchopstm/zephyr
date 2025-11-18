/*
 * Copyright (c) 2024 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/dt-bindings/gpio/stm32-gpio.h>

#include <zephyr/pm/pm.h>
#include <stm32_ll_bus.h>
#include <stm32_ll_pwr.h>

/* 0: Standby | 1: Poweroff */
#define POWEROFF 0

#define WAIT_TIME_US 4000000

#define WKUP_SRC_NODE DT_ALIAS(wkup_src)
#if !DT_NODE_HAS_STATUS_OKAY(WKUP_SRC_NODE)
#error "Unsupported board: wkup_src devicetree alias is not defined"
#endif

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(WKUP_SRC_NODE, gpios);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

int main(void)
{
	while (1) {
		printk("\nWake-up button is connected to %s pin %d\n", button.port->name, button.pin);

		__ASSERT_NO_MSG(gpio_is_ready_dt(&led));
		gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
		gpio_pin_set(led.port, led.pin, 1);

		/* Setup button GPIO pin as a source for exiting Poweroff */
		gpio_pin_configure_dt(&button, GPIO_INPUT | STM32_GPIO_WKUP);
		printk("Will wait %ds before powering the system off\n", (WAIT_TIME_US / 1000000));
		k_busy_wait(WAIT_TIME_US);

		printk("Powering off\n");
		printk("Press the user button to power the system on\n\n");

	if (POWEROFF) {
		printk("POWEROFF\n");
		sys_poweroff();
		return 0; // NEVER REACHED
	} /* else: S2RAM */
	printk("STANDBY\n");
	pm_state_set(PM_STATE_SUSPEND_TO_RAM, 0U);

	/*
	 * Re-enable GPIO controller clock
	 * (needed because we are bypassing PM)
	 *
	 * NOTE: after wake-up, UART is dead!
	 * rely on LEDs for proper behavior
	 * observation :)
	 */
	LL_AHB2_GRP1_EnableClock(
		LL_AHB2_GRP1_PERIPH_GPIOA |
		LL_AHB2_GRP1_PERIPH_GPIOB |
		LL_AHB2_GRP1_PERIPH_GPIOC
#ifdef CONFIG_SOC_STM32WBA65XX
		| LL_AHB2_GRP1_PERIPH_GPIOD
#endif
	);

	// enable the second LED
	gpio_pin_configure_dt(&led2, GPIO_OUTPUT_ACTIVE);
	k_busy_wait(WAIT_TIME_US);
	// then turn it off
	gpio_pin_set_dt(&led2, 0);

	//do it all over again
	//
	}

	return 0;
}
