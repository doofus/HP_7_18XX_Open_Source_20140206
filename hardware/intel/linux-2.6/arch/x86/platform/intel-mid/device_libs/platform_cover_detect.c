/*
 * platform_kpd_led_gpio.c: kpd_led_gpio platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/lnw_gpio.h>
#include <asm/intel-mid.h>
#include <asm/cover_detect.h>
#include "platform_cover_detect.h"

static int __init platform_cover_detect_init(void)
{
	int ret;
	struct platform_device *pdev;
	static struct cover_detect_pdata pdata;

	pdev = platform_device_alloc(DEVICE_NAME, -1);
	if (!pdev) {
		pr_err("===cover_detect Failed to create platform device: cover detect\n");
		return -ENOMEM;
	}
      /*
	ret = get_gpio_by_name("sim_control");
	if (ret == -1) {
		pr_err("===cover_detect Failed to get sim_control gpio pin from SFI table\n");
		platform_device_put(pdev);
		return ret;
	} else
		pdata.gpio_sim_control = ret;

	ret = get_gpio_by_name("cover_detect");
	if (ret == -1) {
		pr_err("===cover_detect Failed to get cover_detect gpio pin from SFI table\n");
		platform_device_put(pdev);
		return ret;
	} else
		pdata.gpio_cover_detect = ret;
*/
	pdev->dev.platform_data = &pdata;

	return platform_device_add(pdev);
}
device_initcall(platform_cover_detect_init);

