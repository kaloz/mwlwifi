/*
 * Copyright (C) 2006-2017, Marvell International Ltd.
 *
 * This software file (the "File") is distributed by Marvell International
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991
 * (the "License").  You may use, redistribute and/or modify this File in
 * accordance with the terms and conditions of the License, a copy of which
 * is available by writing to the Free Software Foundation, Inc.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 * this warranty disclaimer.
 */

/* Description:  This file defines interrupt related functions. */

#ifndef _ISR_H_
#define _ISR_H_

#include <linux/interrupt.h>

irqreturn_t mwl_isr(int irq, void *dev_id);
void mwl_chnl_switch_event(struct work_struct *work);
void mwl_watchdog_ba_events(struct work_struct *work);

#endif /* _ISR_H_ */
