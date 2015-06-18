/*
 * Copyright (C) 2006-2015, Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Description:  This file defines interrupt related functions.
 */

#ifndef _mwl_isr_h_
#define _mwl_isr_h_

#include <linux/interrupt.h>

irqreturn_t mwl_isr(int irq, void *dev_id);
void mwl_watchdog_ba_events(struct work_struct *work);

#endif /* _mwl_isr_h_ */
