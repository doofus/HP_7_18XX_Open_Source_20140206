/*
 * platform_hi256.c: hi256 platform data initilization file
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
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/atomisp_platform.h>
#include <asm/intel-mid.h>
#include <asm/intel_scu_ipcutil.h>
#include <media/v4l2-subdev.h>
#include "platform_camera.h"
#include "platform_hi256.h"
#include "platform_mt9e013.h"
#include "platform_ov5640.h"
static int camera_1_pwr_en;
static int camera_power_down;
static int camera_vprog1_on;

static int camera_reset;
static int camera_power_down;
static int camera_ldo_en;

/*
 * MFLD PR2 secondary camera sensor - OV7692 platform data
 */
static int hi256_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;
printk("---%s flag = %d -----\n",__func__,flag);
	if (camera_reset < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_0_RESET,
					GPIOF_DIR_OUT, 0);
		if (ret < 0)
			return ret;
		camera_reset = ret;
	}
	/*
	if (camera_power_down < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_PWDN,
					GPIOF_DIR_OUT, 1);
		if (ret < 0)
			return ret;
		camera_power_down = ret;
	}
	*/
	if (flag) {
		/*
		msleep(5);
		gpio_set_value(camera_power_down, 0);
		*/
		//gpio_set_value(camera_reset, 0);
		msleep(20);
		gpio_set_value(camera_reset, 1);
	} else {
		gpio_set_value(camera_reset, 0);
		//gpio_set_value(camera_power_down, 1);
	}
	return 0;
}

static int hi256_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	static const unsigned int clock_khz = 19200;
	return intel_scu_ipc_osc_clk(OSC_CLK_CAM0, flag ? clock_khz : 0);
}

static int ov5640_reset_value;
static int hi256_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;

	/* Note here, there maybe a workaround to avoid I2C SDA issue */

printk("---%s flag = %d -----\n",__func__,flag);
	if (camera_ldo_en < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_PWR_EN,
					GPIOF_DIR_OUT, 1);
		if (ret < 0)
			return ret;
		camera_ldo_en = ret;
	}

	if (camera_power_down < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_PWDN,
					GPIOF_DIR_OUT, 1);
		if (ret < 0)
			return ret;
		camera_power_down = ret;
	}
	
	if (flag) {
		gpio_set_value(camera_ldo_en, 1);	
		msleep(10);
		gpio_set_value(camera_power_down, 0);
	} else {
		gpio_set_value(camera_power_down, 1);
		msleep(5);
		gpio_set_value(camera_power_down, 0);
		gpio_set_value(camera_ldo_en, 0);
	}

//end
	return 0;
}
void hi256_reset(struct v4l2_subdev *sd)
{
     hi256_power_ctrl(sd,0);
}

static int hi256_csi_configure(struct v4l2_subdev *sd, int flag)
{
	/* soc sensor, there is no raw bayer order (set to -1) */
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_PRIMARY, 1,
		ATOMISP_INPUT_FORMAT_YUV422_8, -1,
		//false, /*SOF */
		flag);
}

static struct camera_sensor_platform_data hi256_sensor_platform_data = {
	.gpio_ctrl	= hi256_gpio_ctrl,
	.flisclk_ctrl	= hi256_flisclk_ctrl,
	.power_ctrl	= hi256_power_ctrl,
	.csi_cfg	= hi256_csi_configure,
};

void *hi256_platform_data(void *info)
{
	camera_reset = -1;
	camera_power_down = -1;
	camera_ldo_en = -1;

	return &hi256_sensor_platform_data;
}

