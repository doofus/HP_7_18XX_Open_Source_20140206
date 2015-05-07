/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef ANDROID
#define LOG_TAG "bluetoothd"
#include <cutils/log.h>
#endif

#ifdef ANDROID
#define info(format, arg...) LOGI(format, ## arg)
#else
void info(const char *format, ...) __attribute__((format(printf, 1, 2)));
#endif

#ifdef ANDROID
#define error(format, arg...) LOGE(format, ## arg)
#else
void error(const char *format, ...) __attribute__((format(printf, 1, 2)));
#endif

#ifdef ANDROID
#define btd_debug(format, arg...) LOGD(format, ## arg)
#else
void btd_debug(const char *format, ...) __attribute__((format(printf, 1, 2)));
#endif

#ifndef ANDROID
void __btd_log_init(const char *debug, int detach);
void __btd_log_cleanup(void);
void __btd_toggle_debug(void);
#else
#define __btd_log_init(debug, detach) (void)0
#define __btd_log_cleanup() (void)0
#define __btd_toggle_debug() (void)0
#endif

struct btd_debug_desc {
	const char *file;
#define BTD_DEBUG_FLAG_DEFAULT (1)
#define BTD_DEBUG_FLAG_PRINT   (1 << 0)
	unsigned int flags;
} __attribute__((aligned(8)));

/**
 * DBG:
 * @fmt: format string
 * @arg...: list of arguments
 *
 * Simple macro around btd_debug() which also include the function
 * name it is called in.
 */
#define DBG(fmt, arg...) do { \
	static struct btd_debug_desc __btd_debug_desc \
	__attribute__((used, section("__debug"), aligned(8))) = { \
		.file = __FILE__, .flags = BTD_DEBUG_FLAG_DEFAULT, \
	}; \
	if (__btd_debug_desc.flags & BTD_DEBUG_FLAG_PRINT) \
		btd_debug("%s:%s() " fmt,  __FILE__, __FUNCTION__ , ## arg); \
} while (0)

