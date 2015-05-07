/*
 * IPC util functions for theft deterrrent lock driver.
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

#ifndef TD_LOCK_DRIVER_IPC_H
#define TD_LOCK_DRIVER_IPC_H

#include <linux/module.h>
#include <linux/kernel.h>

#define SCU_SUPPORT_SRTC
extern bool td_is_tdos(void);
extern bool td_ipc_get_sclock(u32 *sclock);

#endif /* TD_LOCK_DRIVER_IPC_H */

