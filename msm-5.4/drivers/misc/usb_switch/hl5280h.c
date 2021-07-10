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
#include <linux/soc/qcom/usb_switch_core.h>

#define HL5280H_SWITCH_SETTINGS 0x04
#define HL5280H_SWITCH_CONTROL  0x05
#define HL5280H_SWITCH_STATUS1  0x07
#define HL5280H_SLOW_L          0x08
#define HL5280H_SLOW_R          0x09
#define HL5280H_SLOW_MIC        0x0A
#define HL5280H_SLOW_SENSE      0x0B
#define HL5280H_SLOW_GND        0x0C
#define HL5280H_DELAY_L_R       0x0D
#define HL5280H_DELAY_L_MIC     0x0E
#define HL5280H_DELAY_L_SENSE   0x0F
#define HL5280H_DELAY_L_AGND    0x10
#define HL5280H_RESET           0x1E

struct hl5280h_priv {
	struct regmap *regmap;
	int switch_control;
};

static struct hl5280h_priv *hl5280h;

struct hl5280h_reg_val {
	u16 reg;
	u8 val;
};


static const struct regmap_config hl5280h_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = HL5280H_RESET,
};

static const struct hl5280h_reg_val hl5280h_reg_i2c_defaults[] = {
	{HL5280H_SLOW_L, 0x00},
	{HL5280H_SLOW_R, 0x00},
	{HL5280H_SLOW_MIC, 0x00},
	{HL5280H_SLOW_SENSE, 0x00},
	{HL5280H_SLOW_GND, 0x00},
	{HL5280H_DELAY_L_R, 0x00},
	{HL5280H_DELAY_L_MIC, 0x00},
	{HL5280H_DELAY_L_SENSE, 0x00},
	{HL5280H_DELAY_L_AGND, 0x09},
	{HL5280H_SWITCH_SETTINGS, 0x98},
};

static void hl5280h_usbc_update_settings(struct hl5280h_priv *hl5280h,
		u32 switch_control, u32 switch_enable)
{
	u32 prev_control, prev_enable;

	if (!hl5280h->regmap) {
		pr_err("%s: regmap invalid\n", __func__);
		return;
	}

	regmap_read(hl5280h->regmap, HL5280H_SWITCH_CONTROL, &prev_control);
	regmap_read(hl5280h->regmap, HL5280H_SWITCH_SETTINGS, &prev_enable);

	if (prev_control == switch_control && prev_enable == switch_enable) {
		pr_debug("%s: settings unchanged\n", __func__);
		return;
	}

	regmap_write(hl5280h->regmap, HL5280H_SWITCH_SETTINGS, 0x80);
	regmap_write(hl5280h->regmap, HL5280H_SWITCH_CONTROL, switch_control);
	/* HL5280H chip hardware requirement */
	usleep_range(50, 55);
	regmap_write(hl5280h->regmap, HL5280H_SWITCH_SETTINGS, switch_enable);
}

static void hl5280h_update_reg_defaults(struct regmap *regmap)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(hl5280h_reg_i2c_defaults); i++)
		regmap_write(regmap, hl5280h_reg_i2c_defaults[i].reg,
				hl5280h_reg_i2c_defaults[i].val);
}

static int hl5280h_switch_mode(int mode)
{
	pr_info("set mode %d\n", mode);
	switch (mode) {
	case MODE_USB:
		hl5280h_usbc_update_settings(hl5280h, 0x18, 0x98);
		break;
	case MODE_HEADSET:
		hl5280h_usbc_update_settings(hl5280h, 0x00, 0x9F);
		break;
	case MODE_GND_MIC_SWAP:
		regmap_read(hl5280h->regmap, HL5280H_SWITCH_CONTROL,
			&hl5280h->switch_control);
		if ((hl5280h->switch_control & 0x07) == 0x07)
			hl5280h->switch_control = 0x0;
		else
			hl5280h->switch_control = 0x7;
		hl5280h_usbc_update_settings(hl5280h, hl5280h->switch_control,
				     0x9F);
		break;
	case MODE_OFF:
		break;
	default:
		pr_warn("Invalid mode %d\n", mode);
		return -1;
	}
	return 0;
}

int init_hl5280h(struct usb_switch_priv *priv_ptr)
{
	int rc = 0;

	if (priv_ptr == NULL || priv_ptr->i2c == NULL)
		return -ENOMEM;

	hl5280h = devm_kzalloc(&priv_ptr->i2c->dev, sizeof(*hl5280h),
				GFP_KERNEL);
	if (!hl5280h)
		return -ENOMEM;

	memset(hl5280h, 0, sizeof(struct hl5280h_priv));

	hl5280h->regmap = devm_regmap_init_i2c(priv_ptr->i2c, &hl5280h_regmap_config);
	if (IS_ERR_OR_NULL(hl5280h->regmap)) {
		pr_err("%s: Failed to initialize regmap: %d\n", __func__, rc);
		if (!hl5280h->regmap) {
			rc = -EINVAL;
			goto err_data;
		}
		rc = PTR_ERR(hl5280h->regmap);
		goto err_data;
	}

	priv_ptr->usb_switch_set_mode = hl5280h_switch_mode;
	rc = usb_switch_register_client(priv_ptr);
	if (rc < 0) {
		pr_err("%s: error! rc = %d", __func__, rc);
		return rc;
	}

	hl5280h_update_reg_defaults(hl5280h->regmap);

	return 0;
err_data:
	return rc;
}
EXPORT_SYMBOL(init_hl5280h);
