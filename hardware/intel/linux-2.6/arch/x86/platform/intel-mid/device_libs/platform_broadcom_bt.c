/*
 * platform_broadcom_bt.c: broadcom bluetooth platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/lnw_gpio.h>
#include <linux/skbuff.h>
#include <asm/intel-mid.h>
#include <platform_broadcom_bt.h>

int bt_power, bt_reset;

static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= BT_HOST_WAKE,
		.end	= BT_HOST_WAKE,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= BT_EXT_WAKE,
		.end	= BT_EXT_WAKE,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake",
		.start	= 1,
		.end	= 1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device brcm_bluesleep_device = {
	.name = "bluesleep", 
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};

static int brcm_bluetooth_power(int on)
{
	int rc = 0;
printk(KERN_ERR "brcm_bluetooth_power called: on = %d, bt_power = %d, bt_reset = %d\n", on, bt_power, bt_reset);

	if(on){
		gpio_direction_output(bt_power, 0);
		gpio_set_value(bt_power,0);
		gpio_direction_output(bt_reset, 0);
		gpio_set_value(bt_reset,0);
		mdelay(100);
		gpio_set_value(bt_power,1);
		gpio_set_value(bt_reset,1);
		mdelay(200);
	} else {
		gpio_direction_output(bt_power,0);
	   	gpio_set_value(bt_power,0);
		gpio_direction_output(bt_reset,0);
	   	gpio_set_value(bt_reset,0);
           	mdelay(200);
	}
//	gpio_free(bt_power); 	
//	gpio_free(bt_reset); 
printk(KERN_ERR "brcm_bluetooth_power done\n");
	return 0;
}

static struct platform_device bcm4330_rfkill = {
                .name = "bcm4330_rfkill", 
                .id = -1,
                .dev.platform_data = &brcm_bluetooth_power
};


int __init mfld_add_bcm4330_bt_device(void)
{
	int ret = 0;
	ret = platform_device_register(&brcm_bluesleep_device);
	if(ret < 0)
	  return ret;
	return platform_device_register(&bcm4330_rfkill);
}

int __init broadcom_bt_init(void)
{
	int ret =0;

	printk(KERN_INFO "%s: start\n", __func__);
	bt_power = get_gpio_by_name(BCM4330_BT_SFI_GPIO_ENABLE_NAME);
	bt_reset = get_gpio_by_name(BCM4330_BT_SFI_GPIO_RESET_NAME);

	/*Broadcom bluetooth host_wake gpio to irq line*/
	bluesleep_resources[2].start = gpio_to_irq(BT_HOST_WAKE);
	bluesleep_resources[2].end = gpio_to_irq(BT_HOST_WAKE);

	ret = gpio_request(bt_power, "brcm_btpower");
	if (unlikely(ret))
	{
printk("brcm_btpower request fail\n")	;
		return ret;
	}
	ret = gpio_request(bt_reset, "brcm_reset");
	if (unlikely(ret))
	{
printk("brcm_reset request fail\n")	;
		return ret;
	}
	ret = mfld_add_bcm4330_bt_device();

	return ret;
}
device_initcall(broadcom_bt_init);
