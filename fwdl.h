/*
 * Copyright (C) 2006-2015, Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Description:  This file defines firmware download related functions.
 */

#ifndef _mwl_fwdl_h_
#define _mwl_fwdl_h_

#include <net/mac80211.h>

int mwl_fwdl_download_firmware(struct ieee80211_hw *hw);

#endif /* _mwl_fwdl_h_ */
