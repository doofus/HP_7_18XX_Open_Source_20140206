/*
 * BQ27x00 battery driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 * Copyright (C) 2010-2011 Lars-Peter Clausen <lars@metafoo.de>
 * Copyright (C) 2011 Pali Rohár <pali.rohar@gmail.com>
 *
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 * Datasheets:
 * http://focus.ti.com/docs/prod/folders/print/bq27000.html
 * http://focus.ti.com/docs/prod/folders/print/bq27500.html
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/lnw_gpio.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/intel-mid.h>

#include <linux/power/bq27x00_battery.h>
#include <linux/power/intel_mdf_battery.h>
#include <bq27x00_dffs.h>

#include <linux/delay.h>
#define DRIVER_VERSION			"1.2.0"

#define BQ27x00_REG_CNTL		0x00
#define BQ27x00_REG_TEMP		0x02
#define BQ27x00_REG_VOLT		0x04
#define BQ27x00_REG_FLAGS		0x06
#define BQ27x00_REG_NAC			0x08
#define BQ27x00_REG_FAC			0x0a
#define BQ27x00_REG_RM			0x0c
#define BQ27x00_REG_FCC			0x0e
#define BQ27x00_REG_AI			0x10
#define BQ27x00_REG_SI			0x12
#define BQ27x00_REG_MLI			0x14
#define BQ27x00_REG_AP			0x18
#define BQ27x00_REG_SOC			0x1c
#define BQ27x00_REG_ITEMP	    0x1e
#define BQ27x00_REG_SOH			0x20

#define BAT_INSERT         		0x0C
#define BAT_REMOVE        		0x0D
#define SET_CFGUPDATE   		0x13
#define BAT_SEALED              0x20
#define SOFT_RESET          	0x42


#define BQ27x00_REG_TTE			0x16
#define BQ27x00_REG_TTF			0x18
#define BQ27x00_REG_TTECP		0x26
#define BQ27x00_REG_LMD			0x12 /* Last measured discharge */
#define BQ27x00_REG_CYCT		0x2A /* Cycle count total */
#define BQ27x00_REG_AE			0x22 /* Available enery */

#define BQ27000_REG_RSOC		0x0B /* Relative State-of-Charge */
#define BQ27000_REG_ILMD		0x76 /* Initial last measured discharge */
#define BQ27000_FLAG_CHGS		BIT(7)
#define BQ27000_FLAG_FC			BIT(5)

#define BQ27500_REG_SOC			0x1C
#define BQ27500_REG_DCAP		0x3C /* Design capacity */
#define BQ27500_FLAG_DSG		BIT(0)

#define BQ27500_FLAG_BATDET		BIT(3) /* Battery insertion detected */
#define BQ27500_FLAG_ITPOR		BIT(5) /* Indicates a Power On Reset or RESET subcommand has occurred */
#define BQ27500_FLAG_FC			BIT(9)

#define BQ27000_RS			20 /* Resistor sense */

struct bq27x00_device_info;
struct bq27x00_access_methods {
	int (*read)(struct bq27x00_device_info *di, u8 reg, bool single);
};
static int bq27x00_reboot_callback(struct notifier_block *nfb,
					unsigned long event, void *data);
static struct notifier_block bq27x00_reboot_notifier_block = {
	.notifier_call = bq27x00_reboot_callback,
	.priority = 0,
};
static int bq27x00_write_i2c(struct bq27x00_device_info *di, u8 reg, u8 value);
static int bq27x00_read_i2c(struct bq27x00_device_info *di, u8 reg, bool single);

enum bq27x00_chip { BQ27000, BQ27500 };

struct bq27x00_reg_cache {
	int temperature;
	int time_to_empty;
	int time_to_empty_avg;
	int time_to_full;
	int charge_full;
	int cycle_count;
	int capacity;
	int flags;
	int status;

	int current_now;
};

struct bq27x00_device_info {
	struct device 		*dev;
	int			id;
	enum bq27x00_chip	chip;

	struct bq27x00_reg_cache cache;
	int charge_design_full;

	unsigned long last_update;
	struct delayed_work work;
#if defined(DEBUG)	
	struct delayed_work dump_register_worker;
#endif
	struct power_supply	bat;

	struct bq27x00_access_methods bus;
	bool plat_rebooting;

	struct mutex lock;
};

static enum power_supply_property bq27x00_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_ENERGY_NOW,
};

static unsigned int poll_interval = 360;
static int probe_passed = 0;
static struct bq27x00_device_info *bq27x00_di;
static ssize_t get_dffs_list(struct device *device,
					struct device_attribute *attr, char *buf);
static DEVICE_ATTR(dffs_list, S_IRUGO, get_dffs_list, NULL);

#if defined(DEBUG)
static char* register_dump_buffer;
#define DUMP_BUFFER_SIZE 1024
#endif

module_param(poll_interval, uint, 0644);
MODULE_PARM_DESC(poll_interval, "battery poll interval in seconds - " \
				"0 disables polling");


static int bq27x00_write_i2c(struct bq27x00_device_info *di, u8 reg, u8 value)
{       
	struct i2c_client *client = to_i2c_client(di->dev);
	int ret;
	u8 buffer[2];

	if (!client->adapter){
		dev_err(di->dev, "client->adapter is null");
		return -ENODEV;
	}

	/* if the shutdown or reboot sequence started
	 * then block the access to maxim registers as chip
	 * cannot be recovered from broken i2c transactions
	 */
	if (di->plat_rebooting) {
		dev_warn(di->dev, "rebooting is in progress\n");
		return -EINVAL;
	}
	
	buffer[0] = reg;
	buffer[1] = value;

	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.buf	= buffer,
		.len	= 2,
	};

	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(di->dev, "Error write i2c, address = 0x%x, value = 0x%x. retcode = %d", reg, value, ret);
		return ret;
	}
	return 0;
}
static int bq27x00_write_i2c_rommode(struct bq27x00_device_info *di, u8 reg, u8 value)
{       
	struct i2c_client *client = to_i2c_client(di->dev);
	int ret;
	u8 buffer[2];

	if (!client->adapter){
		dev_err(di->dev, "client->adapter is null");
		return -ENODEV;
	}

	/* if the shutdown or reboot sequence started
	 * then block the access to maxim registers as chip
	 * cannot be recovered from broken i2c transactions
	 */
	if (di->plat_rebooting) {
		dev_warn(di->dev, "rebooting is in progress\n");
		return -EINVAL;
	}
	
	buffer[0] = reg;
	buffer[1] = value;

	struct i2c_msg msg = {
		.addr	= 0x0B,
		.flags	= 0,
		.buf	= buffer,
		.len	= 2,
	};

	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret < 0) {
		dev_err(di->dev, "Error write i2c in rom mode, address = 0x%x, value = 0x%x\n. retcode = %d", reg, value, ret);
		return ret;
	}
	return 0;
}
static int bq27x00_read_i2c_rommode(struct bq27x00_device_info *di, u8 reg, bool single)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[2];
	unsigned char data[2];
	int ret;

	if (!client->adapter)
		return -ENODEV;

	/* if the shutdown or reboot sequence started
	 * then block the access to maxim registers as chip
	 * cannot be recovered from broken i2c transactions
	 */
	if (di->plat_rebooting) {
		dev_warn(di->dev, "rebooting is in progress\n");
		return -EINVAL;
	}
	
	msg[0].addr = 0x0B;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(reg);
	msg[1].addr = 0x0B;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;
	if (single)
		msg[1].len = 1;
	else
		msg[1].len = 2;

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0){
		dev_err(di->dev, "Error read i2c in rom mode, address = 0x%x, retcode: %d", reg, ret);
		return ret;
	}

	if (!single)
		ret = get_unaligned_le16(data);
	else
		ret = data[0];

	return ret;
}

static int bq27425_init(struct bq27x00_device_info *di, int (*dffs)[10])
{
	int i = 0;
	int j = 0;
	int ret = 0;
	unsigned char address = 0;
        
	dev_info(di->dev, "%s is called.", __func__);
	for(i = 0; dffs[i][0] != 0; i++) {
		//printk("===test dffs[i][0] = %d\n", dffs[i][0]);
		switch(dffs[i][0])
		{
			case 'W': 
				address = dffs[i][2];
				if(dffs[i][1]==0xaa){
					for(j = 3; dffs[i][j]!= -1; j++)
					{     			
						bq27x00_write_i2c(di, address, dffs[i][j]);
						address++;
					}
				}
				else{
					for(j = 3; dffs[i][j]!= -1; j++)
						{     			
							bq27x00_write_i2c_rommode(di, address, dffs[i][j]);
							address++;
						}
				}
				break;
			case 'R':
			    break;
			case 'C':
			    address = dffs[i][2];
			    for(j = 3; dffs[i][j]!= -1; j++)
					{     	
						if(bq27x00_read_i2c_rommode(di, address, 1)!=dffs[i][j])
							dev_warn(di->dev, "===test bq27x00_read error address= 0x%x, dffs[i][j]=0x%x, i=%d, j=%d\n", address, dffs[i][j],i,j);
						address++;
					}
				
				break;
			case 'X': 
				msleep(dffs[i][1]);
				break;
			default:
			   break;
		}
	}
	return 0;	

}	

static inline int bq27x00_read(struct bq27x00_device_info *di, u8 reg,
		bool single)
{
	return di->bus.read(di, reg, single);
}

/*
 * Return the battery Relative State-of-Charge
 * Or < 0 if something fails.
 */
static int bq27x00_battery_rsoc_adjust(struct bq27x00_device_info *di, int orsoc)
{
	int rsoc;
	int temp_rsoc;
	static int rsoc_peak = 100;
	static int rsoc_factor = 0;
	int status = POWER_SUPPLY_STATUS_DISCHARGING;

	rsoc = orsoc;
	if (rsoc>100)
		rsoc = 100;
	else if (rsoc<0)
		rsoc = 0;

	status = intel_msic_check_battery_status();
	switch(status){
	case POWER_SUPPLY_STATUS_CHARGING:
		if (rsoc>=100){
			rsoc = 100;
			rsoc_factor = -1;
			rsoc_peak = 100;
		}else if (rsoc<=0){
			rsoc = 0;
			rsoc_factor = 0;
			rsoc_peak = 100;
		}else{
			rsoc_factor = 0;
			temp_rsoc = 100 * rsoc / rsoc_peak;
			if(temp_rsoc>=100)
				rsoc_peak = 100 * rsoc / 99;

			temp_rsoc = 100 * rsoc / rsoc_peak;
			if(temp_rsoc>=100)
				rsoc_peak++;
		}
		break;
	case POWER_SUPPLY_STATUS_FULL:
		rsoc_factor = 0;
		rsoc_peak = rsoc;
		break;
	default:
		if (rsoc < 50 && rsoc_factor == -1)
			rsoc_factor = 0;
		break;
	}

	temp_rsoc = 100 * (rsoc + rsoc_factor) / rsoc_peak;
	if (temp_rsoc != rsoc){
		dev_warn(di->dev, "rsoc is adjusted, orsoc = %d, rsoc = %d,"
				"adjusted_rsoc = %d, rsoc_peak = %d, rsoc_factor = %d", 
				orsoc, rsoc, temp_rsoc, rsoc_peak, rsoc_factor);
		rsoc = temp_rsoc;
	}
	
	if (rsoc > 100)
		rsoc = 100;

	return rsoc;
}

static int bq27x00_battery_read_rsoc(struct bq27x00_device_info *di)
{
	int rsoc;

	if (di == NULL) {
		printk(KERN_ERR "%s: NULL pointer(di) detected. ", __func__);
		return -1;
	}

	if (di->chip == BQ27500)
		rsoc = bq27x00_read(di, BQ27500_REG_SOC, false);
	else
		rsoc = bq27x00_read(di, BQ27000_REG_RSOC, true);

	if (rsoc >= 0) {
		rsoc = bq27x00_battery_rsoc_adjust(di, rsoc);
	} else {
		dev_err(di->dev, "error reading relative State-of-Charge\n");
	}

	return rsoc;
}

/*
 * Return the battery temperature 
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_temp(struct bq27x00_device_info *di)
{
    u8 address = BQ27x00_REG_TEMP;
    int battery_temp=0;
    u8 temp1=0;
    u8 temp2=0;
	int rc;

	rc = intel_msic_get_battery_pack_temp(&battery_temp);
	if (rc < 0 ) {
		battery_temp = 25;
		dev_err(di->dev, "error reading temperature from MSIC.\n");
	}

	battery_temp = battery_temp * 10 + 2731;
	temp1= battery_temp & 0x00FF;   
	temp2 = battery_temp >> 8;
	bq27x00_write_i2c(di, address, temp1);

	address++;
	bq27x00_write_i2c(di, address, temp2);	
	
	battery_temp = bq27x00_read(di, BQ27x00_REG_TEMP, false);
	if (battery_temp < 0)
		dev_err(di->dev, "error reading temperature from FG.\n");

	return battery_temp;
}



/*
 * Return a battery charge value in µAh
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_charge(struct bq27x00_device_info *di, u8 reg)
{
	int charge;

	charge = bq27x00_read(di, reg, false);
	if (charge < 0) {
		dev_err(di->dev, "error reading nominal available capacity\n");
		return charge;
	}

	if (di->chip == BQ27500)
		charge *= 1000;
	else
		charge = charge * 3570 / BQ27000_RS;

	return charge;
}

/*
 * Return the battery Nominal available capaciy in µAh
 * Or < 0 if something fails.
 */
static inline int bq27x00_battery_read_nac(struct bq27x00_device_info *di)
{
	return bq27x00_battery_read_charge(di, BQ27x00_REG_NAC);
}

/*
 * Return the battery Last measured discharge in µAh
 * Or < 0 if something fails.
 */
static inline int bq27x00_battery_read_lmd(struct bq27x00_device_info *di)
{
	return bq27x00_battery_read_charge(di, BQ27x00_REG_LMD);
}

/*
 * Return the battery Initial last measured discharge in µAh
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_ilmd(struct bq27x00_device_info *di)
{
	int ilmd;

	if (di->chip == BQ27500)
		ilmd = bq27x00_read(di, BQ27500_REG_DCAP, false);
	else
		ilmd = bq27x00_read(di, BQ27000_REG_ILMD, true);

	if (ilmd < 0) {
		dev_err(di->dev, "error reading initial last measured discharge\n");
		return ilmd;
	}

	if (di->chip == BQ27500)
		ilmd *= 1000;
	else
		ilmd = ilmd * 256 * 3570 / BQ27000_RS;

	return ilmd;
}

/*
 * Return the battery Cycle count total
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_cyct(struct bq27x00_device_info *di)
{
	int cyct;

	cyct = bq27x00_read(di, BQ27x00_REG_CYCT, false);
	if (cyct < 0)
		dev_err(di->dev, "error reading cycle count total\n");

	return cyct;
}

/*
 * Read a time register.
 * Return < 0 if something fails.
 */
static int bq27x00_battery_read_time(struct bq27x00_device_info *di, u8 reg)
{
	int tval;

	tval = bq27x00_read(di, reg, false);
	if (tval < 0) {
		dev_err(di->dev, "error reading register %02x: %d\n", reg, tval);
		return tval;
	}

	if (tval == 65535)
		return -ENODATA;

	return tval * 60;
}

static void bq27x00_update(struct bq27x00_device_info *di)
{
	struct bq27x00_reg_cache cache = {0, };
	bool is_bq27500 = di->chip == BQ27500;

	cache.flags = bq27x00_read(di, BQ27x00_REG_FLAGS, !is_bq27500);
	if (cache.flags >= 0) {
		cache.capacity = bq27x00_battery_read_rsoc(di);
		cache.temperature =	bq27x00_battery_read_temp(di);			

		if (!is_bq27500)
			cache.current_now = bq27x00_read(di, BQ27x00_REG_AI, false);

		/* We only have to read charge design full once */
		/*if (di->charge_design_full <= 0)
			di->charge_design_full = bq27x00_battery_read_ilmd(di);*/
	} else {
		dev_err(di->dev, "Error to read bq27425, cache.flags: %d", cache.flags);
	}

	if (intel_msic_check_battery_present() == 0) {
		cache.status = POWER_SUPPLY_STATUS_UNKNOWN;	
	} else {
		cache.status = intel_msic_check_battery_status();
		if ((cache.status == POWER_SUPPLY_STATUS_CHARGING) && 
				(cache.flags >= 0) && 
				(cache.capacity == 100) ) {
			cache.status = POWER_SUPPLY_STATUS_FULL;
		}
	}

	/* Ignore current_now which is a snapshot of the current battery state
	 * and is likely to be different even between two consecutive reads */
	if (memcmp(&di->cache, &cache, sizeof(cache) - sizeof(int)) != 0) {
		di->cache = cache;
		power_supply_changed(&di->bat);
	}

	di->last_update = jiffies;
}

static void bq27x00_battery_poll(struct work_struct *work)
{
	struct bq27x00_device_info *di =
		container_of(work, struct bq27x00_device_info, work.work);

	if (di == NULL) {
		printk(KERN_ERR "bq27425: error in %s, for di is NULL.\n", __func__);
		return;
	}


	bq27x00_update(di);

	if (poll_interval > 0) {
		/* The timer does not have to be accurate. */
		set_timer_slack(&di->work.timer, poll_interval * HZ / 4);
		schedule_delayed_work(&di->work, poll_interval * HZ);
	}
}
#if defined(DEBUG)
static void bq27425_dump_all_register(struct bq27x00_device_info *di)
{
	u8 addr;
	u16 value;
	
	for (addr = 0x00; ; addr+=2)
	{
		value = bq27x00_read(di, addr, false);
		if (value < 0)
			dev_err(di->dev, "Error reading data from register 0x%x, ret: %d", addr, value);
		else 
			dev_info(di->dev, "address: 0x%02x%02x, value: 0x%04x", addr, addr+1, value);

		if(addr >= 0x20)
			break;
	}
}

static void bq27425_dump_register(struct bq27x00_device_info *di)
{
	u8 addr;
	u16 value;
	char *p;

	if (!register_dump_buffer)
		return;

	register_dump_buffer[0] = '\t';
	p = register_dump_buffer + 1;

	for (addr = 0x00; ; addr+=2)
	{
		value = bq27x00_read(di, addr, false);
		if (value < 0){
			dev_err(di->dev, "Error reading data from register 0x%02x%02x, ret: %d", addr, addr+1, value);
			sprintf(p, "0x%s\t", "ERR!");
		} else {
			sprintf(p, "0x%04x\t", value);
			p += strlen(p);
		}

		if(addr >= 0x20)
			break;
	}

	sprintf(p, "%s", "\n");
	dev_info(di->dev, register_dump_buffer);
}

static void bq27425_dump_register_worker(struct work_struct *work)
{
	struct bq27x00_device_info *di =
		container_of(work, struct bq27x00_device_info, dump_register_worker.work);

	bq27425_dump_register(di);

	schedule_delayed_work(&di->dump_register_worker, 60*(HZ));
}
#endif

/*
 * Return the battery temperature in tenths of degree Celsius
 * Or < 0 if something fails.
 */
static int bq27x00_battery_temperature(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{   
	if (di->cache.temperature < 0)
		return di->cache.temperature;
   
	if (di->chip == BQ27500)
		val->intval = di->cache.temperature - 2731;
	else
		val->intval = ((di->cache.temperature * 5) - 5463) / 2;

	return 0;
}

/*
 * Return the battery average current in µA
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
static int bq27x00_battery_current(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int curr;

	if (di->chip == BQ27500)
	    curr = bq27x00_read(di, BQ27x00_REG_AI, false);
	else
	    curr = di->cache.current_now;

	if (curr < 0)
		return curr;

	if (di->chip == BQ27500) {
		/* bq27500 returns signed value */
		val->intval = (int)((s16)curr) * 1000;
	} else {
		if (di->cache.flags & BQ27000_FLAG_CHGS) {
			dev_dbg(di->dev, "negative current!\n");
			curr = -curr;
		}

		val->intval = curr * 3570 / BQ27000_RS;
	}

	return 0;
}

static int bq27x00_battery_status(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	val->intval = di->cache.status;

	return 0;
}

//start@du.yichun get battery health staus, add
static int bq27x00_battery_health(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{

	if (di->cache.temperature < 0) {
		val->intval = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	} else {
		val->intval = intel_msic_check_battery_health();
	}

	return 0;
}
//end

/*
 * Return the battery Voltage in milivolts
 * Or < 0 if something fails.
 */
static int bq27x00_battery_voltage(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int volt;

	volt = bq27x00_read(di, BQ27x00_REG_VOLT, false);
	if (volt < 0)
		return volt;

	val->intval = volt * 1000;

	return 0;
}

/*
 * Return the battery Available energy in µWh
 * Or < 0 if something fails.
 */
static int bq27x00_battery_energy(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int ae;

	ae = bq27x00_read(di, BQ27x00_REG_AE, false);
	if (ae < 0) {
		dev_err(di->dev, "error reading available energy\n");
		return ae;
	}

	if (di->chip == BQ27500)
		ae *= 1000;
	else
		ae = ae * 29200 / BQ27000_RS;

	val->intval = ae;

	return 0;
}


static int bq27x00_simple_value(int value,
	union power_supply_propval *val)
{
	if (value < 0)
		return value;

	val->intval = value;

	return 0;
}

#define to_bq27x00_device_info(x) container_of((x), \
				struct bq27x00_device_info, bat);

static int bq27x00_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret = 0;
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);

	if (!probe_passed)
		return -ENODEV;

	mutex_lock(&di->lock);
	if (time_is_before_jiffies(di->last_update + 5 * HZ)) {
		cancel_delayed_work_sync(&di->work);
		bq27x00_battery_poll(&di->work.work);
	}
	mutex_unlock(&di->lock);

	if (psp != POWER_SUPPLY_PROP_PRESENT && di->cache.flags < 0)
		return -ENODEV;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = bq27x00_battery_status(di, val);
		break;
	case POWER_SUPPLY_PROP_HEALTH:  //du.yichun add
		ret = bq27x00_battery_health(di, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		ret = bq27x00_battery_voltage(di, val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		//val->intval = di->cache.flags < 0 ? 0 : 1;
		val->intval = intel_msic_check_battery_present();
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = bq27x00_battery_current(di, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = bq27x00_simple_value(di->cache.capacity, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = bq27x00_battery_temperature(di, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		ret = bq27x00_simple_value(di->cache.time_to_empty, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		ret = bq27x00_simple_value(di->cache.time_to_empty_avg, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		ret = bq27x00_simple_value(di->cache.time_to_full, val);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = bq27x00_simple_value(bq27x00_battery_read_nac(di), val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = bq27x00_simple_value(di->cache.charge_full, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = bq27x00_simple_value(di->charge_design_full, val);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = bq27x00_simple_value(di->cache.cycle_count, val);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		ret = bq27x00_battery_energy(di, val);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void bq27x00_external_power_changed(struct power_supply *psy)
{
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);

	cancel_delayed_work_sync(&di->work);
	schedule_delayed_work(&di->work, 0);
}

static int bq27x00_powersupply_init(struct bq27x00_device_info *di)
{
	int ret;

	di->bat.type = POWER_SUPPLY_TYPE_BATTERY;
	di->bat.properties = bq27x00_battery_props;
	di->bat.num_properties = ARRAY_SIZE(bq27x00_battery_props);
	di->bat.get_property = bq27x00_battery_get_property;
	di->bat.external_power_changed = bq27x00_external_power_changed;

	INIT_DELAYED_WORK(&di->work, bq27x00_battery_poll);
	mutex_init(&di->lock);

	ret = power_supply_register(di->dev, &di->bat);
	if (ret) {
		dev_err(di->dev, "failed to register battery: %d\n", ret);
		return ret;
	}

	dev_info(di->dev, "support ver. %s enabled\n", DRIVER_VERSION);

	bq27x00_update(di);

	return 0;
}

static void bq27x00_powersupply_unregister(struct bq27x00_device_info *di)
{

	cancel_delayed_work_sync(&di->work);
#if defined(DEBUG)
	if (register_dump_buffer) {
		cancel_delayed_work_sync(&di->dump_register_worker);
		kfree(register_dump_buffer);
	}
#endif
	power_supply_unregister(&di->bat);

	mutex_destroy(&di->lock);
}


/* i2c specific code */
#ifdef CONFIG_BATTERY_BQ27X00_I2C

/* If the system has several batteries we need a different name for each
 * of them...
 */
static DEFINE_IDR(battery_id);
static DEFINE_MUTEX(battery_mutex);

static int bq27x00_read_i2c(struct bq27x00_device_info *di, u8 reg, bool single)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[2];
	unsigned char data[2];
	int ret;

	if (!client->adapter)
		return -ENODEV;

	/* if the shutdown or reboot sequence started
	 * then block the access to maxim registers as chip
	 * cannot be recovered from broken i2c transactions
	 */
	if (di->plat_rebooting) {
		dev_warn(di->dev, "rebooting is in progress\n");
		return -EINVAL;
	}
	
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(reg);
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;
	if (single)
		msg[1].len = 1;
	else
		msg[1].len = 2;

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0)
		return ret;

	if (!single)
		ret = get_unaligned_le16(data);
	else
		ret = data[0];

	return ret;
}

static irqreturn_t bq27x00_battery_det_irq(int irq, void *_host)
{
    struct bq27x00_device_info *di = _host;
	printk("===bq27425 bq27x00_battery_det_irq soc=%d \n",di->cache.capacity);	
	schedule_delayed_work(&di->work, 0);
	return IRQ_HANDLED;
}

static void bq27x00_battery_seal(struct bq27x00_device_info *di)
{
    int result;

	bq27x00_write_i2c(di, 0x00, 0x20);
    bq27x00_write_i2c(di, 0x01, 0x00);	
    msleep(20);
	result = bq27x00_read_i2c(di, 0x00, 0);
	dev_info(di->dev, "sealed FG, result: 0x%04x", result);
}

static int bq27x00_battery_unseal(struct bq27x00_device_info *di)
{
    int result;

    bq27x00_write_i2c(di, 0x00, 0x14);
    bq27x00_write_i2c(di, 0x01, 0x04);
    bq27x00_write_i2c(di, 0x00, 0x72);
    bq27x00_write_i2c(di, 0x01, 0x36);

    msleep(1000);
    bq27x00_write_i2c(di, 0x00, 0x00);
	bq27x00_write_i2c(di, 0x01, 0x00);
	result = bq27x00_read_i2c(di, 0x00, 0);
	dev_info(di->dev, "unsealed FG, result: 0x%04x", result);
	return !(result & 0x2000);
}

static int bq27x00_get_oemstr(struct bq27x00_device_info *di, u8 *oem_str)
{
	u8  addr, i;
	int result;

    bq27x00_write_i2c(di, 0x00, 0x00);
	bq27x00_write_i2c(di, 0x01, 0x00);
	result = bq27x00_read_i2c(di, 0x00, 0);
	if (result & 0x2000) {
		bq27x00_write_i2c(di, 0x3F, 0x01);
	} else {
		bq27x00_write_i2c(di, 0x00, 0x13);
		bq27x00_write_i2c(di, 0x3E, 0x3A);	
		bq27x00_write_i2c(di, 0x3F, 0x00);
		msleep(20);
	}

	addr = 0x40;
	for (i=0; i<8; i++) 
		oem_str[i] = bq27x00_read_i2c(di, addr+i, 1);
	oem_str[i] = 0;
	dev_info(di->dev, "got oem string: %s.\n", oem_str);
	
	return 0;
}

static int bq27x00_get_firmware_version(struct bq27x00_device_info *di, u16 *fw_ver )
{
	bq27x00_write_i2c(di, 0x00, 0x02);
	bq27x00_write_i2c(di, 0x01, 0x00);
	msleep(20);
	*fw_ver = bq27x00_read_i2c(di, 0x00, 0);
	dev_info(di->dev, "got firmware version: 0x%04x\n", *fw_ver);

	return 0;
}

static int bq27x00_insert_battery(struct bq27x00_device_info *di)
{
	int flags;
	bool is_bq27500;
	bool is_bq27425;
	bool is_changed = false;

	is_bq27500 = di->chip == BQ27500;

	flags = bq27x00_read(di, BQ27x00_REG_FLAGS, !is_bq27500);
	if(flags & BQ27500_FLAG_BATDET)
	{
		dev_info(di->dev, "flags: 0x%04x, battery is inserted already.", flags);
	} else {
		bq27x00_write_i2c(di, 0x00, 0x0c);
		bq27x00_write_i2c(di, 0x01, 0x00);
		is_changed = true;
		msleep(20);
	}

	if(flags & BQ27500_FLAG_ITPOR)
	{
		bq27x00_battery_unseal(di);
		bq27x00_write_i2c(di, 0x00, 0x42);
		bq27x00_write_i2c(di, 0x01, 0x00);
		bq27x00_battery_seal(di);
		is_changed = true;
		msleep(20);
	} else {
		dev_info(di->dev, "flags: 0x%04x, ITPOR flag is cleared already.", flags);
	}

	if(is_changed)
	{
		flags = bq27x00_read(di, BQ27x00_REG_FLAGS, !(is_bq27500 || is_bq27425));
		dev_info(di->dev, "flags is changed to 0x%04x.", flags);
	}

	return 0;
}

static ssize_t get_dffs_list(struct device *device,
						struct device_attribute *attr, char *buf)
{
	bq27x00_dffs_ls(buf);
	return strlen(buf);
}

void bq27x00_i2c_reset_workaround(struct bq27x00_device_info *di)
{
	int ret;

#define I2C_1_DAT_GPIO_PIN 26
#define I2C_1_CLK_GPIO_PIN 27
/* toggle clock pin of I2C to recover devices from abnormal status. */
	ret = gpio_request(I2C_1_CLK_GPIO_PIN, "i2c-1-clock");
	if (ret) {
		dev_err(di->dev, "Unable to request i2c-1-clock pin\n");	
		return;
	}

	ret = gpio_request(I2C_1_DAT_GPIO_PIN, "i2c-1-data");
	if (ret) {
		dev_err(di->dev, "Unable to request i2c-1-data pin\n");	
		return;
	}

 	lnw_gpio_set_alt(I2C_1_CLK_GPIO_PIN, LNW_GPIO);
 	lnw_gpio_set_alt(I2C_1_DAT_GPIO_PIN, LNW_GPIO);

	gpio_direction_output(I2C_1_CLK_GPIO_PIN, 0);
	gpio_direction_output(I2C_1_DAT_GPIO_PIN, 0);

//As TI engineer talked, if I2C is held by FG, pull data/clock line to low for 2 seconds,
//FG will release I2C line.
	gpio_set_value(I2C_1_CLK_GPIO_PIN, 0);
	gpio_set_value(I2C_1_DAT_GPIO_PIN, 0);
	mdelay(2 * 1000);

	lnw_gpio_set_alt(I2C_1_CLK_GPIO_PIN, LNW_ALT_1);
	lnw_gpio_set_alt(I2C_1_DAT_GPIO_PIN, LNW_ALT_1);
	mdelay(50);
}

void bq27x00_recover_link(struct bq27x00_device_info *di)
{
	int    result;
	int    i;
/* No of times we should reset I2C lines */
#define NR_I2C_RESET_CNT	8
	
    result = bq27x00_write_i2c(di, 0x00, 0x00);
	if (result < 0) {
		dev_warn(di->dev, "reset i2c device:%d\n", result);
		for (i = 0; i < NR_I2C_RESET_CNT; i++) {
			bq27x00_i2c_reset_workaround(di);
			result = bq27x00_write_i2c(di, 0x00, 0x00);
			if (result < 0)
				dev_warn(di->dev,
						"reset i2c device:%d\n", result);
			else
				break;
		}
	}	
	bq27x00_write_i2c(di, 0x01, 0x00);
	result = bq27x00_read_i2c(di, 0x00, 0);
	dev_info(di->dev, "Status code: %04x", result);
}

static void bq27x00_init_dffs(struct bq27x00_device_info *di, char *model_str)
{
	int rc;
	u16 fw_ver   = 0xBDBD;
	u8  oem_str[16];
	char curr_dffs_info[256];
	int (*dffs)[10];

	dev_info(di->dev, "start to initailize dffs.");

//Get OEM string 
	rc = bq27x00_get_oemstr(di, oem_str);
	if (rc < 0) {
		return;
	}

//Check FG dffs initialization state 
	if (strlen(oem_str) != 0) {
		bq27x00_dffs_find_by_oemstr(oem_str, curr_dffs_info);
		dev_info(di->dev, "fg dffs info is:");
		dev_info(di->dev, curr_dffs_info);

		if(strstr(curr_dffs_info, model_str) != NULL) {
			dev_info(di->dev, "fg dffs is not need to be initialized.");
			return;
		} else {
			dev_info(di->dev, "fg dffs will be switched to %s.", model_str);
		}
	} else {
		dev_info(di->dev, "fg dffs is not initialized!");
	}

//Get FG firmware version
	rc = bq27x00_get_firmware_version(di, &fw_ver);
	if (rc < 0) {
		return;
	}

//Get FG dffs 
	dffs = bq27x00_dffs_get(model_str, fw_ver);
	if (dffs == NULL) {
		dev_info(di->dev, "have not found dffs of model: %s, firmware:%04x", model_str, fw_ver);
		return;
	}

//Write dffs to FG 
	dev_info(di->dev, "initializing dffs, model: %s, firmware:%04x", model_str, fw_ver);
	bq27425_init(di, dffs);
	bq27x00_battery_seal(di);

//Release the access right of i2c 
	dev_info(di->dev, "dffs is initialized.\n");
}
 
static int bq27x00_battery_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	char *name;
	struct bq27x00_device_info *di;
	int num;
	int retval = 0;

	/* Get new ID for the new battery device */
	retval = idr_pre_get(&battery_id, GFP_KERNEL);
	if (retval == 0)
		return -ENOMEM;
	mutex_lock(&battery_mutex);
	retval = idr_get_new(&battery_id, client, &num);
	mutex_unlock(&battery_mutex);
	if (retval < 0)
		return retval;

	name = kasprintf(GFP_KERNEL, "%s-%d", id->name, num);
	if (!name) {
		dev_err(&client->dev, "failed to allocate device name\n");
		retval = -ENOMEM;
		goto batt_failed_1;
	}

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&client->dev, "failed to allocate device info data\n");
		retval = -ENOMEM;
		goto batt_failed_2;
	}

#if defined(DEBUG)
	register_dump_buffer = kzalloc(DUMP_BUFFER_SIZE, GFP_KERNEL);
	if (!register_dump_buffer) 
		dev_err(&client->dev, "Failed to alloc register dump buffer.\n");
	else
		INIT_DELAYED_WORK(&di->dump_register_worker, bq27425_dump_register_worker);
#endif	

	di->id = num;
	di->dev = &client->dev;
	//di->chip = id->driver_data;
	di->chip = BQ27500;//bq27425 similar to BQ27500,so not to add chip bq27425,use bq27500 instead it 
	di->bat.name = name;
	di->bus.read = &bq27x00_read_i2c;

	//Recover i2c line
	i2c_set_clientdata(client, di);
	bq27x00_di = di;

	//Recover i2c line
	bq27x00_recover_link(di);

	//initailize bq27x00 dffs
	bq27x00_init_dffs(di, BATTERY_MODEL);

	//Notify bq27x00, battery is inserted.
	bq27x00_insert_battery(di);

	if (bq27x00_powersupply_init(di))
		goto batt_failed_3;
    if(request_irq(gpio_to_irq(51),bq27x00_battery_det_irq, 0, "bq27425_battery", di))
		 printk("===bq27425 request_irq fail \n");

	register_reboot_notifier(&bq27x00_reboot_notifier_block);

#if defined(DEBUG)
	if (register_dump_buffer) 
		schedule_delayed_work(&di->dump_register_worker, 60 * (HZ));
#endif

	retval = device_create_file(di->dev, &dev_attr_dffs_list);
	if (retval)
		goto batt_failed_3;
	probe_passed = 1;
	return 0;

batt_failed_3:
	kfree(di);
#if defined(DEBUG)
	if (register_dump_buffer) 
		kfree(register_dump_buffer);	
#endif	
batt_failed_2:
	kfree(name);
batt_failed_1:
	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, num);
	mutex_unlock(&battery_mutex);

	return retval;
}

static int bq27x00_battery_remove(struct i2c_client *client)
{
	struct bq27x00_device_info *di = i2c_get_clientdata(client);
    unregister_reboot_notifier(&bq27x00_reboot_notifier_block);
    free_irq(gpio_to_irq(51), di);

	bq27x00_powersupply_unregister(di);

	kfree(di->bat.name);

	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, di->id);
	mutex_unlock(&battery_mutex);

	kfree(di);

	return 0;
}

static const struct i2c_device_id bq27x00_id[] = {
	{ "bq27425_battery", BQ27500 },
	//{ "bq27200", BQ27000 },	/* bq27200 is same as bq27000, but with i2c */
	//{ "bq27500", BQ27500 },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq27x00_id);

static struct i2c_driver bq27x00_battery_driver = {
	.driver = {
		.name = "bq27425_battery",
	},
	.probe = bq27x00_battery_probe,
	.remove = bq27x00_battery_remove,
	.id_table = bq27x00_id,
};

static inline int bq27x00_battery_i2c_init(void)
{
	int ret = i2c_add_driver(&bq27x00_battery_driver);
	if (ret)
		printk(KERN_ERR "Unable to register BQ27x00 i2c driver\n");

	return ret;
}

static inline void bq27x00_battery_i2c_exit(void)
{
	i2c_del_driver(&bq27x00_battery_driver);
}

#else

static inline int bq27x00_battery_i2c_init(void) { return 0; }
static inline void bq27x00_battery_i2c_exit(void) {};

#endif

/* platform specific code */
#ifdef CONFIG_BATTERY_BQ27X00_PLATFORM

static int bq27000_read_platform(struct bq27x00_device_info *di, u8 reg,
			bool single)
{
	struct device *dev = di->dev;
	struct bq27000_platform_data *pdata = dev->platform_data;
	unsigned int timeout = 3;
	int upper, lower;
	int temp;

	if (!single) {
		/* Make sure the value has not changed in between reading the
		 * lower and the upper part */
		upper = pdata->read(dev, reg + 1);
		do {
			temp = upper;
			if (upper < 0)
				return upper;

			lower = pdata->read(dev, reg);
			if (lower < 0)
				return lower;

			upper = pdata->read(dev, reg + 1);
		} while (temp != upper && --timeout);

		if (timeout == 0)
			return -EIO;

		return (upper << 8) | lower;
	}

	return pdata->read(dev, reg);
}

static int __devinit bq27000_battery_probe(struct platform_device *pdev)
{
	struct bq27x00_device_info *di;
	struct bq27000_platform_data *pdata = pdev->dev.platform_data;
	int ret;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform_data supplied\n");
		return -EINVAL;
	}

	if (!pdata->read) {
		dev_err(&pdev->dev, "no hdq read callback supplied\n");
		return -EINVAL;
	}

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&pdev->dev, "failed to allocate device info data\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, di);

	di->dev = &pdev->dev;
	di->chip = BQ27000;

	di->bat.name = pdata->name ?: dev_name(&pdev->dev);
	di->bus.read = &bq27000_read_platform;

	ret = bq27x00_powersupply_init(di);
	if (ret)
		goto err_free;

	return 0;

err_free:
	platform_set_drvdata(pdev, NULL);
	kfree(di);

	return ret;
}

static int __devexit bq27000_battery_remove(struct platform_device *pdev)
{
	struct bq27x00_device_info *di = platform_get_drvdata(pdev);

	bq27x00_powersupply_unregister(di);

	platform_set_drvdata(pdev, NULL);
	kfree(di);

	return 0;
}

static struct platform_driver bq27000_battery_driver = {
	.probe	= bq27000_battery_probe,
	.remove = __devexit_p(bq27000_battery_remove),
	.driver = {
		.name = "bq27000-battery",
		.owner = THIS_MODULE,
	},
};

static inline int bq27x00_battery_platform_init(void)
{
	int ret = platform_driver_register(&bq27000_battery_driver);
	if (ret)
		printk(KERN_ERR "Unable to register BQ27000 platform driver\n");

	return ret;
}

static inline void bq27x00_battery_platform_exit(void)
{
	platform_driver_unregister(&bq27000_battery_driver);
}

#else

static inline int bq27x00_battery_platform_init(void) { return 0; }
static inline void bq27x00_battery_platform_exit(void) {};

#endif

static int bq27x00_reboot_callback(struct notifier_block *nfb,
					unsigned long event, void *data)
{
	/* if the shutdown or reboot sequence started
	 * then block the access to maxim registers as chip
	 * cannot be recovered from broken i2c transactions
	 */
	mutex_lock(&bq27x00_di->lock);
	bq27x00_di->plat_rebooting = true;
	mutex_unlock(&bq27x00_di->lock);

	return NOTIFY_OK;
}

/*
 * Module stuff
 */

static int __init bq27x00_battery_init(void)
{
	int ret;

	probe_passed = 0;

	ret = bq27x00_battery_i2c_init();
	if (ret)
		return ret;

	ret = bq27x00_battery_platform_init();
	if (ret)
		bq27x00_battery_i2c_exit();

	return ret;
}

static void __exit bq27x00_battery_exit(void)
{
	probe_passed = 0;
	bq27x00_battery_platform_exit();
	bq27x00_battery_i2c_exit();
}

module_init(bq27x00_battery_init);
module_exit(bq27x00_battery_exit);

MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("BQ27x00 battery monitor driver");
MODULE_LICENSE("GPL");
