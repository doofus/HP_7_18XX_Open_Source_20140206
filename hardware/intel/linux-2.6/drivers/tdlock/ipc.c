/*
 * IPC util function for theft deterrrent lock driver.
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
#include "ipc.h"
#include "util.h"
#include <asm/intel_scu_ipcutil.h> /* intel_scu_ipc_read_osnib_rr */

#ifdef SCU_SUPPORT_SRTC
	#include <asm/intel_scu_ipc.h> /* intel_scu_ipc_command */
	#include <linux/time.h> /* mktime */
#else
	#include <linux/timer.h> /* do_gettimeofday */
#endif

#ifdef SCU_SUPPORT_SRTC
	#define IPCMSG_OEM_IPC	0xE8
	#define IPCMSG_OEM_IPC_SRTC_OPCODE 0x81
#endif

#define SIGNED_TDOS_ATTR	0x14

bool td_is_tdos(void)
{
	int ret;
	u8 reboot_reason;

	ret = intel_scu_ipc_read_osnib_rr(&reboot_reason);
	if (ret < 0) {
		k_err("Fail to get reboot reason.\n");
		return false;
	}

	k_info("%s: reboot reason = %d\n", __func__, reboot_reason);
	return reboot_reason == SIGNED_TDOS_ATTR;
}

bool td_ipc_get_sclock(u32 *sclock)
{
#ifdef SCU_SUPPORT_SRTC
	int      ret;
	u8       fwver[16] = {0};
	u8       inbuf[16] = {0};
	u32      outbuf[4] = {0};
	int      reset;

	ret = intel_scu_ipc_command(IPCMSG_FW_REVISION, 0, NULL, 0,
			(u32 *)fwver, 4);
	if (ret < 0) {
		k_err("Fail to get firmware version, ret=0x%x\n",
			ret);
	} else {
		k_debug(KERN_DEBUG "Firmware version info is as follows:\n");
		k_print_hexbuf("", fwver, 16);
	}

	inbuf[3] = IPCMSG_OEM_IPC_SRTC_OPCODE;	/* opcode for STRC */
	ret = intel_scu_ipc_command(IPCMSG_OEM_IPC, 0, inbuf, 8, outbuf, 2);
	if (ret < 0) {
		k_err("Fail to get SRTC via IPC 0xE8, ret=0x%x\n",
			ret);
		return false;
	} else {
		k_debug("SRTC: hour %02d: min %02d: sec %02d - ",
			   (int)((outbuf[0] >> 2*8) & 0xff),
			   (int)((outbuf[0] >> 1*8) & 0xff),
			   (int)((outbuf[0] >> 0*8) & 0xff));

		k_debug("day %02d: month %02d: year %04d: ",
			   (int)((outbuf[1] >> 0*8) & 0xff),
			   (int)((outbuf[1] >> 1*8) & 0xff),
			   (int)((outbuf[1] >> 2*8) & 0xff) + 1900);

		k_debug("reset1 %02d: reset2 %02d\n",
			   (int)((outbuf[1] >> 3*8) & 0xff),
			   (int)((outbuf[0] >> 3*8) & 0xff));

		reset = (int)((outbuf[1] >> 3*8) & 0xff);
		/* if reset value is 0, the battery is drying out
		   or hacked. */
		if (reset == 0)
			return false;

		*sclock = (u32) mktime(
			(int)((outbuf[1] >> 2*8) & 0xff) + 1900,
			(int)((outbuf[1] >> 1*8) & 0xff) + 1,
			(int)((outbuf[1] >> 0*8) & 0xff) + 1,
			(int)((outbuf[0] >> 2*8) & 0xff),
			(int)((outbuf[0] >> 1*8) & 0xff),
			(int)((outbuf[0] >> 0*8) & 0xff));
		k_debug("Convert SRTC to timestamp: 0x%x.\n", *sclock);
	}
#else
	/* use RTC if target platform do not support SRTC */
	struct timeval txc;
	do_gettimeofday(&txc);
	*sclock = txc.tv_sec;
#endif
	return true;
}

