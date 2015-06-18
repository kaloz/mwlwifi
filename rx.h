/*
 * Copyright (C) 2006-2015, Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Description:  This file defines receive related functions.
 */

#ifndef _mwl_rx_h_
#define _mwl_rx_h_

int mwl_rx_init(struct ieee80211_hw *hw);
void mwl_rx_deinit(struct ieee80211_hw *hw);
void mwl_rx_recv(unsigned long data);

#endif /* _mwl_rx_h_ */
