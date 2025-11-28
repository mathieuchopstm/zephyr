/*
 * Copyright (c) 2025 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(usbd_sample_config);

#define ZEPHYR_PROJECT_USB_VID		0x2fe3

/* How many ports are supported by the sample */
#define MAX_PORTS_NUM 10

/*
 * Common descriptors shared by all devices
 */
USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, CONFIG_SAMPLE_USBD_MANUFACTURER);
IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(sample_sn)));

USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "FS Configuration");
USBD_DESC_CONFIG_DEFINE(hs_cfg_desc, "HS Configuration");

static const uint8_t attributes = (IS_ENABLED(CONFIG_SAMPLE_USBD_SELF_POWERED) ?
				   USB_SCD_SELF_POWERED : 0) |
				  (IS_ENABLED(CONFIG_SAMPLE_USBD_REMOTE_WAKEUP) ?
				   USB_SCD_REMOTE_WAKEUP : 0);

/* Full speed configuration */
USBD_CONFIGURATION_DEFINE(sample_fs_config,
			  attributes,
			  CONFIG_SAMPLE_USBD_MAX_POWER, &fs_cfg_desc);

/* High speed configuration */
USBD_CONFIGURATION_DEFINE(sample_hs_config,
			  attributes,
			  CONFIG_SAMPLE_USBD_MAX_POWER, &hs_cfg_desc);

#if CONFIG_SAMPLE_USBD_20_EXTENSION_DESC
/*
 * This does not yet provide valuable information, but rather serves as an
 * example, and will be improved in the future.
 */
static const struct usb_bos_capability_lpm bos_cap_lpm = {
	.bLength = sizeof(struct usb_bos_capability_lpm),
	.bDescriptorType = USB_DESC_DEVICE_CAPABILITY,
	.bDevCapabilityType = USB_BOS_CAPABILITY_EXTENSION,
	.bmAttributes = 0UL,
};

USBD_DESC_BOS_DEFINE(sample_usbext, sizeof(bos_cap_lpm), &bos_cap_lpm);
#endif

#define CDC_ACM_DEFINE(node, udc_n)					\
	USBD_DEVICE_DEFINE(cdc_acm_serial ## udc_n,			\
			   DEVICE_DT_GET(node),				\
			   ZEPHYR_PROJECT_USB_VID,			\
			   CONFIG_SAMPLE_USBD_PID);			\
	USBD_DESC_PRODUCT_DEFINE(cdc_acm_serial ## udc_n ## _product,	\
		CONFIG_SAMPLE_USBD_PRODUCT " on zephyr_udc" # udc_n);	\

#define CDC_ACM_DEFINE_IF_EXISTS(udc_n, _)				\
	IF_ENABLED(DT_NODE_EXISTS(DT_NODELABEL(zephyr_udc ## udc_n)),	\
		(CDC_ACM_DEFINE(DT_NODELABEL(zephyr_udc ## udc_n), udc_n)))

#define CDC_ACM_GET_IF_EXISTS(udc_n, _)					\
	COND_CODE_1(DT_NODE_EXISTS(DT_NODELABEL(zephyr_udc ## udc_n)),	\
			(&cdc_acm_serial ## udc_n), (NULL))

#define CDC_ACM_PRODUCT_IF_EXISTS(udc_n, _)				 \
	COND_CODE_1(DT_NODE_EXISTS(DT_NODELABEL(zephyr_udc ## udc_n)),	\
			(&cdc_acm_serial ## udc_n ## _product), (NULL))

//TODO: port number
LISTIFY(MAX_PORTS_NUM, CDC_ACM_DEFINE_IF_EXISTS, (;));

struct usbd_context *const contexts[MAX_PORTS_NUM] = {
	LISTIFY(MAX_PORTS_NUM, CDC_ACM_GET_IF_EXISTS, (,))
};

struct usbd_desc_node *const products[MAX_PORTS_NUM] = {
	LISTIFY(MAX_PORTS_NUM, CDC_ACM_PRODUCT_IF_EXISTS, (,))
};

static int register_cdc_acm_class(struct usbd_context *const uds_ctx,
				  const enum usbd_speed speed)
{
	struct usbd_config_node *cfg_nd;
	int err;

	if (speed == USBD_SPEED_HS) {
		cfg_nd = &sample_hs_config;
	} else {
		cfg_nd = &sample_fs_config;
	}

	err = usbd_add_configuration(uds_ctx, speed, cfg_nd);
	if (err) {
		LOG_ERR("Failed to add configuration");
		return err;
	}

	err = usbd_register_class(uds_ctx, uds_ctx->dev->name, speed, 1);
	if (err) {
		LOG_ERR("Failed to register classes");
		return err;
	}

	return usbd_device_set_code_triple(uds_ctx, speed, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
}

static int cdc_acm_serial_prepare_device(struct usbd_context *uds_ctx,
				      struct usbd_desc_node *serial_product,
				      usbd_msg_cb_t msg_cb)
{
	int err;

	err = usbd_add_descriptor(uds_ctx, &sample_lang);
	if (err) {
		LOG_ERR("Failed to initialize %s (%d)", "language descriptor", err);
		return err;
	}

	err = usbd_add_descriptor(uds_ctx, &sample_mfr);
	if (err) {
		LOG_ERR("Failed to initialize %s (%d)", "manufacturer descriptor", err);
		return err;
	}

	err = usbd_add_descriptor(uds_ctx, serial_product);
	if (err) {
		LOG_ERR("Failed to initialize %s (%d)", "product descriptor", err);
		return err;
	}

	IF_ENABLED(CONFIG_HWINFO, (
		err = usbd_add_descriptor(uds_ctx, &sample_sn);
	))
	if (err) {
		LOG_ERR("Failed to initialize %s (%d)", "SN descriptor", err);
		return err;
	}

	if (USBD_SUPPORTS_HIGH_SPEED &&
	    usbd_caps_speed(uds_ctx) == USBD_SPEED_HS) {
		err = register_cdc_acm_class(uds_ctx, USBD_SPEED_HS);
		if (err) {
			return err;
		}
	}

	err = register_cdc_acm_class(uds_ctx, USBD_SPEED_FS);
	if (err) {
		return err;
	}

	err = usbd_init(uds_ctx);
	if (err) {
		LOG_ERR("Failed to initialize %s (%d)", "device support", err);
		return err;
	}

	usbd_self_powered(uds_ctx, attributes & USB_SCD_SELF_POWERED);

	if (msg_cb != NULL) {
		err = usbd_msg_register_cb(uds_ctx, msg_cb);
		if (err) {
			LOG_ERR("Failed to register message callback");
			return err;
		}
	}

#if CONFIG_SAMPLE_USBD_20_EXTENSION_DESC
	(void)usbd_device_set_bcd_usb(uds_ctx, USBD_SPEED_FS, 0x0201);
	(void)usbd_device_set_bcd_usb(uds_ctx, USBD_SPEED_HS, 0x0201);

	err = usbd_add_descriptor(uds_ctx, &sample_usbext);
	if (err) {
		LOG_ERR("Failed to add USB 2.0 Extension Descriptor");
		return err;
	}
#endif

	if (!usbd_can_detect_vbus(uds_ctx)) {
		err = usbd_enable(uds_ctx);
		if (err) {
			LOG_ERR("Failed to enable device (%d)", err);
			return err;
		}
	}

	return 0;
}

int sample_cdc_acm_multiport_init_all_devices(usbd_msg_cb_t msg_cb)
{
	int err;

	for (int i = 0; i < MAX_PORTS_NUM; i++) {
		struct usbd_context *ctx = contexts[i];
		struct usbd_desc_node *product = products[i];

		if (ctx != NULL) {
			err = cdc_acm_serial_bringup_device(ctx, product, msg_cb);
			if (err) {
				LOG_ERR("Failed to bring up device udc%d (%d)", i, err);
				return err;
			}

			LOG_INF("udc%d device enabled", i);
		}
	}

	return 0;
}
