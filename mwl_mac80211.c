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

/*
*
*   Description:  This file implements mac80211 related functions.
*
*/

#include <linux/etherdevice.h>

#include "mwl_sysadpt.h"
#include "mwl_dev.h"
#include "mwl_debug.h"
#include "mwl_fwcmd.h"
#include "mwl_tx.h"
#include "mwl_mac80211.h"

/* CONSTANTS AND MACROS
*/

#define MWL_DRV_NAME        KBUILD_MODNAME

#define MAX_AMPDU_ATTEMPTS  5

/* PRIVATE FUNCTION DECLARATION
*/

static void mwl_mac80211_tx(struct ieee80211_hw *hw,
			    struct ieee80211_tx_control *control,
			    struct sk_buff *skb);
static int mwl_mac80211_start(struct ieee80211_hw *hw);
static void mwl_mac80211_stop(struct ieee80211_hw *hw);
static int mwl_mac80211_add_interface(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif);
static void mwl_mac80211_remove_interface(struct ieee80211_hw *hw,
					  struct ieee80211_vif *vif);
static int mwl_mac80211_config(struct ieee80211_hw *hw,
			       u32 changed);
static void mwl_mac80211_bss_info_changed(struct ieee80211_hw *hw,
					  struct ieee80211_vif *vif,
					  struct ieee80211_bss_conf *info,
					  u32 changed);
static void mwl_mac80211_configure_filter(struct ieee80211_hw *hw,
					  unsigned int changed_flags,
					  unsigned int *total_flags,
					  u64 multicast);
static int mwl_mac80211_set_key(struct ieee80211_hw *hw,
				enum set_key_cmd cmd_param,
				struct ieee80211_vif *vif,
				struct ieee80211_sta *sta,
				struct ieee80211_key_conf *key);
static int mwl_mac80211_set_rts_threshold(struct ieee80211_hw *hw,
					  u32 value);
static int mwl_mac80211_sta_add(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				struct ieee80211_sta *sta);
static int mwl_mac80211_sta_remove(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   struct ieee80211_sta *sta);
static int mwl_mac80211_conf_tx(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				u16 queue,
				const struct ieee80211_tx_queue_params *params);
static int mwl_mac80211_get_stats(struct ieee80211_hw *hw,
				  struct ieee80211_low_level_stats *stats);
static int mwl_mac80211_get_survey(struct ieee80211_hw *hw,
				   int idx,
				   struct survey_info *survey);
static int mwl_mac80211_ampdu_action(struct ieee80211_hw *hw,
				     struct ieee80211_vif *vif,
				     enum ieee80211_ampdu_mlme_action action,
				     struct ieee80211_sta *sta,
				     u16 tid, u16 *ssn, u8 buf_size);
static void mwl_mac80211_remove_vif(struct mwl_priv *priv,
				    struct mwl_vif *vif);
static void mwl_mac80211_bss_info_changed_sta(struct ieee80211_hw *hw,
					      struct ieee80211_vif *vif,
					      struct ieee80211_bss_conf *info,
					      u32 changed);
static void mwl_mac80211_bss_info_changed_ap(struct ieee80211_hw *hw,
					     struct ieee80211_vif *vif,
					     struct ieee80211_bss_conf *info,
					     u32 changed);

/* PRIVATE VARIABLES
*/

static struct ieee80211_ops mwl_mac80211_ops = {
	.tx                 = mwl_mac80211_tx,
	.start              = mwl_mac80211_start,
	.stop               = mwl_mac80211_stop,
	.add_interface      = mwl_mac80211_add_interface,
	.remove_interface   = mwl_mac80211_remove_interface,
	.config             = mwl_mac80211_config,
	.bss_info_changed   = mwl_mac80211_bss_info_changed,
	.configure_filter   = mwl_mac80211_configure_filter,
	.set_key            = mwl_mac80211_set_key,
	.set_rts_threshold  = mwl_mac80211_set_rts_threshold,
	.sta_add            = mwl_mac80211_sta_add,
	.sta_remove         = mwl_mac80211_sta_remove,
	.conf_tx            = mwl_mac80211_conf_tx,
	.get_stats          = mwl_mac80211_get_stats,
	.get_survey         = mwl_mac80211_get_survey,
	.ampdu_action       = mwl_mac80211_ampdu_action,
};

static irqreturn_t (*mwl_mac80211_isr)(int irq, void *dev_id);

static const struct ieee80211_rate mwl_rates_24[] = {
	{ .bitrate = 10, .hw_value = 2, },
	{ .bitrate = 20, .hw_value = 4, },
	{ .bitrate = 55, .hw_value = 11, },
	{ .bitrate = 110, .hw_value = 22, },
	{ .bitrate = 220, .hw_value = 44, },
	{ .bitrate = 60, .hw_value = 12, },
	{ .bitrate = 90, .hw_value = 18, },
	{ .bitrate = 120, .hw_value = 24, },
	{ .bitrate = 180, .hw_value = 36, },
	{ .bitrate = 240, .hw_value = 48, },
	{ .bitrate = 360, .hw_value = 72, },
	{ .bitrate = 480, .hw_value = 96, },
	{ .bitrate = 540, .hw_value = 108, },
};

static const struct ieee80211_rate mwl_rates_50[] = {
	{ .bitrate = 60, .hw_value = 12, },
	{ .bitrate = 90, .hw_value = 18, },
	{ .bitrate = 120, .hw_value = 24, },
	{ .bitrate = 180, .hw_value = 36, },
	{ .bitrate = 240, .hw_value = 48, },
	{ .bitrate = 360, .hw_value = 72, },
	{ .bitrate = 480, .hw_value = 96, },
	{ .bitrate = 540, .hw_value = 108, },
};

/* PUBLIC FUNCTION DEFINITION
*/

struct ieee80211_ops *mwl_mac80211_get_ops(void)
{
	return &mwl_mac80211_ops;
}

void mwl_mac80211_set_isr(irqreturn_t (*isr)(int irq, void *dev_id))
{
	WLDBG_ENTER(DBG_LEVEL_5);

	mwl_mac80211_isr = isr;

	WLDBG_EXIT(DBG_LEVEL_5);
}

/* PRIVATE FUNCTION DEFINITION
*/

static void mwl_mac80211_tx(struct ieee80211_hw *hw,
			    struct ieee80211_tx_control *control,
	struct sk_buff *skb)
{
	struct mwl_priv *priv;
	int index;

	WLDBG_ENTER(DBG_LEVEL_5);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	BUG_ON(!skb);

	if (!priv->radio_on) {
		WLDBG_EXIT_INFO(DBG_LEVEL_5,
				"dropped TX frame since radio disabled");
		dev_kfree_skb_any(skb);
		return;
	}

	index = skb_get_queue_mapping(skb);

	mwl_tx_xmit(hw, index, control->sta, skb);

	WLDBG_EXIT(DBG_LEVEL_5);
}

static int mwl_mac80211_start(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;
	int rc;

	WLDBG_ENTER(DBG_LEVEL_5);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	rc = request_irq(priv->pdev->irq, mwl_mac80211_isr,
			 IRQF_SHARED, MWL_DRV_NAME, hw);
	if (rc) {
		priv->irq = -1;
		WLDBG_ERROR(DBG_LEVEL_5, "fail to register IRQ handler");
		return rc;
	}
	priv->irq = priv->pdev->irq;

	/* Enable TX reclaim and RX tasklets.
	*/
	tasklet_enable(&priv->tx_task);
	tasklet_enable(&priv->rx_task);

	/* Enable interrupts
	*/
	mwl_fwcmd_int_enable(hw);

	rc = mwl_fwcmd_radio_enable(hw);

	if (!rc)
		rc = mwl_fwcmd_set_rate_adapt_mode(hw, 0);

	if (!rc)
		rc = mwl_fwcmd_set_wmm_mode(hw, true);

	if (!rc)
		rc = mwl_fwcmd_set_dwds_stamode(hw, true);

	if (!rc)
		rc = mwl_fwcmd_set_fw_flush_timer(hw, 0);

	if (rc) {
		mwl_fwcmd_int_disable(hw);
		free_irq(priv->pdev->irq, hw);
		priv->irq = -1;
		tasklet_disable(&priv->tx_task);
		tasklet_disable(&priv->rx_task);
	} else {
		ieee80211_wake_queues(hw);
	}

	WLDBG_EXIT(DBG_LEVEL_5);

	return rc;
}

static void mwl_mac80211_stop(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;

	WLDBG_ENTER(DBG_LEVEL_5);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	mwl_fwcmd_radio_disable(hw);

	ieee80211_stop_queues(hw);

	/* Disable interrupts
	*/
	mwl_fwcmd_int_disable(hw);

	if (priv->irq != -1) {
		free_irq(priv->pdev->irq, hw);
		priv->irq = -1;
	}

	cancel_work_sync(&priv->watchdog_ba_handle);

	/* Disable TX reclaim and RX tasklets.
	*/
	tasklet_disable(&priv->tx_task);
	tasklet_disable(&priv->rx_task);

	/* Return all skbs to mac80211
	*/
	mwl_tx_done((unsigned long)hw);

	WLDBG_EXIT(DBG_LEVEL_5);
}

static int mwl_mac80211_add_interface(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	u32 macids_supported;
	int macid;

	WLDBG_ENTER(DBG_LEVEL_5);

	switch (vif->type) {
	case NL80211_IFTYPE_AP:
		macids_supported = priv->ap_macids_supported;
		break;
	case NL80211_IFTYPE_STATION:
		macids_supported = priv->sta_macids_supported;
		break;
	default:
		return -EINVAL;
	}

	macid = ffs(macids_supported & ~priv->macids_used);

	if (!macid--) {
		WLDBG_EXIT_INFO(DBG_LEVEL_5, "no macid can be allocated");
		return -EBUSY;
	}

	/* Setup driver private area.
	*/
	mwl_vif = MWL_VIF(vif);
	memset(mwl_vif, 0, sizeof(*mwl_vif));
	mwl_vif->vif = vif;
	mwl_vif->macid = macid;
	mwl_vif->seqno = 0;
	mwl_vif->is_hw_crypto_enabled = false;
	mwl_vif->is_sta = false;
	mwl_vif->beacon_info.valid = false;
	mwl_vif->iv16 = 1;
	mwl_vif->iv32 = 0;
	mwl_vif->keyidx = 0;

	if (vif->type == NL80211_IFTYPE_STATION) {
		ether_addr_copy(mwl_vif->sta_mac, vif->addr);
		mwl_vif->is_sta = true;
		mwl_fwcmd_bss_start(hw, vif, true);
		mwl_fwcmd_set_infra_mode(hw, vif);
		mwl_fwcmd_set_mac_addr_client(hw, vif, vif->addr);
	}

	if (vif->type == NL80211_IFTYPE_AP) {
		ether_addr_copy(mwl_vif->bssid, vif->addr);
		mwl_fwcmd_set_new_stn_add_self(hw, vif);
	}

	priv->macids_used |= 1 << mwl_vif->macid;
	list_add_tail(&mwl_vif->list, &priv->vif_list);

	WLDBG_EXIT(DBG_LEVEL_5);

	return 0;
}

static void mwl_mac80211_remove_interface(struct ieee80211_hw *hw,
					  struct ieee80211_vif *vif)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif = MWL_VIF(vif);

	WLDBG_ENTER(DBG_LEVEL_5);

	if (vif->type == NL80211_IFTYPE_STATION)
		mwl_fwcmd_remove_mac_addr(hw, vif, vif->addr);

	if (vif->type == NL80211_IFTYPE_AP)
		mwl_fwcmd_set_new_stn_del(hw, vif, vif->addr);

	mwl_mac80211_remove_vif(priv, mwl_vif);

	WLDBG_EXIT(DBG_LEVEL_5);
}

static int mwl_mac80211_config(struct ieee80211_hw *hw,
			       u32 changed)
{
	struct ieee80211_conf *conf = &hw->conf;
	int rc;

	WLDBG_ENTER(DBG_LEVEL_5);

	WLDBG_INFO(DBG_LEVEL_5, "change: 0x%x", changed);

	if (conf->flags & IEEE80211_CONF_IDLE)
		rc = mwl_fwcmd_radio_disable(hw);
	else
		rc = mwl_fwcmd_radio_enable(hw);

	if (rc)
		goto out;

	if (changed & IEEE80211_CONF_CHANGE_CHANNEL) {
		int rate = 0;

		if (conf->chandef.chan->band == IEEE80211_BAND_2GHZ) {
			mwl_fwcmd_set_apmode(hw, AP_MODE_2_4GHZ_11AC_MIXED);
			rate = mwl_rates_24[0].hw_value;
		} else if (conf->chandef.chan->band == IEEE80211_BAND_5GHZ) {
			mwl_fwcmd_set_apmode(hw, AP_MODE_11AC);
			rate = mwl_rates_50[0].hw_value;
		}

		rc = mwl_fwcmd_set_rf_channel(hw, conf);

		if (rc)
			goto out;

		rc = mwl_fwcmd_use_fixed_rate(hw, rate, rate);

		if (rc)
			goto out;

		rc = mwl_fwcmd_max_tx_power(hw, conf, 0);

		if (rc)
			goto out;

		rc = mwl_fwcmd_tx_power(hw, conf, 0);

		if (rc)
			goto out;

		rc = mwl_fwcmd_set_cdd(hw);
	}

out:

	WLDBG_EXIT_INFO(DBG_LEVEL_5, "return code: %d", rc);

	return rc;
}

static void mwl_mac80211_bss_info_changed(struct ieee80211_hw *hw,
					  struct ieee80211_vif *vif,
					  struct ieee80211_bss_conf *info,
					  u32 changed)
{
	WLDBG_ENTER(DBG_LEVEL_5);

	WLDBG_INFO(DBG_LEVEL_5, "interface: %d, change: 0x%x",
		   vif->type, changed);

	if (vif->type == NL80211_IFTYPE_STATION)
		mwl_mac80211_bss_info_changed_sta(hw, vif, info, changed);

	if (vif->type == NL80211_IFTYPE_AP)
		mwl_mac80211_bss_info_changed_ap(hw, vif, info, changed);

	WLDBG_EXIT(DBG_LEVEL_5);
}

static void mwl_mac80211_configure_filter(struct ieee80211_hw *hw,
					  unsigned int changed_flags,
					  unsigned int *total_flags,
					  u64 multicast)
{
	WLDBG_ENTER(DBG_LEVEL_5);

	/*
	 * AP firmware doesn't allow fine-grained control over
	 * the receive filter.
	 */
	*total_flags &= FIF_ALLMULTI | FIF_BCN_PRBRESP_PROMISC;

	WLDBG_EXIT(DBG_LEVEL_5);
}

static int mwl_mac80211_set_key(struct ieee80211_hw *hw,
				enum set_key_cmd cmd_param,
				struct ieee80211_vif *vif,
				struct ieee80211_sta *sta,
				struct ieee80211_key_conf *key)
{
	int rc = 0;
	u8 encr_type;
	u8 *addr;
	struct mwl_vif *mwl_vif = MWL_VIF(vif);
	struct mwl_sta *sta_info = MWL_STA(sta);

	WLDBG_ENTER(DBG_LEVEL_5);

	BUG_ON(!mwl_vif);
	BUG_ON(!sta_info);

	if (sta == NULL) {
		addr = vif->addr;
	} else {
		addr = sta->addr;
		if (mwl_vif->is_sta == true)
			ether_addr_copy(mwl_vif->bssid, addr);
	}

	if (cmd_param == SET_KEY) {
		rc = mwl_fwcmd_encryption_set_key(hw, vif, addr, key);

		if (rc)
			goto out;

		if ((key->cipher == WLAN_CIPHER_SUITE_WEP40)
			|| (key->cipher == WLAN_CIPHER_SUITE_WEP104)) {
			encr_type = ENCR_TYPE_WEP;
		} else if (key->cipher == WLAN_CIPHER_SUITE_CCMP) {
			encr_type = ENCR_TYPE_AES;
			if ((key->flags & IEEE80211_KEY_FLAG_PAIRWISE) == 0) {
				if (mwl_vif->is_sta == false)
					mwl_vif->keyidx = key->keyidx;
			}
		} else if (key->cipher == WLAN_CIPHER_SUITE_TKIP) {
			encr_type = ENCR_TYPE_TKIP;
		} else {
			encr_type = ENCR_TYPE_DISABLE;
		}

		rc = mwl_fwcmd_update_encryption_enable(hw, vif, addr,
							encr_type);

		if (rc)
			goto out;

		mwl_vif->is_hw_crypto_enabled = true;
	} else {
		rc = mwl_fwcmd_encryption_remove_key(hw, vif, addr, key);

		if (rc)
			goto out;
	}

out:

	WLDBG_EXIT_INFO(DBG_LEVEL_5, "return code: %d", rc);

	return rc;
}

static int mwl_mac80211_set_rts_threshold(struct ieee80211_hw *hw,
					  u32 value)
{
	int rc;

	WLDBG_ENTER(DBG_LEVEL_5);

	rc =  mwl_fwcmd_set_rts_threshold(hw, value);

	WLDBG_EXIT_INFO(DBG_LEVEL_5, "return code: %d", rc);

	return rc;
}

static int mwl_mac80211_sta_add(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				struct ieee80211_sta *sta)
{
	struct mwl_vif *mwl_vif = MWL_VIF(vif);
	struct mwl_sta *sta_info = MWL_STA(sta);
	struct ieee80211_key_conf *key;
	int rc;
	int i;

	WLDBG_ENTER(DBG_LEVEL_5);

	BUG_ON(!mwl_vif);
	BUG_ON(!sta_info);

	memset(sta_info, 0, sizeof(*sta_info));
	sta_info->iv16 = 1;
	sta_info->iv32 = 0;
	if (sta->ht_cap.ht_supported)
		sta_info->is_ampdu_allowed = true;

	if (mwl_vif->is_sta == true)
		mwl_fwcmd_set_new_stn_del(hw, vif, sta->addr);

	rc = mwl_fwcmd_set_new_stn_add(hw, vif, sta);

	for (i = 0; i < NUM_WEP_KEYS; i++) {
		key = IEEE80211_KEY_CONF(mwl_vif->wep_key_conf[i].key);

		if (mwl_vif->wep_key_conf[i].enabled)
			mwl_mac80211_set_key(hw, SET_KEY, vif, sta, key);
	}

	WLDBG_EXIT_INFO(DBG_LEVEL_5, "return code: %d", rc);

	return rc;
}

static int mwl_mac80211_sta_remove(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   struct ieee80211_sta *sta)
{
	int rc;

	WLDBG_ENTER(DBG_LEVEL_5);

	rc = mwl_fwcmd_set_new_stn_del(hw, vif, sta->addr);

	WLDBG_EXIT_INFO(DBG_LEVEL_5, "return code: %d", rc);

	return rc;
}

static int mwl_mac80211_conf_tx(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				u16 queue,
				const struct ieee80211_tx_queue_params *params)
{
	struct mwl_priv *priv = hw->priv;
	int rc = 0;

	WLDBG_ENTER(DBG_LEVEL_5);

	BUG_ON(queue > SYSADPT_TX_WMM_QUEUES - 1);

	memcpy(&priv->wmm_params[queue], params, sizeof(*params));

	if (!priv->wmm_enabled) {
		rc = mwl_fwcmd_set_wmm_mode(hw, true);
		priv->wmm_enabled = true;
	}

	if (!rc) {
		int q = SYSADPT_TX_WMM_QUEUES - 1 - queue;

		rc = mwl_fwcmd_set_edca_params(hw, q,
					       params->cw_min, params->cw_max,
			params->aifs, params->txop);
	}

	WLDBG_EXIT_INFO(DBG_LEVEL_5, "return code: %d", rc);

	return rc;
}

static int mwl_mac80211_get_stats(struct ieee80211_hw *hw,
				  struct ieee80211_low_level_stats *stats)
{
	int rc;

	WLDBG_ENTER(DBG_LEVEL_5);

	rc = mwl_fwcmd_get_stat(hw, stats);

	WLDBG_EXIT_INFO(DBG_LEVEL_5, "return code: %d", rc);

	return rc;
}

static int mwl_mac80211_get_survey(struct ieee80211_hw *hw,
				   int idx,
	struct survey_info *survey)
{
	struct mwl_priv *priv = hw->priv;
	struct ieee80211_conf *conf = &hw->conf;

	WLDBG_ENTER(DBG_LEVEL_5);

	if (idx != 0)
		return -ENOENT;

	survey->channel = conf->chandef.chan;
	survey->filled = SURVEY_INFO_NOISE_DBM;
	survey->noise = priv->noise;

	WLDBG_EXIT(DBG_LEVEL_5);

	return 0;
}

static int mwl_mac80211_ampdu_action(struct ieee80211_hw *hw,
				     struct ieee80211_vif *vif,
				     enum ieee80211_ampdu_mlme_action action,
				     struct ieee80211_sta *sta,
				     u16 tid, u16 *ssn, u8 buf_size)
{
	int i, rc = 0;
	struct mwl_priv *priv = hw->priv;
	struct mwl_ampdu_stream *stream;
	u8 *addr = sta->addr, idx;
	struct mwl_sta *sta_info = MWL_STA(sta);

	WLDBG_ENTER(DBG_LEVEL_5);

	if (!(hw->flags & IEEE80211_HW_AMPDU_AGGREGATION)) {
		WLDBG_EXIT_INFO(DBG_LEVEL_5, "no HW AMPDU");
		return -ENOTSUPP;
	}

	SPIN_LOCK(&priv->locks.stream_lock);

	stream = mwl_fwcmd_lookup_stream(hw, addr, tid);

	switch (action) {
	case IEEE80211_AMPDU_RX_START:
	case IEEE80211_AMPDU_RX_STOP:
		break;
	case IEEE80211_AMPDU_TX_START:
		/* By the time we get here the hw queues may contain outgoing
		 * packets for this RA/TID that are not part of this BA
		 * session.  The hw will assign sequence numbers to these
		 * packets as they go out.  So if we query the hw for its next
		 * sequence number and use that for the SSN here, it may end up
		 * being wrong, which will lead to sequence number mismatch at
		 * the recipient.  To avoid this, we reset the sequence number
		 * to O for the first MPDU in this BA stream.
		 */
		*ssn = 0;

		if (stream == NULL) {
			/* This means that somebody outside this driver called
			 * ieee80211_start_tx_ba_session.  This is unexpected
			 * because we do our own rate control.  Just warn and
			 * move on.
			 */
			stream = mwl_fwcmd_add_stream(hw, sta, tid);
		}

		if (stream == NULL) {
			WLDBG_EXIT_INFO(DBG_LEVEL_5, "no stream found");
			rc = -EBUSY;
			break;
		}

		stream->state = AMPDU_STREAM_IN_PROGRESS;

		/* Release the lock before we do the time consuming stuff
		*/
		SPIN_UNLOCK(&priv->locks.stream_lock);

		for (i = 0; i < MAX_AMPDU_ATTEMPTS; i++) {
			/* Check if link is still valid
			*/
			if (!sta_info->is_ampdu_allowed) {
				SPIN_LOCK(&priv->locks.stream_lock);
				mwl_fwcmd_remove_stream(hw, stream);
				SPIN_UNLOCK(&priv->locks.stream_lock);
				WLDBG_EXIT_INFO(DBG_LEVEL_5,
						"link is no valid now");
				return -EBUSY;
			}

			rc = mwl_fwcmd_check_ba(hw, stream, vif);

			if (!rc)
				break;

			WL_MSEC_SLEEP(1000);
		}

		SPIN_LOCK(&priv->locks.stream_lock);

		if (rc) {
			mwl_fwcmd_remove_stream(hw, stream);
			WLDBG_EXIT_INFO(DBG_LEVEL_5, "error code: %d", rc);
			rc = -EBUSY;
			break;
		}

		ieee80211_start_tx_ba_cb_irqsafe(vif, addr, tid);
		break;
	case IEEE80211_AMPDU_TX_STOP_CONT:
	case IEEE80211_AMPDU_TX_STOP_FLUSH:
	case IEEE80211_AMPDU_TX_STOP_FLUSH_CONT:
		if (stream) {
			if (stream->state == AMPDU_STREAM_ACTIVE) {
				idx = stream->idx;
				SPIN_UNLOCK(&priv->locks.stream_lock);
				mwl_fwcmd_destroy_ba(hw, idx);
				SPIN_LOCK(&priv->locks.stream_lock);
			}

			mwl_fwcmd_remove_stream(hw, stream);
		}

		ieee80211_stop_tx_ba_cb_irqsafe(vif, addr, tid);
		break;
	case IEEE80211_AMPDU_TX_OPERATIONAL:
		BUG_ON(stream == NULL);
		BUG_ON(stream->state != AMPDU_STREAM_IN_PROGRESS);
		SPIN_UNLOCK(&priv->locks.stream_lock);
		rc = mwl_fwcmd_create_ba(hw, stream, buf_size, vif);
		SPIN_LOCK(&priv->locks.stream_lock);

		if (!rc)
			stream->state = AMPDU_STREAM_ACTIVE;
		else {
			idx = stream->idx;
			SPIN_UNLOCK(&priv->locks.stream_lock);
			mwl_fwcmd_destroy_ba(hw, idx);
			SPIN_LOCK(&priv->locks.stream_lock);
			mwl_fwcmd_remove_stream(hw, stream);
		}
		break;
	default:
		rc = -ENOTSUPP;
	}

	SPIN_UNLOCK(&priv->locks.stream_lock);

	WLDBG_EXIT(DBG_LEVEL_5);

	return rc;
}

static void mwl_mac80211_remove_vif(struct mwl_priv *priv, struct mwl_vif *vif)
{
	if (!priv->macids_used)
		return;

	priv->macids_used &= ~(1 << vif->macid);
	list_del(&vif->list);
}

static void mwl_mac80211_bss_info_changed_sta(struct ieee80211_hw *hw,
					      struct ieee80211_vif *vif,
					      struct ieee80211_bss_conf *info,
					      u32 changed)
{
	if (changed & BSS_CHANGED_ERP_PREAMBLE) {
		mwl_fwcmd_set_radio_preamble(hw,
					     vif->bss_conf.use_short_preamble);
	}

	if ((changed & BSS_CHANGED_ASSOC) && vif->bss_conf.assoc) {

		mwl_fwcmd_set_aid(hw, vif, (u8 *) vif->bss_conf.bssid,
				  vif->bss_conf.aid);
	}
}

static void mwl_mac80211_bss_info_changed_ap(struct ieee80211_hw *hw,
					     struct ieee80211_vif *vif,
					     struct ieee80211_bss_conf *info,
					     u32 changed)
{
	if (changed & BSS_CHANGED_ERP_PREAMBLE) {
		mwl_fwcmd_set_radio_preamble(hw,
					     vif->bss_conf.use_short_preamble);
	}

	if (changed & BSS_CHANGED_BASIC_RATES) {
		int idx;
		int rate;

		/*
		 * Use lowest supported basic rate for multicasts
		 * and management frames (such as probe responses --
		 * beacons will always go out at 1 Mb/s).
		 */
		idx = ffs(vif->bss_conf.basic_rates);
		if (idx)
			idx--;

		if (hw->conf.chandef.chan->band == IEEE80211_BAND_2GHZ)
			rate = mwl_rates_24[idx].hw_value;
		else
			rate = mwl_rates_50[idx].hw_value;

		mwl_fwcmd_use_fixed_rate(hw, rate, rate);
	}

	if (changed & (BSS_CHANGED_BEACON_INT | BSS_CHANGED_BEACON)) {
		struct sk_buff *skb;

		skb = ieee80211_beacon_get(hw, vif);

		if (skb != NULL) {

			mwl_fwcmd_set_beacon(hw, vif, skb->data, skb->len);
			dev_kfree_skb_any(skb);
		}

		if ((info->ssid[0] != '\0') &&
		    (info->ssid_len != 0) &&
		    (!info->hidden_ssid))
			mwl_fwcmd_broadcast_ssid_enable(hw, vif, true);
		else
			mwl_fwcmd_broadcast_ssid_enable(hw, vif, false);
	}

	if (changed & BSS_CHANGED_BEACON_ENABLED)
		mwl_fwcmd_bss_start(hw, vif, info->enable_beacon);
}
