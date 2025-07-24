
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

#define STM32_REG_FIELD_INIT(_reg_addr, _field_off, _field_sz)		\
	{								\
		.reg_offset = (uint16_t)(_reg_addr - RCC_ADDR),		\
		.mask = (uint8_t)(SIZE2MASK(_field_sz)),		\
		.offset = (uint8_t)(_field_off),			\
	}

/**
 * Transform property @p prop of @p node into struct stm32_reg_field.
 *
 * Property is an array with two or three elements:
 *	[0]: address of RCC register
 *	[1]: offset to first bit (LSB) of field
 *	[2]: number of bits in field
 * A default field size of 1 (single-bit field) is assumed when the
 * property has only two elements.
 */
#define STM32_NODE_REG_FIELD_FROM_PROP(node, prop_name)			\
	STM32_REG_FIELD_INIT(						\
		DT_PROP_BY_IDX(node, prop_name, 0),			\
		DT_PROP_BY_IDX(node, prop_name, 1),			\
		COND_CODE_1(DT_PROP_HAS_IDX(node, prop_name, 2),	\
			(DT_PROP_BY_IDX(node, prop_name, 2)),		\
			(1))						\
		)

/**
 * Transform `reg-and-field` property of @p node into stm32_reg_field.
 *
 * Property is an array with two or three elements:
 *	[0]: address of RCC register
 *	[1]: offset to first bit (LSB) of field
 *	[2]: number of bits in field
 * A default field size of 1 (single-bit field) is assumed when the
 * property has only two elements.
 */
#define STM32_NODE_REG_FIELD(node)					\
	STM32_NODE_REG_FIELD_FROM_PROP(node, reg_and_field)

#define STM32_NODE_REG_BIT(node, _bit_prop_name)			\
	STM32_REG_FIELD_INIT(						\
		DT_PROP(node, rcc_reg), DT_PROP(node, _bit_prop_name), 1)

#define STM32_INST_REG_FIELD_FROM_PROP(inst, prop_name)			\
	STM32_NODE_REG_FIELD_FROM_PROP(DT_DRV_INST(inst), prop_name)

#define STM32_INST_REG_FIELD(inst)					\
	STM32_NODE_REG_FIELD(DT_DRV_INST(inst))

#define STM32_INST_REG_BIT(inst, _bit_prop_name)			\
	STM32_NODE_REG_BIT(DT_DRV_INST(inst), _bit_prop_name)

#if !defined(CONFIG_CLOCK_MANAGEMENT_STM32_INLINE)
uint32_t stm32_clk_read_field(struct stm32_reg_field field);

/**
 * Set the new value for a register field (via Read-Modify-Write)
 *
 * @warning @p val is written verbatim; caller is responsible for
 * ensuring no bits absent from @p field.mask are set in @p val
 */
void stm32_clk_write_field(struct stm32_reg_field field, uint32_t val);

/**
 * Poll register field until it has a specific value.
 *
 * @warning @p expected is compared verbatim; caller is responsible
 * for ensuring no bits absent from @p field.mask are set in it
 */
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
