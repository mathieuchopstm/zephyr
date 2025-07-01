/**
 *
 */
#include "stm32_clock_management_common.h"

#include <stm32_ll_utils.h>
#include <zephyr/sys/math_extras.h>

#include <zephyr/devicetree.h>
#include <zephyr/arch/common/sys_io.h>
#include <zephyr/drivers/clock_management.h>

#if !defined(CONFIG_CLOCK_MANAGEMENT_STM32_INLINE)
/**
 * Common helper functions, called by other STM32 clock drivers.
 * By having a unified implementation, we reduce the ROM footprint,
 * at a slight performance cost (?). This can be opt'ed-out by
 * enabling CONFIG_CLOCK_MANAGEMENT_STM32_INLINE for benchmarking
 * purposes. This implementation and the inline versions in the
 * header should be kept in sync for that reason.
 */
uint32_t stm32_clk_read_field(const struct stm32_reg_field field)
{
	uint32_t regval = sys_read32(RCC_ADDR + field.reg_offset);

	return (regval >> field.offset) & field.mask;
}

void stm32_clk_write_field(const struct stm32_reg_field field, uint32_t val)
{
	/* Assert value is in valid range for this field */
	__ASSERT_NO_MSG((val & ~((uint32_t)field.mask)) == 0);

	/* MCH: this should be under IRQ (and HSEM) lock... */
	const mem_addr_t addr = RCC_ADDR + field.reg_offset;
	uint32_t regval = sys_read32(addr);

	regval &= ~(field.mask << field.offset);
	regval |= (val << field.offset);

	sys_write32(regval, addr);

	/**
	 * Return only once we are use the value has been written
	 * and taken into account by reading back the register.
	 * RCC will block until the operation has been completed
	 * if we wrote to e.g., peripheral enable register.
	 */
	(void)sys_read32(addr);
}

void stm32_clk_poll_field(struct stm32_reg_field field, uint32_t expected)
{
	/* Assert value is in valid range for this field */
	__ASSERT_NO_MSG((expected & ~((uint32_t)field.mask)) == 0);

	const mem_addr_t addr = RCC_ADDR + field.reg_offset;
	const uint32_t mask = field.mask << field.offset;
	const uint32_t value = expected << field.offset;

	while ((sys_read32(addr) & mask) != value) {
		/* Poll until we see what we want */
	}
}
#endif

/**
 * Static clock configuration routine
 *
 * This is equivalent to stm32_clock_control_init except it uses clock states.
 * More specifically, it applies the one and only clock state on the SoC's CPU.
 * This should enable all generators and configure the "upper part" of the clock
 * tree - lower level nodes such as peripheral clock gates should be managed in
 * drivers instead, just like with clock_control.
 *
 * There is one HW-specific thing we have to handle: setting the proper flash
 * latency. Even though the subsystem was not designed with this in mind, we
 * can (ab?)use it to reduce the footprint of this routine, by using what
 * the subsystem provides. The main issue is that the subsystem is supposed
 * to hide the clock tree inner details, but here we actually use this as
 * prior knowledge to write things that are more efficient.
 */

/* helper DT macros that could be made available generally */
#define CLOCK_MANAGEMENT_DT_GET_STATE_NODE(dev_id, state_name, output_name)	\
	DT_PHANDLE_BY_IDX(dev_id, CONCAT(clock_state_,				\
		DT_CLOCK_STATE_NAME_IDX(dev_id, state_name)),			\
		DT_CLOCK_OUTPUT_NAME_IDX(dev_id, output_name))

#define CLOCK_MANAGEMENT_DT_INST_GET_STATE_NODE(inst, state_name, output_name)	\
	CLOCK_MANAGEMENT_DT_GET_STATE_NODE(DT_DRV_INST(inst), state_name, output_name)


/**
 * Declare ourselves as compatible with the CPU node
 * (NOTE: this doesn't work on multicore)
 */
#define DT_DRV_COMPAT IDENTITY(DT_STRING_TOKEN_BY_IDX(DT_PATH(cpus, cpu_0), compatible, 0))

/* Define the "clock-output" for CPU node */
CLOCK_MANAGEMENT_DT_INST_DEFINE_OUTPUT(0);

static int stm32_clock_initialize(void)
{
	const struct clock_output *output = CLOCK_MANAGEMENT_DT_INST_GET_OUTPUT(0);
	clock_management_state_t state = CLOCK_MANAGEMENT_DT_INST_GET_STATE(0, default, default);
	int ret;

#if !defined(FLASH_ACR_LATENCY)
	/**
	 * No flash latency setting to worry about.
	 * Just apply the clock state and return.
	 */
	ret = clock_management_apply_state(output, state);
#else /* !defined(FLASH_ACR_LATENCY) */
	/**
	 * In order to know whether the flash latency must be modified,
	 * and when to do so, we have to check whether the configuration
	 * we'll apply will increase, decrease or leave unchanged the
	 * clock frequency of the bus on which the flash is attached.
	 * To this end, the SYSCLK is calculated by using the function
	 * HAL_RCC_GetSysClockFreq(), then divided using AHBPrescTable
	 * which is very inefficient in terms of footprint (the former
	 * is 112 bytes on C0, the latter uses u32 unnecessarily).
	 *
	 * Now that the clock tree is known by the device, we can just
	 * leverage the subsystem to obtain this information for "free"
	 * To avoid an otherwise unnecessary "clock-output", we are
	 * querying the AHBCLK device directly. On paper, this violates
	 * the API boundaries since `clock_XXX` are private, but this
	 * is just leveraging prior knowledge which IMO is acceptable.
	 *
	 * Computing the post-configuration-applied frequency is more
	 * gory. All information we need is in the Device Tree, but
	 * we have no way to cleanly extract it as far as I can tell!
	 * Something like "DT_PHA_FOR_PHANDLE()" would be required.
	 * Pretend it exists by hardcoding the index of PHA we're
	 * interested in, a.k.a. the AHB prescaler value. From this,
	 * we can apply the same calculation as the HAL macro,
	 * except we'll borrow the shift table from the bus prescaler
	 * driver to save ROM.
	 */
#	define AHBPRE_INDEX_IN_STATE	3 /* index of &ahbpre phandle in clock state DT array */
#	define STATIC_CONFIG_STATE	CLOCK_MANAGEMENT_DT_INST_GET_STATE_NODE(0, default, default)
#	define AHBPRE_STATE_PRESCALER	DT_PHA_BY_IDX(STATIC_CONFIG_STATE, clocks, \
							AHBPRE_INDEX_IN_STATE, prescaler)
	BUILD_ASSERT(DT_SAME_NODE(DT_NODELABEL(hclk),
			DT_PHANDLE_BY_IDX(STATIC_CONFIG_STATE, clocks, AHBPRE_INDEX_IN_STATE)),
			"AHBPRE_INDEX_IN_STATE is wrong");

	extern const uint8_t ahbpre_to_shift_table[];
	const struct clk *ahbclk = CLOCK_DT_GET(DT_NODELABEL(hclk));

	uint32_t old_flash_freq = clock_get_rate(ahbclk);
	uint32_t new_flash_freq = CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;

	if (AHBPRE_STATE_PRESCALER != 0) {
		/**
		 * c.f. logic in stm32_bus_prescaler
		 * ==> strip the top bit but without consuming DT
		 * With `clock_configure_recalc()` we wouldn't need this...
		 *
		 * TODO: assert this works as it should...
		 */
		const uint32_t index = AHBPRE_STATE_PRESCALER &
				~(BIT(LOG2CEIL(AHBPRE_STATE_PRESCALER)));
		new_flash_freq >>= ahbpre_to_shift_table[index];
	}

	/**
	 * Compare current and post-apply flash frequency.
	 * If new frequency is higher, we must increase the
	 * flash wait states before switching to make sure
	 * we never read invalid data from the flash. On
	 * the other hand, if the new frequency is lower,
	 * we can reduce the flash wait states after the
	 * switch to remove unnecessary delays.
	 *
	 * NOTE: it is cheaper to have duplicate calls
	 * inside the various if{} cases of a single
	 * block compared to a common path with multiple
	 * if{} blocks around it.
	 *
	 * MCH: We can shave 12 bytes off this function
	 * by replacing "old < new" with "old <= new".
	 * However, this would not be at feature parity
	 * with the existing clock control, as that will
	 * skip LL_SetFlashLatency() if both are equal.
	 */
	if (old_flash_freq < new_flash_freq) {
		LL_SetFlashLatency(new_flash_freq);
		ret = clock_management_apply_state(output, state);
	} else if (new_flash_freq < old_flash_freq) {
		ret = clock_management_apply_state(output, state);
		LL_SetFlashLatency(new_flash_freq);
	} else {
		/* Both frequencies equal - just apply the state */
		ret = clock_management_apply_state(output, state);
	}
#endif /* !defined(FLASH_ACR_LATENCY) */
	__ASSERT_NO_MSG(ret >= 0);

	return ret;
}

SYS_INIT(stm32_clock_initialize, PRE_KERNEL_1, 1);
