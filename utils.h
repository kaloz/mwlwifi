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

/* Description:  This file defines common utility functions. */

#ifndef _UTILS_H_
#define _UTILS_H_

static inline int utils_tid_to_ac(u8 tid)
{
	switch (tid) {
	case 0:
	case 3:
		return IEEE80211_AC_BE;
	case 1:
	case 2:
		return IEEE80211_AC_BK;
	case 4:
	case 5:
		return IEEE80211_AC_VI;
	case 6:
	case 7:
		return IEEE80211_AC_VO;
	default:
		break;
	}

	return -1;
}

static inline void utils_add_basic_rates(int band, struct sk_buff *skb)
{
	struct ieee80211_mgmt *mgmt;
	int len;
	u8 *pos;

	mgmt = (struct ieee80211_mgmt *)skb->data;
	len = skb->len - ieee80211_hdrlen(mgmt->frame_control);
	len -= 4;
	pos = (u8 *)cfg80211_find_ie(WLAN_EID_SUPP_RATES,
				     mgmt->u.assoc_req.variable,
				     len);
	if (pos) {
		pos++;
		len = *pos++;
		while (len) {
			if (band == NL80211_BAND_2GHZ) {
				if ((*pos == 2) || (*pos == 4) ||
				    (*pos == 11) || (*pos == 22))
					*pos |= 0x80;
			} else {
				if ((*pos == 12) || (*pos == 24) ||
				    (*pos == 48))
					*pos |= 0x80;
			}
			pos++;
			len--;
		}
	}
}

static inline void utils_dump_data_info(const char *prefix_str,
					const void *buf, size_t len)
{
	print_hex_dump(KERN_INFO, prefix_str, DUMP_PREFIX_OFFSET,
		       16, 1, buf, len, true);
}

static inline void utils_dump_data_debug(const char *prefix_str,
					 const void *buf, size_t len)
{
	print_hex_dump(KERN_DEBUG, prefix_str, DUMP_PREFIX_OFFSET,
		       16, 1, buf, len, true);
}

#endif /* _UTILS_H_ */
