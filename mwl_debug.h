/*
* Copyright (c) 2006-2015 Marvell International Ltd.
*
* Permission to use, copy, modify, and/or distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
* SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
* OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
* CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/* Description:  This file defines debug related functions.
*/

#ifndef _mwl_debug_h_
#define _mwl_debug_h_

#include <linux/types.h>
#include <linux/bitops.h>

/* CONSTANTS AND MACROS
*/

#define DBG_LEVEL_0 BIT(0)    /* mwl_main.c     */
#define DBG_LEVEL_1 BIT(1)    /* mwl_fwdl.c     */
#define DBG_LEVEL_2 BIT(2)    /* mwl_fwcmd.c    */
#define DBG_LEVEL_3 BIT(3)    /* mwl_tx.c       */
#define DBG_LEVEL_4 BIT(4)    /* mwl_rx.c       */
#define DBG_LEVEL_5 BIT(5)    /* mwl_mac80211.c */
#define DBG_LEVEL_6 BIT(6)
#define DBG_LEVEL_7 BIT(7)
#define DBG_LEVEL_8 BIT(8)
#define DBG_LEVEL_9 BIT(9)
#define DBG_LEVEL_10 BIT(10)
#define DBG_LEVEL_11 BIT(11)
#define DBG_LEVEL_12 BIT(12)
#define DBG_LEVEL_13 BIT(13)
#define DBG_LEVEL_14 BIT(14)
#define DBG_LEVEL_15 BIT(15)

#define DBG_CLASS_PANIC BIT(16)
#define DBG_CLASS_ERROR BIT(17)
#define DBG_CLASS_WARNING BIT(18)
#define DBG_CLASS_ENTER BIT(19)
#define DBG_CLASS_EXIT BIT(20)
#define DBG_CLASS_INFO BIT(21)
#define DBG_CLASS_DATA BIT(22)
#define DBG_CLASS_7 BIT(23)
#define DBG_CLASS_8 BIT(24)
#define DBG_CLASS_9 BIT(25)
#define DBG_CLASS_10 BIT(26)
#define DBG_CLASS_11 BIT(27)
#define DBG_CLASS_12 BIT(28)
#define DBG_CLASS_13 BIT(29)
#define DBG_CLASS_14 BIT(30)
#define DBG_CLASS_15 BIT(31)

#define WLDBG_PRINT(...) \
	mwl_debug_prt(0, __func__, __VA_ARGS__)

#ifdef MWL_DEBUG

#define WLDBG_DUMP_DATA(classlevel, data, len) \
	mwl_debug_prtdata(classlevel | DBG_CLASS_DATA, \
	__func__, data, len, NULL)

#define WLDBG_ENTER(classlevel) \
	mwl_debug_prt(classlevel | DBG_CLASS_ENTER, __func__, NULL)

#define WLDBG_ENTER_INFO(classlevel, ...) \
	mwl_debug_prt(classlevel | DBG_CLASS_ENTER, __func__, __VA_ARGS__)

#define WLDBG_EXIT(classlevel) \
	mwl_debug_prt(classlevel | DBG_CLASS_EXIT, __func__, NULL)

#define WLDBG_EXIT_INFO(classlevel, ...) \
	mwl_debug_prt(classlevel | DBG_CLASS_EXIT, __func__, __VA_ARGS__)

#define WLDBG_INFO(classlevel, ...) \
	mwl_debug_prt(classlevel | DBG_CLASS_INFO, __func__, __VA_ARGS__)

#define WLDBG_WARNING(classlevel, ...) \
	mwl_debug_prt(classlevel | DBG_CLASS_WARNING, __func__, __VA_ARGS__)

#define WLDBG_ERROR(classlevel, ...) \
	mwl_debug_prt(classlevel | DBG_CLASS_ERROR, __func__, __VA_ARGS__)

#define WLDBG_PANIC(classlevel, ...) \
	mwl_debug_prt(classlevel | DBG_CLASS_PANIC, __func__, __VA_ARGS__)

#else

#define WLDBG_DUMP_DATA(classlevel, data, len)
#define WLDBG_ENTER(classlevel)
#define WLDBG_ENTER_INFO(classlevel, ...)
#define WLDBG_EXIT(classlevel)
#define WLDBG_EXIT_INFO(classlevel, ...)
#define WLDBG_INFO(classlevel, ...)
#define WLDBG_WARNING(classlevel, ...)
#define WLDBG_ERROR(classlevel, ...)
#define WLDBG_PANIC(classlevel, ...)

#endif /* MWL_DEBUG */

/* PUBLIC FUNCTION DECLARATION
*/

void mwl_debug_prt(u32 classlevel, const char *func, const char *format, ...);
void mwl_debug_prtdata(u32 classlevel, const char *func,
		       const void *data, int len, const char *format, ...);
void mwl_debug_dumpdata(const void *data, int len, char *marker);

#endif /* _mwl_debug_h_ */
