/*
* Copyright (c) 2006-2014 Marvell International Ltd.
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

/*
*
*   Description:  This file implements debug related functions.
*
*/

#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "mwl_debug.h"

/* CONSTANTS AND MACROS
*/

#define WLDBG_CLASSES ( \
	DBG_CLASS_PANIC | \
	DBG_CLASS_ERROR | \
	DBG_CLASS_WARNING | \
	DBG_CLASS_INFO | \
	DBG_CLASS_DATA | \
	DBG_CLASS_ENTER | \
	DBG_CLASS_EXIT)

/* PRIVATE VARIABLES
*/

static u32 dbg_levels = 0;

/* PUBLIC FUNCTION DEFINITION
*/

void mwl_debug_prt(u32 classlevel, const char *func, const char *format, ...)
{
	unsigned char *debug_string;
	u32 level = classlevel & 0x0000ffff;
	u32 class = classlevel & 0xffff0000;
	va_list a_start;

	if (classlevel != 0) {

		if ((class & WLDBG_CLASSES) != class)
			return;

		if ((level & dbg_levels) != level) {

			if (class != DBG_CLASS_PANIC && class != DBG_CLASS_ERROR)
				return;
		}
	}

	if ((debug_string = kmalloc(1024, GFP_ATOMIC)) == NULL)
		return;

	if (format != NULL) {

		va_start(a_start, format);
		vsprintf(debug_string, format, a_start);
		va_end(a_start);

	} else {

		debug_string[0] = '\0';
	}

	switch (class) {

	case DBG_CLASS_ENTER:
		printk("Enter %s() ...\n", func);
		break;
	case DBG_CLASS_EXIT:
		printk("... Exit %s()\n", func);
		break;
	case DBG_CLASS_WARNING:
		printk("WARNING: ");
		break;
	case DBG_CLASS_ERROR:
		printk("ERROR: ");
		break;
	case DBG_CLASS_PANIC:
		printk("PANIC: ");
		break;
	default:
		break;
	}

	if (strlen(debug_string) > 0) {

		if (debug_string[strlen(debug_string)-1] == '\n')
			debug_string[strlen(debug_string)-1] = '\0';
			printk("%s(): %s\n", func, debug_string);
	}

	kfree(debug_string);
}

void mwl_debug_prtdata(u32 classlevel, const char *func, const void *data, int len, const char *format, ...)
{
	unsigned char *dbg_string;
	unsigned char dbg_data[16] = "";
	unsigned char *memptr = (unsigned char *)data;
	u32 level = classlevel & 0x0000ffff;
	u32 class = classlevel & 0xffff0000;
	int curr_byte = 0;
	int num_bytes = 0;
	int offset = 0;
	va_list a_start;

	if ((class & WLDBG_CLASSES) != class)
		return;

	if ((level & dbg_levels) != level)
		return;

	if ((dbg_string = kmalloc(len + 1024, GFP_ATOMIC)) == NULL)
		return;

	if (format != NULL) {

		va_start(a_start, format);
		vsprintf(dbg_string, format, a_start);
		va_end(a_start);

	} else {

		dbg_string[0] = '\0';
	}

	if (strlen(dbg_string) > 0) {

		if (dbg_string[strlen(dbg_string) - 1] == '\n')
			dbg_string[strlen(dbg_string)-1] = '\0';
			printk("%s() %s\n", func, dbg_string);

	} else {

		printk("%s()\n", func);
	}

	for (curr_byte = 0; curr_byte < len; curr_byte = curr_byte + 8) {

		if ((curr_byte + 8) < len) {

			printk("0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
				*(memptr + curr_byte + 0),
				*(memptr + curr_byte + 1),
				*(memptr + curr_byte + 2),
				*(memptr + curr_byte + 3),
				*(memptr + curr_byte + 4),
				*(memptr + curr_byte + 5),
				*(memptr + curr_byte + 6),
				*(memptr + curr_byte + 7));
		} else {

			num_bytes = len - curr_byte;
			offset = curr_byte;
			for (curr_byte = 0; curr_byte < num_bytes; curr_byte++) {

				sprintf(dbg_data, "0x%02x ", *(memptr + offset + curr_byte));
				strcat(dbg_string, dbg_data);
			}
			printk("%s\n", dbg_string);
			break;
		}
	}

	kfree(dbg_string);
}

void mwl_debug_dumpdata(const void *data, int len, char *marker)
{
	unsigned char *memptr = (unsigned char *)data;
	int curr_byte = 0;
	int num_bytes = 0;
	int offset = 0;

	printk("%s\n", marker);

	for (curr_byte = 0; curr_byte < len; curr_byte = curr_byte + 8) {

		if ((curr_byte + 8) < len) {

			printk("0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
				*(memptr + curr_byte + 0),
				*(memptr + curr_byte + 1),
				*(memptr + curr_byte + 2),
				*(memptr + curr_byte + 3),
				*(memptr + curr_byte + 4),
				*(memptr + curr_byte + 5),
				*(memptr + curr_byte + 6),
				*(memptr + curr_byte + 7));
		} else {

			num_bytes = len - curr_byte;
			offset = curr_byte;
			for (curr_byte = 0; curr_byte < num_bytes; curr_byte++)
				printk("0x%02x ", *(memptr + offset + curr_byte));
			printk("\n\n");
			break;
		}
	}
}
