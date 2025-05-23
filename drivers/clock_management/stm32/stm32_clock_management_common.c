/**
 *
 */
#include "stm32_clock_management_common.h"

#include <zephyr/arch/common/sys_io.h>
#include <zephyr/drivers/clock_management.h>

#if !defined(CONFIG_CLOCK_MANAGEMENT_STM32_INLINE)
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
}

void stm32_clk_poll_field(struct stm32_reg_field field, uint32_t expected)
{
	const mem_addr_t addr = RCC_ADDR + field.reg_offset;
	const uint32_t mask = field.mask << field.offset;
	const uint32_t value = expected << field.offset;

	while ((sys_read32(addr) & mask) != value) {
		/* ... */
	}
}
#endif

/* --- */

#define DT_DRV_COMPAT arm_cortex_m0_
CLOCK_MANAGEMENT_DT_INST_DEFINE_OUTPUT(0);

static int stm32_clock_initialize(void)
{
	const struct clock_output *output = CLOCK_MANAGEMENT_DT_INST_GET_OUTPUT(0);
	clock_management_state_t state = CLOCK_MANAGEMENT_DT_INST_GET_STATE(0, default, default);

	int ret = clock_management_apply_state(output, state);

	__ASSERT_NO_MSG(ret >= 0);

	return ret;
}

SYS_INIT(stm32_clock_initialize, PRE_KERNEL_1, 1);
