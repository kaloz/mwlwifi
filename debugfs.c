/*
 * Copyright (C) 2006-2016, Marvell International Ltd.
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
#include "thermal.h"
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

static void dump_data(char *p, int size, int *len, u8 *data,
		      int data_len, char *title)
{
	int cur_byte = 0;
	int i;

	*len += scnprintf(p + *len, size - *len, "%s\n", title);

	for (cur_byte = 0; cur_byte < data_len; cur_byte += 8) {
		if ((cur_byte + 8) < data_len) {
			for (i = 0; i < 8; i++)
				*len += scnprintf(p + *len, size - *len,
						  "0x%02x ",
						  *(data + cur_byte + i));
			*len += scnprintf(p + *len, size - *len, "\n");
		} else {
			for (i = 0; i < (data_len - cur_byte); i++)
				*len += scnprintf(p + *len, size - *len,
						  "0x%02x ",
						  *(data + cur_byte + i));
			*len += scnprintf(p + *len, size - *len, "\n");
			break;
		}
	}
}

static ssize_t mwl_debugfs_info_read(struct file *file, char __user *ubuf,
				     size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long page = get_zeroed_page(GFP_KERNEL);
	char *p = (char *)page;
	int len = 0, size = PAGE_SIZE;
	ssize_t ret;

	if (!p)
		return -ENOMEM;

	len += scnprintf(p + len, size - len, "\n");
	len += scnprintf(p + len, size - len,
			 "driver name: %s\n", MWL_DRV_NAME);
	len += scnprintf(p + len, size - len, "chip type: %s\n",
			 (priv->chip_type == MWL8864) ? "88W8864" : "88W8897");
	len += scnprintf(p + len, size - len,
			 "hw version: %X\n", priv->hw_data.hw_version);
	len += scnprintf(p + len, size - len,
			 "driver version: %s\n", MWL_DRV_VERSION);
	len += scnprintf(p + len, size - len, "firmware version: 0x%08x\n",
			 priv->hw_data.fw_release_num);
	len += scnprintf(p + len, size - len,
			 "power table loaded from dts: %s\n",
			 priv->forbidden_setting ? "no" : "yes");
	len += scnprintf(p + len, size - len, "firmware region code: 0x%x\n",
			 priv->fw_region_code);
	len += scnprintf(p + len, size - len,
			 "mac address: %pM\n", priv->hw_data.mac_addr);
	len += scnprintf(p + len, size - len,
			 "2g: %s\n", priv->disable_2g ? "disable" : "enable");
	len += scnprintf(p + len, size - len,
			 "5g: %s\n", priv->disable_5g ? "disable" : "enable");
	len += scnprintf(p + len, size - len, "antenna: %d %d\n",
			 (priv->antenna_tx == ANTENNA_TX_4_AUTO) ? 4 : 2,
			 (priv->antenna_rx == ANTENNA_TX_4_AUTO) ? 4 : 2);
	len += scnprintf(p + len, size - len, "irq number: %d\n", priv->irq);
	len += scnprintf(p + len, size - len, "iobase0: %p\n", priv->iobase0);
	len += scnprintf(p + len, size - len, "iobase1: %p\n", priv->iobase1);
	len += scnprintf(p + len, size - len,
			 "tx limit: %d\n", priv->txq_limit);
	len += scnprintf(p + len, size - len,
			 "rx limit: %d\n", priv->recv_limit);
	len += scnprintf(p + len, size - len, "ap macid support: %08x\n",
			 priv->ap_macids_supported);
	len += scnprintf(p + len, size - len, "sta macid support: %08x\n",
			 priv->sta_macids_supported);
	len += scnprintf(p + len, size - len,
			 "macid used: %08x\n", priv->macids_used);
	len += scnprintf(p + len, size - len,
			 "qe trigger number: %d\n", priv->qe_trigger_num);
	len += scnprintf(p + len, size - len, "\n");

	ret = simple_read_from_buffer(ubuf, count, ppos, p, len);
	free_page(page);

	return ret;
}

static ssize_t mwl_debugfs_vif_read(struct file *file, char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long page = get_zeroed_page(GFP_KERNEL);
	char *p = (char *)page;
	int len = 0, size = PAGE_SIZE;
	struct mwl_vif *mwl_vif;
	struct ieee80211_vif *vif;
	char ssid[IEEE80211_MAX_SSID_LEN + 1];
	ssize_t ret;

	if (!p)
		return -ENOMEM;

	len += scnprintf(p + len, size - len, "\n");
	spin_lock_bh(&priv->vif_lock);
	list_for_each_entry(mwl_vif, &priv->vif_list, list) {
		vif = container_of((char *)mwl_vif, struct ieee80211_vif,
				   drv_priv[0]);
		len += scnprintf(p + len, size - len,
				 "macid: %d\n", mwl_vif->macid);
		switch (vif->type) {
		case NL80211_IFTYPE_AP:
			len += scnprintf(p + len, size - len, "type: ap\n");
			memcpy(ssid, vif->bss_conf.ssid,
			       vif->bss_conf.ssid_len);
			ssid[vif->bss_conf.ssid_len] = 0;
			len += scnprintf(p + len, size - len,
					 "ssid: %s\n", ssid);
			len += scnprintf(p + len, size - len,
					 "mac address: %pM\n", mwl_vif->bssid);
			break;
		case NL80211_IFTYPE_STATION:
			len += scnprintf(p + len, size - len, "type: sta\n");
			len += scnprintf(p + len, size - len,
					 "mac address: %pM\n",
					 mwl_vif->sta_mac);
			break;
		default:
			len += scnprintf(p + len, size - len,
					 "type: unknown\n");
			break;
		}
		len += scnprintf(p + len, size - len, "hw_crypto_enabled: %s\n",
				 mwl_vif->is_hw_crypto_enabled ?
				 "true" : "false");
		len += scnprintf(p + len, size - len,
				 "key idx: %d\n", mwl_vif->keyidx);
		len += scnprintf(p + len, size - len,
				 "IV: %08x%04x\n", mwl_vif->iv32,
				 mwl_vif->iv16);
		dump_data(p, size, &len, mwl_vif->beacon_info.ie_wmm_ptr,
			  mwl_vif->beacon_info.ie_wmm_len, "WMM:");
		dump_data(p, size, &len, mwl_vif->beacon_info.ie_rsn_ptr,
			  mwl_vif->beacon_info.ie_rsn_len, "RSN:");
		dump_data(p, size, &len, mwl_vif->beacon_info.ie_rsn48_ptr,
			  mwl_vif->beacon_info.ie_rsn48_len, "RSN48:");
		dump_data(p, size, &len, mwl_vif->beacon_info.ie_ht_ptr,
			  mwl_vif->beacon_info.ie_ht_len, "HT:");
		dump_data(p, size, &len, mwl_vif->beacon_info.ie_vht_ptr,
			  mwl_vif->beacon_info.ie_vht_len, "VHT:");
		len += scnprintf(p + len, size - len, "\n");
	}
	spin_unlock_bh(&priv->vif_lock);

	ret = simple_read_from_buffer(ubuf, count, ppos, p, len);
	free_page(page);

	return ret;
}

static ssize_t mwl_debugfs_sta_read(struct file *file, char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long page = get_zeroed_page(GFP_KERNEL);
	char *p = (char *)page;
	int len = 0, size = PAGE_SIZE;
	struct mwl_sta *sta_info;
	struct ieee80211_sta *sta;
	ssize_t ret;

	if (!p)
		return -ENOMEM;

	len += scnprintf(p + len, size - len, "\n");
	spin_lock_bh(&priv->sta_lock);
	list_for_each_entry(sta_info, &priv->sta_list, list) {
		sta = container_of((char *)sta_info, struct ieee80211_sta,
				   drv_priv[0]);
		len += scnprintf(p + len, size - len,
				 "mac address: %pM\n", sta->addr);
		len += scnprintf(p + len, size - len, "aid: %u\n", sta->aid);
		len += scnprintf(p + len, size - len, "ampdu: %s\n",
				 sta_info->is_ampdu_allowed ? "true" : "false");
		len += scnprintf(p + len, size - len, "amsdu: %s\n",
				 sta_info->is_amsdu_allowed ? "true" : "false");
		if (sta_info->is_amsdu_allowed) {
			len += scnprintf(p + len, size - len,
					 "amsdu cap: 0x%02x\n",
					 sta_info->amsdu_ctrl.cap);
		}
		if (sta->ht_cap.ht_supported) {
			len += scnprintf(p + len, size - len,
					 "ht_cap: 0x%04x, ampdu: %02x, %02x\n",
					 sta->ht_cap.cap,
					 sta->ht_cap.ampdu_factor,
					 sta->ht_cap.ampdu_density);
			len += scnprintf(p + len, size - len,
					 "rx_mask: 0x%02x, %02x, %02x, %02x\n",
					 sta->ht_cap.mcs.rx_mask[0],
					 sta->ht_cap.mcs.rx_mask[1],
					 sta->ht_cap.mcs.rx_mask[2],
					 sta->ht_cap.mcs.rx_mask[3]);
		}
		if (sta->vht_cap.vht_supported) {
			len += scnprintf(p + len, size - len,
					 "vht_cap: 0x%08x, mcs: %02x, %02x\n",
					 sta->vht_cap.cap,
					 sta->vht_cap.vht_mcs.rx_mcs_map,
					 sta->vht_cap.vht_mcs.tx_mcs_map);
		}
		len += scnprintf(p + len, size - len, "rx_bw: %d, rx_nss: %d\n",
				 sta->bandwidth, sta->rx_nss);
		len += scnprintf(p + len, size - len,
				 "tdls: %d, tdls_init: %d\n",
				 sta->tdls, sta->tdls_initiator);
		len += scnprintf(p + len, size - len, "wme: %d, mfp: %d\n",
				 sta->wme, sta->mfp);
		len += scnprintf(p + len, size - len, "IV: %08x%04x\n",
				 sta_info->iv32, sta_info->iv16);
		len += scnprintf(p + len, size - len, "\n");
	}
	spin_unlock_bh(&priv->sta_lock);

	ret = simple_read_from_buffer(ubuf, count, ppos, p, len);
	free_page(page);

	return ret;
}

static ssize_t mwl_debugfs_ampdu_read(struct file *file, char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long page = get_zeroed_page(GFP_KERNEL);
	char *p = (char *)page;
	int len = 0, size = PAGE_SIZE;
	struct mwl_ampdu_stream *stream;
	int i;
	struct mwl_sta *sta_info;
	struct ieee80211_sta *sta;
	ssize_t ret;

	if (!p)
		return -ENOMEM;

	len += scnprintf(p + len, size - len, "\n");
	spin_lock_bh(&priv->stream_lock);
	for (i = 0; i < SYSADPT_TX_AMPDU_QUEUES; i++) {
		stream = &priv->ampdu[i];
		len += scnprintf(p + len, size - len, "stream: %d\n", i);
		len += scnprintf(p + len, size - len, "idx: %u\n", stream->idx);
		len += scnprintf(p + len, size - len,
				 "state: %u\n", stream->state);
		if (stream->sta) {
			len += scnprintf(p + len, size - len,
					 "mac address: %pM\n",
					 stream->sta->addr);
			len += scnprintf(p + len, size - len,
					 "tid: %u\n", stream->tid);
		}
	}
	spin_unlock_bh(&priv->stream_lock);
	spin_lock_bh(&priv->sta_lock);
	list_for_each_entry(sta_info, &priv->sta_list, list) {
		for (i = 0; i < MWL_MAX_TID; i++) {
			if (sta_info->check_ba_failed[i]) {
				sta = container_of((char *)sta_info,
						   struct ieee80211_sta,
						   drv_priv[0]);
				len += scnprintf(p + len, size - len,
						 "%pM(%d): %d\n",
						 sta->addr, i,
						 sta_info->check_ba_failed[i]);
			}
		}
	}
	spin_unlock_bh(&priv->sta_lock);
	len += scnprintf(p + len, size - len, "\n");

	ret = simple_read_from_buffer(ubuf, count, ppos, p, len);
	free_page(page);

	return ret;
}

static ssize_t mwl_debugfs_device_pwrtbl_read(struct file *file,
					      char __user *ubuf,
					      size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long page = get_zeroed_page(GFP_KERNEL);
	char *p = (char *)page;
	int len = 0, size = PAGE_SIZE;
	int i, j;
	ssize_t ret;

	if (!p)
		return -ENOMEM;

	len += scnprintf(p + len, size - len, "\n");
	len += scnprintf(p + len, size - len,
			 "power table loaded from dts: %s\n",
			 priv->forbidden_setting ? "no" : "yes");
	len += scnprintf(p + len, size - len, "firmware region code: 0x%x\n",
			 priv->fw_region_code);
	len += scnprintf(p + len, size - len, "number of channel: %d\n",
			 priv->number_of_channels);
	for (i = 0; i < priv->number_of_channels; i++) {
		len += scnprintf(p + len, size - len, "%3d ",
				 priv->device_pwr_tbl[i].channel);
		for (j = 0; j < SYSADPT_TX_POWER_LEVEL_TOTAL; j++)
			len += scnprintf(p + len, size - len, "%3d ",
					 priv->device_pwr_tbl[i].tx_pwr[j]);
		len += scnprintf(p + len, size - len, "%3d ",
				 priv->device_pwr_tbl[i].dfs_capable);
		len += scnprintf(p + len, size - len, "%3d ",
				 priv->device_pwr_tbl[i].ax_ant);
		len += scnprintf(p + len, size - len, "%3d\n",
				 priv->device_pwr_tbl[i].cdd);
	}
	len += scnprintf(p + len, size - len, "\n");

	ret = simple_read_from_buffer(ubuf, count, ppos, p, len);
	free_page(page);

	return ret;
}

static ssize_t mwl_debugfs_tx_desc_read(struct file *file,
					char __user *ubuf,
					size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long page = get_zeroed_page(GFP_KERNEL);
	char *p = (char *)page;
	int len = 0, size = PAGE_SIZE;
	struct mwl_desc_data *desc;
	int i, num, write_item = -1, free_item = -1;
	ssize_t ret;

	spin_lock_bh(&priv->tx_desc_lock);
	num = priv->tx_desc_num;
	desc = &priv->desc_data[num];
	len += scnprintf(p + len, size - len, "num: %i fw_desc_cnt:%i\n",
			 num, priv->fw_desc_cnt[num]);
	for (i = 0; i < SYSADPT_MAX_NUM_TX_DESC; i++) {
		len += scnprintf(p + len, size - len, "%3i %x\n", i,
				 desc->tx_hndl[i].pdesc->status);
		if (desc->pnext_tx_hndl == &desc->tx_hndl[i])
			write_item = i;
		if (desc->pstale_tx_hndl == &desc->tx_hndl[i])
			free_item = i;
	}
	len += scnprintf(p + len, size - len, "next:%i stale:%i\n",
			 write_item, free_item);
	len += scnprintf(p + len, size - len, "\n");
	spin_unlock_bh(&priv->tx_desc_lock);

	ret = simple_read_from_buffer(ubuf, count, ppos, p, len);
	free_page(page);

	return ret;
}

static ssize_t mwl_debugfs_tx_desc_write(struct file *file,
					 const char __user *ubuf,
					 size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long addr = get_zeroed_page(GFP_KERNEL);
	char *buf = (char *)addr;
	size_t buf_size = min_t(size_t, count, PAGE_SIZE - 1);
	int tx_desc_num = 0;
	ssize_t ret;

	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, ubuf, buf_size)) {
		ret = -EFAULT;
		goto err;
	}

	if (kstrtoint(buf, 0, &tx_desc_num)) {
		ret = -EINVAL;
		goto err;
	}

	if ((tx_desc_num < 0) || (tx_desc_num >= SYSADPT_NUM_OF_DESC_DATA)) {
		ret = -EINVAL;
		goto err;
	}

	priv->tx_desc_num = tx_desc_num;
	ret = count;

err:
	free_page(addr);
	return ret;
}

static ssize_t mwl_debugfs_dfs_channel_read(struct file *file,
					    char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long page = get_zeroed_page(GFP_KERNEL);
	char *p = (char *)page;
	int len = 0, size = PAGE_SIZE;
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *channel;
	int i;
	ssize_t ret;

	if (!p)
		return -ENOMEM;

	sband = priv->hw->wiphy->bands[NL80211_BAND_5GHZ];
	if (!sband)
		return -EINVAL;

	len += scnprintf(p + len, size - len, "\n");
	for (i = 0; i < sband->n_channels; i++) {
		channel = &sband->channels[i];
		if (channel->flags & IEEE80211_CHAN_RADAR) {
			len += scnprintf(p + len, size - len,
					 "%d(%d): flags: %08x dfs_state: %d\n",
					 channel->hw_value,
					 channel->center_freq,
					 channel->flags, channel->dfs_state);
			len += scnprintf(p + len, size - len,
					 "cac timer: %d ms\n",
					 channel->dfs_cac_ms);
		}
	}
	len += scnprintf(p + len, size - len, "\n");

	ret = simple_read_from_buffer(ubuf, count, ppos, p, len);
	free_page(page);

	return ret;
}

static ssize_t mwl_debugfs_dfs_channel_write(struct file *file,
					     const char __user *ubuf,
					     size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	struct ieee80211_supported_band *sband;
	unsigned long addr = get_zeroed_page(GFP_KERNEL);
	char *buf = (char *)addr;
	size_t buf_size = min_t(size_t, count, PAGE_SIZE - 1);
	int dfs_state = 0;
	int cac_time = -1;
	struct ieee80211_channel *channel;
	int i;
	ssize_t ret;

	if (!buf)
		return -ENOMEM;

	sband = priv->hw->wiphy->bands[NL80211_BAND_5GHZ];
	if (!sband) {
		ret = -EINVAL;
		goto err;
	}

	if (copy_from_user(buf, ubuf, buf_size)) {
		ret = -EFAULT;
		goto err;
	}

	ret = sscanf(buf, "%d %d", &dfs_state, &cac_time);

	if ((ret < 1) || (ret > 2)) {
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < sband->n_channels; i++) {
		channel = &sband->channels[i];
		if (channel->flags & IEEE80211_CHAN_RADAR) {
			channel->dfs_state = dfs_state;
			if (cac_time != -1)
				channel->dfs_cac_ms = cac_time * 1000;
		}
	}
	ret = count;

err:
	free_page(addr);
	return ret;
}

static ssize_t mwl_debugfs_dfs_radar_read(struct file *file, char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long page = get_zeroed_page(GFP_KERNEL);
	char *p = (char *)page;
	int len = 0, size = PAGE_SIZE;
	ssize_t ret;

	if (!p)
		return -ENOMEM;

	len += scnprintf(p + len, size - len, "\n");
	len += scnprintf(p + len, size - len,
			 "csa_active: %d\n", priv->csa_active);
	len += scnprintf(p + len, size - len,
			 "dfs_region: %d\n", priv->dfs_region);
	len += scnprintf(p + len, size - len,
			 "chirp_count_min: %d\n", priv->dfs_chirp_count_min);
	len += scnprintf(p + len, size - len, "chirp_time_interval: %d\n",
			 priv->dfs_chirp_time_interval);
	len += scnprintf(p + len, size - len,
			 "pw_filter: %d\n", priv->dfs_pw_filter);
	len += scnprintf(p + len, size - len,
			 "min_num_radar: %d\n", priv->dfs_min_num_radar);
	len += scnprintf(p + len, size - len,
			 "min_pri_count: %d\n", priv->dfs_min_pri_count);
	len += scnprintf(p + len, size - len, "\n");

	ret = simple_read_from_buffer(ubuf, count, ppos, p, len);
	free_page(page);

	return ret;
}

static ssize_t mwl_debugfs_dfs_radar_write(struct file *file,
					   const char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;

	wiphy_info(priv->hw->wiphy, "simulate radar detected\n");
	ieee80211_radar_detected(priv->hw);

	return count;
}

static ssize_t mwl_debugfs_thermal_read(struct file *file,
					char __user *ubuf,
					size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long page = get_zeroed_page(GFP_KERNEL);
	char *p = (char *)page;
	int len = 0, size = PAGE_SIZE;
	ssize_t ret;

	if (!p)
		return -ENOMEM;

	mwl_fwcmd_get_temp(priv->hw, &priv->temperature);

	len += scnprintf(p + len, size - len, "\n");
	len += scnprintf(p + len, size - len, "quiet period: %d\n",
			 priv->quiet_period);
	len += scnprintf(p + len, size - len, "throttle state: %d\n",
			 priv->throttle_state);
	len += scnprintf(p + len, size - len, "temperature: %d\n",
			 priv->temperature);
	len += scnprintf(p + len, size - len, "\n");

	ret = simple_read_from_buffer(ubuf, count, ppos, p, len);
	free_page(page);

	return ret;
}

static ssize_t mwl_debugfs_thermal_write(struct file *file,
					 const char __user *ubuf,
					 size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long addr = get_zeroed_page(GFP_KERNEL);
	char *buf = (char *)addr;
	size_t buf_size = min_t(size_t, count, PAGE_SIZE - 1);
	int throttle_state;
	ssize_t ret;

	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, ubuf, buf_size)) {
		ret = -EFAULT;
		goto err;
	}

	if (kstrtoint(buf, 0, &throttle_state)) {
		ret = -EINVAL;
		goto err;
	}

	if (throttle_state > SYSADPT_THERMAL_THROTTLE_MAX) {
		wiphy_warn(priv->hw->wiphy,
			   "throttle state %d is exceeding the limit %d\n",
			   throttle_state, SYSADPT_THERMAL_THROTTLE_MAX);
		ret = -EINVAL;
		goto err;
	}

	priv->throttle_state = throttle_state;
	mwl_thermal_set_throttling(priv);
	ret = count;

err:
	free_page(addr);
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
		} else {
			ret = -ENOMEM;
		}
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
	unsigned long page = get_zeroed_page(GFP_KERNEL);
	char *p = (char *)page;
	int len = 0, size = PAGE_SIZE;
	int ret = 0;

	if (!p)
		return -ENOMEM;

	if (!priv->reg_type) {
		/* No command has been given */
		len += scnprintf(p + len, size - len, "0");
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
	if (!ret)
		len += scnprintf(p + len, size - len, "%u 0x%08x 0x%08x\n",
				 priv->reg_type, priv->reg_offset,
				 priv->reg_value);
	else
		len += scnprintf(p + len, size - len,
				 "error: %d(%u 0x%08x 0x%08x)\n",
				 ret, priv->reg_type, priv->reg_offset,
				 priv->reg_value);

	ret = simple_read_from_buffer(ubuf, count, ppos, p, len);

none:

	free_page(page);
	return ret;
}

static ssize_t mwl_debugfs_regrdwr_write(struct file *file,
					 const char __user *ubuf,
					 size_t count, loff_t *ppos)
{
	struct mwl_priv *priv = (struct mwl_priv *)file->private_data;
	unsigned long addr = get_zeroed_page(GFP_KERNEL);
	char *buf = (char *)addr;
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
MWLWIFI_DEBUGFS_FILE_READ_OPS(device_pwrtbl);
MWLWIFI_DEBUGFS_FILE_OPS(tx_desc);
MWLWIFI_DEBUGFS_FILE_OPS(dfs_channel);
MWLWIFI_DEBUGFS_FILE_OPS(dfs_radar);
MWLWIFI_DEBUGFS_FILE_OPS(thermal);
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
	MWLWIFI_DEBUGFS_ADD_FILE(device_pwrtbl);
	MWLWIFI_DEBUGFS_ADD_FILE(tx_desc);
	MWLWIFI_DEBUGFS_ADD_FILE(dfs_channel);
	MWLWIFI_DEBUGFS_ADD_FILE(dfs_radar);
	MWLWIFI_DEBUGFS_ADD_FILE(thermal);
	MWLWIFI_DEBUGFS_ADD_FILE(regrdwr);
}

void mwl_debugfs_remove(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv = hw->priv;

	debugfs_remove(priv->debugfs_phy);
	priv->debugfs_phy = NULL;
}
