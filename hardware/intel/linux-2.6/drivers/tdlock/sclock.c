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
#include <linux/proc_fs.h>      /* create_proc_entry() */

#include "util.h"               /* k_err(), k_func_enter(), k_func_leave() */
#include "sclock.h"             /* declaration */
#include "ipc.h"                /* td_ipc_get_sclocka() */

static int td_sclock_procfile_read(
	char *buffer,
	char **buffer_location,
	off_t offset,
	int buffer_length,
	int *eof,
	void *unused_v
	)
{
	u32 clock = 0;

	if (buffer_length < sizeof(clock))
		return 0;

	/* indicate file is always in EOF position after read */
	*eof = 1;
	if (offset > 0)
		return 0;

	td_ipc_get_sclock(&clock);
	*((u32 *)buffer) = clock;
	return sizeof(clock);
}

void td_register_secure_clock_file(void)
{
	struct proc_dir_entry *proc_sclock;

	k_func_enter();
	proc_sclock = create_proc_entry("sclock", S_IRUGO|S_IFREG, NULL);
	if (proc_sclock)
		proc_sclock->read_proc = td_sclock_procfile_read;
	else
		k_err("Fail to create proc entry for secure clock!\n");
	k_func_leave();
}

