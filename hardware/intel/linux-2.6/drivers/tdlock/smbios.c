/*
 * SMBIOS function for theft deterrrent lock driver.
 *
 * Copyright (c) 2012, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sfi.h>
#include <asm/intel-mid.h>
#include "util.h"

struct sfi_table_oemb_extend {
	struct sfi_table_header header;
	u32 board_id;
	u32 board_fab;
	u8 iafw_major_version;
	u8 iafw_main_version;
	u8 val_hooks_major_version;
	u8 val_hooks_minor_version;
	u8 ia_suppfw_major_version;
	u8 ia_suppfw_minor_version;
	u8 scu_runtime_major_version;
	u8 scu_runtime_minor_version;
	u8 ifwi_major_version;
	u8 ifwi_minor_version;
	struct sfi_soft_platform_id spid;
	u8 serial_number[32];
	u8 sysflags;
	u8 uuid[16];
	u8 bios_version[48];
	u8 system_product_name[32];
} __packed;

static struct sfi_table_oemb_extend *oembex_table;

u8 td_get_td_flag(void)
{
	if (!oembex_table)
		return 0xff;

	k_info("OEMB->sysflags : 0x%x", oembex_table->sysflags);

	return oembex_table->sysflags;
}

static int __init td_sfi_parse_oembex(struct sfi_table_header *table)
{
	if (!table) {
		k_err("Fail to read SFI OEMB Layout\n");
		return -ENODEV;
	} else if (table->len < sizeof(struct sfi_table_oemb_extend)) {
		k_err("Invalid length of sfi oemb extend table\n");
		return -ENODEV;
	}
	oembex_table = (struct sfi_table_oemb_extend *)table;
	return 0;
}

#define smbios_attr(_name) \
static struct kobj_attribute _name##_attr = { \
	.attr = { \
		.name = __stringify(_name), \
		.mode = 0444, \
	}, \
	.show = _name##_show, \
}

static ssize_t product_id_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct uuid product_id;
	td_arr_to_uuid(oembex_table->uuid, &product_id);
	return sprintf(buf,
		"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
		product_id.time_low, product_id.time_mid,
		product_id.time_hi_and_version,
		product_id.clock_seq>>8, product_id.clock_seq&0xFF,
		product_id.node[0], product_id.node[1], product_id.node[2],
		product_id.node[3], product_id.node[4], product_id.node[5]);
}
smbios_attr(product_id);

static ssize_t bios_version_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", oembex_table->bios_version);
}
smbios_attr(bios_version);

static ssize_t system_product_name_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", oembex_table->system_product_name);
}
smbios_attr(system_product_name);

static ssize_t serial_number_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", oembex_table->serial_number);
}
smbios_attr(serial_number);

static struct attribute *smbios_attrs[] = {
	&product_id_attr.attr,
	&bios_version_attr.attr,
	&system_product_name_attr.attr,
	&serial_number_attr.attr,
	NULL,
};

static struct attribute_group smbios_attr_group = {
	.attrs = smbios_attrs,
};

int td_sfi_init(void)
{
	int ret;
	struct kobject *smbios_kobj;

	ret = sfi_table_parse(SFI_SIG_OEMB, NULL, NULL, td_sfi_parse_oembex);
	if (ret < 0) {
		k_err("Fail to parse the extend info in OEMB table");
		return ret;
	}

	smbios_kobj = kobject_create_and_add("smbios", NULL);
	if (!smbios_kobj)
		k_err("TD SMBIOS: ENOMEM for smbios_kobj\n");
	if (sysfs_create_group(smbios_kobj, &smbios_attr_group))
		k_err("TD SMBIOS: failed to create /sys/smbios\n");

	return 0;
}
