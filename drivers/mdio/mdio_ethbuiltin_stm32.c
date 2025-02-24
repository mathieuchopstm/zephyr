/**
 * Copyright (c) 2025 STMicroelectronics
 *
 * Driver for the built-in MDIO controller
 * of the STM32 Ethernet controller.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <zephyr/sys/check.h>
#include <zephyr/sys/util.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/mdio.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mdio_stm32_hal, CONFIG_MDIO_LOG_LEVEL);

/* Include HAL headers */
#include <soc.h>

/* Include ETH driver private header and its dependencies */
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/net/net_stats.h>
#include "ethernet/eth_stm32_hal_priv.h"

struct eth_mdio_stm32_data {
	struct k_sem lock;
	ETH_HandleTypeDef *heth;
};

struct eth_mdio_stm32_config {
	const struct pinctrl_dev_config *pinctrl;
	const struct device *eth_ctrl;
	struct stm32_pclken clk;
};

static int hal2errno(HAL_StatusTypeDef status)
{
	/**
	 * NOTE: could be made faster by using a LUT
	 */
	switch (status) {
	case HAL_OK:
		return 0;
	case HAL_ERROR:
		return -EIO;
	case HAL_BUSY:
		/**
		 * TODO: mdio_{read/write} are not documented
		 * as being allowed to return this value
		 */
		return -EBUSY;
	case HAL_TIMEOUT:
		return -ETIMEDOUT;
	default:
		/* CODE_UNREACHABLE; */
		return -EIO;
	}
}

int eth_mdio_stm32_read(const struct device *dev, uint8_t phy_address,
				uint8_t reg_address, uint16_t *pdata)
{
	struct eth_mdio_stm32_data *data = dev->data;
	HAL_StatusTypeDef res;
	uint32_t val = 0;

	ARG_UNUSED(phy_address); /* unused if !HAL_V2 */

	k_sem_take(&data->lock, K_FOREVER);

#if defined(CONFIG_ETH_STM32_HAL_API_V2)
	res = HAL_ETH_ReadPHYRegister(data->heth, phy_address, reg_address, &val);
#else
	CHECKIF(phy_address != data->heth->Init.PhyAddress) {
		LOG_ERR("Wrong phy_address (%d) != hEth->Init.PhyAddress (%d)",
			phy_address, data->heth->Init.PhyAddress);
	}

	res = HAL_ETH_ReadPHYRegister(data->heth, reg_address, &val);
#endif /* CONFIG_ETH_STM32_HAL_API_V2 */

	k_sem_give(&data->lock);

	*pdata = val;
	return hal2errno(res);
}

int eth_mdio_stm32_write(const struct device *dev, uint8_t phy_address,
				uint8_t reg_address, uint16_t wdata)
{
	struct eth_mdio_stm32_data *data = dev->data;
	HAL_StatusTypeDef res;

	ARG_UNUSED(phy_address); /* unused if !HAL_V2 */

	k_sem_take(&data->lock, K_FOREVER);

#if defined(CONFIG_ETH_STM32_HAL_API_V2)
	res = HAL_ETH_WritePHYRegister(data->heth, phy_address, reg_address, wdata);
#else
	CHECKIF(phy_address != data->heth->Init.PhyAddress) {
		LOG_ERR("wrong phy_address (%d) != hEth->Init.PhyAddress (%d)",
			phy_address, data->heth->Init.PhyAddress);
	}

	res = HAL_ETH_WritePHYRegister(data->heth, reg_address, wdata);
#endif /* CONFIG_ETH_STM32_HAL_API_V2 */

	k_sem_give(&data->lock);

	return hal2errno(res);
}

int eth_mdio_stm32_init(const struct device *dev)
{
	const struct device *rcc = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);
	const struct eth_mdio_stm32_config *config = dev->config;
	struct eth_mdio_stm32_data *data = dev->data;
	struct eth_stm32_hal_dev_data *eth_data = config->eth_ctrl->data;
	int ret;

	/* Initialize driver lock */
	k_sem_init(&data->lock, 1, 1);

	/**
	 * Obtain pointer to hEth of Ethernet driver instance.
	 * (Could be done at compile time, but would require
	 *  tightly coupling this driver and the Ethernet one)
	 */
	data->heth = &eth_data->heth;

	/**
	 * The built-in MDIO registers are part of the MAC's MMIO region.
	 * Enable the MAC clock right now to allow usage of MDIO properly.
	 */
	ret = clock_control_on(rcc, (clock_control_subsys_t)&config->clk);
	if (ret < 0) {
		return ret;
	}

	/**
	 * Configure pins associated to MDIO signals.
	 * They are required so ENOENT is an error here.
	 */
	ret = pinctrl_apply_state(config->pinctrl, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

static DEVICE_API(mdio, eth_mdio_stm32_api) = {
	.read = eth_mdio_stm32_read,
	.write = eth_mdio_stm32_write,
};

/**
 * Generate unique {data/config} name from @p node_id - usually, we'd use
 * the instance number, but it is not known here, so use ordinal instead.
 */
#define DRV_DATA_NAME(node_id)	_CONCAT(eth_mdio_stm32_data_o, DT_DEP_ORD(node_id))
#define DRV_CFG_NAME(node_id)	_CONCAT(eth_mdio_stm32_config_o, DT_DEP_ORD(node_id))

/**
 * Instantiate node with ID @p mdio_id as a built-in MDIO controller.
 * @p eth_ctrl_id is the node ID of the ETH controller @p mdio_id is built into.
 */
#define ETH_MDIO_STM32_DEVICE(mdio_id, eth_ctrl_id)				\
	BUILD_ASSERT(DT_NUM_CLOCKS(mdio_id) == 1,				\
		DT_NODE_FULL_NAME(mdio_id) " should have only one `clocks`");	\
										\
	PINCTRL_DT_DEFINE(mdio_id);						\
										\
	static struct eth_mdio_stm32_data DRV_DATA_NAME(mdio_id) = {		\
		/* Initialized at runtime */					\
	};									\
	static const struct eth_mdio_stm32_config DRV_CFG_NAME(mdio_id) = {	\
		.pinctrl = PINCTRL_DT_DEV_CONFIG_GET(mdio_id),			\
		.eth_ctrl = DEVICE_DT_GET(eth_ctrl_id),			\
		.clk = STM32_CLOCK_INFO(0, mdio_id),				\
	};									\
	DEVICE_DT_DEFINE(mdio_id, &eth_mdio_stm32_init, NULL,			\
			&DRV_DATA_NAME(mdio_id), &DRV_CFG_NAME(mdio_id),	\
			POST_KERNEL, CONFIG_MDIO_INIT_PRIORITY,			\
			&eth_mdio_stm32_api);

/**
 * Given the node ID of an Ethernet controller, obtains the node ID
 * of the bus device that the ETH controller's PHY is attached to.
 *
 * @param eth_node_id Node ID of a "ethernet-controller" device
 */
#define ETH_PHY_BUS(eth_node_id)	DT_BUS(DT_PHANDLE(eth_node_id, phy_handle))

/**
 * Same as @ref{DT_SAME_NODE} but expands to a literal 0 or 1.
 *
 * NOTE: Due to IS_EQ limitations, this may break if too many
 * nodes exist in DTS.
 */
#define NODES_EQUAL_COMPTIME(_a, _b)	IS_EQ(DT_DEP_ORD(_a), DT_DEP_ORD(_b))

/**
 * Compatible instantiated by this driver, and compatible
 * of the Ethernet controllers it can be built into.
 */
#define DT_DRV_COMPAT	st_stm32_ethernet_mdio
#define ETH_CTRL_COMPAT	st_stm32_ethernet

/**
 * Expands to 1 if node with ID @p eth is an Ethernet controller
 * with a built-in MDIO that should be instantiated.
 *
 * The conditions are:
 *	- @p eth has selected a PHY (`phy-handle` exists)
 *	- The selected PHY is attached to an active built-in MDIO
 *	- The built-in MDIO and @p eth have the same parent
 *
 * The these conditions come from the expected DT layout:
 *
 * @code{dts}
 * ethernet@xxx {
 * 	mac {   //@p eth is the node ID of this
 * 		phy-handle = <&eth_phy>;
 *	}
 * 	mdio {
 * 		eth_phy: ethernet-phy@xxx { }
 * 	}
 * }
 * @endcode
 */
#define ETH_HAS_MDIO_TO_INSTANTIATE(eth)			\
	UTIL_AND(DT_NODE_HAS_PROP(eth, phy_handle),		\
	UTIL_AND(DT_NODE_HAS_COMPAT_STATUS(			\
			ETH_PHY_BUS(eth), DT_DRV_COMPAT, okay),	\
		NODES_EQUAL_COMPTIME(				\
			DT_PARENT(eth),				\
			DT_PARENT(ETH_PHY_BUS(eth)))		\
	))

/**
 * Instantiates MDIO device if @p eth_node_id is an Ethernet
 * controller whose `phy_handle` points to a PHY attached to
 * the built-in MDIO of the controller.
 */
#define INSTANTIATE_BUILTIN_MDIO(eth_node_id)			\
	IF_ENABLED(ETH_HAS_MDIO_TO_INSTANTIATE(eth_node_id),	\
		(ETH_MDIO_STM32_DEVICE(				\
			ETH_PHY_BUS(eth_node_id), eth_node_id)) \
	)

/**
 * NOTE: there is no need to include e.g. "st,stm32h7-ethernet" as
 * long as all Ethernet controller nodes in DTSI are marked with
 * "st,stm32-ethernet" in addition to their compatible.
 */
DT_FOREACH_STATUS_OKAY(ETH_CTRL_COMPAT, INSTANTIATE_BUILTIN_MDIO);

/**
 * Assert that at least one instance was created.
 * This driver should never get enabled if an instance is not
 * required but might still be by accident - break build if so.
 */
#define WRP(x) ETH_HAS_MDIO_TO_INSTANTIATE(x) +
BUILD_ASSERT((DT_FOREACH_STATUS_OKAY(ETH_CTRL_COMPAT, WRP) 0) > 0,
	"STM32 Ethernet MAC must be enabled to use this driver");
