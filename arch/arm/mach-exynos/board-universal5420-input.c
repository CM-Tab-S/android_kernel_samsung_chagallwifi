/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/gpio_keys.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXTS
#include <linux/i2c/mxts.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI
#include <linux/i2c/synaptics_rmi.h>
#include <linux/interrupt.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_I2C
#include <linux/i2c/synaptics_rmi.h>
#endif
#include <plat/iic.h>
#include <mach/hs-iic.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>

#ifdef CONFIG_TOUCHSCREEN_MELFAS
#include <mach/midas-tsp.h>
#endif
#include <mach/gpio.h>

#include <linux/i2c-gpio.h>

#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
#include <linux/i2c/touchkey_i2c.h>
#endif
#ifdef CONFIG_INPUT_WACOM
#include <linux/wacom_i2c.h>
#endif

#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

#include "board-universal5420.h"
#include <mach/sec_debug.h>

extern unsigned int system_rev;

#define GPIO_TSP_INT		EXYNOS5420_GPX1(6)
#define GPIO_TSP_SDA		EXYNOS5420_GPB3(0)
#define GPIO_TSP_SCL		EXYNOS5420_GPB3(1)
#define GPIO_TOUCHKEY_INT	EXYNOS5420_GPY7(1)
#define GPIO_TOUCHKEY_SDA	EXYNOS5420_GPD1(4)
#define GPIO_TOUCHKEY_SCL	EXYNOS5420_GPD1(5)
#define GPIO_LEVEL_LOW		0

#ifdef CONFIG_SENSORS_HALL
#define GPIO_HALL_SENSOR_INT	EXYNOS5420_GPX0(1)
#endif

#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
static struct i2c_board_info i2c_devs11_emul[];
#if 0
static struct touchkey_callbacks *tk_charger_callbacks;

void touchkey_charger_infom(bool en)
{
	if (tk_charger_callbacks && tk_charger_callbacks->inform_charger)
		tk_charger_callbacks->inform_charger(tk_charger_callbacks, en);
}

static void touchkey_register_callback(void *cb)
{
	tk_charger_callbacks = cb;
}
#endif
static void touchkey_init_hw(void)
{
#ifndef LED_LDO_WITH_REGULATOR
	gpio_request(GPIO_3_TOUCH_EN, "gpio_3_touch_en");
#endif
	gpio_request(GPIO_TOUCHKEY_INT, "2_TOUCH_INT");
	s3c_gpio_setpull(GPIO_TOUCHKEY_INT, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(GPIO_TOUCHKEY_INT);
	gpio_direction_input(GPIO_TOUCHKEY_INT);

	i2c_devs11_emul[0].irq = gpio_to_irq(GPIO_TOUCHKEY_INT);
	irq_set_irq_type(gpio_to_irq(GPIO_TOUCHKEY_INT), IRQF_TRIGGER_FALLING);
	s3c_gpio_cfgpin(GPIO_TOUCHKEY_INT, S3C_GPIO_SFN(0xf));

	s3c_gpio_setpull(GPIO_TOUCHKEY_SCL, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(GPIO_TOUCHKEY_SDA, S3C_GPIO_PULL_DOWN);
}

static int touchkey_suspend(void)
{
	struct regulator *regulator;

	regulator = regulator_get(NULL, TK_REGULATOR_NAME);
	if (IS_ERR(regulator)) {
		printk(KERN_ERR
		"[Touchkey] touchkey_suspend : TK regulator_get failed\n");
		return -EIO;
	}

	if (regulator_is_enabled(regulator))
		regulator_disable(regulator);

	s3c_gpio_setpull(GPIO_TOUCHKEY_SCL, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(GPIO_TOUCHKEY_SDA, S3C_GPIO_PULL_DOWN);

	regulator_put(regulator);

	return 1;
}

static int touchkey_resume(void)
{
	struct regulator *regulator;

	regulator = regulator_get(NULL, TK_REGULATOR_NAME);
	if (IS_ERR(regulator)) {
		printk(KERN_ERR
		"[Touchkey] touchkey_resume : TK regulator_get failed\n");
		return -EIO;
	}

	regulator_enable(regulator);
	regulator_put(regulator);

	s3c_gpio_setpull(GPIO_TOUCHKEY_SCL, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TOUCHKEY_SDA, S3C_GPIO_PULL_NONE);

	return 1;
}

static int touchkey_power_on(bool on)
{
	int ret;

	if (on) {
		gpio_direction_output(GPIO_TOUCHKEY_INT, 1);
		irq_set_irq_type(gpio_to_irq(GPIO_TOUCHKEY_INT),
			IRQF_TRIGGER_FALLING);
		s3c_gpio_cfgpin(GPIO_TOUCHKEY_INT, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_TOUCHKEY_INT, S3C_GPIO_PULL_NONE);
	} else
		gpio_direction_input(GPIO_TOUCHKEY_INT);

	if (on)
		ret = touchkey_resume();
	else
		ret = touchkey_suspend();

	return ret;
}

static int touchkey_led_power_on(bool on)
{
#ifdef LED_LDO_WITH_REGULATOR
	struct regulator *regulator;

	regulator = regulator_get(NULL, TK_LED_REGULATOR_NAME);
	if (IS_ERR(regulator)) {
		printk(KERN_ERR
			"[Touchkey] touchkey_led_power_on : TK_LED regulator_get failed\n");
		return -EIO;
	}

	if (on)
		regulator_enable(regulator);
	else {
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
	}

	regulator_put(regulator);
#else
	if (on)
		gpio_direction_output(GPIO_3_TOUCH_EN, 1);
	else
		gpio_direction_output(GPIO_3_TOUCH_EN, 0);
#endif
	return 1;
}

static struct touchkey_platform_data touchkey_pdata = {
	.gpio_sda = GPIO_TOUCHKEY_SDA,
	.gpio_scl = GPIO_TOUCHKEY_SCL,
	.gpio_int = GPIO_TOUCHKEY_INT,
	.init_platform_hw = touchkey_init_hw,
	.suspend = touchkey_suspend,
	.resume = touchkey_resume,
	.power_on = touchkey_power_on,
	.led_power_on = touchkey_led_power_on,
/*	.register_cb = touchkey_register_callback, */
};

static struct i2c_gpio_platform_data gpio_i2c_data11 = {
	.sda_pin = GPIO_TOUCHKEY_SDA,
	.scl_pin = GPIO_TOUCHKEY_SCL,
	.udelay = 1,
};

struct platform_device s3c_device_i2c11 = {
	.name = "i2c-gpio",
	.id = 11,
	.dev.platform_data = &gpio_i2c_data11,
};

/* I2C8 */
static struct i2c_board_info i2c_devs11_emul[] = {
	{
		I2C_BOARD_INFO("sec_touchkey", 0x20),
		.platform_data = &touchkey_pdata,
	},
};
#endif /*CONFIG_KEYBOARD_CYPRESS_TOUCH*/

static void touch_i2c0_cfg_gpio(struct platform_device *dev)
{
	s3c_gpio_cfgall_range(GPIO_TSP_SDA, 2,
				  S3C_GPIO_SFN(2), S3C_GPIO_PULL_NONE);
};

static struct s3c2410_platform_i2c touch_i2c0_platdata __initdata = {
	.bus_num	= 0,
	.flags		= 0,
	.slave_addr	= 0x10,
	.frequency	= 400*1000,
	.sda_delay	= 100,
	.cfg_gpio	= touch_i2c0_cfg_gpio,
};

#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXTS
/*
 * Please locate model dependent define at below position
 * such as bootloader address, app address and firmware name
 */
#if defined(CONFIG_UNIVERSAL5410_REV04)
#define MXT_BOOT_ADDRESS	0x25
#define MXT_APP_ADDRESS		0x4B
#else
#define MXT_BOOT_ADDRESS	0x24
#define MXT_APP_ADDRESS		0x4A
#endif

/* H/W revision to support Revision S */
#if defined(CONFIG_TARGET_LOCALE_KOR)
#define MXT_SUPPORT_REV_S      11
#else
#define MXT_SUPPORT_REV_S      12
#endif


/* We need to support two types of IC revision at once,
 * So two firmwwares are loaded, and we need to add proper firmware name
 * to platform data according to revision of IC.
 *
 * REV_G : Firmware version is like 1.x.
 * REV_I : Firmware version is like 2.x and it added Hovering functionality
 * compared with REV_G.
 */
#define MXT_FIRMWARE_NAME_REVISION_G	"mXT540Sg.fw"
#define MXT_FIRMWARE_NAME_REVISION_I	"mXT540Si.fw"

/* To display configuration version on *#2663# */
#define MXT_PROJECT_NAME	"GT-I95XX"

static struct mxt_callbacks *charger_callbacks;

void tsp_charger_infom(bool en)
{
	if (charger_callbacks && charger_callbacks->inform_charger)
		charger_callbacks->inform_charger(charger_callbacks, en);
}

static void mxt_register_callback(void *cb)
{
	charger_callbacks = cb;
}

static bool mxt_read_chg(void)
{
	return gpio_get_value(GPIO_TSP_INT);
}

static int mxt_power_on(void)
{
	struct regulator *regulator;

	/* enable I2C pullup */
	regulator = regulator_get(NULL, "touch_1.8v");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: tsp_vdd regulator_get failed\n",
			__func__);
		return -EIO;
	}
	regulator_enable(regulator);
	regulator_put(regulator);

	/* enable DVDD */
	regulator = regulator_get(NULL, "tsp_vdd_3.0v");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: tsp_dvdd regulator_get failed\n",
			__func__);
		return -EIO;
	}
	regulator_enable(regulator);
	regulator_put(regulator);

	/* enable AVDD */
	regulator = regulator_get(NULL, "tsp_avdd");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: tsp_avdd regulator_get failed\n",
			__func__);
		return -EIO;
	}
	regulator_enable(regulator);
	regulator_put(regulator);

	/* touch interrupt pin */
	s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);

	printk(KERN_ERR "mxt_power_on is finished\n");

	return 0;
}

static int mxt_power_off(void)
{
	struct regulator *regulator;

	/* disable AVDD */
	regulator = regulator_get(NULL, "tsp_avdd");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: tsp_avdd regulator_get failed\n",
			__func__);
		return -EIO;
	}

	if (regulator_is_enabled(regulator))
		regulator_disable(regulator);
	regulator_put(regulator);

	/* disable DVDD */
	regulator = regulator_get(NULL, "tsp_vdd_3.0v");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: tsp_dvdd regulator_get failed\n",
			__func__);
		return -EIO;
	}

	if (regulator_is_enabled(regulator))
		regulator_disable(regulator);
	regulator_put(regulator);

	/* disable I2C pullup */
	regulator = regulator_get(NULL, "touch_1.8v");
	if (IS_ERR(regulator)) {
		printk(KERN_ERR "[TSP] %s: tsp_vdd regulator_get failed\n",
			__func__);
		return -EIO;
	}

	if (regulator_is_enabled(regulator))
		regulator_disable(regulator);
	regulator_put(regulator);

	/* touch interrupt pin */
	s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);

	printk(KERN_ERR "mxt_power_off is finished\n");

	return 0;
}

static void mxt_gpio_init(void)
{
	/* touch interrupt */
	gpio_request(GPIO_TSP_INT, "GPIO_TSP_INT");
	s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(GPIO_TSP_INT);

	s3c_gpio_setpull(GPIO_TSP_SCL, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SDA, S3C_GPIO_PULL_NONE);
}

static struct mxt_platform_data mxt_data = {
	.num_xnode = 27,
	.num_ynode = 15,
	.max_x = 4095,
	.max_y = 4095,
	.irqflags = IRQF_TRIGGER_LOW | IRQF_ONESHOT,
	.boot_address = MXT_BOOT_ADDRESS,
	.firmware_name = MXT_FIRMWARE_NAME_REVISION_G,
	.project_name = MXT_PROJECT_NAME,
	.revision = MXT_REVISION_G,
	.read_chg = mxt_read_chg,
	.power_on = mxt_power_on,
	.power_off = mxt_power_off,
	.register_cb = mxt_register_callback,
};

static struct i2c_board_info mxt_i2c_devs0[] __initdata = {
	{
		I2C_BOARD_INFO(MXT_DEV_NAME, MXT_APP_ADDRESS),
		.platform_data = &mxt_data,
	}
};

void __init atmel_tsp_init(void)
{
	mxt_gpio_init();

	mxt_i2c_devs0[0].irq = gpio_to_irq(GPIO_TSP_INT);

	/* Revision_I is applied from H/W revision(1.1).*/
	if (system_rev >= MXT_SUPPORT_REV_S) {
		mxt_data.num_xnode = 28;
		mxt_data.num_ynode = 16;
		mxt_data.firmware_name = MXT_FIRMWARE_NAME_REVISION_I;
		mxt_data.revision = MXT_REVISION_I;
	}
	s3c_i2c0_set_platdata(&touch_i2c0_platdata);
	i2c_register_board_info(0, mxt_i2c_devs0,
			ARRAY_SIZE(mxt_i2c_devs0));

	printk(KERN_ERR "%s touch : %d\n",
		 __func__, mxt_i2c_devs0[0].irq);
}
#endif

/*	Synaptics Thin Driver	*/
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI
/* Below function is added by synaptics requirement. they want observe
 * the LCD's HSYNC signal in there IC. So we enable the Tout1 pad(on DDI)
 * after power up TSP.
 * I think this is not good solution. because it is not nature, to control
 * LCD's register on TSP driver....
 */
extern void (*panel_touchkey_on)(void);
extern void (*panel_touchkey_off)(void);

static void synaptics_enable_sync(bool on)
{
	if (on) {
		if (panel_touchkey_on)
			panel_touchkey_on();
	} else {
		if (panel_touchkey_off)
			panel_touchkey_off();
	}
}

/* Define for DDI multiplication
 * OLED_ID is 0 represent that MAGNA LDI is attatched
 * OLED ID is 1 represent that SDC LDI is attatched
 * From H/W revision (1001) support mulit ddi.
 */
#define REVISION_NEED_TO_SUPPORT_DDI	9
#define DDI_SDC	0
#define DDI_MAGNA	1

static unsigned char synaptics_get_ldi_info(void)
{
	return DDI_SDC;
#if 0
	if (!gpio_get_value(GPIO_OLED_ID))
		return DDI_MAGNA;
	else
		return DDI_SDC;
#endif
}

static int synaptics_power(bool on)
{
	struct regulator *regulator_vdd;
	struct regulator *regulator_avdd;
	static bool enabled;

	if (enabled == on)
		return 0;

	regulator_vdd = regulator_get(NULL, "touch_1.8v");
	if (IS_ERR(regulator_vdd)) {
		printk(KERN_ERR "[TSP]ts_power_on : tsp_vdd regulator_get failed\n");
		return PTR_ERR(regulator_vdd);
	}

	regulator_avdd = regulator_get(NULL, "tsp_avdd");
	if (IS_ERR(regulator_avdd)) {
		printk(KERN_ERR "[TSP]ts_power_on : tsp_avdd regulator_get failed\n");
		return PTR_ERR(regulator_avdd);
	}

	printk(KERN_ERR "[TSP] %s %s\n", __func__, on ? "on" : "off");

	if (on) {
		regulator_enable(regulator_vdd);
		regulator_enable(regulator_avdd);

		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
	} else {
		/*
		 * TODO: If there is a case the regulator must be disabled
		 * (e,g firmware update?), consider regulator_force_disable.
		 */
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);

		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_DOWN);
	}

	enabled = on;
	regulator_put(regulator_vdd);
	regulator_put(regulator_avdd);

	return 0;
}

static int synaptics_gpio_setup(unsigned gpio, bool configure)
{
	int retval = 0;

	if (configure) {
		gpio_request(gpio, "TSP_INT");
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_register_gpio_interrupt(gpio);
	} else {
		pr_warn("%s: No way to deconfigure gpio %d.",
		       __func__, gpio);
	}

	return retval;
}
#ifdef NO_0D_WHILE_2D

static unsigned char tm1940_f1a_button_codes[] = {KEY_MENU, KEY_BACK};

static struct synaptics_rmi_f1a_button_map tm1940_f1a_button_map = {
	.nbuttons = ARRAY_SIZE(tm1940_f1a_button_codes),
	.map = tm1940_f1a_button_codes,
};

static int ts_led_power_on(bool on)
{
	struct regulator *regulator;

	if (on) {
		regulator = regulator_get(NULL, "touchkey_led");
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "[TSP_KEY] ts_led_power_on : TK_LED regulator_get failed\n");
			return -EIO;
		}

		regulator_enable(regulator);
		regulator_put(regulator);
	} else {
		regulator = regulator_get(NULL, "touchkey_led");
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "[TSP_KEY] ts_led_power_on : TK_LED regulator_get failed\n");
			return -EIO;
		}

		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);
	}

	return 0;
}
#endif

#define TM1940_ADDR 0x20
#define TM1940_ATTN 130

#define FW_IMAGE_NAME_A0_H	"tsp_synaptics/synaptics_a0_h.fw"
#define FW_IMAGE_NAME_A0_H_NO_TR	"tsp_synaptics/synaptics_a0_h_no_tr.fw"

#define PROJECT_HA_3G	"N900XX"

#define NUM_RX	32
#define NUM_TX	18

/* HOST ID bit is used for notifiaction that which method is used in charger connetion. */
#define TSP_CHG_NOTI_GPIO	0
#define TSP_CHG_NOTI_RMI	1

static struct synaptics_rmi4_platform_data rmi4_platformdata = {
	.sensor_max_x = 1079,
	.sensor_max_y = 1919,
	.max_touch_width = 28,
	.irq_type = IRQF_TRIGGER_LOW | IRQF_ONESHOT,/*IRQF_TRIGGER_FALLING,*/
	.power = synaptics_power,
	.gpio = GPIO_TSP_INT,
	.gpio_config = synaptics_gpio_setup,
#ifdef NO_0D_WHILE_2D
	.led_power_on = ts_led_power_on,
	.f1a_button_map = &tm1940_f1a_button_map,
#endif
	.firmware_name = NULL,
	.fac_firmware_name = NULL,
	.project_name = PROJECT_HA_3G,
	.get_ddi_type = NULL,
	.num_of_rx = NUM_RX,
	.num_of_tx = NUM_TX,
};

static struct i2c_board_info synaptics_i2c_devs0[] = {
	{
		I2C_BOARD_INFO("synaptics_rmi4_i2c", 0x20),
		.platform_data = &rmi4_platformdata,
	}
};

static void synaptics_verify_panel_revision(void)
{
	u8 el_type = (lcdtype & 0x300) >> 8;
	u8 touch_type = (lcdtype & 0xF000) >> 12;

	rmi4_platformdata.firmware_name = FW_IMAGE_NAME_A0_H;
	rmi4_platformdata.fac_firmware_name = FW_IMAGE_NAME_A0_H;

#ifdef SYNAPTICS_WORKAROUND_FOR_H_PROJECT
	/* touch_type(0x03) represent that OCTA dosen't have TR */
	if (touch_type == 0x03) {
		rmi4_platformdata.firmware_name = FW_IMAGE_NAME_A0_H_NO_TR;
		rmi4_platformdata.fac_firmware_name = FW_IMAGE_NAME_A0_H_NO_TR;
	}
#endif

	rmi4_platformdata.panel_revision = touch_type;

	/* ID2 of LDI : from 0x25 of ID2, TOUT1 pin is connected
	 * to TSP IC to observe HSYNC signal on TSP IC.
	 * So added enable_sync function added in platform data.
	 */
	rmi4_platformdata.enable_sync = &synaptics_enable_sync;
	rmi4_platformdata.get_ddi_type = &synaptics_get_ldi_info;

	/* Define the notification method in charger event
	 * according to H/W logic. we are recomend to use GPIO noti instead RMI.
	 */
	if (system_rev >= 7)
		rmi4_platformdata.charger_noti_type = TSP_CHG_NOTI_GPIO;
	else
		rmi4_platformdata.charger_noti_type = TSP_CHG_NOTI_RMI;

	printk(KERN_ERR "%s: panel_revision: %d, lcdtype: 0x%06X, touch_type: %d, el_type: %s, h_sync: %s, ddi_type: %s, charger_noti_type: %s\n",
		__func__, rmi4_platformdata.panel_revision,
		lcdtype, touch_type, el_type ? "M4+" : "M4",
		!rmi4_platformdata.enable_sync ? "NA" : "OK",
		!rmi4_platformdata.get_ddi_type ? "NA" :
		rmi4_platformdata.get_ddi_type() ? "MAGNA" : "SDC",
		rmi4_platformdata.charger_noti_type ? "RMI" : "GPIO");
};

void __init synaptics_tsp_init(void)
{
	gpio_request(GPIO_TSP_INT, "TSP_INT");
	s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(GPIO_TSP_INT);
	rmi4_platformdata.gpio = GPIO_TSP_INT;
	synaptics_i2c_devs0[0].irq = gpio_to_irq(GPIO_TSP_INT);

	synaptics_verify_panel_revision();

	if (lpcharge) {
		printk(KERN_ERR "%s: disable TSP for lpm\n", __func__);
		return;
	}

	s3c_i2c0_set_platdata(&touch_i2c0_platdata);
	i2c_register_board_info(0, synaptics_i2c_devs0,
		 ARRAY_SIZE(synaptics_i2c_devs0));

	printk(KERN_ERR "%s touch : %d\n",
		 __func__, synaptics_i2c_devs0[0].irq);
}
#endif

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_I2C

#define PROJECT_V2A_NAME	"SM-P9XX"
#define FW_IMAGE_NAME_V2 "tsp_synaptics/synaptics_v2.fw"
#define FW_IMAGE_NAME_V2_G2 "tsp_synaptics/synaptics_v2_g2.fw"

#define DSX_I2C_ADDR 0x20
#define DSX_ATTN_GPIO GPIO_TOUCH_CHG
#define DSX_RESET_GPIO GPIO_TOUCH_RESET
#define DSX_AVDD_GPIO	GPIO_TOUCH_EN

#define NUM_OF_RX	58
#define NUM_OF_TX	36

static int synaptics_power(bool on)
{
	struct regulator *regulator_vdd;
	struct regulator *regulator_avdd;
	static bool enabled;

	if (enabled == on)
		return 0;

	regulator_vdd = regulator_get(NULL, "tsp_vdd_1.8v");
	if (IS_ERR(regulator_vdd)) {
		printk(KERN_ERR "[TSP]ts_power_on : tsp_vdd regulator_get failed\n");
		return PTR_ERR(regulator_vdd);
	}

	if (on) {
		gpio_set_value(DSX_AVDD_GPIO, 1);

		regulator_enable(regulator_vdd);
		regulator_put(regulator_vdd);

		gpio_set_value(DSX_RESET_GPIO, 1);

		s3c_gpio_cfgpin(DSX_ATTN_GPIO, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(DSX_ATTN_GPIO, S3C_GPIO_PULL_NONE);
	} else {
		gpio_set_value(DSX_RESET_GPIO, 0);

		/*
		 * TODO: If there is a case the regulator must be disabled
		 * (e,g firmware update?), consider regulator_force_disable.
		 */
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		regulator_put(regulator_vdd);

		gpio_set_value(DSX_AVDD_GPIO, 0);

		s3c_gpio_cfgpin(DSX_ATTN_GPIO, S3C_GPIO_INPUT);
		s3c_gpio_setpull(DSX_ATTN_GPIO, S3C_GPIO_PULL_NONE);
	}

	enabled = on;

	printk(KERN_ERR "[TSP] %s %s\n", __func__, on ? "on" : "off");

	return 0;
}

static int synaptics_gpio_setup(unsigned gpio, bool configure)
{
	int retval = 0;

	if (configure) {
		gpio_request(gpio, "TSP_INT");
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_register_gpio_interrupt(gpio);
	} else {
		pr_warn("%s: No way to deconfigure gpio %d.",
		       __func__, gpio);
	}

	return retval;
}

#ifdef NO_0D_WHILE_2D
static unsigned char tm1940_cap_button_codes[] = {
	KEY_DUMMY_MENU,
	KEY_MENU,
	KEY_DUMMY_HOME1,
	KEY_DUMMY_HOME2,
	KEY_BACK,
	KEY_DUMMY_BACK,
};

static unsigned char tm1940_cap_button_codes_rev05[] = {
	KEY_DUMMY_MENU,
	KEY_MENU,
	KEY_BACK,
	KEY_DUMMY_BACK,
};

static struct synaptics_rmi_f1a_button_map tm1940_cap_button_map = {
	.nbuttons = ARRAY_SIZE(tm1940_cap_button_codes),
	.map = tm1940_cap_button_codes,
};

static struct synaptics_rmi_f1a_button_map tm1940_cap_button_map_rev05 = {
	.nbuttons = ARRAY_SIZE(tm1940_cap_button_codes_rev05),
	.map = tm1940_cap_button_codes_rev05,
};

static int ts_led_power_on(bool on)
{
	struct regulator *regulator;

	if (on) {
		regulator = regulator_get(NULL, "key_led_3.3v");
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "[TSP_KEY] ts_led_power_on : TK_LED regulator_get failed\n");
			return -EIO;
		}

		regulator_enable(regulator);
		regulator_put(regulator);
	} else {
		regulator = regulator_get(NULL, "key_led_3.3v");
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "[TSP_KEY] ts_led_power_on : TK_LED regulator_get failed\n");
			return -EIO;
		}

		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);
	}

	printk(KERN_ERR "[TSP_KEY] %s %s\n", __func__, on ? "on" : "off");

	return 0;
}
#endif

static struct synaptics_rmi4_platform_data dsx_platformdata = {
	.sensor_max_x = 2559,
	.sensor_max_y = 1599,
	.max_touch_width = 28,
	.irq_type = IRQF_TRIGGER_LOW | IRQF_ONESHOT,/*IRQF_TRIGGER_FALLING,*/
	.power = synaptics_power,
	.gpio = DSX_ATTN_GPIO,
	.gpio_config = synaptics_gpio_setup,
#ifdef NO_0D_WHILE_2D
	.led_power_on = ts_led_power_on,
	.f1a_button_map = &tm1940_cap_button_map,
#endif
	.firmware_name = FW_IMAGE_NAME_V2,
	.fac_firmware_name = FW_IMAGE_NAME_V2,
	.project_name = PROJECT_V2A_NAME,
	.get_ddi_type = NULL,
	.num_of_rx = NUM_OF_RX,
	.num_of_tx = NUM_OF_TX,
};

static struct i2c_board_info synaptics_dsx_i2c_devs0[] = {
	{
		I2C_BOARD_INFO("synaptics_dsx_i2c", DSX_I2C_ADDR),
		.platform_data = &dsx_platformdata,
	},
};

static void synaptics_dsx_gpio_init(void)
{
	/* touch interrupt */
	gpio_request(DSX_ATTN_GPIO, "TSP_INT");
	s3c_gpio_cfgpin(DSX_ATTN_GPIO, S3C_GPIO_INPUT);
	s3c_gpio_setpull(DSX_ATTN_GPIO, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(DSX_ATTN_GPIO);

	s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_NONE);

	gpio_request_one(DSX_RESET_GPIO, GPIOF_OUT_INIT_LOW, "synaptics_dsx_i2c nRESET");

	gpio_request(DSX_AVDD_GPIO, "GPIO_TOUCH_EN");
	s3c_gpio_cfgpin(DSX_AVDD_GPIO, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(DSX_AVDD_GPIO, S3C_GPIO_PULL_NONE);

	s5p_gpio_set_pd_cfg(DSX_ATTN_GPIO, S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_pull(DSX_ATTN_GPIO, S5P_GPIO_PD_UPDOWN_DISABLE);
	s5p_gpio_set_pd_cfg(DSX_RESET_GPIO, S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_cfg(DSX_AVDD_GPIO, S5P_GPIO_PD_PREV_STATE);
}

void __init synaptics_dsx_tsp_init(void)
{
	synaptics_dsx_gpio_init();

	synaptics_dsx_i2c_devs0[0].irq = gpio_to_irq(DSX_ATTN_GPIO);

	if (system_rev >= 4) {
#ifdef NO_0D_WHILE_2D
		dsx_platformdata.f1a_button_map = &tm1940_cap_button_map_rev05;
#endif
		dsx_platformdata.firmware_name = FW_IMAGE_NAME_V2_G2;
		dsx_platformdata.fac_firmware_name = FW_IMAGE_NAME_V2_G2;
	}
	s3c_i2c0_set_platdata(&touch_i2c0_platdata);
	i2c_register_board_info(0, synaptics_dsx_i2c_devs0,
		 ARRAY_SIZE(synaptics_dsx_i2c_devs0));

	printk(KERN_ERR "%s touch : %d\n",
		 __func__, synaptics_dsx_i2c_devs0[0].irq);
}
#endif

#ifdef CONFIG_INPUT_WACOM
static struct wacom_g5_callbacks *wacom_callbacks;
static bool wacom_power_enabled;

int wacom_power(bool on)
{
#ifdef GPIO_PEN_LDO_EN
	gpio_direction_output(GPIO_PEN_LDO_EN, on);
#else
	struct regulator *regulator_vdd;

	if (wacom_power_enabled == on)
		return 0;

	regulator_vdd = regulator_get(NULL, "wacom_3.0v");
	if (IS_ERR(regulator_vdd)) {
		printk(KERN_ERR"epen: %s reg get err\n", __func__);
		return PTR_ERR(regulator_vdd);
	}

	if (on) {
		regulator_enable(regulator_vdd);
	} else {
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		else
			regulator_force_disable(regulator_vdd);
	}
	regulator_put(regulator_vdd);

	wacom_power_enabled = on;
#endif
	return 0;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static int wacom_early_suspend_hw(void)
{
#ifdef GPIO_PEN_RESET_N_18V
	gpio_direction_output(GPIO_PEN_RESET_N_18V, 0);
#endif
	wacom_power(0);

	return 0;
}

static int wacom_late_resume_hw(void)
{
#ifdef GPIO_PEN_RESET_N_18V
	gpio_direction_output(GPIO_PEN_RESET_N_18V, 1);
#endif
	gpio_direction_output(GPIO_PEN_PDCT_18V, 1);
	wacom_power(1);
	msleep(100);
	gpio_direction_input(GPIO_PEN_PDCT_18V);
	return 0;
}
#endif

static int wacom_suspend_hw(void)
{
#ifdef GPIO_PEN_RESET_N_18V
	gpio_direction_output(GPIO_PEN_RESET_N_18V, 0);
#endif
	wacom_power(0);
	return 0;
}

static int wacom_resume_hw(void)
{
#ifdef GPIO_PEN_RESET_N_18V
	gpio_direction_output(GPIO_PEN_RESET_N_18V, 1);
#endif
	gpio_direction_output(GPIO_PEN_PDCT_18V, 1);
	wacom_power(1);
	/*msleep(100);*/
	gpio_direction_input(GPIO_PEN_PDCT_18V);
	return 0;
}

static int wacom_reset_hw(void)
{
	wacom_suspend_hw();
	msleep(100);
	wacom_resume_hw();

	return 0;
}

static void wacom_register_callbacks(struct wacom_g5_callbacks *cb)
{
	wacom_callbacks = cb;
};

static void wacom_compulsory_flash_mode(bool en)
{
	gpio_direction_output(GPIO_PEN_FWE1_18V, en);
}

static int wacom_get_irq_state(void)
{
	return gpio_get_value(GPIO_PEN_IRQ_18V);
}

static struct wacom_g5_platform_data wacom_platform_data = {
	.x_invert = WACOM_X_INVERT,
	.y_invert = WACOM_Y_INVERT,
	.xy_switch = WACOM_XY_SWITCH,
	.min_x = 0,
	.max_x = WACOM_MAX_COORD_X,
	.min_y = 0,
	.max_y = WACOM_MAX_COORD_Y,
	.min_pressure = 0,
	.max_pressure = WACOM_MAX_PRESSURE,
	.gpio_pendct = GPIO_PEN_PDCT_18V,
	/*.init_platform_hw = wacom_init,*/
	/*      .exit_platform_hw =,    */
	.suspend_platform_hw = wacom_suspend_hw,
	.resume_platform_hw = wacom_resume_hw,
#ifdef CONFIG_HAS_EARLYSUSPEND
	.early_suspend_platform_hw = wacom_early_suspend_hw,
	.late_resume_platform_hw = wacom_late_resume_hw,
#endif
	.reset_platform_hw = wacom_reset_hw,
	.register_cb = wacom_register_callbacks,
	.compulsory_flash_mode = wacom_compulsory_flash_mode,
	.gpio_pen_insert = GPIO_WACOM_SENSE,
	.get_irq_state = wacom_get_irq_state,
};

/* I2C */
static struct i2c_board_info wacom_i2c_devs[] __initdata = {
	{
		I2C_BOARD_INFO("wacom_g5sp_i2c", WACOM_I2C_ADDR),
		.platform_data = &wacom_platform_data,
	},
};

#define WACOM_SET_I2C(ch, pdata, i2c_info)	\
	do {		\
	s3c_i2c##ch##_set_platdata(pdata);	\
	i2c_register_board_info(ch, i2c_info,	\
	ARRAY_SIZE(i2c_info));	\
	platform_device_register(&s3c_device_i2c##ch);	\
	} while (0);

void __init wacom_init(void)
{
	int gpio;
	int ret;

#ifdef GPIO_PEN_RESET_N_18V
	/*Reset*/
	gpio = GPIO_PEN_RESET_N_18V;
	ret = gpio_request(gpio, "PEN_RESET_N");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_RESET_N.(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0x1));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_direction_output(gpio, 0);
#endif

	/*SLP & FWE1*/
	gpio = GPIO_PEN_FWE1_18V;
	ret = gpio_request(gpio, "PEN_FWE1");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_FWE1.(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0x1));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_direction_output(gpio, 0);

	/*PDCT*/
	gpio = GPIO_PEN_PDCT_18V;
	ret = gpio_request(gpio, "PEN_PDCT");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_PDCT.(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(gpio);
	gpio_direction_input(gpio);

	irq_set_irq_type(gpio_to_irq(gpio), IRQ_TYPE_EDGE_BOTH);

	/*IRQ*/
	gpio = GPIO_PEN_IRQ_18V;
	ret = gpio_request(gpio, "PEN_IRQ");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_IRQ.(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(gpio);
	gpio_direction_input(gpio);

	wacom_i2c_devs[0].irq = gpio_to_irq(gpio);
	irq_set_irq_type(wacom_i2c_devs[0].irq, IRQ_TYPE_EDGE_RISING);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));

	/*LDO_EN*/
#ifdef GPIO_PEN_LDO_EN
	gpio = GPIO_PEN_LDO_EN;
	ret = gpio_request(gpio, "PEN_LDO_EN");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_LDO_EN.(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	gpio_direction_output(gpio, 0);
#else
	wacom_power(0);
#endif

	/*WACOM_SET_I2C(3, NULL, wacom_i2c_devs);*/
	if (lpcharge)
		printk(KERN_DEBUG"epen:disable wacom for lpm\n");
	else {
		exynos5_hs_i2c4_set_platdata(NULL);
		i2c_register_board_info(8, wacom_i2c_devs, ARRAY_SIZE(wacom_i2c_devs));
	}

	printk(KERN_INFO "epen:init\n");
}
#endif

#define GPIO_POWER_BUTTON       EXYNOS5420_GPX2(2)
#define GPIO_VOLUP_BUTTON       EXYNOS5420_GPX0(2)
#define GPIO_VOLDOWN_BUTTON     EXYNOS5420_GPX0(3)
#define GPIO_HOMEPAGE_BUTTON     EXYNOS5420_GPX0(5)


static void universal5410_gpio_keys_config_setup(u32 system_rev)
{
	s3c_gpio_setpull(GPIO_VOLUP_BUTTON, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_VOLDOWN_BUTTON, S3C_GPIO_PULL_UP);
};

static struct gpio_keys_button universal5410_button[] = {
	{
		.code = KEY_POWER,
		.gpio = GPIO_POWER_BUTTON,
		.desc = "gpio-keys: KEY_POWER",
		.active_low = 1,
		.wakeup = 1,
		.isr_hook = sec_debug_check_crash_key,
		.debounce_interval = 10,
	}, {
		.code = KEY_VOLUMEUP,
		.gpio = GPIO_VOLUP_BUTTON,
		.desc = "gpio-keys: KEY_VOLUMEUP",
		.active_low = 1,
		.isr_hook = sec_debug_check_crash_key,
		.debounce_interval = 10,
	}, {
		.code = KEY_VOLUMEDOWN,
		.gpio = GPIO_VOLDOWN_BUTTON,
		.desc = "gpio-keys: KEY_VOLUMEDOWN",
		.active_low = 1,
		.isr_hook = sec_debug_check_crash_key,
		.debounce_interval = 10,
	}, {
		.code = KEY_HOMEPAGE,
		.gpio = GPIO_HOMEPAGE_BUTTON,
		.desc = "gpio-keys: KEY_HOMEPAGE",
		.active_low = 1,
		.wakeup = 1,
		.isr_hook = sec_debug_check_crash_key,
		.debounce_interval = 10,
	},
};

static struct gpio_keys_platform_data universal5410_gpiokeys_platform_data = {
	universal5410_button,
	ARRAY_SIZE(universal5410_button),
#ifdef CONFIG_SENSORS_HALL
	.gpio_flip_cover = GPIO_HALL_SENSOR_INT,
#endif
};

static struct platform_device universal5410_gpio_keys = {
	.name   = "gpio-keys",
	.dev    = {
		.platform_data = &universal5410_gpiokeys_platform_data,
	},
};

#ifdef CONFIG_INPUT_BOOSTER
static enum booster_device_type get_booster_device(int code)
{
	switch (code) {
	case KEY_HOMEPAGE:
		return BOOSTER_DEVICE_KEY;
	break;
	case KEY_MENU:
	case KEY_BACK:
		return BOOSTER_DEVICE_TOUCHKEY;
	break;
	case KEY_BOOSTER_TOUCH:
		return BOOSTER_DEVICE_TOUCH;
	break;
	case KEY_BOOSTER_PEN:
		return BOOSTER_DEVICE_PEN;
	break;
	default:
		return BOOSTER_DEVICE_NOT_DEFINED;
	break;
	}
}

static const struct dvfs_freq key_freq_table[BOOSTER_DEVICE_MAX] = {
	[BOOSTER_LEVEL1] = BOOSTER_DVFS_FREQ(650000,	400000,	111000),
};

static const struct dvfs_freq touchkey_freq_table[BOOSTER_DEVICE_MAX] = {
	[BOOSTER_LEVEL1] = BOOSTER_DVFS_FREQ(1600000,	667000,	333000),
	[BOOSTER_LEVEL2] = BOOSTER_DVFS_FREQ(650000,	400000,	111000),
};

static const struct dvfs_freq touch_freq_table[BOOSTER_DEVICE_MAX] = {
	[BOOSTER_LEVEL1] = BOOSTER_DVFS_FREQ(1600000,	667000,	333000),
	[BOOSTER_LEVEL2] = BOOSTER_DVFS_FREQ(650000,	400000,	111000),
	[BOOSTER_LEVEL3] = BOOSTER_DVFS_FREQ(650000,	667000,	333000),
};

static struct booster_key booster_keys[] = {
	BOOSTER_KEYS("HOMEPAGE", KEY_HOMEPAGE,
			BOOSTER_DEFAULT_ON_TIME,
			BOOSTER_DEFAULT_OFF_TIME,
			key_freq_table),
	BOOSTER_KEYS("MENU", KEY_MENU,
			BOOSTER_DEFAULT_ON_TIME,
			BOOSTER_DEFAULT_OFF_TIME,
			touchkey_freq_table),
	BOOSTER_KEYS("BACK", KEY_BACK,
			BOOSTER_DEFAULT_ON_TIME,
			BOOSTER_DEFAULT_OFF_TIME,
			touchkey_freq_table),
	BOOSTER_KEYS("TOUCH", KEY_BOOSTER_TOUCH,
			BOOSTER_DEFAULT_CHG_TIME,
			BOOSTER_DEFAULT_OFF_TIME,
			touch_freq_table),
	BOOSTER_KEYS("PEN", KEY_BOOSTER_PEN,
			BOOSTER_DEFAULT_CHG_TIME,
			BOOSTER_DEFAULT_OFF_TIME,
			touch_freq_table),
};

/* Caution : keys, nkeys, get_device_type should be defined */

static struct input_booster_platform_data input_booster_pdata = {
	.keys = booster_keys,
	.nkeys = ARRAY_SIZE(booster_keys),
	.get_device_type = get_booster_device,
};

static struct platform_device universal5420_input_booster = {
	.name = INPUT_BOOSTER_NAME,
	.dev.platform_data = &input_booster_pdata,
};
#endif

static struct platform_device *universal5410_input_devices[] __initdata = {
	&s3c_device_i2c0,
	&universal5410_gpio_keys,
#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
	&s3c_device_i2c11,
#endif
#ifdef CONFIG_INPUT_WACOM
	&exynos5_device_hs_i2c4,
#endif
#ifdef CONFIG_INPUT_BOOSTER
	&universal5420_input_booster,
#endif
};

void __init tsp_check_and_init(void)
{
	/* We do not need to check Ic vendor anymore */
	synaptics_tsp_init();
}

void __init exynos5_universal5420_input_init(void)
{

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_I2C)
	synaptics_dsx_tsp_init();
#else
#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI)
	tsp_check_and_init();
#elif defined(CONFIG_TOUCHSCREEN_MELFAS)
	s3c_i2c0_set_platdata(&touch_i2c0_platdata);
	midas_tsp_init();
#endif
#endif
	universal5410_gpio_keys_config_setup(system_rev);

#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
	touchkey_init_hw();
	if (lpcharge)
		printk(KERN_DEBUG"touchkey:disable touchkey for lpm\n");
	else
		i2c_register_board_info(11, i2c_devs11_emul, ARRAY_SIZE(i2c_devs11_emul));
#endif
#ifdef CONFIG_INPUT_WACOM
	wacom_init();
#endif
#ifdef CONFIG_SENSORS_HALL
	s3c_gpio_setpull(GPIO_HALL_SENSOR_INT, S3C_GPIO_PULL_UP);
	gpio_request(GPIO_HALL_SENSOR_INT, "GPIO_HALL_SENSOR_INT");
	s3c_gpio_cfgpin(GPIO_HALL_SENSOR_INT, S3C_GPIO_SFN(0xf));
	s5p_register_gpio_interrupt(GPIO_HALL_SENSOR_INT);
	gpio_direction_input(GPIO_HALL_SENSOR_INT);
#endif
	platform_add_devices(universal5410_input_devices,
			ARRAY_SIZE(universal5410_input_devices));
}
