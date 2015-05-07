/*
 * platform_broadcom_bt.h: broadcom_bt platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_BROADCOM_BT_H_
#define _PLATFORM_BROADCOM_BT_H_

#define	UART_PORT_INDEX	0
#define	UART_BAUD_RATE	3500000
#define BCM4330_BT_SFI_GPIO_ENABLE_NAME "BT_RSTN"
#define BCM4330_BT_SFI_GPIO_RESET_NAME 	"BT-reset"

/*There is no definition in SFI table for BT wakeup pins,
 * so we will define the bt wakeup gpio directly
 */
#define BT_HOST_WAKE 78 //host wake
#define BT_EXT_WAKE 114 //ext wake

#endif
