/*
 * GPS BCM4751 interface.
 *
 * (C) Copyright 2009 BYD.
 * All Rights Reserved
 */
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/atomisp_platform.h>
#include <asm/intel_scu_ipcutil.h>
#include <media/v4l2-subdev.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include "gps_bcm4751.h"
#include <asm/intel-mid.h>


#include <linux/serial_core.h>
#include <linux/pm_runtime.h>
#include <linux/serial_mfd.h>
#include <asm/intel_mid_hsu.h>
#include <linux/mfd.h>


#define GPS_TRACE    1
#ifdef GPS_TRACE
#define GPS_DEBUG(fmt,args...) printk(fmt, ## args) //default level KERN_WARNING
#else
#define GPS_DEBUG(fmt,args...)
#endif
//GPS
//#define GPIO_AGPS_POWER_EN           31
//#define GPIO_AGPS_PON                62
//#define GPIO_AGPS_RESET_N            63
static int GPIO_AGPS_PON;
static int GPIO_AGPS_RESET_N;
//////////////////////////////////////////////////////////////////////////////////////


extern struct hsu_port * get_hsu_p(void);

void bcm_get_runtimpm(void)
{
    struct hsu_port * hsu_p= get_hsu_p();
    struct uart_hsu_port *up =& hsu_p->port[2];
	
	printk("sdl gps_prwer_on\n");
	if(hsu_p!=NULL)
	{
    	pm_runtime_get(up->dev);
	}
}

void bcm_put_runtimpm(void)
{
   struct hsu_port * hsu_p= get_hsu_p();
    struct uart_hsu_port *up =& hsu_p->port[2];

    printk("sdl gps_power_off \n");
	if(hsu_p!=NULL)
	{
        pm_runtime_put(up->dev);
	}
}
static int gps_power_on(void)
{
    //config gps gpio direction
//    gpio_set_value(GPIO_AGPS_POWER_EN,1);
    mdelay(10);
    gpio_set_value(GPIO_AGPS_RESET_N,0);
    mdelay(2);
    gpio_set_value(GPIO_AGPS_RESET_N,1);
    mdelay(20);
    gpio_set_value(GPIO_AGPS_PON,0);
    mdelay(100);
    gpio_set_value(GPIO_AGPS_PON,1);
    mdelay(20);
    printk("sdl  %s :bcm4751 gps chip powered on\n", __func__);
    return 0;
}

static int gps_power_off(void)
{
    //config gps gpio direction
//    gpio_set_value(GPIO_AGPS_POWER_EN,1);
    mdelay(1);
    gpio_set_value(GPIO_AGPS_RESET_N,1);
    mdelay(100);
    gpio_set_value(GPIO_AGPS_PON,0);
    mdelay(100);
//    gpio_set_value(GPIO_AGPS_POWER_EN,0);
    printk("%s :bcm4751 gps chip powered off\n", __func__);
    return 0;
}

static int gps_reset(int flag)
{
//    gpio_set_value(GPIO_AGPS_POWER_EN,1);
    mdelay(1);
    gpio_set_value(GPIO_AGPS_RESET_N,1);
    gpio_set_value(GPIO_AGPS_RESET_N,0);
    mdelay(10);
    gpio_set_value(GPIO_AGPS_RESET_N,1);
    mdelay(100);
    gpio_set_value(GPIO_AGPS_PON,1);
    printk("%s :bcm4751 gps chip reset\n", __func__);
    return 0;
}

static int gps_on_off(int flag)
{
	printk("%s :bcm4751 gps chip offon\n", __func__);
    return 0;
}

#ifdef	CONFIG_PROC_FS
#define PROC_PRINT(fmt, args...) 	do {len += sprintf(page + len, fmt, ##args); } while(0)

static char bcm4751_status[4] = "off";
static ssize_t bcm4751_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int len = strlen(bcm4751_status);
GPS_DEBUG("bcm4751_read_proc\r\n");
	sprintf(page, "%s\n", bcm4751_status);
	return len + 1;
}

static ssize_t bcm4751_write_proc(struct file *filp,
		const char *buff, size_t len, loff_t *off)
{
	char messages[256];
	int flag, ret;
	char buffer[7];

	if (len > 256)
		len = 256;

	if (copy_from_user(messages, buff, len))
		return -EFAULT;
printk("sdl bcm4751_write_proc messages=%s\n",messages);
	if (strncmp(messages, "off", 3) == 0) {
		strcpy(bcm4751_status, "off");
		gps_power_off();
	} else if (strncmp(messages, "on", 2) == 0) {
		strcpy(bcm4751_status, "on");
		gps_power_on();
	} else if (strncmp(messages, "reset", 5) == 0) {
		strcpy(bcm4751_status, messages);
		ret = sscanf(messages, "%s %d", buffer, &flag);
		if (ret == 2)
			gps_reset(flag);
	} else if (strncmp(messages, "bcm4751on", 5) == 0) {
		strcpy(bcm4751_status, messages);
		ret = sscanf(messages, "%s %d", buffer, &flag);
		if (ret == 2)
			gps_on_off(flag);
	} else {
		printk("usage: echo {on/off} > /proc/driver/bcm4751\n");
	}

	return len;
}

static void create_bcm4751_proc_file(void)
{
	struct proc_dir_entry *bcm4751_proc_file = 
		create_proc_entry("driver/bcm4751", 0644, NULL);

	if (bcm4751_proc_file) {
		bcm4751_proc_file->read_proc = bcm4751_read_proc;
		bcm4751_proc_file->write_proc = bcm4751_write_proc;
	} else 
		printk(KERN_INFO "proc file create failed!\n");
}
#endif



//////////////////////////////////////////////////////////////////////////////////////////////////



static int gps_bcm4751_open(struct inode *inode, struct file *filp)
{
    //do nothing
    GPS_DEBUG("gps_bcm4751: file open\r\n");
    return 0;
}

static int gps_bcm4751_close(struct inode *inode, struct file *filp)
{
    //do nothing
    GPS_DEBUG("gps_bcm4751: file close\r\n");
    return 0;
}

static int gps_bcm4751_ioctl( struct file *filp,
			     unsigned int cmd, unsigned long arg)
{
    unsigned int data = 0;

    // Check type and command number
    GPS_DEBUG("gps_bcm4751: ioctl cmd magic = %d,cmd number = %d\r\n",_IOC_TYPE(cmd),_IOC_NR(cmd));
    if (_IOC_TYPE(cmd) != GPS_IOC_MAGIC) {
        return -ENOTTY;
    }
    if (_IOC_NR(cmd) > GPS_IOC_MAXNR) {
        return -ENOTTY;
    }

    // check cmd direction. if write,copy data from user space
    GPS_DEBUG("gps_bcm4751: ioctl cmd direction = %d\r\n",_IOC_DIR(cmd));
    if (_IOC_DIR(cmd) & _IOC_WRITE) {
        //if arg is a ptr,used copy_from_user
        #if 0
        data = arg;
        #else
        if (copy_from_user(&data, (int *)arg, sizeof(int))) {
            return -EFAULT;
        }
        #endif           
        GPS_DEBUG("gps_bcm4751: get arg right\r\n");
    }
printk("gps_bcm4751_ioctl cmd=0x%x\n",cmd);
    switch (cmd) {
        case GPS_IOCWRITEGPIO:	//set gpio
            GPS_DEBUG("gps_bcm4751: ioctl cmd data = %d\r\n",data);
            if(data & 0x01) {
                data = (data & 0x02) >> 1;                      
                gpio_set_value(GPIO_AGPS_PON,data);
            }
            else {
                gpio_set_value(GPIO_AGPS_RESET_N,data);
            }
            break;

        case GPS_IOCREADGPIO:	//read gpio
            GPS_DEBUG("gps_bcm4751: ioctl cmd read\r\n");
            //do nothing,if need,add it
            break;

        default:
            GPS_DEBUG("gps_bcm4751: ioctl cmd default\r\n");
            return -EFAULT;
    }
    
    return 0;
}
//add by lxn
static ssize_t bcm4751_write_standby_proc(struct file *filp,
		const char *buff, size_t len, loff_t *off)
{
	char messages[256]; 

	if (copy_from_user(messages, buff, 1))
		return -EFAULT;	
	
	if(strncmp(messages, "1", 1) == 0){
		gpio_set_value(GPIO_AGPS_PON,1);
		GPS_DEBUG("gpio_set_value(GPIO_AGPS_PON,1)\r\n");
	bcm_get_runtimpm();
printk("sdl bcm4751_write_standby_proc messages=1\n");
	}
	else if(strncmp(messages, "0", 1) == 0){
		gpio_set_value(GPIO_AGPS_PON,0);
	bcm_put_runtimpm();
printk("sdl bcm4751_write_standby_proc messages=0\n");
		GPS_DEBUG("gpio_set_value(GPIO_AGPS_PON,0)\r\n");
	}
mdelay(100);
	return len;
}
static ssize_t bcm4751_read_standby_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int len = gpio_get_value(GPIO_AGPS_PON);
	GPS_DEBUG("bcm4751_read_standby_proc:len=%x\r\n", len);
	if (len != 0)
	{
		len = 1;
	}

	len = sprintf(page, "%d\n",len);
	return len;
}
static ssize_t bcm4751_write_reset_proc(struct file *filp,
		const char *buff, size_t len, loff_t *off)
{
	char messages[256];

	if (copy_from_user(messages, buff, 1))
		return -EFAULT;

printk("sdl bcm4751_write_reset_proc messages=%s\n",messages);
	if(strncmp(messages, "1", 1) == 0){	
		gpio_set_value(GPIO_AGPS_RESET_N,1);
		GPS_DEBUG("gpio_set_value(GPIO_AGPS_RESET_N,1)\r\n");
	}
	else if(strncmp(messages, "0", 1) == 0){
		gpio_set_value(GPIO_AGPS_RESET_N,0);
		GPS_DEBUG("gpio_set_value(GPIO_AGPS_RESET_N,0)\r\n");
	}

	return len;
}
static ssize_t bcm4751_read_reset_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int len = gpio_get_value(GPIO_AGPS_RESET_N);
	GPS_DEBUG("bcm4751_read_reset_proc:len=%x\r\n", len);
	if (len != 0)
	{
		len = 1;
	}

	len = sprintf(page, "%d\n",len);
	return len;
}
static void create_bcm4751_proc_standby_file(void)
{
	struct proc_dir_entry *bcm4751_proc_file = 
		create_proc_entry("driver/bcms", 0644, NULL);

	if (bcm4751_proc_file) {
		bcm4751_proc_file->read_proc = bcm4751_read_standby_proc;
		bcm4751_proc_file->write_proc = bcm4751_write_standby_proc;
	} else 
		printk(KERN_INFO "proc standby file create failed!\n");
}


static void create_bcm4751_proc_reset_file(void)
{
	struct proc_dir_entry *bcm4751_proc_file = 
		create_proc_entry("driver/bcmr", 0644, NULL);

	if (bcm4751_proc_file) {
		bcm4751_proc_file->read_proc = bcm4751_read_reset_proc;
		bcm4751_proc_file->write_proc = bcm4751_write_reset_proc;
	} else 
		printk(KERN_INFO "proc reset file create failed!\n");
}
//end by lxn
static const struct file_operations gps_bcm4751_fops = {
	.owner = THIS_MODULE,
	.open = gps_bcm4751_open,	/* open */
	.release = gps_bcm4751_close,	/* release */
	.unlocked_ioctl = gps_bcm4751_ioctl,	/* ioctl */
};

static struct miscdevice gps_bcm4751_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = GPS_DRIVER_NAME,
    .fops  = &gps_bcm4751_fops,
};

static int gps_bcm4751_probe(struct platform_device *pdev)
{
    int result = 0;
    int ret;
    printk("sdl --- gps_bcm4751_probe\n");
    GPS_DEBUG("gps_bcm4751: gps bcm4751 probe\r\n");
    GPIO_AGPS_PON = get_gpio_by_name("GPS_PON");  
	if (GPIO_AGPS_PON == -1) {
		printk("gps_bcm4751_probe GPIO_AGPS_PON error\n");
		return -EINVAL;
	}
	ret = gpio_request(GPIO_AGPS_PON, "GPS_PON");
		if (ret) {
		pr_err("gps_bcm4751_probe failed to request GPS_PON \n");
		return -EINVAL;
	}

    
    GPIO_AGPS_RESET_N = get_gpio_by_name("GPS_RESET_N");    
	if (GPIO_AGPS_RESET_N == -1) {
		printk("gps_bcm4751_probe GPIO_AGPS_RESET_N error\n");
		return -EINVAL;
	}
	ret = gpio_request(GPIO_AGPS_RESET_N, "GPS_RESET_N");
		if (ret) {
		pr_err("gps_bcm4751_probe failed to request GPS_RESET_N \n");
		return -EINVAL;
	}
    
    
//bcm_get_runtimpm();
    gpio_direction_output(GPIO_AGPS_RESET_N, 1);
    gpio_direction_output(GPIO_AGPS_PON, 1);
    result = misc_register(&gps_bcm4751_miscdev);
    if (result) {
    	GPS_DEBUG("gps_bcm4751: misc register fail, result=%d\r\n", result);
    	return result;
    }
//    gpio_set_value(GPIO_AGPS_POWER_EN,1);
    //gps bcm4751 reset
    GPS_DEBUG("gps_bcm4751: reset gps\r\n"); 
    gpio_set_value(GPIO_AGPS_RESET_N,1);
    gpio_set_value(GPIO_AGPS_RESET_N,0);
    mdelay(50);
    gpio_set_value(GPIO_AGPS_RESET_N,1);
    mdelay(20);
   // uart2_gps_mfp_config();
    mdelay(20);
    //gps default standby mode
    GPS_DEBUG("gps_bcm4751: default set gps to standby mode\r\n"); 
    gpio_set_value(GPIO_AGPS_PON,0);
    GPS_DEBUG("create proc node of gps_bcm4751\r\n"); 
    create_bcm4751_proc_file();	
    create_bcm4751_proc_standby_file();
    create_bcm4751_proc_reset_file();
    return 0;
}

static int gps_bcm4751_remove(struct platform_device *pdev)
{
	misc_deregister(&gps_bcm4751_miscdev);

	return 0;
}

static struct platform_driver gps_bcm4751_driver = {
    .driver = {
        .name	= GPS_DRIVER_NAME,
    },
    .probe		= gps_bcm4751_probe,
    .remove		= gps_bcm4751_remove,
};


static int __init gps_bcm4751_init(void)
{

	return platform_driver_register(&gps_bcm4751_driver);
}

static void __exit gps_bcm4751_exit(void)
{
	platform_driver_unregister(&gps_bcm4751_driver);
}

module_init(gps_bcm4751_init);
module_exit(gps_bcm4751_exit);

MODULE_LICENSE("GPL");

