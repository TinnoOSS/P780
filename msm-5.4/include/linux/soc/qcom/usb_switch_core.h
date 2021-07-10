// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/usb/typec.h>
#include <linux/usb/ucsi_glink.h>
#include <linux/iio/consumer.h>
#include <linux/of.h>
#include <linux/notifier.h>

struct usb_switch_priv {
	struct device *dev;
	struct power_supply *usb_psy;
	struct notifier_block nb;
	struct iio_channel *iio_ch;
	atomic_t usbc_mode;
	struct work_struct usbc_analog_work;
	struct blocking_notifier_head usb_switch_notifier;
	struct mutex notification_lock;
	u32 use_powersupply;
	struct i2c_client *i2c;
	int (*usb_switch_set_mode)(int mode);
	bool swap_gnd_mic;
};

enum {
	MODE_INVALID = -1,
	MODE_USB = TYPEC_ACCESSORY_NONE,
	MODE_HEADSET = TYPEC_ACCESSORY_AUDIO,
	MODE_GND_MIC_SWAP,
	MODE_OFF,
	MODE_MAX
};

int usb_switch_register_client(struct usb_switch_priv *priv_ptr);
#ifdef CONFIG_USB_SWITCH
int usb_switch_event(struct device_node *node, int event);
int usb_switch_reg_notifier(struct notifier_block *nb, struct device_node *node);
int usb_switch_unreg_notifier(struct notifier_block *nb, struct device_node *node);
#else
static inline int usb_switch_event(struct device_node *node, int event)
{
	return 0;
}

static inline int usb_switch_reg_notifier(struct notifier_block *nb, struct device_node *node)
{
	return 0;
}

static inline int usb_switch_unreg_notifier(struct notifier_block *nb, struct device_node *node)
{
	return 0;
}
#endif
