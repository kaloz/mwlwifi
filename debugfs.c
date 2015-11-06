/*
 * Copyright (C) 2006-2015, Marvell International Ltd.
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

/* Description:  This file implements debug fs related functions. */

#include <linux/debugfs.h>

#include "sysadpt.h"
#include "dev.h"
#include "hostcmd.h"
#include "fwcmd.h"
#include "debugfs.h"

#define MWLWIFI_DEBUGFS_ADD_FILE(name) do { \
	if (!debugfs_create_file(#name, 0644, priv->debugfs_phy, \
				 priv, &mwl_debugfs_##name##_fops)) \
		return; \
} while (0)

#define MWLWIFI_DEBUGFS_FILE_OPS(name) \
static const struct file_operations mwl_debugfs_##name##_fops = { \
	.read = mwl_debugfs_##name##_read, \
	.write = mwl_debugfs_##name##_write, \
	.open = simple_open, \
}

#define MWLWIFI_DEBUGFS_FILE_READ_OPS(name) \
static const struct file_operations mwl_debugfs_##name##_fops = { \
	.read = mwl_debugfs_##name##_read, \
	.open = simple_open, \
}

#define MWLWIFI_DEBUGFS_FILE_WRITE_OPS(name) \
static const struct file_operations mwl_debugfs_##name##_fops = { \
	.write = mwl_debugfs_##name##_write, \
	.open = simple_open, \
}

static int print_mac_addr(char *p, u8 *mac_addr)
{
	int i;
	char *str = p;

	str += sprintf(str, "mac address: %02x", mac_addr[0]);
	for (i = 1; i < ETH_ALEN; i++)
		str += sprintf(str, ":%02x", mac_addr[i]);
	str += sprintf(str, "\n");

	return str-p;
}

static int dump_data(char *p, u8 *data, int len, char *title)
{
	char *str = p;
	int cur_byte = 0;
	int i;

	str += sprintf(str, "%s\n", title);
	for (cur_byte = 0; cur_byte < len; cur_byte += 8) {
		if ((cur_byte + 8) < len) {
			for (i = 0; i < 8; i++)
				str += sprintf(str, "0x%02x ",
					       *(data+cur_byte+i));
			str += sprintf(str, "\n");
		} else {
			for (i = 0; i < (len - cur_byte); i++)
				str += sprintf(str, "0x%02x ",
					       *(data+cur_byte+i));
			str += sprintf(str, "\n");
			break;
		}
	}

	return str-p;
}

static ssize_t mwl_debugfs_info_read(struct file *file, char __user *ubuf,
				     size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long page = get_zeroed_page(GFP_KERNEL);
	char *p = (char *)page;
	ssize_t ret;

	if (!p)
		return -ENOMEM;

	p += sprintf(p, "\n");
	p += sprintf(p, "driver name: %s\n", MWL_DRV_NAME);
	p += sprintf(p, "chip type: %s\n",
		     (priv->chip_type == MWL8864) ? "88W8864" : "88W8897");
	p += sprintf(p, "hw version: %X\n", priv->hw_data.hw_version);
	p += sprintf(p, "driver version: %s\n", MWL_DRV_VERSION);
	p += sprintf(p, "firmware version: 0x%08x\n",
		     priv->hw_data.fw_release_num);
	p += print_mac_addr(p, priv->hw_data.mac_addr);
	p += sprintf(p, "2g: %s\n", priv->disable_2g ? "disable" : "enable");
	p += sprintf(p, "5g: %s\n", priv->disable_5g ? "disable" : "enable");
	p += sprintf(p, "antenna: %d %d\n",
		     (priv->antenna_tx == ANTENNA_TX_4_AUTO) ? 4 : 2,
		     (priv->antenna_rx == ANTENNA_TX_4_AUTO) ? 4 : 2);
	p += sprintf(p, "irq number: %d\n", priv->irq);
	p += sprintf(p, "iobase0: %p\n", priv->iobase0);
	p += sprintf(p, "iobase1: %p\n", priv->iobase1);
	p += sprintf(p, "tx limit: %d\n", priv->txq_limit);
	p += sprintf(p, "rx limit: %d\n", priv->recv_limit);
	p += sprintf(p, "ap macid support: %08x\n",
		     priv->ap_macids_supported);
	p += sprintf(p, "sta macid support: %08x\n",
		     priv->sta_macids_supported);
	p += sprintf(p, "macid used: %08x\n", priv->macids_used);
	p += sprintf(p, "mfg mode: %s\n", priv->mfg_mode ? "true" : "false");
	p += sprintf(p, "\n");

	ret = simple_read_from_buffer(ubuf, count, ppos, (char *) page,
				      (unsigned long) p - page);
	free_page(page);

	return ret;
}

static ssize_t mwl_debugfs_vif_read(struct file *file, char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long page = get_zeroed_page(GFP_KERNEL);
	char *p = (char *)page;
	struct mwl_vif *mwl_vif;
	struct ieee80211_vif *vif;
	char ssid[IEEE80211_MAX_SSID_LEN+1];
	ssize_t ret;

	if (!p)
		return -ENOMEM;

	p += sprintf(p, "\n");
	spin_lock_bh(&priv->vif_lock);
	list_for_each_entry(mwl_vif, &priv->vif_list, list) {
		vif = container_of((char *)mwl_vif, struct ieee80211_vif,
				   drv_priv[0]);
		p += sprintf(p, "macid: %d\n", mwl_vif->macid);
		switch (vif->type) {
		case NL80211_IFTYPE_AP:
			p += sprintf(p, "type: ap\n");
			memcpy(ssid, vif->bss_conf.ssid,
			       vif->bss_conf.ssid_len);
			ssid[vif->bss_conf.ssid_len] = 0;
			p += sprintf(p, "ssid: %s\n", ssid);
			p += print_mac_addr(p, mwl_vif->bssid);
			break;
		case NL80211_IFTYPE_MESH_POINT:
			p += sprintf(p, "type: mesh\n");
			p += print_mac_addr(p, mwl_vif->bssid);
			break;
		case NL80211_IFTYPE_STATION:
			p += sprintf(p, "type: sta\n");
			p += print_mac_addr(p, mwl_vif->sta_mac);
			break;
		default:
			p += sprintf(p, "type: unknown\n");
			break;
		}
		p += sprintf(p, "hw_crypto_enabled: %s\n",
			     mwl_vif->is_hw_crypto_enabled ? "true" : "false");
		p += sprintf(p, "key idx: %d\n", mwl_vif->keyidx);
		p += sprintf(p, "IV: %08x%04x\n", mwl_vif->iv32, mwl_vif->iv16);
		p += dump_data(p, mwl_vif->beacon_info.ie_wmm_ptr,
			       mwl_vif->beacon_info.ie_wmm_len, "WMM:");
		p += dump_data(p, mwl_vif->beacon_info.ie_rsn_ptr,
			       mwl_vif->beacon_info.ie_rsn_len, "RSN:");
		p += dump_data(p, mwl_vif->beacon_info.ie_rsn48_ptr,
			       mwl_vif->beacon_info.ie_rsn48_len, "RSN48:");
		p += dump_data(p, mwl_vif->beacon_info.ie_ht_ptr,
			       mwl_vif->beacon_info.ie_ht_len, "HT:");
		p += dump_data(p, mwl_vif->beacon_info.ie_vht_ptr,
			       mwl_vif->beacon_info.ie_vht_len, "VHT:");
		p += sprintf(p, "\n");
	}
	spin_unlock_bh(&priv->vif_lock);

	ret = simple_read_from_buffer(ubuf, count, ppos, (char *) page,
				      (unsigned long) p - page);
	free_page(page);

	return ret;
}

static ssize_t mwl_debugfs_sta_read(struct file *file, char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long page = get_zeroed_page(GFP_KERNEL);
	char *p = (char *)page;
	struct mwl_sta *sta_info;
	struct ieee80211_sta *sta;
	ssize_t ret;

	if (!p)
		return -ENOMEM;

	p += sprintf(p, "\n");
	spin_lock_bh(&priv->sta_lock);
	list_for_each_entry(sta_info, &priv->sta_list, list) {
		sta = container_of((char *)sta_info, struct ieee80211_sta,
				   drv_priv[0]);
		p += print_mac_addr(p, sta->addr);
		p += sprintf(p, "aid: %u\n", sta->aid);
		p += sprintf(p, "ampdu: %s\n",
			     sta_info->is_ampdu_allowed ? "true" : "false");
		p += sprintf(p, "amsdu: %s\n",
			     sta_info->is_amsdu_allowed ? "true" : "false");
		if (sta_info->is_amsdu_allowed) {
			p += sprintf(p, "amsdu cap: 0x%02x\n",
				     sta_info->amsdu_ctrl.cap);
		}
		p += sprintf(p, "IV: %08x%04x\n",
			     sta_info->iv32, sta_info->iv16);
		p += sprintf(p, "\n");
	}
	spin_unlock_bh(&priv->sta_lock);

	ret = simple_read_from_buffer(ubuf, count, ppos, (char *) page,
				      (unsigned long) p - page);
	free_page(page);

	return ret;
}

static ssize_t mwl_debugfs_ampdu_read(struct file *file, char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long page = get_zeroed_page(GFP_KERNEL);
	char *p = (char *)page;
	struct mwl_ampdu_stream *stream;
	int i;
	ssize_t ret;

	if (!p)
		return -ENOMEM;

	p += sprintf(p, "\n");
	spin_lock_bh(&priv->stream_lock);
	for (i = 0; i < SYSADPT_TX_AMPDU_QUEUES; i++) {
		stream = &priv->ampdu[i];
		p += sprintf(p, "stream: %d\n", i);
		p += sprintf(p, "idx: %u\n", stream->idx);
		p += sprintf(p, "state: %u\n", stream->state);
		if (stream->sta) {
			p += print_mac_addr(p, stream->sta->addr);
			p += sprintf(p, "tid: %u\n", stream->tid);
		}
	}
	spin_unlock_bh(&priv->stream_lock);
	p += sprintf(p, "\n");

	ret = simple_read_from_buffer(ubuf, count, ppos, (char *) page,
				      (unsigned long) p - page);
	free_page(page);

	return ret;
}

static int mwl_debugfs_reg_access(struct mwl_priv *priv, bool write)
{
	struct ieee80211_hw *hw = priv->hw;
	u8 set;
	u32 *addr_val;
	int ret = 0;

	set = write ? WL_SET : WL_GET;

	switch (priv->reg_type) {
	case MWL_ACCESS_RF:
		ret = mwl_fwcmd_reg_rf(hw, set, priv->reg_offset,
				       &priv->reg_value);
		break;
	case MWL_ACCESS_BBP:
		ret = mwl_fwcmd_reg_bb(hw, set, priv->reg_offset,
				       &priv->reg_value);
		break;
	case MWL_ACCESS_CAU:
		ret = mwl_fwcmd_reg_cau(hw, set, priv->reg_offset,
					&priv->reg_value);
		break;
	case MWL_ACCESS_ADDR0:
		if (set == WL_GET)
			priv->reg_value =
				readl(priv->iobase0 + priv->reg_offset);
		else
			writel(priv->reg_value,
			       priv->iobase0 + priv->reg_offset);
		break;
	case MWL_ACCESS_ADDR1:
		if (set == WL_GET)
			priv->reg_value =
				readl(priv->iobase1 + priv->reg_offset);
		else
			writel(priv->reg_value,
			       priv->iobase1 + priv->reg_offset);
		break;
	case MWL_ACCESS_ADDR:
		addr_val = kmalloc(64 * sizeof(u32), GFP_KERNEL);
		if (addr_val) {
			memset(addr_val, 0, 64 * sizeof(u32));
			addr_val[0] = priv->reg_value;
			ret = mwl_fwcmd_get_addr_value(hw, priv->reg_offset,
						       4, addr_val, set);
			if ((!ret) && (set == WL_GET))
				priv->reg_value = addr_val[0];
			kfree(addr_val);
		} else
			ret = -ENOMEM;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static ssize_t mwl_debugfs_regrdwr_read(struct file *file, char __user *ubuf,
					size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long addr = get_zeroed_page(GFP_KERNEL);
	char *buf = (char *) addr;
	int pos = 0, ret = 0;

	if (!buf)
		return -ENOMEM;

	if (!priv->reg_type) {
		/* No command has been given */
		pos += snprintf(buf, PAGE_SIZE, "0");
		goto none;
	}

	/* Set command has been given */
	if (priv->reg_value != UINT_MAX) {
		ret = mwl_debugfs_reg_access(priv, true);
		goto done;
	}
	/* Get command has been given */
	ret = mwl_debugfs_reg_access(priv, false);

done:
	pos += snprintf(buf, PAGE_SIZE, "%u 0x%08x 0x%08x\n",
			priv->reg_type, priv->reg_offset,
			priv->reg_value);
	ret = simple_read_from_buffer(ubuf, count, ppos, buf, pos);

none:

	free_page(addr);
	return ret;
}

static ssize_t mwl_debugfs_regrdwr_write(struct file *file,
					 const char __user *ubuf,
					 size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long addr = get_zeroed_page(GFP_KERNEL);
	char *buf = (char *) addr;
	size_t buf_size = min_t(size_t, count, PAGE_SIZE - 1);
	int ret;
	u32 reg_type = 0, reg_offset = 0, reg_value = UINT_MAX;

	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, ubuf, buf_size)) {
		ret = -EFAULT;
		goto done;
	}

	ret = sscanf(buf, "%u %x %x", &reg_type, &reg_offset, &reg_value);

	if (!reg_type) {
		ret = -EINVAL;
		goto done;
	} else {
		priv->reg_type = reg_type;
		priv->reg_offset = reg_offset;
		priv->reg_value = reg_value;
		ret = count;
	}
done:

	free_page(addr);
	return ret;
}

MWLWIFI_DEBUGFS_FILE_READ_OPS(info);
MWLWIFI_DEBUGFS_FILE_READ_OPS(vif);
MWLWIFI_DEBUGFS_FILE_READ_OPS(sta);
MWLWIFI_DEBUGFS_FILE_READ_OPS(ampdu);
MWLWIFI_DEBUGFS_FILE_OPS(regrdwr);

void mwl_debugfs_init(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv = hw->priv;

	if (!priv->debugfs_phy)
		priv->debugfs_phy = debugfs_create_dir("mwlwifi",
						       hw->wiphy->debugfsdir);

	if (!priv->debugfs_phy)
		return;

	MWLWIFI_DEBUGFS_ADD_FILE(info);
	MWLWIFI_DEBUGFS_ADD_FILE(vif);
	MWLWIFI_DEBUGFS_ADD_FILE(sta);
	MWLWIFI_DEBUGFS_ADD_FILE(ampdu);
	MWLWIFI_DEBUGFS_ADD_FILE(regrdwr);
}

void mwl_debugfs_remove(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv = hw->priv;

	debugfs_remove(priv->debugfs_phy);
	priv->debugfs_phy = NULL;
}
