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

#ifndef TD_LOCK_DRIVER_UTIL_H
#define TD_LOCK_DRIVER_UTIL_H

#include <linux/module.h>
#include <linux/kernel.h>

#define TD_DEBUG

#ifdef TD_DEBUG
	#define k_info(fmt, ...)    \
		printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
	#define k_debug(fmt, ...)   \
		printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
	#define k_warn(fmt, ...)    \
		printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
	#define k_func_enter()      \
		k_debug("[%s] >>>\n", __func__)
	#define k_func_leave()      \
		k_debug("[%s] <<<\n", __func__)
	#define k_print_hexbuf(prefix, buf, len) \
		print_hex_dump(KERN_DEBUG, prefix, DUMP_PREFIX_OFFSET, 16, 1, \
						buf, len, true)
#else
	#define k_info(fmt, ...)
	#define k_debug(fmt, ...)
	#define k_warn(fmt, ...)
	#define k_func_enter()
	#define k_func_leave()
	#define k_print_hexbuf(prefix, buf, len)
#endif

#define k_err(fmt, ...)     \
	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define k_crit(fmt, ...)    \
	printk(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)

struct td_date {
	u16 year;
	u8  month;
	u8  day;
};

struct uuid {
	u32	time_low;
	u16	time_mid;
	u16 time_hi_and_version;
	u16 clock_seq;
	u8	node[6];
};

extern ssize_t td_tpm_transmit(const u8 *buf, size_t size);
extern bool td_tpm_select_chip_num(int chip_num);
extern u32 td_hexascii2int(u8 *arr, size_t size);
extern void td_ascii2date(u8 *arr, struct td_date *date);
extern void td_ascii2hwid(u8 *arr, u8 *hwid);
extern u32 td_bits2int(u8 *arr, size_t size);
extern void td_int2bits(u32 value, u8 *arr, size_t size);
/* tpm_chip_num_exists() and tpm_transmit_by_num is implemented by
 * drivers/char/tpm driver.
 */
extern int tpm_chip_num_exists(int chip_num);
extern ssize_t tpm_transmit_by_num(int chip_num, const char *buf,
		size_t bufsiz);
extern void td_arr_to_uuid(u8 *in, struct uuid *uu);

#endif
