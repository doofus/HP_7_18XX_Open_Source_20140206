/*
 * Util functions for theft deterrrent lock driver.
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
#include <linux/tpm.h>
#include "tpmcmd.h"
#include "util.h"

static int td_tpm_chip_num;

bool td_tpm_select_chip_num(int chip_num)
{
	td_tpm_chip_num = chip_num;
	if (!tpm_chip_num_exists(chip_num)) {
		k_err("Invalid TPM chip /dev/tpm%d\n", chip_num);
		return false;
	}
	k_debug("Found TPM chip /dev/tpm%d.\n", chip_num);
	return true;
}

ssize_t td_tpm_transmit(const u8 *buf, size_t size)
{
	union tpm_cmd_header  *header_ptr;
	int rc;

	k_func_enter();

	header_ptr = (union tpm_cmd_header *)buf;
	k_debug("[Command 0x%02x input buffer(size:0x%x)]\n",
			be32_to_cpu(header_ptr->in.ordinal),
			be32_to_cpu(header_ptr->in.length));
	k_print_hexbuf("", buf, be32_to_cpu(header_ptr->in.length));

	rc = tpm_transmit_by_num(td_tpm_chip_num, buf, size);
	if (rc < 0) {
		k_err("Fail to execute command! rc=%d\n", rc);
		k_func_leave();
		return rc;
	}
	if (header_ptr->out.return_code != 0) {
		k_err("Fail to execute command! TPM return code=0x%x\n",
			  be32_to_cpu(header_ptr->out.return_code));
		k_func_leave();
		return -EIO;
	}
	k_debug("[Command output buffer(size:0x%x)]\n",
			be32_to_cpu(header_ptr->out.length));
	k_print_hexbuf("", buf, be32_to_cpu(header_ptr->out.length));

	k_func_leave();
	return rc;
}

u32 td_hexascii2int(u8 *arr, size_t size)
{
	u32 index = 0;
	u32 ret = 0;
	while (index < size) {
		if (arr[index] >= '0' && arr[index] <= '9') {
			ret += (arr[index] - '0') <<
				   ((size - index - 1) * 4);
		}
		if (arr[index] >= 'a' && arr[index] <= 'f') {
			ret += (arr[index] - 'a' + 0xa) <<
				   ((size - index - 1) * 4);
		}
		if (arr[index] >= 'A' && arr[index] <= 'F') {
			ret += (arr[index] - 'A' + 0xa) <<
				   ((size - index - 1) * 4);
		}
		index++;
	}
	return ret;
}

u32 td_bits2int(u8 *arr, size_t size)
{
	u8 index = 0;
	u32 ret = 0;
	BUG_ON(size > 4);

	while (index < size) {
		ret += arr[index] << (8 * (size - index - 1));
		index++;
	}
	return ret;
}

void td_int2bits(u32 value, u8 *arr, size_t size)
{
	int index = 0;
	BUG_ON(size > 4);

	while (index < size) {
		arr[index] = (value >> ((size - index - 1) * 8)) & 0xFF;
		index++;
	}
}

void td_ascii2date(u8 *arr, struct td_date *date)
{
	date->year  = (arr[0] - 0x30) * 10 + (arr[1] - 0x30) + 2000;
	date->month = (arr[2] - 0x30) * 10 + (arr[3] - 0x30);
	date->day   = (arr[4] - 0x30) * 10 + (arr[5] - 0x30);
}

void td_ascii2hwid(u8 *arr, u8 *hwid)
{
	hwid[0] = (u8)td_hexascii2int(&arr[0], 2);
	hwid[1] = (u8)td_hexascii2int(&arr[2], 2);
	hwid[2] = (u8)td_hexascii2int(&arr[4], 2);
	hwid[3] = (u8)td_hexascii2int(&arr[6], 2);
	hwid[4] = (u8)td_hexascii2int(&arr[8], 2);
	hwid[5] = (u8)td_hexascii2int(&arr[10], 2);
}

void td_arr_to_uuid(u8 *in, struct uuid *uu)
{
	int index;
	uu->time_low = (in[0]<<24) | (in[1]<<16) | (in[2]<<8) | in[3];
	uu->time_mid = (in[4]<<8) | in[5];
	uu->time_hi_and_version = (in[6]<<8) | in[7];
	uu->clock_seq = (in[8]<<8) | in[9];
	for (index = 0; index < 6; index++)
		uu->node[index] = in[10 + index];
}

