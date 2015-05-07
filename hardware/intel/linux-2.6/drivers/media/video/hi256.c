/*
 * Support for hi256 Camera Sensor.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <linux/gpio.h>

#include "hi256.h"

#define to_hi256_sensor(sd) container_of(sd, struct hi256_device, sd)

/*
 * TODO: use debug parameter to actually define when debug messages should
 * be printed.
 */
static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

int dbg_stream;
module_param(dbg_stream, int, 0644);
MODULE_PARM_DESC(dbg_stream, "debug message on/off (default:off)");

static int hi256_t_vflip(struct v4l2_subdev *sd, int value);
static int hi256_t_hflip(struct v4l2_subdev *sd, int value);
static int
hi256_read_reg(struct i2c_client *client, unsigned char reg,unsigned char *value)
{
	int err;
	struct i2c_msg msg;
	u8 data = reg;
	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	/*
	 * Send out the register address...
	 */
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = &data;
	err = i2c_transfer(client->adapter, &msg, 1);
	if (err < 0) {
		printk(KERN_ERR "Error %d on register write\n", err);
		return err;
	}
	/*
	 * ...then read back the result.
	 */
	msg.flags = I2C_M_RD;
	err = i2c_transfer(client->adapter, &msg, 1);
	if (err >= 0) {
		*value = data;
		err = 0;
	}
	return err;
}

static int
hi256_write_reg(struct i2c_client *client, unsigned char reg,unsigned char value)
{
	int err;
	struct i2c_msg msg;
	unsigned char data[2] = { reg, value };
	int retry = 0;
	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}
again:
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2 ;
	msg.buf = data;
	err = i2c_transfer(client->adapter, &msg, 1);

	/*
	 * HACK: Need some delay here for Rev 2 sensors otherwise some
	 * registers do not seem to load correctly.
	 */
	//mdelay(2);
	if (err>=0){
	if (reg == REG_COM7 && (value & COM7_RESET))
		msleep(4);  /* Wait for reset to run */
		return 0;
		
		}
	dev_err(&client->dev, "write register err reg=0x%x error= %d \n", reg, err);
	
	if (retry <= I2C_RETRY_COUNT) {
		dev_err(&client->dev, "0v7692  retrying i2c write transfer... %d",
			retry);
		retry++;
		msleep(10);
		goto again;
	}

	return err;

}


/*
 * hi256_write_reg_array - Initializes a list of hi256 registers
 * @client: i2c driver client structure
 * @vals: list of registers to be written
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * hi256_write_reg_array() and should be not used anywhere else.
 *
 */
static int hi256_write_reg_array(struct i2c_client *client,
				struct regval_list *vals)
{
	int tmp = 0;//lwl
	while ((vals->reg_num != 0xff) ||( vals->value != 0xff)) {
		int ret = hi256_write_reg(client, vals->reg_num, vals->value);

		//hi256_read_reg(client, vals->reg_num,&tmp);
		//printk("==lwl== reg  %02x = %02x\n",vals->reg_num,tmp);
		if (ret < 0)
			return ret;
		vals++;
		
	}
	return 0;
}


static int hi256_set_suspend(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	printk("%s  \n",__func__);
	
	//return 0;//lwl
	return hi256_write_reg_array(client, hi256_suspend);
}

static int hi256_set_streaming(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	printk("%s  \n",__func__);
	//return 0;//lwl
	return hi256_write_reg_array(client, hi256_streaming);
}

static int hi256_init_common(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	ret = hi256_write_reg_array(client, hi256_common);
	printk("%s  \n",__func__);

	if (ret<0)
		dev_err(&client->dev, "hi256_init_common err");
	else
		ret=0;
	return ret;
}

static int power_up(struct v4l2_subdev *sd)
{
	struct hi256_device *dev = to_hi256_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (NULL == dev->platform_data) {
		dev_err(&client->dev, "no camera_sensor_platform_data");
		return -ENODEV;
	}

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 1);
	
	if (ret)
		goto fail_power;

	/* flis clock control */
	ret = dev->platform_data->flisclk_ctrl(sd, 1);
	if (ret)
		goto fail_clk;

	mdelay(10);
	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 1);
	if (ret)
		dev_err(&client->dev, "gpio failed 1\n");
	/*
	 * according to DS, 44ms is needed between power up and first i2c
	 * commend
	 */
	msleep(50);
	printk("hi256_power_up_suc  \n");
	return 0;
fail_clk:
	dev->platform_data->flisclk_ctrl(sd, 0);
fail_power:
	dev->platform_data->power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct hi256_device *dev = to_hi256_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (NULL == dev->platform_data) {
		dev_err(&client->dev, "no camera_sensor_platform_data");
		return -ENODEV;
	}

	ret = dev->platform_data->gpio_ctrl(sd, 0); //reset
	mdelay(10);
	/* gpio ctrl */

	if (ret)
		dev_err(&client->dev, "gpio failed 1\n");

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "power failed.\n");
	
	ret = dev->platform_data->flisclk_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "flisclk failed\n");

	/*according to DS, 20ms is needed after power down*/
	msleep(20);
	printk("hi256_power_down_suc  \n");
	return ret;
}

static int hi256_s_power(struct v4l2_subdev *sd, int power)
{
	int ret = -1;
	if (power == 0)
		return power_down(sd);
	else {
		if (power_up(sd))
			return -EINVAL;
	printk("==lwl== add stream off after power up for camera stream \n");
	ret = hi256_set_suspend(sd);
	if (ret) {
		printk("==lwl== set suspend failed\n");
		return ret;
	}
        msleep(2);
		return hi256_init_common(sd);
	}
}

static int hi256_try_res(u32 *w, u32 *h)
{
	int i;
	/*
	 * The mode list is in ascending order. We're done as soon as
	 * we have found the first equal or bigger size.
	 */
	for (i = 0; i < N_RES; i++) {
		if ((hi256_res[i].width >= *w) &&
		    (hi256_res[i].height >= *h))
			break;
	}
	/*
	 * If no mode was found, it means we can provide only a smaller size.
	 * Returning the biggest one available in this case.
	 */
	if (i == N_RES)
		i--;

	*w = hi256_res[i].width;
	*h = hi256_res[i].height;
	printk("hi256_try_res  w = %d  h = %d\n",(*w),(*h));

	return 0;
}

static struct hi256_res_struct *hi256_to_res(u32 w, u32 h)
{
	int  index;

	for (index = 0; index < N_RES; index++) {
		if ((hi256_res[index].width == w) &&
		    (hi256_res[index].height == h))
			break;
	}

	/* No mode found */
	if (index >= N_RES)
		return NULL;
	return &hi256_res[index];
}

static int hi256_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	return hi256_try_res(&fmt->width, &fmt->height);
}

static int hi256_res2size(unsigned int res, int *h_size, int *v_size)
{
	unsigned short hsize;
	unsigned short vsize;
	printk("hi256_res2size  res= %d\n",res);
	switch (res) {
	case HI256_RES_QCIF:
		hsize = HI256_RES_QCIF_SIZE_H;
		vsize = HI256_RES_QCIF_SIZE_V;
		break;
	case HI256_RES_CIF:
		hsize = HI256_RES_CIF_SIZE_H;
		vsize = HI256_RES_CIF_SIZE_V;
		break;
	case HI256_RES_QVGA:
		hsize = HI256_RES_QVGA_SIZE_H;
		vsize = HI256_RES_QVGA_SIZE_V;
		break;
	case HI256_RES_VGA:
		hsize = HI256_RES_VGA_SIZE_H;
		vsize = HI256_RES_VGA_SIZE_V;
		break;
	default:
		WARN(1, "%s: Resolution 0x%08x unknown\n", __func__, res);
		return -EINVAL;
	}

	if (h_size != NULL)
		*h_size = hsize;
	if (v_size != NULL)
		*v_size = vsize;

	return 0;
}

static int hi256_get_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct hi256_device *dev = to_hi256_sensor(sd);
	int width, height;
	int ret;
	ret = hi256_res2size(dev->res, &width, &height);
	if (ret)
		return ret;
	fmt->width = width;
	fmt->height = height;
	printk("hi256_get_mbus_fmt  width = %d  height = %d\n",width,height);
	return 0;
}

static int hi256_set_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct hi256_device *dev = to_hi256_sensor(sd);
	struct hi256_res_struct *res_index;
	u32 width = fmt->width;
	u32 height = fmt->height;
	int ret;

	hi256_try_res(&width, &height);
	res_index = hi256_to_res(width, height);

	/* Sanity check */
	if (unlikely(!res_index)) {
		WARN_ON(1);
		return -EINVAL;
	}
	printk("  hi256_set_mbus_fmt = %d  width=%d   height=%d\n",(res_index->res),width,height);
	
#if 0 //lwl
	switch (res_index->res) {
	case HI256_RES_QCIF:
		ret = hi256_write_reg_array(c, hi256_qcif_init);
		break;
	case HI256_RES_CIF:
		ret = hi256_write_reg_array(c, hi256_cif_init);
		break;
	case HI256_RES_QVGA:
		ret = hi256_write_reg_array(c, hi256_qvga_init);
		break;
	case HI256_RES_VGA:
		ret = hi256_write_reg_array(c, hi256_vga_init);
		break;
	default:
		v4l2_err(sd, "set resolution: %d failed!\n", res_index->res);
		return -EINVAL;
	}
#endif
	/*
	 * hi256 - we don't poll for context switch
	 * because it does not happen with streaming disabled.
	 */
	dev->res = res_index->res;

	fmt->width = width;
	fmt->height = height;
printk("==lwl== set fmt done\n");
	return 0;
}

/* TODO: Update to SOC functions, remove exposure and gain */
static int hi256_g_focal(struct v4l2_subdev *sd, s32 * val)
{
	*val = (HI256_FOCAL_LENGTH_NUM << 16) | HI256_FOCAL_LENGTH_DEM;
	return 0;
}

static int hi256_g_fnumber(struct v4l2_subdev *sd, s32 * val)
{
	/*const f number for hi256*/
	*val = (HI256_F_NUMBER_DEFAULT_NUM << 16) | HI256_F_NUMBER_DEM;
	return 0;
}

static int hi256_g_fnumber_range(struct v4l2_subdev *sd, s32 * val)
{
	*val = (HI256_F_NUMBER_DEFAULT_NUM << 24) |
		(HI256_F_NUMBER_DEM << 16) |
		(HI256_F_NUMBER_DEFAULT_NUM << 8) | HI256_F_NUMBER_DEM;
	return 0;
}

/* Horizontal flip the image. */
static int hi256_g_hflip(struct v4l2_subdev *sd, s32 * val)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	int ret;
	unsigned char data;
	ret = hi256_read_reg(c, REG_MVFP, &data);
	*val = (data & MVFP_MLIP) == MVFP_MLIP;
	return ret;
}

static int hi256_g_vflip(struct v4l2_subdev *sd, s32 * val)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	int ret;
	unsigned char data;

	ret = hi256_read_reg(c, REG_MVFP, &data);
	*val = (data & MVFP_FLIP) == MVFP_FLIP;
	return ret;
}

static int hi256_s_freq(struct v4l2_subdev *sd, s32  val)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct hi256_device *dev = to_hi256_sensor(sd);
	int ret;
	unsigned char temp;
	if (val != HI256_FLICKER_MODE_50HZ &&
			val != HI256_FLICKER_MODE_60HZ)
		return -EINVAL;

	if (val == HI256_FLICKER_MODE_50HZ) {
		ret = hi256_read_reg(c, 0x14, &temp);
		temp=temp|0x01; //bit[1]=1, 50hz
		ret+=hi256_write_reg(c,0x14,temp);
		
		if (ret < 0)
			return ret;
	} else {
		ret = hi256_read_reg(c, 0x14, &temp);
		temp=temp&0xfe; //bit[0]=0, 60hz
		
		ret+=hi256_write_reg(c,0x14,temp);
		if (ret < 0)
			return ret;
	}

	if (ret == 0)
		dev->lightfreq = val;

	return ret;
}



static struct hi256_control hi256_controls[] = {
	{
		.qc = {
			.id = V4L2_CID_VFLIP,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Image v-Flip",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.query = hi256_g_vflip,
		.tweak = hi256_t_vflip,
	},
	{
		.qc = {
			.id = V4L2_CID_HFLIP,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Image h-Flip",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.query = hi256_g_hflip,
		.tweak = hi256_t_hflip,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = HI256_FOCAL_LENGTH_DEFAULT,
			.maximum = HI256_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = HI256_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = hi256_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = HI256_F_NUMBER_DEFAULT,
			.maximum = HI256_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = HI256_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = hi256_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = HI256_F_NUMBER_RANGE,
			.maximum =  HI256_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = HI256_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = hi256_g_fnumber_range,
	},
	{
		.qc = {
			.id = V4L2_CID_POWER_LINE_FREQUENCY,
			.type = V4L2_CTRL_TYPE_MENU,
			.name = "Light frequency filter",
			.minimum = 1,
			.maximum =  2, /* 1: 50Hz, 2:60Hz */
			.step = 1,
			.default_value = 1,
			.flags = 0,
		},
		.tweak = hi256_s_freq,
	},
		
};
#define N_CONTROLS (ARRAY_SIZE(hi256_controls))

static struct hi256_control *hi256_find_control(__u32 id)
{
	int i;
	for (i = 0; i < N_CONTROLS; i++) {
		if (hi256_controls[i].qc.id == id)
			return &hi256_controls[i];
	}
	printk("%s: the contrl is unkown  ID=0x%x\n", __func__,id);
	return NULL;
}

static int hi256_detect(struct hi256_device *dev, struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	unsigned char high,low;
	int ret = 0,i=0;
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: i2c error", __func__);
		return -ENODEV;
	}
#if 0
	
	ret = hi256_read_reg(client, HI256_MIDH, &retvalue);
	//printk("hi256_detect   0x1c==0x%x\n", retvalue);
	if (ret < 0)
		return ret;
	if (retvalue != 0x7f) /* OV manuf. id. */
	{
		dev_err(&client->dev, "%s: failed: client->addr = %x\n",
			__func__, client->addr);

		return -ENODEV;}
	ret = hi256_read_reg(client, HI256_MIDL, &retvalue);
	//printk("hi256_detect   0x1d==0x%2x\n ", retvalue);
	if (ret < 0)
		return ret;
	if (retvalue != 0xa2){
		dev_err(&client->dev, "%s: failed: client->addr = %x\n",
			__func__, client->addr);
		return -ENODEV;}
	hi256_read_reg(client, HI256_PID_REG, &retvalue);
	dev->real_model_id = retvalue;

	if (retvalue != HI256_PID) {
		dev_err(&client->dev, "%s: failed: client->addr = %x\n",
			__func__, client->addr);
		return -ENODEV;
	}
	/*
	 * OK, we know we have an OmniVision chip...but which one?
	 */
#endif
	for(i=0;i<I2C_RETRY_COUNT;i++){
             ret = hi256_read_reg(client, HI256_PIDH, &high);
			 printk("==lwl== hi256 detect chip id = %d\n",high);
             if((high == 0xC))
             break;
	}
	#if 0
	if (ret < 0)
	     return ret;
	if (high != 0x76)  
	{
              dev_err(&client->dev, "%s: failed: client->addr = %x 0x0a==0x%x\n",
              __func__, client->addr,high);
              return -ENODEV;
	}

	for(i=0;i<I2C_RETRY_COUNT;i++){
             ret = hi256_read_reg(client, HI256_PIDL, &low);
             if(low == 0x92)
             break;
	}
	if (ret < 0)
	    return ret;
	if (low != 0x92)  /* PID + VER = 0x76 / 0x92 */
	{
            dev_err(&client->dev, "%s: failed: client->addr = %x  0x0b==0x%x\n",
            __func__, client->addr,low);
            return -ENODEV;
	}

	dev->real_model_id =0x7692;
	printk("hi256_detect  suc \n ");
	#endif

	return 0;
}

static int
hi256_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct hi256_device *dev = to_hi256_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (NULL == platform_data)
		return -ENODEV;

	dev->platform_data =
	    (struct camera_sensor_platform_data *)platform_data;

	ret = hi256_s_power(sd, 1);
	if (ret) {
		v4l2_err(client, "hi256 power-up err");
		return ret;
	}

	/* config & detect sensor */
	ret = hi256_detect(dev, client);
	if (ret) {
		v4l2_err(client, "hi256_detect err s_config.\n");
		goto fail_detect;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;
/*
	ret = hi256_set_suspend(sd);
	if (ret) {
		v4l2_err(client, "hi256 suspend err");
		return ret;
	}
*/
//#if 0 //lwl modify
	ret = hi256_s_power(sd, 0);
	if (ret) {
		v4l2_err(client, "hi256 power down err");
		return ret;
	}

//#endif
	return 0;

fail_csi_cfg:
	dev->platform_data->csi_cfg(sd, 0);
fail_detect:
	hi256_s_power(sd, 0);
	dev_err(&client->dev, "sensor power-gating failed\n");
	return ret;
}

static int hi256_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct hi256_control *ctrl = hi256_find_control(qc->id);
	printk("hi256_queryctrl  \n");
	if (ctrl == NULL)
		return -EINVAL;
	*qc = ctrl->qc;
	return 0;
}

/* Horizontal flip the image. */
static int hi256_t_hflip(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	int err;
	unsigned char v = 0;
		//pr_err("hi256_t_hflip%s:  value==%d\n", __func__,value);
	err = hi256_read_reg(c, REG_MVFP, &v);
	if (value)
		v &= ~MVFP_MLIP;
	else
		v |= MVFP_MLIP;
	msleep(1);  /* FIXME */
	err += hi256_write_reg(c, REG_MVFP, v);
	return err;
}

/* Vertically flip the image */
static int hi256_t_vflip(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	int err;
	unsigned char v = 0;

		//pr_err("hi256_t_vflip%s: value==%d\n", __func__,value);

	err = hi256_read_reg(c, REG_MVFP, &v);
	if (value)
		v &= ~MVFP_FLIP;
	else
		v |= MVFP_FLIP;
	msleep(1);  /* FIXME */
	err += hi256_write_reg(c, REG_MVFP, v);
	return err;
}

static int hi256_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct hi256_control *octrl = hi256_find_control(ctrl->id);
	int ret;

	if (octrl == NULL)
		return -EINVAL;
	printk("hi256_g_ctrl  \n");
	ret = octrl->query(sd, &ctrl->value);
	if (ret < 0)
		return ret;

	return 0;
}

static int hi256_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct hi256_control *octrl = hi256_find_control(ctrl->id);
	int ret;
	printk("hi256_s_ctrl  octrl->qc.name=%s\n",octrl->qc.name);
	if (!octrl || !octrl->tweak)
		return -EINVAL;

	ret = octrl->tweak(sd, ctrl->value);
	if (ret < 0)
		return ret;

	return 0;
}

static int hi256_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	printk("==lwl== %s come in enable fengying = %d\n",__func__,enable);
	if (dbg_stream == 1) {
		printk("==lwl==stream will be returned for debug\n");
		return 0;
	}
	if (enable) {
		ret = hi256_set_streaming(sd);

	} else {
		ret = hi256_set_suspend(sd);
	}

	return ret;
}

static int
hi256_enum_framesizes(struct v4l2_subdev *sd, struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;
	printk("hi256_enum_framesizes  \n");
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = hi256_res[index].width;
	fsize->discrete.height = hi256_res[index].height;

	/* FIXME: Wrong way to know used mode */
	fsize->reserved[0] = hi256_res[index].used;

	return 0;
}

static int hi256_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	unsigned int index = fival->index;
	int i;

	if (index >= N_RES)
		return -EINVAL;
	printk("hi256_enum_frameintervals  \n");
	/* find out the first equal or bigger size */
	for (i = 0; i < N_RES; i++) {
		if ((hi256_res[i].width >= fival->width) &&
		    (hi256_res[i].height >= fival->height))
			break;
	}
	if (i == N_RES)
		i--;

	index = i;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = hi256_res[index].fps;

	return 0;
}

static int
hi256_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	printk("hi256_g_chip_ident  \n");
	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV7692, 0);
}

static int hi256_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= MAX_FMTS)
		return -EINVAL;
	code->code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

static int hi256_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh,
	struct v4l2_subdev_frame_size_enum *fse)
{

	unsigned int index = fse->index;


	if (index >= N_RES)
		return -EINVAL;
	printk("hi256_enum_frame_size  \n");
	fse->min_width = hi256_res[index].width;
	fse->min_height = hi256_res[index].height;
	fse->max_width = hi256_res[index].width;
	fse->max_height = hi256_res[index].height;

	return 0;
}

static struct v4l2_mbus_framefmt *
__hi256_get_pad_format(struct hi256_device *sensor,
			 struct v4l2_subdev_fh *fh, unsigned int pad,
			 enum v4l2_subdev_format_whence which)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);

	if (pad != 0) {
		dev_err(&client->dev,  "%s err. pad %x\n", __func__, pad);
		return NULL;
	}
	printk("__hi256_get_pad_format  \n");
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &sensor->format;
	default:
		return NULL;
	}
}

static int
hi256_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct hi256_device *snr = to_hi256_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__hi256_get_pad_format(snr, fh, fmt->pad, fmt->which);

	if (format == NULL)
		return -EINVAL;
	fmt->format = *format;

	return 0;
}

static int
hi256_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct hi256_device *snr = to_hi256_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__hi256_get_pad_format(snr, fh, fmt->pad, fmt->which);

	if (format == NULL)
		return -EINVAL;

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		snr->format = fmt->format;

	return 0;
}

static int
hi256_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct hi256_device *dev = to_hi256_sensor(sd);

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(param, 0, sizeof(*param));
	param->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (dev->res >= 0 && dev->res < N_RES) {
		param->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
		param->parm.capture.timeperframe.numerator = 1;
		param->parm.capture.timeperframe.denominator =
			hi256_res[dev->res].fps;
	}
	return 0;
}

static int
hi256_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{

		 return hi256_g_parm(sd, param);
}

static int hi256_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	int index;
	struct hi256_device *snr = to_hi256_sensor(sd);

	if (frames == NULL)
		return -EINVAL;
	for (index = 0; index < N_RES; index++) {
		if (hi256_res[index].res == snr->res)
			break;
	}

	if (index >= N_RES)
		return -EINVAL;

	*frames = hi256_res[index].skip_frames;

	return 0;
}
static const struct v4l2_subdev_video_ops hi256_video_ops = {
	.try_mbus_fmt = hi256_try_mbus_fmt,
	.s_mbus_fmt = hi256_set_mbus_fmt,
	.g_mbus_fmt = hi256_get_mbus_fmt,
	.s_stream = hi256_s_stream,
	.enum_framesizes = hi256_enum_framesizes,
	.s_parm = hi256_s_parm,
	.g_parm = hi256_g_parm,
	.enum_frameintervals = hi256_enum_frameintervals,
};

static struct v4l2_subdev_sensor_ops hi256_sensor_ops = {
	.g_skip_frames	= hi256_g_skip_frames,
};

static const struct v4l2_subdev_core_ops hi256_core_ops = {
	.g_chip_ident = hi256_g_chip_ident,
	.queryctrl = hi256_queryctrl,
	.g_ctrl = hi256_g_ctrl,
	.s_ctrl = hi256_s_ctrl,
	.s_power = hi256_s_power,
};

/* REVISIT: Do we need pad operations? */
static const struct v4l2_subdev_pad_ops hi256_pad_ops = {
	.enum_mbus_code = hi256_enum_mbus_code,
	.enum_frame_size = hi256_enum_frame_size,
	.get_fmt = hi256_get_pad_format,
	.set_fmt = hi256_set_pad_format,
};

static const struct v4l2_subdev_ops hi256_ops = {
	.core = &hi256_core_ops,
	.video = &hi256_video_ops,
	.pad = &hi256_pad_ops,
	.sensor = &hi256_sensor_ops,
};

static const struct media_entity_operations hi256_entity_ops = {
	.link_setup = NULL,
};


static int hi256_remove(struct i2c_client *client)
{
	struct hi256_device *dev;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	dev = container_of(sd, struct hi256_device, sd);
	dev->platform_data->csi_cfg(sd, 0);

	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&dev->sd.entity);
	kfree(dev);

	return 0;
}

static int hi256_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct hi256_device *dev;
	int ret;
	printk("hi256_probe   begin\n ");
	/* Setup sensor configuration structure */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&dev->sd, client, &hi256_ops);
	if (client->dev.platform_data) {
		ret = hi256_s_config(&dev->sd, client->irq,
				       client->dev.platform_data);
		if (ret) {
			v4l2_device_unregister_subdev(&dev->sd);
			kfree(dev);
			return ret;
		}
	}

	/*TODO add format code here*/
	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;

	/* REVISIT: Do we need media controller? */
	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret) {
		hi256_remove(client);
		return ret;
	}

	/* set res index to be invalid */
	dev->res = -1;

	return 0;
}

MODULE_DEVICE_TABLE(i2c, hi256_id);

static struct i2c_driver hi256_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "hi256"
	},
	.probe = hi256_probe,
	.remove = hi256_remove,
	.id_table = hi256_id,
};

static __init int init_hi256(void)
{
printk("==lwl newest555-final== %s",__func__);
	return i2c_add_driver(&hi256_driver);
	
}

static __exit void exit_hi256(void)
{

	i2c_del_driver(&hi256_driver);
}

module_init(init_hi256);
module_exit(exit_hi256);

MODULE_AUTHOR("Shuguang Gong <Shuguang.gong@intel.com>");
MODULE_LICENSE("GPL");
