/*
 * Copyright (c) 2025 STMicroelectronics.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * STM32WB0 Deepstop implementation for Power Management framework
 *
 * TODO:
 *	- document the control flow on PM transitions
 *	- assertions around system configuration
 *	  (e.g., valid slow clock selected, RTC enabled, ...)
 *	- ...
 */
#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include <zephyr/sys_clock.h>
#include <zephyr/init.h>
#include <zephyr/arch/common/pm_s2ram.h>

/* Private headers in zephyr/drivers/... */
#include <timer/cortex_m_systick.h>
#include <clock_control/clock_stm32_ll_common.h>

#include <soc.h>
#include <stm32_ll_pwr.h>
#include <stm32_ll_rcc.h>
#include <stm32_ll_rtc.h>
#include <stm32_ll_cortex.h>
#include <stm32_ll_system.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(soc, CONFIG_SOC_LOG_LEVEL);

#if defined(CONFIG_SOC_STM32WB05XX) || defined(CONFIG_SOC_STM32WB09XX)
#define HAS_GPIO_RETENTION 1
#else
#define HAS_GPIO_RETENTION 0
#endif /* CONFIG_SOC_STM32WB05XX || CONFIG_SOC_STM32WB09XX */

/**
 * Cortex-M SysTick low-power timer hooks implementation
 */
static uint32_t pre_deepstop_seconds, pre_deepstop_subseconds;


/* Converts from RTC's BCD format to plain integer */
static uint32_t bcd2int(uint32_t bcd)
{
	return (bcd & 0xF0) * 10 + (bcd & 0xF);
}

#if !defined(CONFIG_BT) /* REMOVE "|| 1" TO TRANSFER RESPONSIBILITY TO BLE DRIVER */ || 1

void z_cms_lptim_hook_on_lpm_entry(uint64_t max_lpm_time_us)
{
	/**
	 * Memorize the current time seen in RTC to be able to calculate
	 * how much time has passed when we wake-up from Deepstop.
	 *
	 * SSR, TR and DR must be read in this order to ensure that the
	 * shadow registers do not stay locked in case they are enabled.
	 */
	pre_deepstop_subseconds = RTC->SSR;
	pre_deepstop_seconds = bcd2int(__LL_RTC_GET_SECOND(RTC->TR));
	(void)RTC->DR;

	/**
	 * The wakeup timer clock can be (assuming 32768Hz LSE):
	 *
	 * |--------------|----------|----------|------------|
	 * |    SOURCE    | MIN TIME | MAX TIME | RESOLUTION |
	 * |--------------|----------|----------|------------|
	 * |  RTCCLK / 2  |  122 µs  |    4 s   |    62 µs   |
	 * |  RTCCLK / 4  |  122 µs  |    8 s   |   123 µs   |
	 * |  RTCCLK / 8  |  244 µs  |   16 s   |   245 µs   |
	 * |  RTCCLK /16  |  488 µs  |   32 s   |   489 µs   |
	 * |   CLK_SPRE   |    1 s   |  ~18 h   |     1 s    |
	 * | CLK_SPRE ext |  ~18 h   |  ~36 h   |     1 s    |
	 * |--------------|----------|----------|------------|
	 *
	 * NOTE: resolutions are rounded up to prevent oversleep.
	 *
	 * In the current implementation, CLK_SPRE is not supported.
	 *
	 * TODO: if LSI source, get frequency? or just consider 24kHZ?
	 *
	 * With 24kHz LSI, the minimum sleep time is ~167µs, a value
	 * we should never see: the Deepstop sequence is slower than
	 * this, so the PM framework should never request entry if
	 * the wait is smaller or equal to that.
	 */

	const uint32_t COUNTER_MAX = 65536;
	const uint32_t MAX_SLEEP_TIME = 32 * USEC_PER_SEC;

	uint32_t clock_source, counter;

	/**
	 * Assert that the PM subsystem is not trying to enter
	 * Deepstop mode for a duration too short to be measured.
	 */
	__ASSERT_NO_MSG(max_lpm_time_us >= /* minimal sleep time */167);

	/**
	 * Note: truncation to 32-bit is performed because
	 * 64-bit division is very expensive on Cortex-M0+.
	 */
	if (max_lpm_time_us <= (4 * USEC_PER_SEC)) {
		clock_source = LL_RTC_WAKEUPCLOCK_DIV_2;
		counter = ((uint32_t)max_lpm_time_us / 62);
	} else if (max_lpm_time_us <= (8 * USEC_PER_SEC)) {
		clock_source = LL_RTC_WAKEUPCLOCK_DIV_4;
		counter = ((uint32_t)max_lpm_time_us / 123);
	} else if (max_lpm_time_us <= (16 * USEC_PER_SEC)) {
		clock_source = LL_RTC_WAKEUPCLOCK_DIV_8;
		counter = ((uint32_t)max_lpm_time_us / 245);
	} else if (max_lpm_time_us <= MAX_SLEEP_TIME) {
		clock_source = LL_RTC_WAKEUPCLOCK_DIV_16;
		counter = ((uint32_t)max_lpm_time_us / 489);
	} else {
		/* Overflow: setup timer to max value with RTCCLK_DIV16 */
		clock_source = LL_RTC_WAKEUPCLOCK_DIV_16;
		counter = COUNTER_MAX;
	}

	/**
	 * Autoreload is one less than the number of ticks to sleep.
	 */
	__ASSERT_NO_MSG(counter > 0);
	counter -= 1;

	/* Unlock RTC registers */
	LL_RTC_DisableWriteProtection(RTC);

	/**
	 * Disable the wake-up timer as we cannot configure
	 * it while active, then apply new configuration.
	 */
	LL_RTC_WAKEUP_Disable(RTC);
	while (!LL_RTC_IsActiveFlag_WUTW(RTC)) {
		/* Wait until RTC Wake-up Timer is disabled */
	}

	LL_RTC_WAKEUP_SetClock(RTC, clock_source);
	while (!LL_RTC_IsActiveFlag_WUTW(RTC)) {
		/* Wait until clock configuration is applied */
	}

	LL_RTC_WAKEUP_SetAutoReload(RTC, counter);
	while (!LL_RTC_IsActiveFlag_WUTW(RTC)) {
		/* Wait until auto-reload counter is configured */
	}

	/* Clear RTC Wake-up flag (if set) to allow entry in Deepstop */
	LL_RTC_ClearFlag_WUT(RTC);

	/* Start Wake-up timer and lock back RTC */
	LL_RTC_WAKEUP_Enable(RTC);
	LL_RTC_EnableWriteProtection(RTC);

	/* Enable Wake-up by RTC at PWRC level */
	LL_PWR_EnableInternWU();
}

uint64_t z_cms_lptim_hook_on_lpm_exit(void)
{
	uint32_t post_tr, post_deepstop_subseconds;
	bool shadow_bypass_enabled;

	/* Unlock RTC registers */
	LL_RTC_DisableWriteProtection(RTC);

	/* Disable shadow RTC registers */
	shadow_bypass_enabled = !!LL_RTC_IsShadowRegBypassEnabled(RTC);
	if (!shadow_bypass_enabled) {
		LL_RTC_EnableShadowRegBypass(RTC);
	}

	while (!LL_RTC_IsShadowRegBypassEnabled(RTC)) {
		/* Wait for Shadow Bypass to be enabled */
	}

	/** Read post-reset time registers */
	post_deepstop_subseconds = RTC->SSR;
	post_tr = RTC->TR;
	(void)RTC->DR;

	/* Enable back shadow registers if they were on */
	if (!shadow_bypass_enabled) {
		LL_RTC_DisableShadowRegBypass(RTC);
	}

	/* Lock bacl RTC registers */
	LL_RTC_EnableWriteProtection(RTC);

	/** Calculate how much time elapsed */
	const uint32_t spre = (LL_RTC_GetSynchPrescaler(RTC) + 1);
	const uint32_t post_deepstop_seconds = bcd2int(__LL_RTC_GET_SECOND(post_tr));
	uint32_t elapsed_sec, elapsed_ssr;

	/**
	 * N.B.: support for minutes/hours could be added
	 * by following the same logic as found here but
	 * with new steps.
	 */
	if (post_deepstop_seconds >= pre_deepstop_seconds) {
		elapsed_sec = post_deepstop_seconds - pre_deepstop_seconds;
	} else {
		elapsed_sec = (SEC_PER_MIN - pre_deepstop_seconds + post_deepstop_seconds);
	}

	/**
	 * N.B.: calculation order is reversed because
	 * the subsecond counter is a downcounter
	 */
	if (post_deepstop_subseconds <= pre_deepstop_subseconds) {
		/* Time advanced by (K.f) seconds */
		elapsed_ssr = pre_deepstop_subseconds - post_deepstop_subseconds;
	} else {
		/**
		 * Time advanced by (K - 0.f) seconds.
		 * Calculate sub-second delta using wraparound
		 * and subtract the fractional second from delta.
		 */
		__ASSERT_NO_MSG(elapsed_sec > 0);
		elapsed_ssr = spre - post_deepstop_subseconds + pre_deepstop_subseconds;
		elapsed_sec -= 1;
	}

	__ASSERT_NO_MSG(elapsed_ssr < spre);

	/**
	 * Calculate amount of time elapsed using the seconds delta,
	 * then adding the second fraction calculated using formula:
	 *	sf = (PREDIV_S - SSR) / (PREDIV_S + 1) in seconds
	 *
	 * Premultiply by USEC_PER_SEC to obtain result in µs instead.
	 */
	uint32_t elapsed_us = elapsed_sec * USEC_PER_SEC;

	elapsed_us += USEC_PER_SEC * ((spre - 1) - elapsed_ssr) / (spre);

	return (uint64_t)elapsed_us;
}

#endif /* !CONFIG_BT */

/**
 * System-level state managed by PM callbacks
 *
 * Things that need to be preserved across Deepstop, but
 * have no associated driver to backup and restore them.
 */
#define SRAM		DT_CHOSEN(zephyr_sram)
#define BL_STK_SIZ	(20 * 4)	/* in bytes */
#define BL_STK_TOP	((void *)(DT_REG_ADDR(SRAM) + DT_REG_SIZE(SRAM) - BL_STK_SIZ))
static uint8_t bl_stk_area_backup[BL_STK_SIZ];

static void save_system_level_state(void)
{
	/**
	 * The STM32WB0 bootloader uses the end of SRAM as stack.
	 * Since it is executed on every reset, including wakeup
	 * from Deepstop, any data placed at the end of SRAM would
	 * be corrupted.
	 *
	 * Backup these words for later restoration to avoid data
	 * corruption. A much better solution would mark this part
	 * of SRAM as unusable, but no easy solution was found to
	 * achieve this.
	 */
	memcpy(bl_stk_area_backup, BL_STK_TOP, BL_STK_SIZ);
}

static void restore_system_level_state(void)
{
	/* Restore bootloader stack area */
	memcpy(BL_STK_TOP, bl_stk_area_backup, BL_STK_SIZ);
}

/**
 * Callback for arch_pm_s2ram_suspend
 */
static int suspend_system_to_deepstop(void)
{
	/* Enable SLEEPDEEP to allow entry in Deepstop */
	LL_LPM_EnableDeepSleep();

	/* Complete all memory transactions */
	__DSB();

	/* Attempt entry in Deepstop */
	__WFI();

	/**
	 * Make sure no meaningful instruction is
	 * executed during the two cycles latency
	 * it takes to power-gate the CPU.
	 */
	__NOP();
	__NOP();

	/**
	 * This code is reached only if the device did not
	 * enter Deepstop mode (e.g., because an interrupt
	 * became pending during preparatory work).
	 *
	 * Disable SLEEPDEEP and return the appropriate error.
	 */
	LL_LPM_EnableSleep();

	return -EBUSY;
}

/**
 * Backup system state to save and configure power
 * controller before entry in Deepstop mode
 */
static void prepare_for_deepstop_entry(void)
{
	/**
	 * DEEPSTOP2 configuration is performed in familiy-wide code
	 * instead of here (see `soc/st/stm32/common/soc_config.c`).
	 *
	 * RAMRET configuration is performed once during SoC init,
	 * since it is retained across Deepstop (see `soc.c`).
	 **/

	/* Clear wakeup reason flags (which inhibit Deepstop) */
	LL_PWR_ClearWakeupSource(LL_PWR_WAKEUP_ALL);
	LL_SYSCFG_PWRC_ClearIT(LL_SYSCFG_PWRC_WKUP);
	LL_PWR_ClearDeepstopSeqFlag();

#if HAS_GPIO_RETENTION
	/**
	 * Enable GPIO state retention in Deepstop if available.
	 *
	 * Do not enable this if low-power mode debugging has been
	 * enabled via Kconfig, because it prevents the debugger
	 * from staying connected to the SoC.
	 */
	if (!IS_ENABLED(CONFIG_STM32_ENABLE_DEBUG_SLEEP_STOP)) {
		LL_PWR_EnableGPIORET();
		LL_PWR_EnableDBGRET();
	}
#endif /* HAS_GPIO_RETENTION */

#if !defined(CONFIG_BT)
	/**
	 * RM0505/RM0529/RM0530 §5.4.2 "Deepstop mode":
	 *
	 * If the MR_BLE is not used at all by the SoC (or not yet started),
	 * the following steps need to be done after any reset to allow low
	 * power modes (Deepstop and Shutdown):
	 * – Enable the MR_BLE clock by setting the RCC_APB2ENR.MRBLEEN bit
	 * – Set the BLE_SLEEP_REQUEST_MODE.FORCE_SLEEPING bit inside the
	 *   Wakeup block of the MR_BLE to have the MR_BLE IP requesting
	 *   low power mode to the SoC
	 * – Gate again the MR_BLE clock by clearing the RCC_APB2ENR.MRBLEEN bit
	 *
	 * N.B.: this assume MR_BLE is used if and only if CONFIG_BT is enabled.
	 */
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_MRBLE);
	while (!LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_MRBLE)) {
		/* Wait until IP clock is enabled */
	}

	LL_RADIO_TIMER_EnableBLEWakeupTimerForceSleeping(WAKEUP);

	LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_MRBLE);
	while (LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_MRBLE)) {
		/* Wait until IP clock is disabled */
	}
#endif /* !CONFIG_BT */

	save_system_level_state();
}

/**
 * @brief	Restore SoC-level configuration lost in Deepstop
 * @note	This function must be called right after wakeup.
 */
static void post_resume_configuration(void)
{
	__ASSERT_NO_MSG(LL_PWR_GetDeepstopSeqFlag() == 1);

	/**
	 * VTOR has been reset to its default value: restore it.
	 * (Note that RAM_VR.AppBase was filled during SoC init)
	 */
	SCB->VTOR = RAM_VR.AppBase;

	/**
	 * RCC has been reset: perform clock configuration again.
	 */
	(void)stm32_clock_control_init(NULL);

	/**
	 * Restore other miscellanous system-level things.
	 */
	restore_system_level_state();
}

/**
 * Power Management subsystem callbacks
 */
void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	/* Ignore substate: STM32WB0 has only one low-power mode */
	ARG_UNUSED(substate_id);

	if (state != PM_STATE_SUSPEND_TO_RAM) {
		/**
		 * Deepstop is a suspend-to-RAM state.
		 * Something is wrong if a different
		 * power state has been requested.
		 */
		LOG_ERR("Unsupported power state %u", state);

	}

	prepare_for_deepstop_entry();

	/* Select Deepstop low-power mode and suspend system */
	LL_PWR_SetPowerMode(LL_PWR_MODE_DEEPSTOP);

	int res = arch_pm_s2ram_suspend(suspend_system_to_deepstop);

	if (res >= 0) {
		/**
		 * Restore system configuration only if the SoC actually
		 * entered Deepstop - otherwise, no state has been lost
		 * and it would be a waste of time to do so.
		 */
		post_resume_configuration();
	}

	/* Disable RTC wake-up timer and clear associated flag */
	LL_RTC_DisableWriteProtection(RTC);
	LL_RTC_WAKEUP_Disable(RTC);
	LL_RTC_ClearFlag_WUT(RTC);
	LL_RTC_EnableWriteProtection(RTC);
}

void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(state);
	ARG_UNUSED(substate_id);

	/**
	 * We restore system state in @ref{post_resume_configuration}.
	 * The only thing we may have to do is release GPIO retention,
	 * which we have not done yet because we wanted the driver to
	 * restore all configuration first.
	 *
	 * We also need to enable IRQs to fullfill the API contract.
	 */
#if HAS_GPIO_RETENTION
	LL_PWR_DisableGPIORET();
	LL_PWR_DisableDBGRET();
#endif /* HAS_GPIO_RETENTION */

	__enable_irq();
}

/**
 * Ugly hack to make sure that RTC is running
 * by the time the PM framework may invoke us
 *
 * Note that RTC clock remains enabled across
 * reset (for obvious reasons), so we only
 * need to do this once per power cycle.
 *
 * BUG: This doesn't assert that slow clock
 * tree is valid and uses a source that remains
 * active in low-power mode.
 */
int force_enable_rtc(void)
{
	if (!LL_APB0_GRP1_IsEnabledClock(LL_APB0_GRP1_PERIPH_RTC)) {
		LL_APB0_GRP1_EnableClock(LL_APB0_GRP1_PERIPH_RTC);

		/**
		 * It takes 2 slow clock cycles for RTC clock to be enabled
		 * but we have no bit to poll while waiting for this to
		 * occur, so we'll just k_busy_wait().
		 *
		 * Slow clock is guaranteed to be at least 24kHz (LSI) so
		 * the longest this can take is:
		 *	t = (2 cycles / 240000 Hz) ≅ 84µs
		 */
		k_busy_wait(84);
	}

	return 0;
}

SYS_INIT(force_enable_rtc, PRE_KERNEL_1, 0);
