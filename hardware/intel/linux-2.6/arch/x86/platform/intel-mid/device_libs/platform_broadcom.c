/*
 * platform_wl12xx.c: wl12xx platform data initilization file
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
#include <linux/gpio.h>
#include <linux/lnw_gpio.h>
#include <linux/wlan_plat.h>
#include <linux/pm_runtime.h>
#include <linux/mmc/sdhci.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <asm/intel-mid.h>
#include "platform_broadcom.h"
#include <linux/skbuff.h>
#include <linux/string.h>
#include <linux/mmc/host.h>
//irq 2

#define ETHER_ADDR_LEN 6
#define PREALLOC_WLAN_NUMBER_OF_SECTIONS	4
#define PREALLOC_WLAN_NUMBER_OF_BUFFERS		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 1024)

#define WLAN_SKB_BUF_NUM	17

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

typedef struct wifi_mem_prealloc_struct {
	void *mem_ptr;
	unsigned long size;
} wifi_mem_prealloc_t;

static wifi_mem_prealloc_t wifi_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

static void *brcm_wifi_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_NUMBER_OF_SECTIONS)
		return wlan_static_skb;
	if ((section < 0) || (section > PREALLOC_WLAN_NUMBER_OF_SECTIONS))
		return NULL;
	if (wifi_mem_array[section].size < size)
		return NULL;
	return wifi_mem_array[section].mem_ptr;
}

static int brcm_wifi_get_macaddr(unsigned char *buf)
{
	struct file *fp = NULL;
	unsigned char macbuffer[18] = {0};
	mm_segment_t oldfs = {0};
	char buffer[18] = {0};
	unsigned char randommac[3] = {0};
	unsigned char octet[ETHER_ADDR_LEN] = {0};
	char* filepath = "/factory/wifi/wifi_macaddr";
	char* nvpath = "/system/etc/wifi/bcmdhd.cal";
	char dummy_mac[ETHER_ADDR_LEN]		= { 0x64, 0xD4, 0xDA, 0x86, 0x80, 0x87 };
	int ret =0;
	fp = filp_open(filepath, O_RDONLY, 0); //try to open wifi_macaddr
	if (IS_ERR(fp)) {
		/*If the file doesn't exist, create a new file and write random mac*/
		fp = filp_open(filepath, O_RDWR | O_CREAT, 0666);
		if (IS_ERR(fp)){
			printk("[WIFI] %s: File open error\n", filepath);
			return -1;
		}
		oldfs = get_fs();
		set_fs(get_ds());

		/* Generating the Random Bytes for 3 last octects of the MAC address */
		get_random_bytes(randommac, 3);
		sprintf(macbuffer,"%02X:%02X:%02X:%02X:%02X:%02X",0x64,0xD4,0xDA,randommac[0],randommac[1],randommac[2]);
		printk("[WIFI] The Random Generated MAC ID : %s\n", macbuffer);

		if(fp->f_mode & FMODE_WRITE){
			/*Write the MAC address into New created file*/
			ret = fp->f_op->write(fp, (const char *)macbuffer, sizeof(macbuffer), &fp->f_pos);
			if(ret < 0)
				printk("[WIFI] Mac address [%s] Failed to write into File: %s\n", macbuffer, filepath);
			else
				printk("[WIFI] Mac address [%s] written into File: %s successfully\n", macbuffer, filepath);
		}
		set_fs(oldfs);
		ret = kernel_read(fp, 0, buffer, 18);
	} else {
		/* Reading the MAC Address from /factory/wifi/ file( the existed file or just created file)*/
		ret = kernel_read(fp, 0, buffer, 18);
		buffer[17] ='\0';	 // to prevent abnormal string display when mac address is displayed on the screen. 
		printk("Read MAC : [%s] [%d] \r\n" , buffer, strncmp(buffer , "00:00:00:00:00:00" , 17));
		if(strncmp(buffer, "00:00:00:00:00:00" , 17) == 0) {
			sprintf(macbuffer,"%02X:%02X:%02X:%02X:%02X:%02X",dummy_mac[0],dummy_mac[1],dummy_mac[2],dummy_mac[3],dummy_mac[4],dummy_mac[5]);
			if(fp->f_mode & FMODE_WRITE){
				ret = fp->f_op->write(fp, (const char *)macbuffer, sizeof(macbuffer), &fp->f_pos);
			} else {
				filp_close(fp, NULL);
				return -EINVAL;//use default mac in nvram.txt
			}
		}
	}
	
	if(ret){
		sscanf(buffer,"%02X:%02X:%02X:%02X:%02X:%02X",
			   &octet[0], &octet[1], &octet[2], 
			   &octet[3], &octet[4], &octet[5]);
		memcpy(buf, (char *)octet, ETHER_ADDR_LEN);
	} else {
		printk("%s: Reading from the '%s' returns 0 bytes\n", __func__, filepath);
		if (fp){
		   filp_close(fp, NULL);
		}
		return -EINVAL;//use default mac in nvram.txt
	}

	if (fp)
		filp_close(fp, NULL);

	return 0;
	
}

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

int wifi_irq_gpio;
int wifi_power_gpio;

int __init brcm_init_wifi_mem(void)
{
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;
	
	for(i=0;( i < PREALLOC_WLAN_NUMBER_OF_SECTIONS );i++) {
		wifi_mem_array[i].mem_ptr = kmalloc(wifi_mem_array[i].size,
							GFP_KERNEL);
		if (wifi_mem_array[i].mem_ptr == NULL)
			goto err_mem_alloc;
	}

	printk("%s: WIFI MEM Allocated\n", __FUNCTION__);
	return 0;
	
err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wifi_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}

static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid=NULL;

int
brcm_wifi_status_register(void (*callback)(int card_present, void *dev_id),
				void *dev_id)
{
	pr_err("%s:@@@@@ platform register callback enter\n",__func__);
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	return 0;
}
EXPORT_SYMBOL(brcm_wifi_status_register);

static int brcm_wifi_cd;	/* WiFi virtual 'card detect' status */

static unsigned int brcm_wifi_status(struct device *dev)
{
	return brcm_wifi_cd;
}

static int brcm_wifi_runtime_get(void)
{
	struct sdhci_host *host = wifi_status_cb_devid;
	return pm_runtime_get_sync(host->mmc->parent);
}

static int brcm_wifi_runtime_put(void)
{
	struct sdhci_host *host = wifi_status_cb_devid;
	pm_runtime_mark_last_busy(host->mmc->parent);
	return pm_runtime_put_autosuspend(host->mmc->parent);
}

static int brcm_wifi_host_is_saved = 0; /* wang.junxian2@byd.com */

static int brcm_wifi_save_host(void)
{
	struct sdhci_host *host = wifi_status_cb_devid;
	if(NULL == host)
		return -EINVAL;
	brcm_wifi_host_is_saved = 1;
	return mmc_power_save_host(host->mmc);
}

static int brcm_wifi_restore_host(void)
{
	struct sdhci_host *host = wifi_status_cb_devid;
	if(NULL == host)
		return -EINVAL;
	if(0 == brcm_wifi_host_is_saved)
		return -EINVAL;
	return mmc_power_restore_host(host->mmc);
}

static void brcm_wifi_set_bus_resume_policy(int manual)
{
	struct sdhci_host *host = wifi_status_cb_devid;
	mmc_set_bus_resume_policy(host->mmc, manual);
	if(mmc_bus_manual_resume(host->mmc)){
		host->mmc->bus_resume_flags |= MMC_BUSRESUME_NEEDS_RESUME;
	} else {
		host->mmc->bus_resume_flags &= ~MMC_BUSRESUME_NEEDS_RESUME;
	}
}

static int brcm_wifi_power(int val)
{
	pr_err("%s:@@@@@ SDIO set power %s \n",__func__,val?"on":"off");
	gpio_request(wifi_power_gpio, "bcm4330_pwr");
        if(val){
	   gpio_direction_output(wifi_power_gpio,0);
	   gpio_set_value(wifi_power_gpio,0);
	   mdelay(100);
	   gpio_set_value(wifi_power_gpio,1);
       mdelay(200);
	   brcm_wifi_restore_host();
       mdelay(50);
	   brcm_wifi_set_bus_resume_policy(0);
	} else {
 	   gpio_direction_output(wifi_power_gpio,0);
	   gpio_set_value(wifi_power_gpio,0);
       mdelay(200);
	   brcm_wifi_save_host();
	   brcm_wifi_set_bus_resume_policy(1);
	}
	gpio_free(wifi_power_gpio);
	
	return 0;
}

static int brcm_wifi_reset(int val)
{
        return 0;
}

static int brcm_wifi_set_carddetect(int val)
{
        brcm_wifi_cd = val;
	if(wifi_status_cb)
	     wifi_status_cb(val, wifi_status_cb_devid);
	else
	     pr_err("%s:@@@@@Nobody to notify\n",__func__);
	return 0;
}


static struct resource brcm_wifi_resources[] = {
	[0] = {
		.name		= "bcm4329_wlan_irq",
		.start		= 1,//gpio_to_irq(wifi_irq_gpio),
		.end		= 1,//gpio_to_irq(wifi_irq_gpio),
		.flags          = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE | IORESOURCE_IRQ_HIGHEDGE,
	},
};

static struct wifi_platform_data mid_wifi_control = {
	.set_power      = brcm_wifi_power,
	.set_reset      = brcm_wifi_reset,
	.set_carddetect = brcm_wifi_set_carddetect,
	.mem_prealloc   = brcm_wifi_mem_prealloc,
	.get_mac_addr = brcm_wifi_get_macaddr,
};

static struct platform_device brcm_wifi_device = {
        .name           = "bcm4329_wlan",
        .id             = -1,
        .num_resources  = ARRAY_SIZE(brcm_wifi_resources),
        .resource       = brcm_wifi_resources,
        .dev            = {
        .platform_data = &mid_wifi_control,
        },
};

void __init bcm4330_platform_data_init_post_scu(void *info)
{
	struct sd_board_info *sd_info = info;
	
	int err;
	//printk(KERN_INFO "%s:@@@@@ POST scu @@@@@ \n",__func__);
	pr_err("%s:@@@@@ BCM4330 POST scu start  @@@@ \n",__func__);
	/*Get GPIO numbers from the SFI table*/
	wifi_irq_gpio = get_gpio_by_name(BCM4330_SFI_GPIO_IRQ_NAME);
	if (wifi_irq_gpio == -1) {
		pr_err("%s: Unable to find WLAN-interrupt GPIO in the SFI table\n",
				__func__);
	
		//return;
	}
		wifi_irq_gpio = 79;

	brcm_wifi_resources[0].start = gpio_to_irq(wifi_irq_gpio);
	brcm_wifi_resources[0].end = gpio_to_irq(wifi_irq_gpio);

	err = gpio_request(wifi_irq_gpio, "bcm4330_irq");
	if (err < 0) {
		pr_err("%s: Unable to request GPIO\n", __func__);
		return;
	}
	err = gpio_direction_input(wifi_irq_gpio);
	if (err < 0) {
		pr_err("%s: Unable to set GPIO direction\n", __func__);
		return;
	}
	/* this is the fake regulator that mmc stack use to power of the
	   wifi sdio card via runtime_pm apis */
	wifi_power_gpio = get_gpio_by_name(BCM4330_SFI_GPIO_ENABLE_NAME);
	if (wifi_power_gpio == -1) {
		pr_err("%s: Unable to find WLAN-enable GPIO in the SFI table\n",
		       __func__);

	//	return;
	}
		wifi_power_gpio = 115;

	err = platform_device_register(&brcm_wifi_device);
	if (err < 0)
		pr_err("error platform_device_register\n");

	sdhci_pci_request_regulators();
	//printk("%s:@@@@@ POST scu COMLETE @@@@@ \n",__func__);
	pr_err("%s:@@@@@ BCM4330 POST scu COMLETE @@@@ \n",__func__);
}

void __init *bcm4330_platform_data(void *info)
{
	struct sd_board_info *sd_info;

	pr_err("%s:@@@@@ bcm4330_platform_data enter @@@@ \n",__func__);
	sd_info = kmemdup(info, sizeof(*sd_info), GFP_KERNEL);
	if (!sd_info) {
		pr_err("MRST: fail to alloc mem for delayed wl12xx dev\n");
		return NULL;
	}
	intel_delayed_device_register(sd_info,
				      bcm4330_platform_data_init_post_scu);

	return &mid_wifi_control;
}
