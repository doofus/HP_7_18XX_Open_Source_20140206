/*
 * leds-intel-kpd-gpio.c - Intel GPIO Keypad LED driver
 *
 * Copyright (C) 2013 BYD Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <asm/cover_detect.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

static int gpio_cover_detect;
static int gpio_sim_control;

//static struct work_struct wq_sim_control;
static struct delayed_work wq_sim_control;


static void sim_control_work(struct work_struct *work)
{
	int irq_detect_gpio_state;
	irq_detect_gpio_state = gpio_get_value(gpio_cover_detect);
	if (irq_detect_gpio_state == 0) {
		gpio_set_value(gpio_sim_control, 1);
		printk(KERN_INFO "===cover_detect irq says sim control should be open, reporting it\n");
	}
	else{
		gpio_set_value(gpio_sim_control, 0);
		printk(KERN_INFO "===cover_detect irq says sim control should be close, reporting it\n");
	}		
}

static irqreturn_t cover_detect_irq(int irq, void *_host)
{
	printk(KERN_INFO "===cover_detect cover detect irq\n");
	//schedule_work(&wq_sim_control);
	cancel_delayed_work_sync(&wq_sim_control);
	schedule_delayed_work(&wq_sim_control, HZ);
}
static int __devinit cover_detect_probe(struct platform_device *pdev)
{
	int ret;
	int gpio_cover_detect_state;
	struct cover_detect_pdata *pdata;

	printk(KERN_INFO "===cover_detect probe\n");

	pdata = pdev->dev.platform_data;
	if (!pdata)
		return -EINVAL;

	//gpio_sim_control = pdata->gpio_sim_control;
	gpio_sim_control = 116;
	if (gpio_sim_control < 0)
		return -EINVAL;

	ret = gpio_request(gpio_sim_control, "sim_control");
	if (ret)
		return ret;

	ret = gpio_direction_output(gpio_sim_control, 0);
	if (ret) {
		gpio_free(gpio_sim_control);
		return ret;
	}

	//gpio_cover_detect = pdata->gpio_cover_detect;
	gpio_cover_detect = 72;
	if (gpio_cover_detect < 0)
		return -EINVAL;

  	ret = gpio_request(gpio_cover_detect, "cover_detect");
	 if (ret)
		   return ret;
	   
	 ret = gpio_direction_input(gpio_cover_detect);
	 if (ret) {
		   gpio_free(gpio_cover_detect);
		   return ret;
	   }
	mdelay(200);
	
	gpio_cover_detect_state = gpio_get_value(gpio_cover_detect);
	if (gpio_cover_detect_state == 0) {
		gpio_set_value(gpio_sim_control, 1);
		printk(KERN_INFO "===cover_detect GPIO says sim control should be open, reporting it\n");
	}
	else{
		gpio_set_value(gpio_sim_control, 0);
		printk(KERN_INFO "===cover_detect GPIO says sim control should be close, reporting it\n");
	}       

     if(request_irq(gpio_to_irq(72),cover_detect_irq, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING | IRQF_DISABLED, "cover_detect", NULL))
	   printk(KERN_INFO "===cover_detect cover detect request_irq fail\n");
	 
     //INIT_WORK(&wq_sim_control, sim_control_work);
     INIT_DELAYED_WORK(&wq_sim_control, sim_control_work);
	return 0;
}

static int __devexit cover_detect_remove(struct platform_device *pdev)
{
	gpio_free(gpio_sim_control);
       gpio_free(gpio_cover_detect);
	return 0;
}

static struct platform_driver cover_detect_driver = {
	.driver = {
		   .name = "cover_detect",
		   .owner = THIS_MODULE,
	},
	.probe = cover_detect_probe,
	.remove = __devexit_p(cover_detect_remove),
};

static int __init cover_detect_init(void)
{
	return platform_driver_register(&cover_detect_driver);
}

static void __exit cover_detect_exit(void)
{
	platform_driver_unregister(&cover_detect_driver);
}

module_init(cover_detect_init);
module_exit(cover_detect_exit);

MODULE_AUTHOR("Peizhong Cong <cong.peizhong@byd.com>");
MODULE_DESCRIPTION("Cover Detect Driver");
MODULE_LICENSE("GPL v2");
