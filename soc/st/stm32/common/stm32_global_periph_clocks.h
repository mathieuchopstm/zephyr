
#ifndef ZEPHYR_INCLUDE_SOC_ST_STM32_GLOBAL_PERIPH_CLOCKS_H_
#define ZEPHYR_INCLUDE_SOC_ST_STM32_GLOBAL_PERIPH_CLOCKS_H_

#include <soc.h>
#include <zephyr/kernel.h>

/**
 * Global peripheral identifier
 *
 * @internal
 * Keep the list in increasing order starting from zero.
 * The list must not go over 32 entries (0~31).
 * @endinternal
 */
enum stm32_global_peripheral_id {
	/** Power controller */
	STM32_GLOBAL_PERIPH_PWR = 0,

	/**
	 * System Configuration controller
	 *
	 * This controller is called SBS instead on certain series.
	 */
	STM32_GLOBAL_PERIPH_SYSCFG,

	STM32_GLOBAL_PERIPH_NUM,
};

#if defined(CONFIG_STM32_GLOBAL_CLOCKS_RUNTIME_GATING)
/*
 * When runtime clock gating is enabled, the RCC callback used
 * to enable all global peripheral clocks becomes a no-op.
 */
#define stm32_global_periph_enable_all_clocks(...)

/**
 * @brief Add reference to global peripheral @p periph_id
 *
 * After a call to this function, the clock of @p periph_id
 * is guaranteed to be enabled and its registers can be
 * accessed.
 *
 * @internal
 * If the current reference count for @p periph_id is 0,
 * the corresponding clock is enabled; then, the reference
 * count for @p periph_id is incremented unconditionally.
 * @endinternal
 */
static inline void stm32_global_periph_refer(enum stm32_global_peripheral_id periph_id) {
	extern void _stm32_global_periph_refer(enum stm32_global_peripheral_id periph_id);

	if (CONFIG_STM32_GLOBAL_CLOCKS_ALWAYS_ON_MASK & (1U << (unsigned int)periph_id)) {
		/* Don't do anything for always-on clocks */
		return;
	}

	_stm32_global_periph_refer(periph_id);
}

/**
 * @brief Release reference to global peripheral @p periph_id
 *
 * Each call to @ref stm32_global_periph_refer should be
 * matched with a call to this function. After calling
 * this function, the caller is no longer allowed to
 * access the registers of @p periph_id
 *
 * @internal
 * Decrements the reference count for @p periph_id then
 * disables the corresponding clock is enabled if the
 * reference count has become 0.
 * @endinternal
 */
void stm32_global_periph_release(enum stm32_global_peripheral_id periph_id) { //OR: "_unref"?
	extern void _stm32_global_periph_refer(enum stm32_global_peripheral_id periph_id);

	if (CONFIG_STM32_GLOBAL_CLOCKS_ALWAYS_ON_MASK & (1U << (unsigned int)periph_id)) {
		/* Don't do anything for always-on clocks */
		return;
	}

	_stm32_global_periph_release(periph_id);
}
#else /* !CONFIG_STM32_GLOBAL_CLOCKS_RUNTIME_GATING */
/*
 * When runtime clock gating is disabled, the RCC callback becomes
 * an actual function but the rest is turned into no-ops.
 */

/**
 * @note This function is reserved for usage by the RCC driver.
 *       No other driver is allowed to invoke it!
 */
int stm32_global_periph_enable_all_clocks(void);
#define stm32_global_periph_refer(...)
#define stm32_global_periph_release(...)
#endif /* CONFIG_STM32_GLOBAL_CLOCKS_RUNTIME_GATING */

/**
 * TODO: on certain series, there are certain clocks that don't exist!
 * Could we determine this and make the refer()/release() calls no-ops
 * on these series?
 */

#endif /* ZEPHYR_INCLUDE_SOC_ST_STM32_GLOBAL_CLOCKS_H_ */
