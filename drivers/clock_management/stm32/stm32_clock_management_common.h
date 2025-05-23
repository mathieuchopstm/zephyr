
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/arch/common/sys_io.h>
#include <zephyr/drivers/clock_management/clock_driver.h>

/**
 * NOTE: this structure is 32-bit sized so it is passed
 * by value as if it was a regular uint32_t.
 */
struct stm32_reg_field {
	/** Offset to target register in RCC */
	uint32_t reg_offset:16;
	/** Position of field's LSB within register */
	uint32_t offset:8;
	/** Mask of field within register */
	uint32_t mask:8;
};

#define RCC_ADDR		DT_REG_ADDR(DT_NODELABEL(rcc))
#define SIZE2MASK(_f_sz)	GENMASK(_f_sz - 1, 0)

#define STM32_REG_FIELD(node, _offset_prop)				\
	{								\
		.reg_offset = (uint16_t)(DT_REG_ADDR(node) - RCC_ADDR),	\
		.mask = (uint8_t)(SIZE2MASK(DT_REG_SIZE(node))),	\
		.offset = (uint8_t)(DT_PROP(node, _offset_prop)),	\
	}

#define STM32_INST_REG_FIELD(inst, _offset_prop)			\
		STM32_REG_FIELD(DT_DRV_INST(inst), _offset_prop)

#if !defined(CONFIG_CLOCK_MANAGEMENT_STM32_INLINE)
uint32_t stm32_clk_read_field(struct stm32_reg_field field);

/**
 * Set the new value for a register field (via Read-Modify-Write)
 */
void stm32_clk_write_field(struct stm32_reg_field field, uint32_t val);

void stm32_clk_poll_field(struct stm32_reg_field field, uint32_t expected);

#else

static inline uint32_t stm32_clk_read_field(const struct stm32_reg_field field)
{
	uint32_t regval = sys_read32(RCC_ADDR + field.reg_offset);

	return (regval >> field.offset) & field.mask;
}

static inline void stm32_clk_write_field(const struct stm32_reg_field field, uint32_t val)
{
	const mem_addr_t addr = RCC_ADDR + field.reg_offset;
	uint32_t regval = sys_read32(addr);

	regval &= ~(field.mask << field.offset);
	regval |= (val << field.offset);

	sys_write32(regval, addr);
}

static inline void stm32_clk_poll_field(struct stm32_reg_field field, uint32_t expected)
{
	const mem_addr_t addr = RCC_ADDR + field.reg_offset;
	const uint32_t mask = field.mask << field.offset;
	const uint32_t value = expected << field.offset;

	while ((sys_read32(addr) & mask) != value) {
		/* ... */
	}
}

#endif
