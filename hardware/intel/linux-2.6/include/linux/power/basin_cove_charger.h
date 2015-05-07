/*
 * basincove_charger.h: Basincove charger header file
 *
 * (C) Copyright 2012 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#ifndef __BASINCOVE_CHARGER_H_
#define __BASINCOVE_CHARGER_H_

#ifdef CONFIG_INTEL_MID_BASIN_COVE_CHARGER
extern int bc_check_battery_health(void);
extern int bc_check_battery_status(void);
extern int bc_get_battery_pack_temp(int *val);
#else
int bc_check_battery_health(void)
{
	return 0;
}
int bc_check_battery_status(void)
{
	return 0;
}
int bc_get_battery_pack_temp(int *val)
{
	return 0;
}
#endif

#endif
