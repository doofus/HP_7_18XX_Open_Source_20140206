/*
 * platform_apds990x.c: apds990x platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/lnw_gpio.h>
#include <linux/i2c/apds990x.h>
#include <asm/intel-mid.h>
#include "platform_apds990x.h"

static struct apds990x_platform_data ctp_data = {
	.cf = {
		.cf1    = 7782,
		.irf1   = 2456,
		.cf2    = 1228,
		.irf2   = 1638,
		.df     = 52,
		.ga     = 15728,
	},
	.pdrive         = 0,
	.ppcount        = 1,
};

static struct apds990x_platform_data lex_data = {
	.cf = {
		.cf1    = 8602,
		.irf1   = 6552,
		.cf2    = 1064,
		.irf2   = 860,
		.df     = 52,
		.ga     = 1474,
	},
	.pdrive         = 0,
	.ppcount        = 1,
};

void *apds990x_platform_data(void *info)
{
	struct apds990x_platform_data *platform_data;

	if (INTEL_MID_BOARD(2, PHONE, MFLD, LEX, ENG) ||
		(INTEL_MID_BOARD(2, PHONE, MFLD, LEX, PRO))) {
		pr_info("apds990x_platform_data: LEX board detected\n");
		platform_data = &lex_data;
		if (SPID_HARDWARE_ID(MFLD, PHONE, LEX, PR21)
			|| SPID_HARDWARE_ID(MFLD, PHONE, LEX, PR2M)
			|| SPID_HARDWARE_ID(MFLD, PHONE, LEX, DV1)) {
			pr_info("apds990x_platform_data: LEX PR2 or DV1\n");
			platform_data->cf.ga = 11796;
		}
	} else {
		pr_info("apds990x_platform_data: CLV board detected\n");
		platform_data = &ctp_data;
	}

	platform_data->gpio_number = get_gpio_by_name("AL-intr");

	return platform_data;
}
