#include <linux/soc/qcom/usb_switch_core.h>

static int usbc_analog_setup_switches_psupply(struct usb_switch_priv *priv_ptr);
extern init_hl5280h(struct usb_switch_priv *priv_ptr);
extern init_as6480(struct usb_switch_priv *priv_ptr);

static void usbc_analog_work_fn(struct work_struct *work)
{
	struct usb_switch_priv *priv_ptr =
		container_of(work, struct usb_switch_priv, usbc_analog_work);

	if (!priv_ptr) {
		pr_err("%s: usb_switch container invalid\n", __func__);
		return;
	}
	usbc_analog_setup_switches_psupply(priv_ptr);
	pm_relax(priv_ptr->dev);
}

static int usbc_event_changed_psupply(struct notifier_block *nb_ptr,
				      unsigned long evt, void *ptr)
{
	struct device *dev = NULL;
	struct usb_switch_priv *priv_ptr =
			container_of(nb_ptr, struct usb_switch_priv, nb);

	if (!priv_ptr)
		return -EINVAL;

	dev = priv_ptr->dev;
	if (!dev)
		return -EINVAL;
	dev_dbg(dev, "%s: queueing usbc_analog_work\n",
		__func__);
	pm_stay_awake(priv_ptr->dev);
	queue_work(system_freezable_wq, &priv_ptr->usbc_analog_work);

	return 0;
}

int usb_switch_register_client(struct usb_switch_priv *priv_ptr)
{
	int rc = 0;

	if (!priv_ptr)
		return -ENOMEM;
	priv_ptr->nb.notifier_call = usbc_event_changed_psupply;
	priv_ptr->nb.priority = 0;

	priv_ptr->use_powersupply = 1;
	priv_ptr->usb_psy = power_supply_get_by_name("usb");
	if (!priv_ptr->usb_psy) {
		rc = -EPROBE_DEFER;
		dev_dbg(priv_ptr->dev,
			"%s: could not get USB psy info: %d\n",
			__func__, rc);
			goto err_data;
	}

	priv_ptr->iio_ch = iio_channel_get(priv_ptr->dev, "typec_mode");

	if (!priv_ptr->iio_ch) {
		dev_err(priv_ptr->dev,
			"%s: iio_channel_get failed for typec_mode\n",
			__func__);
		goto err_supply;
	}

	rc = power_supply_reg_notifier(&priv_ptr->nb);
	if (rc) {
		dev_err(priv_ptr->dev,
			"%s: power supply reg failed: %d\n",
			__func__, rc);
		goto err_supply;
	}

	mutex_init(&priv_ptr->notification_lock);
	i2c_set_clientdata(priv_ptr->i2c, priv_ptr);
	INIT_WORK(&priv_ptr->usbc_analog_work,
		  usbc_analog_work_fn);

	priv_ptr->usb_switch_notifier.rwsem =
		(struct rw_semaphore)__RWSEM_INITIALIZER
		((priv_ptr->usb_switch_notifier).rwsem);
	priv_ptr->usb_switch_notifier.head = NULL;

	return 0;

err_supply:
	power_supply_put(priv_ptr->usb_psy);
err_data:
	return rc;
}
EXPORT_SYMBOL(usb_switch_register_client);

static int usbc_update_settings(struct usb_switch_priv *priv_ptr)
{
	int mode;
	if (priv_ptr == NULL
		|| priv_ptr->usb_switch_set_mode == NULL)
		return -EINVAL;
	mode = atomic_read(&(priv_ptr->usbc_mode));
	return priv_ptr->usb_switch_set_mode(mode);
}

static int usbc_analog_setup_switches_psupply(struct usb_switch_priv *priv_ptr)
{
	int rc = 0;
	union power_supply_propval mode;
	struct device *dev;

	if (!priv_ptr)
		return -EINVAL;
	dev = priv_ptr->dev;
	if (!dev)
		return -EINVAL;

	rc = iio_read_channel_processed(priv_ptr->iio_ch, &mode.intval);

	mutex_lock(&priv_ptr->notification_lock);
	/* get latest mode again within locked context */
	if (rc < 0) {
		dev_err(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
			__func__, rc);
		goto done;
	}

	dev_dbg(dev, "%s: setting GPIOs active = %d rcvd intval 0x%X\n",
		__func__, mode.intval != TYPEC_ACCESSORY_NONE, mode.intval);
	if (mode.intval == atomic_read(&(priv_ptr->usbc_mode))) {
		dev_dbg(dev, "%s: ignore the same mode %d\n", __func__, mode.intval);
		goto done;
	}

	atomic_set(&(priv_ptr->usbc_mode), mode.intval);

	switch (mode.intval) {
	/* add all modes USB Switch should notify for in here */
	case TYPEC_ACCESSORY_AUDIO:
		usbc_update_settings(priv_ptr);
		/* notify call chain on event */
		blocking_notifier_call_chain(&priv_ptr->usb_switch_notifier,
		mode.intval, NULL);
		break;
	case TYPEC_ACCESSORY_NONE:
		usbc_update_settings(priv_ptr);
		/* notify call chain on event */
		blocking_notifier_call_chain(&priv_ptr->usb_switch_notifier,
				TYPEC_ACCESSORY_NONE, NULL);
		break;
	default:
		/* ignore other usb connection modes */
		break;
	}

done:
	mutex_unlock(&priv_ptr->notification_lock);
	return rc;

}

/*
 * usb_switch_event - configure usb switch position based on event
 *
 * @node - phandle node to usb switch device
 * @event - usb switch_function enum
 *
 * Returns int on whether the switch happened or not
 */
int usb_switch_event(struct device_node *node,
			 int event)
{
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct usb_switch_priv *priv_ptr;

	if (!client)
		return -EINVAL;

	priv_ptr = (struct usb_switch_priv *)i2c_get_clientdata(client);
	if (!priv_ptr)
		return -EINVAL;

	if (!priv_ptr->usb_switch_set_mode)
		return -EINVAL;

	switch (event) {
	case MODE_GND_MIC_SWAP:
		priv_ptr->swap_gnd_mic = true;
		priv_ptr->usb_switch_set_mode(MODE_GND_MIC_SWAP);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(usb_switch_event);

/*
 * usb_switch_reg_notifier - register notifier block with usb_switch driver
 *
 * @nb - notifier block of usb_switch
 * @node - phandle node to usb_switch device
 *
 * Returns 0 on success, or error code
 */
int usb_switch_reg_notifier(struct notifier_block *nb,
			 struct device_node *node)
{
	int rc = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct usb_switch_priv *priv_ptr;

	if (!client)
		return -EINVAL;

	priv_ptr = (struct usb_switch_priv *)i2c_get_clientdata(client);
	if (!priv_ptr)
		return -EINVAL;

	rc = blocking_notifier_chain_register
				(&priv_ptr->usb_switch_notifier, nb);
	if (rc)
		return rc;

	/*
	 * as part of the init sequence check if there is a connected
	 * USB C analog adapter
	 */
	dev_dbg(priv_ptr->dev, "%s: verify if USB adapter is already inserted\n",
		__func__);

	rc = usbc_analog_setup_switches_psupply(priv_ptr);
	if (priv_ptr->swap_gnd_mic) {
		pr_info("%s: swap gnd and mic\n", __func__);
		priv_ptr->usb_switch_set_mode(MODE_GND_MIC_SWAP);
	}
	return rc;
}
EXPORT_SYMBOL(usb_switch_reg_notifier);

/*
 * usb_switch_unreg_notifier - unregister notifier block with usb_switch driver
 *
 * @nb - notifier block of usb_swtich
 * @node - phandle node to usb_switch device
 *
 * Returns 0 on pass, or error code
 */
int usb_switch_unreg_notifier(struct notifier_block *nb,
			     struct device_node *node)
{
	int rc = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct usb_switch_priv *priv_ptr;

	if (!client)
		return -EINVAL;

	priv_ptr = (struct usb_switch_priv *)i2c_get_clientdata(client);
	if (!priv_ptr)
		return -EINVAL;

	mutex_lock(&priv_ptr->notification_lock);

//	need set device state to default
	rc = blocking_notifier_chain_unregister
				(&priv_ptr->usb_switch_notifier, nb);
	mutex_unlock(&priv_ptr->notification_lock);

	return rc;
}
EXPORT_SYMBOL(usb_switch_unreg_notifier);

static int i2c_read_id(struct i2c_client *client)
{
	int ret;
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
			.len = 1,
			.buf = &block_data[1],
		}
	};

	block_data[0] = 0x0;
	block_data[1] = 0;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		pr_err("%s: fail to read device id! ret=%d\n", __func__, ret);
		return ret;
	} else
		return block_data[1];
}

static int usb_switch_probe(struct i2c_client *i2c,
			 const struct i2c_device_id *id)
{
	struct usb_switch_priv *usb_switch_ptr;
	int device_id;
	int rc = 0;

	usb_switch_ptr = devm_kzalloc(&i2c->dev, sizeof(*usb_switch_ptr),
				GFP_KERNEL);
	if (!usb_switch_ptr)
		return -ENOMEM;

	memset(usb_switch_ptr, 0, sizeof(struct usb_switch_priv));
	usb_switch_ptr->dev = &i2c->dev;
	usb_switch_ptr->i2c = i2c;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "%s: no support for i2c read/write"
				"byte data\n", __func__);
		return -EIO;
	}
#define HL5280H_ID 0x49
	device_id = i2c_read_id(i2c);
	if (device_id == HL5280H_ID)
		rc = init_hl5280h(usb_switch_ptr);
	else
		rc = init_as6480(usb_switch_ptr);

	if (rc < 0)
		dev_err(&i2c->dev, "%s: %s error!\n", __func__,
			(device_id == HL5280H_ID) ? "init_hl5280h" : "init_as6480");
	return rc;
}

static int usb_switch_remove(struct i2c_client *i2c)
{
	struct usb_switch_priv *priv_ptr =
			(struct usb_switch_priv *)i2c_get_clientdata(i2c);

	if (!priv_ptr)
		return -EINVAL;

	if (priv_ptr->use_powersupply) {
		/* deregister from PMI */
		power_supply_unreg_notifier(&priv_ptr->nb);
		power_supply_put(priv_ptr->usb_psy);
//	} else {
//		unregister_ucsi_glink_notifier(&priv_ptr->nb);
	}

//	set device state to default
	cancel_work_sync(&priv_ptr->usbc_analog_work);
	pm_relax(priv_ptr->dev);
	mutex_destroy(&priv_ptr->notification_lock);
	dev_set_drvdata(&i2c->dev, NULL);

	return 0;
}

static const struct of_device_id usb_switch_i2c_dt_match[] = {
	{
		.compatible = "tinno,usb-switch",
	},
	{}
};

static struct i2c_driver usb_switch_i2c_driver = {
	.driver = {
		.name = "usb_switch",
		.of_match_table = usb_switch_i2c_dt_match,
	},
	.probe = usb_switch_probe,
	.remove = usb_switch_remove,
};

static int __init usb_switch_init(void)
{
	int rc;

	rc = i2c_add_driver(&usb_switch_i2c_driver);
	if (rc)
		pr_err("usb switch: Failed to register I2C driver: %d\n", rc);

	return rc;
}
module_init(usb_switch_init);

static void __exit usb_switch_exit(void)
{
	i2c_del_driver(&usb_switch_i2c_driver);
}
module_exit(usb_switch_exit);

MODULE_DESCRIPTION("USB Switch I2C driver");
MODULE_LICENSE("GPL v2");
