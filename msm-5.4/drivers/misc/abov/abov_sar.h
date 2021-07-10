/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */
#ifndef ABOV_SAR_H
#define ABOV_SAR_H

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#define KEY_CAP_CS0              0x270
#define KEY_CAP_CS1              0x271
#define KEY_CAP_CS2              0x272
#define KEY_CAP_COMB             0x273
/*
 *  I2C Registers
 */
#define ABOV_VERSION_REG		    0x01
#define ABOV_MODELNO_REG		    0x02
#define ABOV_VENDOR_ID_REG		    0x03
#define ABOV_IRQSTAT_REG			0x04
#define ABOV_SOFTRESET_REG  		0x06
#define ABOV_CTRL_MODE_REG			0x07
#define ABOV_CTRL_CHANNEL_REG		0x08
#define ABOV_RECALI_REG				0xFB

#define ABOV_CH0_DIFF_MSB_REG       0x0A 
#define ABOV_CH0_DIFF_LSB_REG       0x0B
#define ABOV_CH1_DIFF_MSB_REG       0x0C
#define ABOV_CH1_DIFF_LSB_REG       0x0D
#define ABOV_CH2_DIFF_MSB_REG       0x0E
#define ABOV_CH2_DIFF_LSB_REG       0x0F

#define ABOV_CH0_CAP_MSB_REG        0x18
#define ABOV_CH0_CAP_LSB_REG        0x19
#define ABOV_CH1_CAP_MSB_REG        0x1A
#define ABOV_CH1_CAP_LSB_REG        0x1B
#define ABOV_CH2_CAP_MSB_REG        0x1C
#define ABOV_CH2_CAP_LSB_REG        0x1D
#define ABOV_REF0_CAP_MSB_REG       0x1E
#define ABOV_REF0_CAP_LSB_REG       0x1F
#define ABOV_REF1_CAP_MSB_REG       0x20
#define ABOV_REF1_CAP_LSB_REG       0x21

#define ABOV_CTRL_MODE_ACTIVE       0x00
#define ABOV_CTRL_MODE_SLEEP        0x01
#define ABOV_CTRL_MODE_STOP         0x02

#define ABOV_DETECT_FAIL            0x00
#define ABOV_DETECT_SUCCESS         0x01
#define ABOV_BOOT_DETECT_SUCCESS    0x02

/* enable body stat */
#define ABOV_TCHCMPSTAT_TCHSTAT0_FLAG   0x03
/* enable body stat */
#define ABOV_TCHCMPSTAT_TCHSTAT1_FLAG   0x0C
/* enable body stat */
#define ABOV_TCHCMPSTAT_TCHSTAT2_FLAG   0x30
/* enable body stat mask */
#define CAP_BUTTON_MASK                 0x15

#if 1
#define KEY_SAR_NEAR   0x2ec
#define KEY_SAR_CLOSE  0x2ed
#define KEY_SAR_FAR    0x2ef
#endif

/*
 * sensor device id, to check firmware download mode enter sucess or fail
 */
#define ABOV_DIVICE_ID_T375DFB    0x71

/*
 * input event report ABS type value
 *  0: far/release state
 *  1: near/ first detect state
 *  2: body/ second detect state
 * -1: unknown state when disable sar sensor
 */
#define ABS_REPORT_VALUE_FAR    0  
#define ABS_REPORT_VALUE_NEAR   1    
#define ABS_REPORT_VALUE_BODY   2 
#define ABS_REPORT_VALUE_NONE  -1 

#define CONFIG_CAPSENSE_USB_CAL

/**************************************
* define platform data
*
**************************************/
struct smtc_reg_data {
	unsigned char reg;
	unsigned char val;
};

typedef struct smtc_reg_data smtc_reg_data_t;
typedef struct smtc_reg_data *psmtc_reg_data_t;

struct _buttonInfo {
	/* The Key to send to the input */
	int keycode;
	/* Mask to look for on Touch Status */
	int mask;
	/* Current state of button. */
	int state;
};

struct _totalButtonInformation {
	struct _buttonInfo *buttons;
	int buttonSize;
	struct input_dev *input_ch0;
	struct input_dev *input_ch1;
	struct input_dev *input_ch2;
};

typedef struct _totalButtonInformation buttonInformation_t;
typedef struct _totalButtonInformation *pbuttonInformation_t;

/* Define Registers that need to be initialized to values different than
 * default
 */
static struct smtc_reg_data abov_i2c_reg_setup[] = {
	{
		.reg = ABOV_CTRL_MODE_REG,
		.val = 0x00,
	},
	{
		.reg = ABOV_CTRL_CHANNEL_REG,
		.val = 0x1F,
	},
	{
		.reg = ABOV_RECALI_REG,
		.val = 0x01,
	},
};



static struct _buttonInfo psmtcButtons[] = {
	{
		.keycode = KEY_CAP_CS0,
		.mask = ABOV_TCHCMPSTAT_TCHSTAT0_FLAG,
	},
	{
		.keycode = KEY_CAP_CS1,
		.mask = ABOV_TCHCMPSTAT_TCHSTAT1_FLAG,
	},
	{
		.keycode = KEY_CAP_CS2,
		.mask = ABOV_TCHCMPSTAT_TCHSTAT2_FLAG,
	},
};

struct abov_platform_data {
	int i2c_reg_num;
	struct smtc_reg_data *pi2c_reg;
	struct regulator *cap_vdd;
	struct regulator *cap_svdd;
	bool cap_vdd_en;
	bool cap_svdd_en;
	unsigned irq_gpio;
	/* used for custom setting for channel and scan period */
	int cap_channel_ch0;
	int cap_channel_ch1;
	int cap_channel_ch2;

	const char *cap_ch0_name;
	const char *cap_ch1_name;
	const char *cap_ch2_name;
	pbuttonInformation_t pbuttonInformation;
	const char *fw_name;
    
    #ifdef CONFIG_CAPSENSE_USB_CAL
	struct work_struct ps_notify_work;
	struct notifier_block ps_notif;
	bool ps_is_present;
	#ifdef CONFIG_CAPSENSE_ATTACH_CAL
	bool phone_is_present;
	#endif
    #endif
	
	int (*get_is_nirq_low)(unsigned irq_gpio);
	int (*init_platform_hw)(void);
	void (*exit_platform_hw)(void);
};
typedef struct abov_platform_data abov_platform_data_t;
typedef struct abov_platform_data *pabov_platform_data_t;

/***************************************
* define data struct/interrupt
* @pdev: pdev common device struction for linux
* @dworker: work struct for worker function
* @board: constant pointer to platform data
* @mutex: mutex for interrupt process
* @bus: either i2c_client or spi_client
* @pDevice: device specific struct pointer
* @read_flag : used for dump specified register
* @irq: irq number used
* @irqTimeout: msecs only set if useIrqTimer is true
* @irq_disabled: whether irq should be ignored
* @irq_gpio: irq gpio number
* @useIrqTimer: older models need irq timer for pen up cases
* @read_reg: record reg address which want to read
* @init: (re)initialize device
* @get_nirq_low: get whether nirq is low (platform data)
* @statusFunc: array of functions to call for corresponding status bit
***************************************/

typedef struct abovXX abovXX_t, *pabovXX_t;
struct abovXX {
	struct device *pdev;
	struct delayed_work dworker;
	struct abov_platform_data *board;
	struct mutex mutex;
	void *bus;
	void *pDevice;
	int read_flag;
	int irq;
	int irqTimeout;
	/* interrupt check flag */
	int int_state;
	char irq_disabled;
	/* whether irq should be ignored.. cases if enable/disable irq is not used
	 * or does not work properly */
	u8 useIrqTimer;
	u8 read_reg;

	bool loading_fw;
	struct work_struct fw_update_work;

	/* Function Pointers */
	int (*init)(pabovXX_t this);
	/* since we are trying to avoid knowing registers, create a pointer to a
	 * common read register which would be to read what the interrupt source
	 * is from
	 */
	int (*get_nirq_low)(unsigned irq_gpio);
	void (*statusFunc)(pabovXX_t this);
};
#endif
