/*
 * Copyright (C) 2006-2015, Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Description:  This file defines transmit related functions.
 */

#ifndef _mwl_tx_h_
#define _mwl_tx_h_

int mwl_tx_init(struct ieee80211_hw *hw);
void mwl_tx_deinit(struct ieee80211_hw *hw);
void mwl_tx_xmit(struct ieee80211_hw *hw,
		 struct ieee80211_tx_control *control,
		 struct sk_buff *skb);
void mwl_tx_done(unsigned long data);

#endif /* _mwl_tx_h_ */
