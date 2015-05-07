/*
 * platform_broadcom.h: bcm4330/4334 platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_BCM4330_H_
#define _PLATFORM_BCM4330_H_

#define BCM4330_SFI_GPIO_IRQ_NAME "WLAN-interrupt"
#define BCM4330_SFI_GPIO_ENABLE_NAME "WLAN-enable"
#define ICDK_BOARD_REF_CLK 26000000
#define NCDK_BOARD_REF_CLK 37400000

extern void __init *bcm4330_platform_data(void *info) __attribute__((weak));
#endif
