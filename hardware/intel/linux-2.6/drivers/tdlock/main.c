/*
 * Therft Deterrent Driver main entry.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/reboot.h>       /* kernel_restart() */

#include "util.h"               /* k_debug, k_error */
#include "tpmcmd.h"
#include "ipc.h"
#include "sclock.h"             /* td_register_secure_clock_file */
#include "smbios.h"

#define TD_BC_NV_INDEX          0x10001003
#define TD_BC_NV_SIZE           24

#define TD_PP_NV_INDEX          0x10001004
#define TD_PP_NV_SIZE           541

struct td_bc_nv_t {
	u32  bt;
	u32  rsc;
	u32  ed;
	u8  hwid[12];
};

/*
 * Judge whether TD is support in this platform.
 */
static bool td_is_supported(void)
{
	return td_get_td_flag() == 1;
}

/*
 * Judge whether TPM is provisioned in manufactory
 * return:  if < 0, TPM error
 *          if = 0, NV is not locked
 *          if > 0, NV is locked.
 */
static int td_check_nvlock(void)
{
	int rc;

	rc = td_tpm_show_nvlock();
	if (rc < 0)
		return rc;
	return rc;
}

static void td_reboot_to_tdos(void)
{
	k_info("reboot into tdos!");
	kernel_restart("tdos");
}

static void td_reboot_to_aos(void)
{
	k_info("reboot back to android OS!");
	kernel_restart("android");
}

static bool td_check_bc_valid(void)
{
	return true;
}

static bool td_check_trust_clock(void)
{
	u32 sclock;

	return td_ipc_get_sclock(&sclock);
}

/*
 * Check whether boot certificate is expired.
 * return:  if < 0, TPM error
 *          if > 0, expired
 *          if = 0, not expired
 */
static int td_check_bc_expired(void)
{
	int rc;
	u32 curr;
	struct td_bc_nv_t *bc_nv;
	u8 buffer[TD_BC_NV_SIZE] = {0};

	rc = td_tpm_read(TD_BC_NV_INDEX, 0, buffer, TD_BC_NV_SIZE);
	if (rc < 0) {
		k_err("Fail to read TPM BC NV!\n");
		return rc;
	}

	bc_nv = (struct td_bc_nv_t *)buffer;

	td_ipc_get_sclock(&curr);
	k_debug("curr: %#X, bc_nv->rsc: %#X, bc_nv->ed: %#X",
			curr, bc_nv->rsc, bc_nv->ed);
	if (curr < bc_nv->rsc || curr > bc_nv->ed)
		return 1;

	/* if not expired, update the rsc as current SRTC. */
	bc_nv->rsc = curr;
	rc = td_tpm_write(TD_BC_NV_INDEX, 0, (u8 *)bc_nv, TD_BC_NV_SIZE);
	if (rc < 0) {
		k_err("Fail to save update BC!\n");
		return rc;
	}
	return 0;
}

/*
 * Check whether provision packets from TD server is put into TPM
 * temporary storage.
 * return: < 0  TPM error
 *         = 0  no provision packet
 *         > 0  has provision packet
 */
static int td_has_provision_packet(void)
{
	u8 buffer[TD_PP_NV_SIZE] = {0};
	int rc;

	rc = td_tpm_read(TD_PP_NV_INDEX, 0, buffer, TD_PP_NV_SIZE);
	if (rc < 0) {
		k_err("Fail to read TPM PP NV!\n");
		return rc;
	}

	if (buffer[0] == 0)
		return 0;
	return rc;
}

/*
 * Lock security storage: TPM NV 1,2,3
 */
static void td_tpm_lock_nv_index(void)
{
	td_tpm_read(0x10001001, 0, NULL, 0);
	td_tpm_write(0x10001001, 0, NULL, 0);
	td_tpm_write(0x10001002, 0, NULL, 0);
	td_tpm_write(0x10001003, 0, NULL, 0);
}

/*
 * Main entry for TD lock driver
 */
static int __init td_lock_driver_init(void)
{
	int rc;

	k_func_enter();
	/*
	 * Register proc file for reading secure clock via IPC
	 */
	td_register_secure_clock_file();

	/*
	 * Retrieve TD flag from SFI table.
	 */
	td_sfi_init();

	/*
	 * check whether the platform support TD feature by judging the flag
	 * in UMIP header.
	 */
	if (!td_is_supported()) {
		/*
		 * If platform did not support TD but mini-OS is entered, kick
		 * user back to user OS but not continue mini-OS booting.
		 */
		if (td_is_tdos()) {
			k_warn("Current platform not support TD\n");
			goto BACK_TO_AOS;
		}
		k_warn("Current platform does not support TD feature.\n");
		goto CONTINUE_BOOT;
	}

	/*
	 * If current boot is TD OS, bypass all lock checking path.
	 */
	if (td_is_tdos()) {
		k_info("I am TD OS!\n");
		goto CONTINUE_BOOT;
	}

	/*
	 * Check whether TPM exist. if not exist, reboot to TD OS
	 */
	if (!td_tpm_select_chip_num(0)) {
		k_err("TPM chipset is not found! TD lock driver failed!\n");
		goto ERROR_TO_MINIOS;
	}

	/*
	 * Check whether TD data is provisioned, if not, continue boot.
	 */
	rc = td_check_nvlock();
	if (rc < 0) {
		k_err("TPM error when read nv lock flag!\n");
		goto ERROR_TO_MINIOS;
	}
	if (rc == 0) {
		k_warn("TPM has not been initialized in manufactory!\n");
		goto CONTINUE_BOOT;
	}

	/*
	 * Check whether secure clock is correct, if incorrect, reboot to TD OS
	 */
	if (!td_check_trust_clock()) {
		k_err("Trust clock does not work! TD lock driver failed!\n");
		goto ERROR_TO_MINIOS;
	}

	if (td_tpm_physicalpresence() < 0) {
		k_err("Fail to indicate the assertion of physical presence.\n");
		goto ERROR_TO_MINIOS;
	}

	td_tpm_continueselftest();

	/* Check whether TPM is enabled. */
	if (td_tpm_show_enabled() == 0) {
		/* Check whether PP is enabled which is used to enable TPM */
		if (td_tpm_physicalenable() < 0)
			goto ERROR_TO_MINIOS;
	}

	/* Check whether TPM is acivated */
	if (td_tpm_show_active() > 0) {
		rc = td_tpm_show_tempactivate();
		if (rc < 0) {
			k_err("Fail to get stclear.deactivated.\n");
			goto ERROR_TO_MINIOS;
		} else if (rc == 0) {
			/*
			 * If TPM is under temp activate status,
			 * a cold reset is need to bring TPM back to active
			 * status.
			 */
			kernel_power_off();
		} else {
			/* Lock TPM pp interface. */
			td_tpm_lock_pp();
		}
	} else {
		/* Active TPM */
		td_tpm_physicalsetdeactive(0);
	}

	/*
	 * If TPM is disable or deactived, reboot to mini-OS.
	 */
	if (!((td_tpm_show_enabled() > 0) && (td_tpm_show_active() > 0))) {
		k_warn("TPM is not enabled! TD driver init failed!\n");
		goto ERROR_TO_MINIOS;
	}

	/*
	 * If TPM owner has not been taken or boot certificate is invalid,
	 * bypass TD logic and continue boot.
	 */
	rc = td_tpm_show_owned();
	if (rc < 0)
		goto ERROR_TO_MINIOS;

	if ((td_tpm_show_owned() == 0) ||
		((td_tpm_show_owned() > 0) && !td_check_bc_valid())) {
		k_debug("TPM owner is not taken or not found TD data in TPM!\n");
		goto CONTINUE_BOOT;
	}

	/*
	 * If server provision packet is downloaded into TPM temporary storage,
	 *  reboot into mini-OS
	 */
	if (td_has_provision_packet() != 0) {
		/*
		 * if < 0, TPM error
		 * if > 0, has provision packet
		 */
		goto ERROR_TO_MINIOS;
	}

	/*
	 * If boot certiticate is expired, boot into TD OS
	 */
	rc = td_check_bc_expired();
	if (rc < 0) {
		k_err("Fail to check BC expired!");
		goto ERROR_TO_MINIOS;
	}

	if (rc > 0) {
		k_debug("Boot certificate is expired, go to TD OS...\n");
		td_reboot_to_tdos();
		panic("Should not reach here!\n");
		k_func_leave();
		return 0;
	}

	td_tpm_lock_nv_index();

CONTINUE_BOOT:
	k_debug("Success pass TD checking, continue boot!");
	k_func_leave();
	return 0;

BACK_TO_AOS:
	td_reboot_to_aos();
	k_func_leave();
	return 0;

ERROR_TO_MINIOS:
	td_reboot_to_tdos();
	k_func_leave();
	return 0;
}

late_initcall(td_lock_driver_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TD lock kernel built-in driver");
