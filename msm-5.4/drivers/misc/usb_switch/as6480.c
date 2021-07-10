/*
 * as6480.c -- as6480 audio switch driver
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/usb/typec.h>
#include <linux/soc/qcom/usb_switch_core.h>
#define AS6480_MODE_CTRL              1

#define AS6480_SWITCH_USB             0
#define AS6480_SWITCH_HEADSET         2
#define AS6480_SWITCH_GND_MIC_SWAP    3
#define AS6480_SWITCH_OFF             7

#define AS_DBG_TYPE_MODE              0

struct as6480_data {
	struct i2c_client *i2c_client;
	int mode;
};

static struct as6480_data *as6480;

static int as6480_write_reg(int reg, u8 value)
{
	int ret;
	struct i2c_client *client = as6480->i2c_client;
	unsigned char block_data[2];
	struct i2c_msg msg[1] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = block_data,
		}
	};

	block_data[0] = (unsigned char)reg;
	block_data[1] = (unsigned char)value;
	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		pr_err("%s:fail to write reg = %d, value=0x%x\n", __func__, reg, value);
	return ret;
}

static int as6480_read_reg(int reg)
{
	int ret;
	struct i2c_client *client = as6480->i2c_client;
	unsigned char block_data[2];
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &block_data[0],
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD | I2C_M_NOSTART,
			.len  = 1,
			.buf  = &block_data[1],
		}
	};

	block_data[0] = (unsigned char)reg;
	block_data[1] = 0;
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0)
		pr_err("%s:fail to read reg = %d, ret=%d\n", __func__, reg, ret);
	else
		ret = block_data[1];
	return ret;
}

static int as6480_get_switch_mode(void)
{
	int ret = as6480_read_reg(AS6480_MODE_CTRL);
	return ret;
}

static bool as6480_switch_to_usb(void)
{
	int ret;
	if (as6480_get_switch_mode() == AS6480_SWITCH_USB) {
		pr_info("%s: mode unchanged", __func__);
		return true;
	}

	ret  = as6480_write_reg(AS6480_MODE_CTRL, AS6480_SWITCH_OFF);
	ret |= as6480_write_reg(AS6480_MODE_CTRL, AS6480_SWITCH_USB);
	return (ret < 0) ? false : true;
}

static bool as6480_switch_to_headset(void)
{
	int ret;
	if (as6480_get_switch_mode() == AS6480_SWITCH_HEADSET) {
		pr_info("%s: mode unchanged", __func__);
		return true;
	}

	ret  = as6480_write_reg(AS6480_MODE_CTRL, AS6480_SWITCH_OFF);
	ret |= as6480_write_reg(AS6480_MODE_CTRL, AS6480_SWITCH_HEADSET);
	return (ret < 0) ? false : true;
}

static bool as6480_swap_gnd_mic(void)
{
	int ret;
	int old_mode, new_mode;
	old_mode = as6480_read_reg(AS6480_MODE_CTRL);
	new_mode = (old_mode == AS6480_SWITCH_HEADSET) ? AS6480_SWITCH_GND_MIC_SWAP : AS6480_SWITCH_HEADSET;
	ret  = as6480_write_reg(AS6480_MODE_CTRL, AS6480_SWITCH_OFF);
	ret |= as6480_write_reg(AS6480_MODE_CTRL, new_mode);
	return (ret < 0) ? false : true;
}

static bool as6480_switch_to_off(void)
{
	int ret;
	ret = as6480_write_reg(AS6480_MODE_CTRL, AS6480_SWITCH_OFF);
	return (ret < 0) ? false : true;
}

static int as6480_switch_mode(int value)
{
	pr_info("%s:set mode %d\n", __func__, value);
	switch (value) {
	case MODE_USB:
		as6480_switch_to_usb();
		break;
	case MODE_HEADSET:
		as6480_switch_to_headset();
		break;
	case MODE_GND_MIC_SWAP:
		as6480_swap_gnd_mic();
		break;
	case MODE_OFF:
		as6480_switch_to_off();
		break;
	default:
		pr_warn("Invalid mode %d\n", value);
		return -1;
	}
	return 0;
}

int init_as6480(struct usb_switch_priv *priv_ptr)
{
	int rc;
	if (priv_ptr == NULL || priv_ptr->i2c == NULL)
		return -ENOMEM;

	as6480 = devm_kzalloc(&priv_ptr->i2c->dev, sizeof(struct as6480_data),
			GFP_KERNEL);

	if (as6480 == NULL)
		return -ENOMEM;

	as6480->i2c_client = priv_ptr->i2c;
	as6480->mode       = MODE_INVALID;
	priv_ptr->usb_switch_set_mode = as6480_switch_mode;
	rc = usb_switch_register_client(priv_ptr);

	if (rc < 0) {
		pr_err("%s: error! rc = %d", __func__, rc);
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(init_as6480);
