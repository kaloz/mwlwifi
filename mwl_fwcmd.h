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
*   Description:  This file defines firmware host command related functions.
*
*/

#ifndef _mwl_fwcmd_h_
#define _mwl_fwcmd_h_

/* CONSTANTS AND MACROS
*/

/*
 *          Define OpMode for SoftAP/Station mode
 *
 *  The following mode signature has to be written to PCI scratch register#0
 *  right after successfully downloading the last block of firmware and
 *  before waiting for firmware ready signature
 */

#define HOSTCMD_STA_MODE                0x5A
#define HOSTCMD_SOFTAP_MODE             0xA5

#define HOSTCMD_STA_FWRDY_SIGNATURE     0xF0F1F2F4
#define HOSTCMD_SOFTAP_FWRDY_SIGNATURE  0xF1F2F4A5

/* TYPE DEFINITION
*/

enum {
	WL_ANTENNATYPE_RX = 1,
	WL_ANTENNATYPE_TX = 2,
};

enum encr_type {
	ENCR_TYPE_WEP = 0,
	ENCR_TYPE_DISABLE = 1,
	ENCR_TYPE_TKIP = 4,
	ENCR_TYPE_AES = 6,
	ENCR_TYPE_MIX = 7,
};

/* PUBLIC FUNCTION DECLARATION
*/

void mwl_fwcmd_reset(struct ieee80211_hw *hw);

void mwl_fwcmd_int_enable(struct ieee80211_hw *hw);

void mwl_fwcmd_int_disable(struct ieee80211_hw *hw);

int mwl_fwcmd_get_hw_specs(struct ieee80211_hw *hw);

int mwl_fwcmd_set_hw_specs(struct ieee80211_hw *hw);

int mwl_fwcmd_get_stat(struct ieee80211_hw *hw,
		       struct ieee80211_low_level_stats *stats);

int mwl_fwcmd_radio_enable(struct ieee80211_hw *hw);

int mwl_fwcmd_radio_disable(struct ieee80211_hw *hw);

int mwl_fwcmd_set_radio_preamble(struct ieee80211_hw *hw,
				 bool short_preamble);

int mwl_fwcmd_max_tx_power(struct ieee80211_hw *hw,
			   struct ieee80211_conf *conf, u8 fraction);

int mwl_fwcmd_tx_power(struct ieee80211_hw *hw,
		       struct ieee80211_conf *conf, u8 fraction);

int mwl_fwcmd_rf_antenna(struct ieee80211_hw *hw, int dir, int antenna);

int mwl_fwcmd_broadcast_ssid_enable(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif, bool enable);

int mwl_fwcmd_set_rf_channel(struct ieee80211_hw *hw,
			     struct ieee80211_conf *conf);

int mwl_fwcmd_set_aid(struct ieee80211_hw *hw,
		      struct ieee80211_vif *vif, u8 *bssid, u16 aid);

int mwl_fwcmd_set_infra_mode(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif);

int mwl_fwcmd_set_rts_threshold(struct ieee80211_hw *hw,
				int threshold);

int mwl_fwcmd_set_edca_params(struct ieee80211_hw *hw, u8 index,
			      u16 cw_min, u16 cw_max, u8 aifs, u16 txop);

int mwl_fwcmd_set_wmm_mode(struct ieee80211_hw *hw,
			   bool enable);

int mwl_fwcmd_use_fixed_rate(struct ieee80211_hw *hw,
			     int mcast, int mgmt);

int mwl_fwcmd_set_rate_adapt_mode(struct ieee80211_hw *hw,
				  u16 mode);

int mwl_fwcmd_set_mac_addr_client(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif, u8 *mac_addr);

int mwl_fwcmd_get_watchdog_bitmap(struct ieee80211_hw *hw,
				  u8 *bitmap);

int mwl_fwcmd_remove_mac_addr(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif, u8 *mac_addr);

int mwl_fwcmd_bss_start(struct ieee80211_hw *hw,
			struct ieee80211_vif *vif, bool enable);

int mwl_fwcmd_set_beacon(struct ieee80211_hw *hw,
			 struct ieee80211_vif *vif, u8 *beacon, int len);

int mwl_fwcmd_set_new_stn_add(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif,
			      struct ieee80211_sta *sta);

int mwl_fwcmd_set_new_stn_add_self(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif);

int mwl_fwcmd_set_new_stn_del(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif, u8 *addr);

int mwl_fwcmd_set_apmode(struct ieee80211_hw *hw, u8 apmode);

int mwl_fwcmd_update_encryption_enable(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       u8 *addr, u8 encr_type);

int mwl_fwcmd_encryption_set_key(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif, u8 *addr,
				 struct ieee80211_key_conf *key);

int mwl_fwcmd_encryption_remove_key(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif, u8 *addr,
				    struct ieee80211_key_conf *key);

int mwl_fwcmd_check_ba(struct ieee80211_hw *hw,
		       struct mwl_ampdu_stream *stream,
		       struct ieee80211_vif *vif);

int mwl_fwcmd_create_ba(struct ieee80211_hw *hw,
			struct mwl_ampdu_stream *stream,
			u8 buf_size, struct ieee80211_vif *vif);

int mwl_fwcmd_destroy_ba(struct ieee80211_hw *hw,
			 u8 idx);

struct mwl_ampdu_stream *mwl_fwcmd_add_stream(struct ieee80211_hw *hw,
					      struct ieee80211_sta *sta,
					      u8 tid);

int mwl_fwcmd_start_stream(struct ieee80211_hw *hw,
			   struct mwl_ampdu_stream *stream);

void mwl_fwcmd_remove_stream(struct ieee80211_hw *hw,
			     struct mwl_ampdu_stream *stream);

struct mwl_ampdu_stream *mwl_fwcmd_lookup_stream(struct ieee80211_hw *hw,
						 u8 *addr, u8 tid);

bool mwl_fwcmd_ampdu_allowed(struct ieee80211_sta *sta, u8 tid);

int mwl_fwcmd_set_dwds_stamode(struct ieee80211_hw *hw, bool enable);

int mwl_fwcmd_set_fw_flush_timer(struct ieee80211_hw *hw, u32 value);

int mwl_fwcmd_set_cdd(struct ieee80211_hw *hw);

#endif /* _mwl_fwcmd_h_ */
