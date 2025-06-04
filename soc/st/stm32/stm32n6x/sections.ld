/*
 * Copyright (c) 2024 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if DT_NODE_HAS_STATUS(DT_NODELABEL(mac), okay)

SECTION_DATA_PROLOGUE(eth_stm32,(NOLOAD),)
{
    . = ABSOLUTE(DT_REG_ADDR(DT_NODELABEL(axisram4)));
    *(.eth_stm32_desc_tx)
    . = ABSOLUTE(DT_REG_ADDR(DT_NODELABEL(axisram4))) + 4K;
    *(.eth_stm32_desc_rx)
    . = ABSOLUTE(DT_REG_ADDR(DT_NODELABEL(axisram4))) + 8K;
    *(.eth_stm32_buf)
    . = ABSOLUTE(DT_REG_ADDR(DT_NODELABEL(axisram4))) + 32K;
} GROUP_DATA_LINK_IN(LINKER_DT_NODE_REGION_NAME(DT_NODELABEL(axisram4)), LINKER_DT_NODE_REGION_NAME(DT_NODELABEL(axisram4)))
#endif
