/*
 * Copyright (C) 2006-2015, Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Description:  This file implements frimware host command related functions.
 */

#include <linux/etherdevice.h>

#include "sysadpt.h"
#include "dev.h"
#include "fwcmd.h"

#define MAX_WAIT_FW_COMPLETE_ITERATIONS         10000
#define MAX_WAIT_GET_HW_SPECS_ITERATONS         3

/* 16 bit host command code */
#define HOSTCMD_CMD_GET_HW_SPEC                 0x0003
#define HOSTCMD_CMD_SET_HW_SPEC                 0x0004
#define HOSTCMD_CMD_802_11_GET_STAT             0x0014
#define HOSTCMD_CMD_802_11_RADIO_CONTROL        0x001c
#define HOSTCMD_CMD_802_11_TX_POWER             0x001f
#define HOSTCMD_CMD_802_11_RF_ANTENNA           0x0020
#define HOSTCMD_CMD_BROADCAST_SSID_ENABLE       0x0050 /* per-vif */
#define HOSTCMD_CMD_SET_RF_CHANNEL              0x010a
#define HOSTCMD_CMD_SET_AID                     0x010d /* per-vif */
#define HOSTCMD_CMD_SET_INFRA_MODE              0x010e /* per-vif */
#define HOSTCMD_CMD_802_11_RTS_THSD             0x0113
#define HOSTCMD_CMD_SET_EDCA_PARAMS             0x0115
#define HOSTCMD_CMD_SET_WMM_MODE                0x0123
#define HOSTCMD_CMD_SET_FIXED_RATE              0x0126
#define HOSTCMD_CMD_SET_IES                     0x0127
#define HOSTCMD_CMD_SET_MAC_ADDR                0x0202 /* per-vif */
#define HOSTCMD_CMD_SET_RATE_ADAPT_MODE         0x0203
#define HOSTCMD_CMD_GET_WATCHDOG_BITMAP         0x0205
#define HOSTCMD_CMD_DEL_MAC_ADDR                0x0206 /* pre-vif */
#define HOSTCMD_CMD_BSS_START                   0x1100 /* per-vif */
#define HOSTCMD_CMD_AP_BEACON                   0x1101 /* per-vif */
#define HOSTCMD_CMD_SET_NEW_STN                 0x1111 /* per-vif */
#define HOSTCMD_CMD_SET_APMODE                  0x1114
#define HOSTCMD_CMD_UPDATE_ENCRYPTION           0x1122 /* per-vif */
#define HOSTCMD_CMD_BASTREAM                    0x1125
#define HOSTCMD_CMD_DWDS_ENABLE                 0x1144
#define HOSTCMD_CMD_FW_FLUSH_TIMER              0x1148
#define HOSTCMD_CMD_SET_CDD                     0x1150

/* Define general result code for each command */
#define HOSTCMD_RESULT_OK                       0x0000
/* General error */
#define HOSTCMD_RESULT_ERROR                    0x0001
/* Command is not valid */
#define HOSTCMD_RESULT_NOT_SUPPORT              0x0002
/* Command is pending (will be processed) */
#define HOSTCMD_RESULT_PENDING                  0x0003
/* System is busy (command ignored) */
#define HOSTCMD_RESULT_BUSY                     0x0004
/* Data buffer is not big enough */
#define HOSTCMD_RESULT_PARTIAL_DATA             0x0005

/* Define channel related constants */
#define FREQ_BAND_2DOT4GHZ                      0x1
#define FREQ_BAND_4DOT9GHZ                      0x2
#define FREQ_BAND_5GHZ                          0x4
#define FREQ_BAND_5DOT2GHZ                      0x8
#define CH_AUTO_WIDTH	                        0
#define CH_10_MHZ_WIDTH                         0x1
#define CH_20_MHZ_WIDTH                         0x2
#define CH_40_MHZ_WIDTH                         0x4
#define CH_80_MHZ_WIDTH                         0x5
#define EXT_CH_ABOVE_CTRL_CH                    0x1
#define EXT_CH_AUTO                             0x2
#define EXT_CH_BELOW_CTRL_CH                    0x3
#define NO_EXT_CHANNEL                          0x0

#define ACT_PRIMARY_CHAN_0                      0
#define ACT_PRIMARY_CHAN_1                      1
#define ACT_PRIMARY_CHAN_2                      2
#define ACT_PRIMARY_CHAN_3                      3

/* Define rate related constants */
#define HOSTCMD_ACT_NOT_USE_FIXED_RATE          0x0002

/* Define station related constants */
#define HOSTCMD_ACT_STA_ACTION_ADD              0
#define HOSTCMD_ACT_STA_ACTION_REMOVE           2

/* Define key related constants */
#define MAX_ENCR_KEY_LENGTH                     16
#define MIC_KEY_LENGTH                          8

#define KEY_TYPE_ID_WEP                         0x00
#define KEY_TYPE_ID_TKIP                        0x01
#define KEY_TYPE_ID_AES	                        0x02

#define ENCR_KEY_FLAG_TXGROUPKEY                0x00000004
#define ENCR_KEY_FLAG_PAIRWISE                  0x00000008
#define ENCR_KEY_FLAG_TSC_VALID                 0x00000040
#define ENCR_KEY_FLAG_WEP_TXKEY                 0x01000000
#define ENCR_KEY_FLAG_MICKEY_VALID              0x02000000

/* Define block ack related constants */
#define BASTREAM_FLAG_IMMEDIATE_TYPE            1
#define BASTREAM_FLAG_DIRECTION_UPSTREAM        0

/* Define general purpose action */
#define HOSTCMD_ACT_GEN_SET                     0x0001
#define HOSTCMD_ACT_GEN_SET_LIST                0x0002
#define HOSTCMD_ACT_GEN_GET_LIST                0x0003

/* Misc */
#define MAX_ENCR_KEY_LENGTH                     16
#define MIC_KEY_LENGTH                          8

enum {
	WL_DISABLE = 0,
	WL_ENABLE = 1,
	WL_DISABLE_VMAC = 0x80,
};

enum {
	WL_GET = 0,
	WL_SET = 1,
	WL_RESET = 2,
};

enum {
	WL_LONG_PREAMBLE = 1,
	WL_SHORT_PREAMBLE = 3,
	WL_AUTO_PREAMBLE = 5,
};

enum encr_action_type {
	/* request to enable/disable HW encryption */
	ENCR_ACTION_ENABLE_HW_ENCR,
	/* request to set encryption key */
	ENCR_ACTION_TYPE_SET_KEY,
	/* request to remove one or more keys */
	ENCR_ACTION_TYPE_REMOVE_KEY,
	ENCR_ACTION_TYPE_SET_GROUP_KEY,
};

enum ba_action_type {
	BA_CREATE_STREAM,
	BA_UPDATE_STREAM,
	BA_DESTROY_STREAM,
	BA_FLUSH_STREAM,
	BA_CHECK_STREAM,
};

enum mac_type {
	WL_MAC_TYPE_PRIMARY_CLIENT,
	WL_MAC_TYPE_SECONDARY_CLIENT,
	WL_MAC_TYPE_PRIMARY_AP,
	WL_MAC_TYPE_SECONDARY_AP,
};

/* General host command header */
struct hostcmd_header {
	__le16 cmd;
	__le16 len;
	u8 seq_num;
	u8 macid;
	__le16 result;
} __packed;

/* HOSTCMD_CMD_GET_HW_SPEC */
struct hostcmd_cmd_get_hw_spec {
	struct hostcmd_header cmd_hdr;
	u8 version;                  /* version of the HW                    */
	u8 host_if;                  /* host interface                       */
	__le16 num_wcb;              /* Max. number of WCB FW can handle     */
	__le16 num_mcast_addr;       /* MaxNbr of MC addresses FW can handle */
	u8 permanent_addr[ETH_ALEN]; /* MAC address programmed in HW         */
	__le16 region_code;
	__le16 num_antenna;          /* Number of antenna used      */
	__le32 fw_release_num;       /* 4 byte of FW release number */
	__le32 wcb_base0;
	__le32 rxpd_wr_ptr;
	__le32 rxpd_rd_ptr;
	__le32 fw_awake_cookie;
	__le32 wcb_base[SYSADPT_TOTAL_TX_QUEUES - 1];
} __packed;

/* HOSTCMD_CMD_SET_HW_SPEC */
struct hostcmd_cmd_set_hw_spec {
	struct hostcmd_header cmd_hdr;
	/* HW revision */
	u8 version;
	/* Host interface */
	u8 host_if;
	/* Max. number of Multicast address FW can handle */
	__le16 num_mcast_addr;
	/* MAC address */
	u8 permanent_addr[ETH_ALEN];
	/* Region Code */
	__le16 region_code;
	/* 4 byte of FW release number, example 0x1234=1.2.3.4 */
	__le32 fw_release_num;
	/* Firmware awake cookie - used to ensure that the device
	 * is not in sleep mode
	 */
	__le32 fw_awake_cookie;
	/* Device capabilities (see above) */
	__le32 device_caps;
	/* Rx shared memory queue */
	__le32 rxpd_wr_ptr;
	/* Actual number of TX queues in WcbBase array */
	__le32 num_tx_queues;
	/* TX WCB Rings */
	__le32 wcb_base[SYSADPT_NUM_OF_DESC_DATA];
	/* Max AMSDU size (00 - AMSDU Disabled,
	 * 01 - 4K, 10 - 8K, 11 - not defined)
	 */
	__le32 features;
	__le32 tx_wcb_num_per_queue;
	__le32 total_rx_wcb;
} __packed;

/* HOSTCMD_CMD_802_11_GET_STAT */
struct hostcmd_cmd_802_11_get_stat {
	struct hostcmd_header cmd_hdr;
	__le32 tx_retry_successes;
	__le32 tx_multiple_retry_successes;
	__le32 tx_failures;
	__le32 rts_successes;
	__le32 rts_failures;
	__le32 ack_failures;
	__le32 rx_duplicate_frames;
	__le32 rx_fcs_errors;
	__le32 tx_watchdog_timeouts;
	__le32 rx_overflows;
	__le32 rx_frag_errors;
	__le32 rx_mem_errors;
	__le32 pointer_errors;
	__le32 tx_underflows;
	__le32 tx_done;
	__le32 tx_done_buf_try_put;
	__le32 tx_done_buf_put;
	/* Put size of requested buffer in here */
	__le32 wait_for_tx_buf;
	__le32 tx_attempts;
	__le32 tx_successes;
	__le32 tx_fragments;
	__le32 tx_multicasts;
	__le32 rx_non_ctl_pkts;
	__le32 rx_multicasts;
	__le32 rx_undecryptable_frames;
	__le32 rx_icv_errors;
	__le32 rx_excluded_frames;
	__le32 rx_weak_iv_count;
	__le32 rx_unicasts;
	__le32 rx_bytes;
	__le32 rx_errors;
	__le32 rx_rts_count;
	__le32 tx_cts_count;
} __packed;

/* HOSTCMD_CMD_802_11_RADIO_CONTROL */
struct hostcmd_cmd_802_11_radio_control {
	struct hostcmd_header cmd_hdr;
	__le16 action;
	/* @bit0: 1/0,on/off, @bit1: 1/0, long/short @bit2: 1/0,auto/fix */
	__le16 control;
	__le16 radio_on;
} __packed;

/* HOSTCMD_CMD_802_11_TX_POWER */
struct hostcmd_cmd_802_11_tx_power {
	struct hostcmd_header cmd_hdr;
	__le16 action;
	__le16 band;
	__le16 ch;
	__le16 bw;
	__le16 sub_ch;
	__le16 power_level_list[SYSADPT_TX_POWER_LEVEL_TOTAL];
} __packed;

/* HOSTCMD_CMD_802_11_RF_ANTENNA */
struct hostcmd_cmd_802_11_rf_antenna {
	struct hostcmd_header cmd_hdr;
	__le16 action;
	__le16 antenna_mode;     /* Number of antennas or 0xffff(diversity) */
} __packed;

/* HOSTCMD_CMD_BROADCAST_SSID_ENABLE */
struct hostcmd_cmd_broadcast_ssid_enable {
	struct hostcmd_header cmd_hdr;
	__le32 enable;
} __packed;

/* HOSTCMD_CMD_SET_RF_CHANNEL */
#define FREQ_BAND_MASK     0x0000003f
#define CHNL_WIDTH_MASK    0x000007c0
#define CHNL_WIDTH_SHIFT   6
#define ACT_PRIMARY_MASK   0x00003800
#define ACT_PRIMARY_SHIFT  11

struct hostcmd_cmd_set_rf_channel {
	struct hostcmd_header cmd_hdr;
	__le16 action;
	u8 curr_chnl;
	__le32 chnl_flags;
} __packed;

/* HOSTCMD_CMD_SET_AID */
struct hostcmd_cmd_set_aid {
	struct hostcmd_header cmd_hdr;
	__le16 aid;
	u8 mac_addr[ETH_ALEN];       /* AP's Mac Address(BSSID) */
	__le32 gprotect;
	u8 ap_rates[SYSADPT_MAX_DATA_RATES_G];
} __packed;

/* HOSTCMD_CMD_SET_INFRA_MODE */
struct hostcmd_cmd_set_infra_mode {
	struct hostcmd_header cmd_hdr;
} __packed;

/* HOSTCMD_CMD_802_11_RTS_THSD */
struct hostcmd_cmd_802_11_rts_thsd {
	struct hostcmd_header cmd_hdr;
	__le16 action;
	__le16	threshold;
} __packed;

/* HOSTCMD_CMD_SET_EDCA_PARAMS */
struct hostcmd_cmd_set_edca_params {
	struct hostcmd_header cmd_hdr;
	/* 0 = get all, 0x1 =set CWMin/Max,  0x2 = set TXOP , 0x4 =set AIFSN */
	__le16 action;
	__le16 txop;                 /* in unit of 32 us */
	__le32 cw_max;               /* 0~15 */
	__le32 cw_min;               /* 0~15 */
	u8 aifsn;
	u8 txq_num;                  /* Tx Queue number. */
} __packed;

/* HOSTCMD_CMD_SET_WMM_MODE */
struct hostcmd_cmd_set_wmm_mode {
	struct hostcmd_header cmd_hdr;
	__le16 action;               /* 0->unset, 1->set */
} __packed;

/* HOSTCMD_CMD_SET_FIXED_RATE */
struct fix_rate_flag {           /* lower rate after the retry count */
	/* 0: legacy, 1: HT */
	__le32 fix_rate_type;
	/* 0: retry count is not valid, 1: use retry count specified */
	__le32 retry_count_valid;
} __packed;

struct fix_rate_entry {
	struct fix_rate_flag fix_rate_type_flags;
	/* depending on the flags above, this can be either a legacy
	 * rate(not index) or an MCS code.
	 */
	__le32 fixed_rate;
	__le32 retry_count;
} __packed;

struct hostcmd_cmd_set_fixed_rate {
	struct hostcmd_header cmd_hdr;
	/* HOSTCMD_ACT_NOT_USE_FIXED_RATE 0x0002 */
	__le32 action;
	/* use fixed rate specified but firmware can drop to */
	__le32 allow_rate_drop;
	__le32 entry_count;
	struct fix_rate_entry fixed_rate_table[4];
	u8 multicast_rate;
	u8 multi_rate_tx_type;
	u8 management_rate;
} __packed;

/* HOSTCMD_CMD_SET_IES */
struct hostcmd_cmd_set_ies {
	struct hostcmd_header cmd_hdr;
	__le16 action;               /* 0->unset, 1->set */
	__le16 ie_list_len_ht;
	__le16 ie_list_len_vht;
	__le16 ie_list_len_proprietary;
	/*Buffer size same as Generic_Beacon*/
	u8 ie_list_ht[148];
	u8 ie_list_vht[24];
	u8 ie_list_proprietary[112];
} __packed;

/* HOSTCMD_CMD_SET_RATE_ADAPT_MODE */
struct hostcmd_cmd_set_rate_adapt_mode {
	struct hostcmd_header cmd_hdr;
	__le16 action;
	__le16 rate_adapt_mode;      /* 0:Indoor, 1:Outdoor */
} __packed;

/* HOSTCMD_CMD_SET_MAC_ADDR, HOSTCMD_CMD_DEL_MAC_ADDR */
struct hostcmd_cmd_set_mac_addr {
	struct hostcmd_header cmd_hdr;
	__le16 mac_type;
	u8 mac_addr[ETH_ALEN];
} __packed;

/* HOSTCMD_CMD_GET_WATCHDOG_BITMAP */
struct hostcmd_cmd_get_watchdog_bitmap {
	struct hostcmd_header cmd_hdr;
	u8 watchdog_bitmap;          /* for SW/BA */
} __packed;

/* HOSTCMD_CMD_BSS_START */
struct hostcmd_cmd_bss_start {
	struct hostcmd_header cmd_hdr;
	__le32 enable;                  /* FALSE: Disable or TRUE: Enable */
} __packed;

/* HOSTCMD_CMD_AP_BEACON */
struct cf_params {
	u8 elem_id;
	u8 len;
	u8 cfp_cnt;
	u8 cfp_period;
	__le16 cfp_max_duration;
	__le16 cfp_duration_remaining;
} __packed;

struct ibss_params {
	u8 elem_id;
	u8 len;
	__le16	atim_window;
} __packed;

union ss_params {
	struct cf_params cf_param_set;
	struct ibss_params ibss_param_set;
} __packed;

struct fh_params {
	u8 elem_id;
	u8 len;
	__le16 dwell_time;
	u8 hop_set;
	u8 hop_pattern;
	u8 hop_index;
} __packed;

struct ds_params {
	u8 elem_id;
	u8 len;
	u8 current_chnl;
} __packed;

union phy_params {
	struct fh_params fh_param_set;
	struct ds_params ds_param_set;
} __packed;

struct rsn_ie {
	u8 elem_id;
	u8 len;
	u8 oui_type[4];              /* 00:50:f2:01 */
	u8 ver[2];
	u8 grp_key_cipher[4];
	u8 pws_key_cnt[2];
	u8 pws_key_cipher_list[4];
	u8 auth_key_cnt[2];
	u8 auth_key_list[4];
} __packed;

struct rsn48_ie {
	u8 elem_id;
	u8 len;
	u8 ver[2];
	u8 grp_key_cipher[4];
	u8 pws_key_cnt[2];
	u8 pws_key_cipher_list[4];
	u8 auth_key_cnt[2];
	u8 auth_key_list[4];
	u8 rsn_cap[2];
	u8 pmk_id_cnt[2];
	u8 pmk_id_list[16];          /* Should modify to 16 * S */
	u8 reserved[8];
} __packed;

struct ac_param_rcd {
	u8 aci_aifsn;
	u8 ecw_min_max;
	__le16 txop_lim;
} __packed;

struct wmm_param_elem {
	u8 elem_id;
	u8 len;
	u8 oui[3];
	u8 type;
	u8 sub_type;
	u8 version;
	u8 rsvd;
	struct ac_param_rcd ac_be;
	struct ac_param_rcd ac_bk;
	struct ac_param_rcd ac_vi;
	struct ac_param_rcd ac_vo;
} __packed;

struct channel_info {
	u8 first_channel_num;
	u8 num_channels;
	u8 max_tx_pwr_level;
} __packed;

struct country {
	u8 elem_id;
	u8 len;
	u8 country_str[3];
	struct channel_info channel_info[40];
} __packed;

struct start_cmd {
	u8 sta_mac_addr[ETH_ALEN];
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 bss_type;
	__le16 bcn_period;
	u8 dtim_period;
	union ss_params ss_param_set;
	union phy_params phy_param_set;
	__le16 probe_delay;
	__le16 cap_info;
	u8 b_rate_set[SYSADPT_MAX_DATA_RATES_G];
	u8 op_rate_set[SYSADPT_MAX_DATA_RATES_G];
	struct rsn_ie rsn_ie;
	struct rsn48_ie rsn48_ie;
	struct wmm_param_elem wmm_param;
	struct country country;
	__le32 ap_rf_type;           /* 0->B, 1->G, 2->Mixed, 3->A, 4->11J */
} __packed;

struct hostcmd_cmd_ap_beacon {
	struct hostcmd_header cmd_hdr;
	struct start_cmd start_cmd;
} __packed;

/* HOSTCMD_CMD_SET_NEW_STN */
struct add_ht_info {
	u8 control_chnl;
	u8 add_chnl;
	__le16 op_mode;
	__le16 stbc;
} __packed;

struct peer_info {
	__le32 legacy_rate_bitmap;
	u8 ht_rates[4];
	__le16 cap_info;
	__le16 ht_cap_info;
	u8 mac_ht_param_info;
	u8 mrvl_sta;
	struct add_ht_info add_ht_info;
	__le32 tx_bf_capabilities;   /* EXBF_SUPPORT */
	__le32 vht_max_rx_mcs;
	__le32 vht_cap;
	/* 0:20Mhz, 1:40Mhz, 2:80Mhz, 3:160 or 80+80Mhz */
	u8 vht_rx_channel_width;
} __packed;

struct hostcmd_cmd_set_new_stn {
	struct hostcmd_header cmd_hdr;
	__le16 aid;
	u8 mac_addr[ETH_ALEN];
	__le16 stn_id;
	__le16 action;
	__le16 reserved;
	struct peer_info peer_info;
	/* UAPSD_SUPPORT */
	u8 qos_info;
	u8 is_qos_sta;
	__le32 fw_sta_ptr;
} __packed;

/* HOSTCMD_CMD_SET_APMODE */
struct hostcmd_cmd_set_apmode {
	struct hostcmd_header cmd_hdr;
	u8 apmode;
} __packed;

/* HOSTCMD_CMD_UPDATE_ENCRYPTION */
struct hostcmd_cmd_update_encryption {
	struct hostcmd_header cmd_hdr;
	/* Action type - see encr_action_type */
	__le32 action_type;          /* encr_action_type */
	/* size of the data buffer attached. */
	__le32 data_length;
	u8 mac_addr[ETH_ALEN];
	u8 action_data[1];
} __packed;

struct wep_type_key {
	/* WEP key material (max 128bit) */
	u8 key_material[MAX_ENCR_KEY_LENGTH];
} __packed;

struct encr_tkip_seqcnt {
	__le16 low;
	__le32 high;
} __packed;

struct tkip_type_key {
	/* TKIP Key material. Key type (group or pairwise key) is
	 * determined by flags
	 */
	/* in KEY_PARAM_SET structure. */
	u8 key_material[MAX_ENCR_KEY_LENGTH];
	/* MIC keys */
	u8 tkip_tx_mic_key[MIC_KEY_LENGTH];
	u8 tkip_rx_mic_key[MIC_KEY_LENGTH];
	struct encr_tkip_seqcnt	tkip_rsc;
	struct encr_tkip_seqcnt	tkip_tsc;
} __packed;

struct aes_type_key {
	/* AES Key material */
	u8 key_material[MAX_ENCR_KEY_LENGTH];
} __packed;

union mwl_key_type {
	struct wep_type_key  wep_key;
	struct tkip_type_key tkip_key;
	struct aes_type_key  aes_key;
} __packed;

struct key_param_set {
	/* Total length of this structure (Key is variable size array) */
	__le16 length;
	/* Key type - WEP, TKIP or AES-CCMP. */
	/* See definitions above */
	__le16 key_type_id;
	/* key flags (ENCR_KEY_FLAG_XXX_ */
	__le32 key_info;
	/* For WEP only - actual key index */
	__le32 key_index;
	/* Size of the key */
	__le16 key_len;
	/* Key material (variable size array) */
	union mwl_key_type key;
	u8 mac_addr[ETH_ALEN];
} __packed;

struct hostcmd_cmd_set_key {
	struct hostcmd_header cmd_hdr;
	/* Action type - see encr_action_type */
	__le32 action_type;          /* encr_action_type */
	/* size of the data buffer attached. */
	__le32 data_length;
	/* data buffer - maps to one KEY_PARAM_SET structure */
	struct key_param_set key_param;
} __packed;

/* HOSTCMD_CMD_BASTREAM */
#define BA_TYPE_MASK       0x00000001
#define BA_DIRECTION_MASK  0x00000006
#define BA_DIRECTION_SHIFT 1

struct ba_context {
	__le32 context;
} __packed;

/* parameters for block ack creation */
struct create_ba_params {
	/* BA Creation flags - see above */
	__le32 flags;
	/* idle threshold */
	__le32 idle_thrs;
	/* block ack transmit threshold (after how many pkts should we
	 * send BAR?)
	 */
	__le32 bar_thrs;
	/* receiver window size */
	__le32 window_size;
	/* MAC Address of the BA partner */
	u8 peer_mac_addr[ETH_ALEN];
	/* Dialog Token */
	u8 dialog_token;
	/* TID for the traffic stream in this BA */
	u8 tid;
	/* shared memory queue ID (not sure if this is required) */
	u8 queue_id;
	u8 param_info;
	/* returned by firmware - firmware context pointer. */
	/* this context pointer will be passed to firmware for all
	 * future commands.
	 */
	struct ba_context fw_ba_context;
	u8 reset_seq_no;             /** 0 or 1**/
	__le16 current_seq;
	/* This is for virtual station in Sta proxy mode for V6FW */
	u8 sta_src_mac_addr[ETH_ALEN];
} __packed;

/* new transmit sequence number information */
struct ba_update_seq_num {
	/* BA flags - see above */
	__le32 flags;
	/* returned by firmware in the create ba stream response */
	struct ba_context fw_ba_context;
	/* new sequence number for this block ack stream */
	__le16 ba_seq_num;
} __packed;

struct ba_stream_context {
	/* BA Stream flags */
	__le32 flags;
	/* returned by firmware in the create ba stream response */
	struct ba_context fw_ba_context;
} __packed;

union ba_info {
	/* information required to create BA Stream... */
	struct create_ba_params	create_params;
	/* update starting/new sequence number etc. */
	struct ba_update_seq_num updt_seq_num;
	/* destroy an existing stream... */
	struct ba_stream_context destroy_params;
	/* destroy an existing stream... */
	struct ba_stream_context flush_params;
} __packed;

struct hostcmd_cmd_bastream {
	struct hostcmd_header cmd_hdr;
	__le32 action_type;
	union ba_info ba_info;
} __packed;

/* HOSTCMD_CMD_DWDS_ENABLE */
struct hostcmd_cmd_dwds_enable {
	struct hostcmd_header cmd_hdr;
	__le32 enable;               /* 0 -- Disable. or 1 -- Enable. */
} __packed;

/* HOSTCMD_CMD_FW_FLUSH_TIMER */
struct hostcmd_cmd_fw_flush_timer {
	struct hostcmd_header cmd_hdr;
	/* 0 -- Disable. > 0 -- holds time value in usecs. */
	__le32 value;
} __packed;

/* HOSTCMD_CMD_SET_CDD */
struct hostcmd_cmd_set_cdd {
	struct hostcmd_header cmd_hdr;
	__le32 enable;
} __packed;

static bool mwl_fwcmd_chk_adapter(struct mwl_priv *priv)
{
	u32 regval;

	regval = readl(priv->iobase1 + MACREG_REG_INT_CODE);

	if (regval == 0xffffffff) {
		wiphy_err(priv->hw->wiphy, "adapter is not existed");
		return false;
	}

	return true;
}

static void mwl_fwcmd_send_cmd(struct mwl_priv *priv)
{
	writel(priv->pphys_cmd_buf, priv->iobase1 + MACREG_REG_GEN_PTR);
	writel(MACREG_H2ARIC_BIT_DOOR_BELL,
	       priv->iobase1 + MACREG_REG_H2A_INTERRUPT_EVENTS);
}

static char *mwl_fwcmd_get_cmd_string(unsigned short cmd)
{
	int max_entries = 0;
	int curr_cmd = 0;

	static const struct {
		u16 cmd;
		char *cmd_string;
	} cmds[] = {
		{ HOSTCMD_CMD_GET_HW_SPEC, "GetHwSpecifications" },
		{ HOSTCMD_CMD_SET_HW_SPEC, "SetHwSepcifications" },
		{ HOSTCMD_CMD_802_11_GET_STAT, "80211GetStat" },
		{ HOSTCMD_CMD_802_11_RADIO_CONTROL, "80211RadioControl" },
		{ HOSTCMD_CMD_802_11_TX_POWER, "80211TxPower" },
		{ HOSTCMD_CMD_802_11_RF_ANTENNA, "80211RfAntenna" },
		{ HOSTCMD_CMD_BROADCAST_SSID_ENABLE, "broadcast_ssid_enable" },
		{ HOSTCMD_CMD_SET_RF_CHANNEL, "SetRfChannel" },
		{ HOSTCMD_CMD_SET_AID, "SetAid" },
		{ HOSTCMD_CMD_SET_INFRA_MODE, "SetInfraMode" },
		{ HOSTCMD_CMD_802_11_RTS_THSD, "80211RtsThreshold" },
		{ HOSTCMD_CMD_SET_EDCA_PARAMS, "SetEDCAParams" },
		{ HOSTCMD_CMD_SET_WMM_MODE, "SetWMMMode" },
		{ HOSTCMD_CMD_SET_FIXED_RATE, "SetFixedRate" },
		{ HOSTCMD_CMD_SET_IES, "SetInformationElements" },
		{ HOSTCMD_CMD_SET_RATE_ADAPT_MODE, "SetRateAdaptationMode" },
		{ HOSTCMD_CMD_SET_MAC_ADDR, "SetMacAddr" },
		{ HOSTCMD_CMD_GET_WATCHDOG_BITMAP, "GetWatchdogBitMap" },
		{ HOSTCMD_CMD_DEL_MAC_ADDR, "DelMacAddr" },
		{ HOSTCMD_CMD_BSS_START, "BssStart" },
		{ HOSTCMD_CMD_AP_BEACON, "SetApBeacon" },
		{ HOSTCMD_CMD_SET_NEW_STN, "SetNewStation" },
		{ HOSTCMD_CMD_SET_APMODE, "SetApMode" },
		{ HOSTCMD_CMD_UPDATE_ENCRYPTION, "UpdateEncryption" },
		{ HOSTCMD_CMD_BASTREAM, "BAStream" },
		{ HOSTCMD_CMD_DWDS_ENABLE, "DwdsEnable" },
		{ HOSTCMD_CMD_FW_FLUSH_TIMER, "FwFlushTimer" },
		{ HOSTCMD_CMD_SET_CDD, "SetCDD" },
	};

	max_entries = sizeof(cmds) / sizeof(cmds[0]);

	for (curr_cmd = 0; curr_cmd < max_entries; curr_cmd++)
		if ((cmd & 0x7fff) == cmds[curr_cmd].cmd)
			return cmds[curr_cmd].cmd_string;

	return "unknown";
}

static int mwl_fwcmd_wait_complete(struct mwl_priv *priv, unsigned short cmd)
{
	unsigned int curr_iteration = MAX_WAIT_FW_COMPLETE_ITERATIONS;

	unsigned short int_code = 0;

	do {
		int_code = le16_to_cpu(*((__le16 *)&priv->pcmd_buf[0]));
		mdelay(1);
	} while ((int_code != cmd) && (--curr_iteration));

	if (curr_iteration == 0) {
		wiphy_err(priv->hw->wiphy, "cmd 0x%04x=%s timed out",
			  cmd, mwl_fwcmd_get_cmd_string(cmd));
		return -EIO;
	}

	mdelay(3);

	return 0;
}

static int mwl_fwcmd_exec_cmd(struct mwl_priv *priv, unsigned short cmd)
{
	bool busy = false;

	if (!mwl_fwcmd_chk_adapter(priv)) {
		wiphy_err(priv->hw->wiphy, "no adapter existed");
		priv->in_send_cmd = false;
		return -EIO;
	}

	if (!priv->in_send_cmd) {
		priv->in_send_cmd = true;
		mwl_fwcmd_send_cmd(priv);
		if (mwl_fwcmd_wait_complete(priv, 0x8000 | cmd)) {
			wiphy_err(priv->hw->wiphy, "timeout");
			priv->in_send_cmd = false;
			return -EIO;
		}
	} else {
		wiphy_warn(priv->hw->wiphy,
			   "previous command is still running");
		busy = true;
	}

	if (!busy)
		priv->in_send_cmd = false;

	return 0;
}

static int mwl_fwcmd_802_11_radio_control(struct mwl_priv *priv,
					  bool enable, bool force)
{
	struct hostcmd_cmd_802_11_radio_control *pcmd;
	unsigned long flags;

	if (enable == priv->radio_on && !force)
		return 0;

	pcmd = (struct hostcmd_cmd_802_11_radio_control *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_802_11_RADIO_CONTROL);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le16(WL_SET);
	pcmd->control = cpu_to_le16(priv->radio_short_preamble ?
		WL_AUTO_PREAMBLE : WL_LONG_PREAMBLE);
	pcmd->radio_on = cpu_to_le16(enable ? WL_ENABLE : WL_DISABLE);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_RADIO_CONTROL)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(priv->hw->wiphy, "failed execution");
		return -EIO;
	}

	priv->radio_on = enable;

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

static int mwl_fwcmd_get_tx_powers(struct mwl_priv *priv, u16 *powlist, u16 ch,
				   u16 band, u16 width, u16 sub_ch)
{
	struct hostcmd_cmd_802_11_tx_power *pcmd;
	unsigned long flags;
	int i;

	pcmd = (struct hostcmd_cmd_802_11_tx_power *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_802_11_TX_POWER);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le16(HOSTCMD_ACT_GEN_GET_LIST);
	pcmd->ch = cpu_to_le16(ch);
	pcmd->bw = cpu_to_le16(width);
	pcmd->band = cpu_to_le16(band);
	pcmd->sub_ch = cpu_to_le16(sub_ch);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_TX_POWER)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(priv->hw->wiphy, "failed execution");
		return -EIO;
	}

	for (i = 0; i < SYSADPT_TX_POWER_LEVEL_TOTAL; i++)
		powlist[i] = le16_to_cpu(pcmd->power_level_list[i]);

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

static int mwl_fwcmd_set_tx_powers(struct mwl_priv *priv, u16 txpow[],
				   u8 action, u16 ch, u16 band,
				   u16 width, u16 sub_ch)
{
	struct hostcmd_cmd_802_11_tx_power *pcmd;
	unsigned long flags;
	int i;

	pcmd = (struct hostcmd_cmd_802_11_tx_power *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_802_11_TX_POWER);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le16(action);
	pcmd->ch = cpu_to_le16(ch);
	pcmd->bw = cpu_to_le16(width);
	pcmd->band = cpu_to_le16(band);
	pcmd->sub_ch = cpu_to_le16(sub_ch);

	for (i = 0; i < SYSADPT_TX_POWER_LEVEL_TOTAL; i++)
		pcmd->power_level_list[i] = cpu_to_le16(txpow[i]);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_TX_POWER)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(priv->hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

static u8 mwl_fwcmd_get_80m_pri_chnl_offset(u8 channel)
{
	u8 act_primary = ACT_PRIMARY_CHAN_0;

	switch (channel) {
	case 36:
		act_primary = ACT_PRIMARY_CHAN_0;
		break;
	case 40:
		act_primary = ACT_PRIMARY_CHAN_1;
		break;
	case 44:
		act_primary = ACT_PRIMARY_CHAN_2;
		break;
	case 48:
		act_primary = ACT_PRIMARY_CHAN_3;
		break;
	case 52:
		act_primary = ACT_PRIMARY_CHAN_0;
		break;
	case 56:
		act_primary = ACT_PRIMARY_CHAN_1;
		break;
	case 60:
		act_primary = ACT_PRIMARY_CHAN_2;
		break;
	case 64:
		act_primary = ACT_PRIMARY_CHAN_3;
		break;
	case 100:
		act_primary = ACT_PRIMARY_CHAN_0;
		break;
	case 104:
		act_primary = ACT_PRIMARY_CHAN_1;
		break;
	case 108:
		act_primary = ACT_PRIMARY_CHAN_2;
		break;
	case 112:
		act_primary = ACT_PRIMARY_CHAN_3;
		break;
	case 116:
		act_primary = ACT_PRIMARY_CHAN_0;
		break;
	case 120:
		act_primary = ACT_PRIMARY_CHAN_1;
		break;
	case 124:
		act_primary = ACT_PRIMARY_CHAN_2;
		break;
	case 128:
		act_primary = ACT_PRIMARY_CHAN_3;
		break;
	case 132:
		act_primary = ACT_PRIMARY_CHAN_0;
		break;
	case 136:
		act_primary = ACT_PRIMARY_CHAN_1;
		break;
	case 140:
		act_primary = ACT_PRIMARY_CHAN_2;
		break;
	case 144:
		act_primary = ACT_PRIMARY_CHAN_3;
		break;
	case 149:
		act_primary = ACT_PRIMARY_CHAN_0;
		break;
	case 153:
		act_primary = ACT_PRIMARY_CHAN_1;
		break;
	case 157:
		act_primary = ACT_PRIMARY_CHAN_2;
		break;
	case 161:
		act_primary = ACT_PRIMARY_CHAN_3;
		break;
	}

	return act_primary;
}

static void mwl_fwcmd_parse_beacon(struct mwl_priv *priv,
				   struct mwl_vif *vif, u8 *beacon, int len)
{
	struct ieee80211_mgmt *mgmt;
	struct beacon_info *beacon_info;
	int baselen;
	u8 *pos;
	size_t left;
	bool elem_parse_failed;

	mgmt = (struct ieee80211_mgmt *)beacon;

	baselen = (u8 *)mgmt->u.beacon.variable - (u8 *)mgmt;
	if (baselen > len)
		return;

	beacon_info = &vif->beacon_info;
	memset(beacon_info, 0, sizeof(struct beacon_info));
	beacon_info->valid = false;
	beacon_info->ie_ht_ptr = &beacon_info->ie_list_ht[0];
	beacon_info->ie_vht_ptr = &beacon_info->ie_list_vht[0];

	beacon_info->cap_info = le16_to_cpu(mgmt->u.beacon.capab_info);

	pos = (u8 *)mgmt->u.beacon.variable;
	left = len - baselen;

	elem_parse_failed = false;

	while (left >= 2) {
		u8 id, elen;

		id = *pos++;
		elen = *pos++;
		left -= 2;

		if (elen > left) {
			elem_parse_failed = true;
			break;
		}

		switch (id) {
		case WLAN_EID_SUPP_RATES:
		case WLAN_EID_EXT_SUPP_RATES:
			{
			int idx, bi, oi;
			u8 rate;

			for (bi = 0; bi < SYSADPT_MAX_DATA_RATES_G;
			     bi++) {
				if (beacon_info->b_rate_set[bi] == 0)
					break;
			}

			for (oi = 0; oi < SYSADPT_MAX_DATA_RATES_G;
			     oi++) {
				if (beacon_info->op_rate_set[oi] == 0)
					break;
			}

			for (idx = 0; idx < elen; idx++) {
				rate = pos[idx];
				if ((rate & 0x80) != 0) {
					if (bi < SYSADPT_MAX_DATA_RATES_G)
						beacon_info->b_rate_set[bi++]
							= rate & 0x7f;
					else {
						elem_parse_failed = true;
						break;
					}
				}
				if (oi < SYSADPT_MAX_DATA_RATES_G)
					beacon_info->op_rate_set[oi++] =
						rate & 0x7f;
				else {
					elem_parse_failed = true;
					break;
				}
			}
			}
			break;
		case WLAN_EID_RSN:
			beacon_info->ie_rsn48_len = (elen + 2);
			beacon_info->ie_rsn48_ptr = (pos - 2);
			break;
		case WLAN_EID_HT_CAPABILITY:
		case WLAN_EID_HT_OPERATION:
		case WLAN_EID_OVERLAP_BSS_SCAN_PARAM:
		case WLAN_EID_EXT_CAPABILITY:
			beacon_info->ie_ht_len += (elen + 2);
			if (beacon_info->ie_ht_len >
			    sizeof(beacon_info->ie_list_ht)) {
				elem_parse_failed = true;
			} else {
				*beacon_info->ie_ht_ptr++ = id;
				*beacon_info->ie_ht_ptr++ = elen;
				memcpy(beacon_info->ie_ht_ptr, pos, elen);
				beacon_info->ie_ht_ptr += elen;
			}
			break;
		case WLAN_EID_VHT_CAPABILITY:
		case WLAN_EID_VHT_OPERATION:
		case WLAN_EID_OPMODE_NOTIF:
			beacon_info->ie_vht_len += (elen + 2);
			if (beacon_info->ie_vht_len >
			    sizeof(beacon_info->ie_list_vht)) {
				elem_parse_failed = true;
			} else {
				*beacon_info->ie_vht_ptr++ = id;
				*beacon_info->ie_vht_ptr++ = elen;
				memcpy(beacon_info->ie_vht_ptr, pos, elen);
				beacon_info->ie_vht_ptr += elen;
			}
			break;
		case WLAN_EID_VENDOR_SPECIFIC:
			if ((pos[0] == 0x00) && (pos[1] == 0x50) &&
			    (pos[2] == 0xf2)) {
				if (pos[3] == 0x01) {
					beacon_info->ie_rsn_len = (elen + 2);
					beacon_info->ie_rsn_ptr = (pos - 2);
				}

				if (pos[3] == 0x02) {
					beacon_info->ie_wmm_len = (elen + 2);
					beacon_info->ie_wmm_ptr = (pos - 2);
				}
			}
			break;
		default:
			break;
		}

		left -= elen;
		pos += elen;
	}

	if (!elem_parse_failed) {
		beacon_info->ie_ht_ptr = &beacon_info->ie_list_ht[0];
		beacon_info->ie_vht_ptr = &beacon_info->ie_list_vht[0];
		beacon_info->valid = true;

		wiphy_info(priv->hw->wiphy,
			   "wmm:%d, rsn:%d, rsn48:%d, ht:%d, vht:%d",
			   beacon_info->ie_wmm_len,
			   beacon_info->ie_rsn_len,
			   beacon_info->ie_rsn48_len,
			   beacon_info->ie_ht_len,
			   beacon_info->ie_vht_len);
	}
}

static int mwl_fwcmd_set_ies(struct mwl_priv *priv, struct mwl_vif *mwl_vif)
{
	struct hostcmd_cmd_set_ies *pcmd;
	unsigned long flags;

	if (!mwl_vif->beacon_info.valid)
		return -EINVAL;

	if (mwl_vif->beacon_info.ie_ht_len > sizeof(pcmd->ie_list_ht))
		goto einval;

	if (mwl_vif->beacon_info.ie_vht_len > sizeof(pcmd->ie_list_vht))
		goto einval;

	pcmd = (struct hostcmd_cmd_set_ies *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_IES);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action = cpu_to_le16(HOSTCMD_ACT_GEN_SET);

	pcmd->ie_list_len_ht = cpu_to_le16(mwl_vif->beacon_info.ie_ht_len);
	memcpy(pcmd->ie_list_ht, mwl_vif->beacon_info.ie_ht_ptr,
	       mwl_vif->beacon_info.ie_ht_len);

	pcmd->ie_list_len_vht = cpu_to_le16(mwl_vif->beacon_info.ie_vht_len);
	memcpy(pcmd->ie_list_vht, mwl_vif->beacon_info.ie_vht_ptr,
	       mwl_vif->beacon_info.ie_vht_len);

	if (priv->chip_type == MWL8897) {
		pcmd->ie_list_len_proprietary =
			cpu_to_le16(mwl_vif->beacon_info.ie_wmm_len);
		memcpy(pcmd->ie_list_proprietary,
		       mwl_vif->beacon_info.ie_wmm_ptr,
		       mwl_vif->beacon_info.ie_wmm_len);
	}

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_IES)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(priv->hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;

einval:

	wiphy_err(priv->hw->wiphy, "length of IE is too long");

	return -EINVAL;
}

static int mwl_fwcmd_set_ap_beacon(struct mwl_priv *priv,
				   struct mwl_vif *mwl_vif,
				   struct ieee80211_bss_conf *bss_conf)
{
	struct hostcmd_cmd_ap_beacon *pcmd;
	unsigned long flags;
	struct ds_params *phy_ds_param_set;

	if (!mwl_vif->beacon_info.valid)
		return -EINVAL;

	/* wmm structure of start command is defined less one byte,
	 * due to following field country is not used, add byte one
	 * to bypass the check.
	 */
	if (mwl_vif->beacon_info.ie_wmm_len >
	    (sizeof(pcmd->start_cmd.wmm_param) + 1))
		goto ielenerr;

	if (mwl_vif->beacon_info.ie_rsn_len > sizeof(pcmd->start_cmd.rsn_ie))
		goto ielenerr;

	if (mwl_vif->beacon_info.ie_rsn48_len >
	    sizeof(pcmd->start_cmd.rsn48_ie))
		goto ielenerr;

	pcmd = (struct hostcmd_cmd_ap_beacon *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_AP_BEACON);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	ether_addr_copy(pcmd->start_cmd.sta_mac_addr, mwl_vif->bssid);
	memcpy(pcmd->start_cmd.ssid, bss_conf->ssid, bss_conf->ssid_len);
	pcmd->start_cmd.bss_type = 1;
	pcmd->start_cmd.bcn_period  = cpu_to_le16(bss_conf->beacon_int);
	pcmd->start_cmd.dtim_period = bss_conf->dtim_period; /* 8bit */

	phy_ds_param_set = &pcmd->start_cmd.phy_param_set.ds_param_set;
	phy_ds_param_set->elem_id = WLAN_EID_DS_PARAMS;
	phy_ds_param_set->len = sizeof(phy_ds_param_set->current_chnl);
	phy_ds_param_set->current_chnl = bss_conf->chandef.chan->hw_value;

	pcmd->start_cmd.probe_delay = cpu_to_le16(10);
	pcmd->start_cmd.cap_info = cpu_to_le16(mwl_vif->beacon_info.cap_info);

	memcpy(&pcmd->start_cmd.wmm_param, mwl_vif->beacon_info.ie_wmm_ptr,
	       mwl_vif->beacon_info.ie_wmm_len);

	memcpy(&pcmd->start_cmd.rsn_ie, mwl_vif->beacon_info.ie_rsn_ptr,
	       mwl_vif->beacon_info.ie_rsn_len);

	memcpy(&pcmd->start_cmd.rsn48_ie, mwl_vif->beacon_info.ie_rsn48_ptr,
	       mwl_vif->beacon_info.ie_rsn48_len);

	memcpy(pcmd->start_cmd.b_rate_set, mwl_vif->beacon_info.b_rate_set,
	       SYSADPT_MAX_DATA_RATES_G);

	memcpy(pcmd->start_cmd.op_rate_set, mwl_vif->beacon_info.op_rate_set,
	       SYSADPT_MAX_DATA_RATES_G);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_AP_BEACON)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(priv->hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;

ielenerr:

	wiphy_err(priv->hw->wiphy, "length of IE is too long");

	return -EINVAL;
}

static int mwl_fwcmd_encryption_set_cmd_info(struct hostcmd_cmd_set_key *cmd,
					     u8 *addr,
					     struct ieee80211_key_conf *key)
{
	cmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_UPDATE_ENCRYPTION);
	cmd->cmd_hdr.len = cpu_to_le16(sizeof(*cmd));
	cmd->key_param.length = cpu_to_le16(sizeof(*cmd) -
		offsetof(struct hostcmd_cmd_set_key, key_param));
	cmd->key_param.key_index = cpu_to_le32(key->keyidx);
	cmd->key_param.key_len = cpu_to_le16(key->keylen);
	ether_addr_copy(cmd->key_param.mac_addr, addr);

	switch (key->cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		cmd->key_param.key_type_id = cpu_to_le16(KEY_TYPE_ID_WEP);
		if (key->keyidx == 0)
			cmd->key_param.key_info =
				cpu_to_le32(ENCR_KEY_FLAG_WEP_TXKEY);
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		cmd->key_param.key_type_id = cpu_to_le16(KEY_TYPE_ID_TKIP);
		cmd->key_param.key_info =
			(key->flags & IEEE80211_KEY_FLAG_PAIRWISE) ?
			cpu_to_le32(ENCR_KEY_FLAG_PAIRWISE) :
			cpu_to_le32(ENCR_KEY_FLAG_TXGROUPKEY);
		cmd->key_param.key_info |=
			cpu_to_le32(ENCR_KEY_FLAG_MICKEY_VALID |
				      ENCR_KEY_FLAG_TSC_VALID);
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		cmd->key_param.key_type_id = cpu_to_le16(KEY_TYPE_ID_AES);
		cmd->key_param.key_info =
			(key->flags & IEEE80211_KEY_FLAG_PAIRWISE) ?
			cpu_to_le32(ENCR_KEY_FLAG_PAIRWISE) :
			cpu_to_le32(ENCR_KEY_FLAG_TXGROUPKEY);
		break;
	default:
		return -ENOTSUPP;
	}

	return 0;
}

void mwl_fwcmd_reset(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;

	priv = hw->priv;

	if (mwl_fwcmd_chk_adapter(priv))
		writel(ISR_RESET,
		       priv->iobase1 + MACREG_REG_H2A_INTERRUPT_EVENTS);
}

void mwl_fwcmd_int_enable(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;

	priv = hw->priv;

	if (mwl_fwcmd_chk_adapter(priv)) {
		writel(0x00,
		       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_MASK);
		writel((MACREG_A2HRIC_BIT_MASK),
		       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_MASK);
	}
}

void mwl_fwcmd_int_disable(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;

	priv = hw->priv;

	if (mwl_fwcmd_chk_adapter(priv))
		writel(0x00,
		       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_MASK);
}

int mwl_fwcmd_get_hw_specs(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_get_hw_spec *pcmd;
	unsigned long flags;
	int retry;
	int i;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_get_hw_spec *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	wiphy_debug(hw->wiphy, "pcmd = %x", (unsigned int)pcmd);
	memset(pcmd, 0x00, sizeof(*pcmd));
	memset(&pcmd->permanent_addr[0], 0xff, ETH_ALEN);
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_GET_HW_SPEC);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->fw_awake_cookie = cpu_to_le32(priv->pphys_cmd_buf + 2048);

	retry = 0;
	while (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_GET_HW_SPEC)) {
		if (retry++ > MAX_WAIT_GET_HW_SPECS_ITERATONS) {
			wiphy_err(hw->wiphy, "can't get hw specs");
			spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
			return -EIO;
		}

		mdelay(1000);
		wiphy_debug(hw->wiphy,
			    "repeat command = %x", (unsigned int)pcmd);
	}

	ether_addr_copy(&priv->hw_data.mac_addr[0], pcmd->permanent_addr);
	priv->desc_data[0].wcb_base =
		le32_to_cpu(pcmd->wcb_base0) & 0x0000ffff;
#if SYSADPT_NUM_OF_DESC_DATA > 3
	for (i = 1; i < SYSADPT_TOTAL_TX_QUEUES; i++)
		priv->desc_data[i].wcb_base =
			le32_to_cpu(pcmd->wcb_base[i - 1]) & 0x0000ffff;
#endif
	priv->desc_data[0].rx_desc_read =
		le32_to_cpu(pcmd->rxpd_rd_ptr) & 0x0000ffff;
	priv->desc_data[0].rx_desc_write =
		le32_to_cpu(pcmd->rxpd_wr_ptr) & 0x0000ffff;
	priv->hw_data.region_code = le16_to_cpu(pcmd->region_code) & 0x00ff;
	priv->hw_data.fw_release_num = le32_to_cpu(pcmd->fw_release_num);
	priv->hw_data.max_num_tx_desc = le16_to_cpu(pcmd->num_wcb);
	priv->hw_data.max_num_mc_addr = le16_to_cpu(pcmd->num_mcast_addr);
	priv->hw_data.num_antennas = le16_to_cpu(pcmd->num_antenna);
	priv->hw_data.hw_version = pcmd->version;
	priv->hw_data.host_interface = pcmd->host_if;

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_hw_specs(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_hw_spec *pcmd;
	unsigned long flags;
	int i;

	priv = hw->priv;

	/* Info for debugging
	*/
	wiphy_info(hw->wiphy, "%s ...", __func__);
	wiphy_info(hw->wiphy, "  -->pPhysTxRing[0] = %x",
		   priv->desc_data[0].pphys_tx_ring);
	wiphy_info(hw->wiphy, "  -->pPhysTxRing[1] = %x",
		   priv->desc_data[1].pphys_tx_ring);
	wiphy_info(hw->wiphy, "  -->pPhysTxRing[2] = %x",
		   priv->desc_data[2].pphys_tx_ring);
	wiphy_info(hw->wiphy, "  -->pPhysTxRing[3] = %x",
		   priv->desc_data[3].pphys_tx_ring);
	wiphy_info(hw->wiphy, "  -->pPhysRxRing    = %x",
		   priv->desc_data[0].pphys_rx_ring);
	wiphy_info(hw->wiphy, "  -->numtxq %d wcbperq %d totalrxwcb %d",
		   SYSADPT_NUM_OF_DESC_DATA,
		   SYSADPT_MAX_NUM_TX_DESC,
		   SYSADPT_MAX_NUM_RX_DESC);

	pcmd = (struct hostcmd_cmd_set_hw_spec *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_HW_SPEC);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->wcb_base[0] = cpu_to_le32(priv->desc_data[0].pphys_tx_ring);
#if SYSADPT_NUM_OF_DESC_DATA > 3
	for (i = 1; i < SYSADPT_TOTAL_TX_QUEUES; i++)
		pcmd->wcb_base[i] =
			cpu_to_le32(priv->desc_data[i].pphys_tx_ring);
#endif
	pcmd->tx_wcb_num_per_queue = cpu_to_le32(SYSADPT_MAX_NUM_TX_DESC);
	pcmd->num_tx_queues = cpu_to_le32(SYSADPT_NUM_OF_DESC_DATA);
	pcmd->total_rx_wcb = cpu_to_le32(SYSADPT_MAX_NUM_RX_DESC);
	pcmd->rxpd_wr_ptr = cpu_to_le32(priv->desc_data[0].pphys_rx_ring);
	pcmd->features = 0;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_HW_SPEC)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_get_stat(struct ieee80211_hw *hw,
		       struct ieee80211_low_level_stats *stats)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_802_11_get_stat *pcmd;
	unsigned long flags;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_802_11_get_stat *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_802_11_GET_STAT);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_GET_STAT)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	stats->dot11ACKFailureCount =
		le32_to_cpu(pcmd->ack_failures);
	stats->dot11RTSFailureCount =
		le32_to_cpu(pcmd->rts_failures);
	stats->dot11FCSErrorCount =
		le32_to_cpu(pcmd->rx_fcs_errors);
	stats->dot11RTSSuccessCount =
		le32_to_cpu(pcmd->rts_successes);

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_radio_enable(struct ieee80211_hw *hw)
{
	return mwl_fwcmd_802_11_radio_control(hw->priv, true, false);
}

int mwl_fwcmd_radio_disable(struct ieee80211_hw *hw)
{
	return mwl_fwcmd_802_11_radio_control(hw->priv, false, false);
}

int mwl_fwcmd_set_radio_preamble(struct ieee80211_hw *hw, bool short_preamble)
{
	struct mwl_priv *priv;
	int rc;

	priv = hw->priv;

	priv->radio_short_preamble = short_preamble;
	rc = mwl_fwcmd_802_11_radio_control(priv, true, true);

	return rc;
}

int mwl_fwcmd_max_tx_power(struct ieee80211_hw *hw,
			   struct ieee80211_conf *conf, u8 fraction)
{
	struct ieee80211_channel *channel = conf->chandef.chan;
	struct mwl_priv *priv;
	int reduce_val = 0;
	u16 band = 0, width = 0, sub_ch = 0;
	u16 maxtxpow[SYSADPT_TX_POWER_LEVEL_TOTAL];
	int i, tmp;
	int rc;

	priv = hw->priv;

	switch (fraction) {
	case 0:
		reduce_val = 0;    /* Max */
		break;
	case 1:
		reduce_val = 2;    /* 75% -1.25db */
		break;
	case 2:
		reduce_val = 3;    /* 50% -3db */
		break;
	case 3:
		reduce_val = 6;    /* 25% -6db */
		break;
	default:
		/* larger than case 3,  pCmd->MaxPowerLevel is min */
		reduce_val = 0xff;
		break;
	}

	if (channel->band == IEEE80211_BAND_2GHZ)
		band = FREQ_BAND_2DOT4GHZ;
	else if (channel->band == IEEE80211_BAND_5GHZ)
		band = FREQ_BAND_5GHZ;

	switch (conf->chandef.width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		width = CH_20_MHZ_WIDTH;
		sub_ch = NO_EXT_CHANNEL;
		break;
	case NL80211_CHAN_WIDTH_40:
		width = CH_40_MHZ_WIDTH;
		if (conf->chandef.center_freq1 > channel->center_freq)
			sub_ch = EXT_CH_ABOVE_CTRL_CH;
		else
			sub_ch = EXT_CH_BELOW_CTRL_CH;
		break;
	case NL80211_CHAN_WIDTH_80:
		width = CH_80_MHZ_WIDTH;
		if (conf->chandef.center_freq1 > channel->center_freq)
			sub_ch = EXT_CH_ABOVE_CTRL_CH;
		else
			sub_ch = EXT_CH_BELOW_CTRL_CH;
		break;
	default:
		return -EINVAL;
	}

	if ((priv->powinited & MWL_POWER_INIT_2) == 0) {
		mwl_fwcmd_get_tx_powers(priv, priv->max_tx_pow,
					channel->hw_value, band, width, sub_ch);
		priv->powinited |= MWL_POWER_INIT_2;
	}

	if ((priv->powinited & MWL_POWER_INIT_1) == 0) {
		mwl_fwcmd_get_tx_powers(priv, priv->target_powers,
					channel->hw_value, band, width, sub_ch);
		priv->powinited |= MWL_POWER_INIT_1;
	}

	for (i = 0; i < SYSADPT_TX_POWER_LEVEL_TOTAL; i++) {
		if (priv->target_powers[i] > priv->max_tx_pow[i])
			tmp = priv->max_tx_pow[i];
		else
			tmp = priv->target_powers[i];
		maxtxpow[i] = ((tmp - reduce_val) > 0) ? (tmp - reduce_val) : 0;
	}

	rc = mwl_fwcmd_set_tx_powers(priv, maxtxpow, HOSTCMD_ACT_GEN_SET,
				     channel->hw_value, band, width, sub_ch);

	return rc;
}

int mwl_fwcmd_tx_power(struct ieee80211_hw *hw,
		       struct ieee80211_conf *conf, u8 fraction)
{
	struct ieee80211_channel *channel = conf->chandef.chan;
	struct mwl_priv *priv;
	int reduce_val = 0;
	u16 band = 0, width = 0, sub_ch = 0;
	u16 txpow[SYSADPT_TX_POWER_LEVEL_TOTAL];
	int index, found = 0;
	int i, tmp;
	int rc;

	priv = hw->priv;

	switch (fraction) {
	case 0:
		reduce_val = 0;    /* Max */
		break;
	case 1:
		reduce_val = 2;    /* 75% -1.25db */
		break;
	case 2:
		reduce_val = 3;    /* 50% -3db */
		break;
	case 3:
		reduce_val = 6;    /* 25% -6db */
		break;
	default:
		/* larger than case 3,  pCmd->MaxPowerLevel is min */
		reduce_val = 0xff;
		break;
	}

	if (channel->band == IEEE80211_BAND_2GHZ)
		band = FREQ_BAND_2DOT4GHZ;
	else if (channel->band == IEEE80211_BAND_5GHZ)
		band = FREQ_BAND_5GHZ;

	switch (conf->chandef.width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		width = CH_20_MHZ_WIDTH;
		sub_ch = NO_EXT_CHANNEL;
		break;
	case NL80211_CHAN_WIDTH_40:
		width = CH_40_MHZ_WIDTH;
		if (conf->chandef.center_freq1 > channel->center_freq)
			sub_ch = EXT_CH_ABOVE_CTRL_CH;
		else
			sub_ch = EXT_CH_BELOW_CTRL_CH;
		break;
	case NL80211_CHAN_WIDTH_80:
		width = CH_80_MHZ_WIDTH;
		if (conf->chandef.center_freq1 > channel->center_freq)
			sub_ch = EXT_CH_ABOVE_CTRL_CH;
		else
			sub_ch = EXT_CH_BELOW_CTRL_CH;
		break;
	default:
		return -EINVAL;
	}

	/* search tx power table if exist */
	for (index = 0; index < SYSADPT_MAX_NUM_CHANNELS; index++) {
		struct mwl_tx_pwr_tbl *tx_pwr;

		tx_pwr = &priv->tx_pwr_tbl[index];

		/* do nothing if table is not loaded */
		if (tx_pwr->channel == 0)
			break;

		if (tx_pwr->channel == channel->hw_value) {
			priv->cdd = tx_pwr->cdd;
			priv->txantenna2 = tx_pwr->txantenna2;

			if (tx_pwr->setcap)
				priv->powinited = MWL_POWER_INIT_1;
			else
				priv->powinited = MWL_POWER_INIT_2;

			for (i = 0; i < SYSADPT_TX_POWER_LEVEL_TOTAL; i++) {
				if (tx_pwr->setcap)
					priv->max_tx_pow[i] =
						tx_pwr->tx_power[i];
				else
					priv->target_powers[i] =
						tx_pwr->tx_power[i];
			}

			found = 1;
			break;
		}
	}

	if ((priv->powinited & MWL_POWER_INIT_2) == 0) {
		mwl_fwcmd_get_tx_powers(priv, priv->max_tx_pow,
					channel->hw_value, band, width, sub_ch);

		priv->powinited |= MWL_POWER_INIT_2;
	}

	if ((priv->powinited & MWL_POWER_INIT_1) == 0) {
		mwl_fwcmd_get_tx_powers(priv, priv->target_powers,
					channel->hw_value, band, width, sub_ch);

		priv->powinited |= MWL_POWER_INIT_1;
	}

	for (i = 0; i < SYSADPT_TX_POWER_LEVEL_TOTAL; i++) {
		if (found) {
			if ((priv->tx_pwr_tbl[index].setcap) &&
			    (priv->tx_pwr_tbl[index].tx_power[i] >
			    priv->max_tx_pow[i]))
				tmp = priv->max_tx_pow[i];
			else
				tmp = priv->tx_pwr_tbl[index].tx_power[i];
		} else {
			if (priv->target_powers[i] > priv->max_tx_pow[i])
				tmp = priv->max_tx_pow[i];
			else
				tmp = priv->target_powers[i];
		}

		txpow[i] = ((tmp - reduce_val) > 0) ? (tmp - reduce_val) : 0;
	}

	rc = mwl_fwcmd_set_tx_powers(priv, txpow, HOSTCMD_ACT_GEN_SET_LIST,
				     channel->hw_value, band, width, sub_ch);

	return rc;
}

int mwl_fwcmd_rf_antenna(struct ieee80211_hw *hw, int dir, int antenna)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_802_11_rf_antenna *pcmd;
	unsigned long flags;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_802_11_rf_antenna *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_802_11_RF_ANTENNA);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));

	pcmd->action = cpu_to_le16(dir);

	if (dir == WL_ANTENNATYPE_RX) {
		u8 rx_antenna = 4; /* if auto, set 4 rx antennas in SC2 */

		if (antenna != 0)
			pcmd->antenna_mode = cpu_to_le16(antenna);
		else
			pcmd->antenna_mode = cpu_to_le16(rx_antenna);
	} else {
		u8 tx_antenna = 0xf; /* if auto, set 4 tx antennas in SC2 */

		if (antenna != 0)
			pcmd->antenna_mode = cpu_to_le16(antenna);
		else
			pcmd->antenna_mode = cpu_to_le16(tx_antenna);
	}

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_RF_ANTENNA)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_broadcast_ssid_enable(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif, bool enable)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_broadcast_ssid_enable *pcmd;
	unsigned long flags;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_broadcast_ssid_enable *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_BROADCAST_SSID_ENABLE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	pcmd->enable = cpu_to_le32(enable);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_BROADCAST_SSID_ENABLE)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_rf_channel(struct ieee80211_hw *hw,
			     struct ieee80211_conf *conf)
{
	struct ieee80211_channel *channel = conf->chandef.chan;
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_rf_channel *pcmd;
	unsigned long flags;
	u32 chnl_flags, freq_band, chnl_width, act_primary;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_set_rf_channel *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_RF_CHANNEL);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le16(WL_SET);
	pcmd->curr_chnl = channel->hw_value;

	if (channel->band == IEEE80211_BAND_2GHZ)
		freq_band = FREQ_BAND_2DOT4GHZ;
	else if (channel->band == IEEE80211_BAND_5GHZ)
		freq_band = FREQ_BAND_5GHZ;
	else
		return -EINVAL;

	switch (conf->chandef.width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		chnl_width = CH_20_MHZ_WIDTH;
		act_primary = ACT_PRIMARY_CHAN_0;
		break;
	case NL80211_CHAN_WIDTH_40:
		chnl_width = CH_40_MHZ_WIDTH;
		if (conf->chandef.center_freq1 > channel->center_freq)
			act_primary = ACT_PRIMARY_CHAN_0;
		else
			act_primary = ACT_PRIMARY_CHAN_1;
		break;
	case NL80211_CHAN_WIDTH_80:
		chnl_width = CH_80_MHZ_WIDTH;
		act_primary =
			mwl_fwcmd_get_80m_pri_chnl_offset(pcmd->curr_chnl);
		break;
	default:
		return -EINVAL;
	}

	chnl_flags = (freq_band & FREQ_BAND_MASK) |
		((chnl_width << CHNL_WIDTH_SHIFT) & CHNL_WIDTH_MASK) |
		((act_primary << ACT_PRIMARY_SHIFT) & ACT_PRIMARY_MASK);

	pcmd->chnl_flags = cpu_to_le32(chnl_flags);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_RF_CHANNEL)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_aid(struct ieee80211_hw *hw,
		      struct ieee80211_vif *vif, u8 *bssid, u16 aid)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_aid *pcmd;
	unsigned long flags;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_aid *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_AID);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	pcmd->aid = cpu_to_le16(aid);
	ether_addr_copy(pcmd->mac_addr, bssid);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_AID)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_infra_mode(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_infra_mode *pcmd;
	unsigned long flags;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_infra_mode *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_INFRA_MODE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_INFRA_MODE)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_rts_threshold(struct ieee80211_hw *hw, int threshold)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_802_11_rts_thsd *pcmd;
	unsigned long flags;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_802_11_rts_thsd *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_802_11_RTS_THSD);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action  = cpu_to_le16(WL_SET);
	pcmd->threshold = cpu_to_le16(threshold);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_RTS_THSD)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_edca_params(struct ieee80211_hw *hw, u8 index,
			      u16 cw_min, u16 cw_max, u8 aifs, u16 txop)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_edca_params *pcmd;
	unsigned long flags;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_set_edca_params *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_EDCA_PARAMS);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));

	pcmd->action = cpu_to_le16(0xffff);
	pcmd->txop = cpu_to_le16(txop);
	pcmd->cw_max = cpu_to_le32(ilog2(cw_max + 1));
	pcmd->cw_min = cpu_to_le32(ilog2(cw_min + 1));
	pcmd->aifsn = aifs;
	pcmd->txq_num = index;

	/* The array index defined in qos.h has a reversed bk and be.
	 * The HW queue was not used this way; the qos code needs to
	 * be changed or checked
	 */
	if (index == 0)
		pcmd->txq_num = 1;
	else if (index == 1)
		pcmd->txq_num = 0;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_EDCA_PARAMS)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_wmm_mode(struct ieee80211_hw *hw, bool enable)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_wmm_mode *pcmd;
	unsigned long flags;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_set_wmm_mode *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_WMM_MODE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le16(enable ? WL_ENABLE : WL_DISABLE);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_WMM_MODE)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_use_fixed_rate(struct ieee80211_hw *hw, int mcast, int mgmt)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_fixed_rate *pcmd;
	unsigned long flags;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_set_fixed_rate *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_FIXED_RATE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));

	pcmd->action = cpu_to_le32(HOSTCMD_ACT_NOT_USE_FIXED_RATE);
	pcmd->multicast_rate = mcast;
	pcmd->management_rate = mgmt;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_FIXED_RATE)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_rate_adapt_mode(struct ieee80211_hw *hw, u16 mode)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_rate_adapt_mode *pcmd;
	unsigned long flags;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_set_rate_adapt_mode *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_RATE_ADAPT_MODE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le16(WL_SET);
	pcmd->rate_adapt_mode = cpu_to_le16(mode);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_RATE_ADAPT_MODE)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_mac_addr_client(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif, u8 *mac_addr)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_mac_addr *pcmd;
	unsigned long flags;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_mac_addr *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_MAC_ADDR);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	pcmd->mac_type = cpu_to_le16(WL_MAC_TYPE_SECONDARY_CLIENT);
	ether_addr_copy(pcmd->mac_addr, mac_addr);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_MAC_ADDR)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_get_watchdog_bitmap(struct ieee80211_hw *hw, u8 *bitmap)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_get_watchdog_bitmap *pcmd;
	unsigned long flags;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_get_watchdog_bitmap *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_GET_WATCHDOG_BITMAP);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_GET_WATCHDOG_BITMAP)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	*bitmap = pcmd->watchdog_bitmap;

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_remove_mac_addr(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif, u8 *mac_addr)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_mac_addr *pcmd;
	unsigned long flags;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_mac_addr *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_DEL_MAC_ADDR);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	ether_addr_copy(pcmd->mac_addr, mac_addr);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_DEL_MAC_ADDR)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_bss_start(struct ieee80211_hw *hw,
			struct ieee80211_vif *vif, bool enable)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_bss_start *pcmd;
	unsigned long flags;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	if (enable && (priv->running_bsses & (1 << mwl_vif->macid)))
		return 0;

	if (!enable && !(priv->running_bsses & (1 << mwl_vif->macid)))
		return 0;

	pcmd = (struct hostcmd_cmd_bss_start *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_BSS_START);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	if (enable) {
		pcmd->enable = cpu_to_le32(WL_ENABLE);
	} else {
		if (mwl_vif->macid == 0)
			pcmd->enable = cpu_to_le32(WL_DISABLE);
		else
			pcmd->enable = cpu_to_le32(WL_DISABLE_VMAC);
	}

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_BSS_START)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	if (enable)
		priv->running_bsses |= (1 << mwl_vif->macid);
	else
		priv->running_bsses &= ~(1 << mwl_vif->macid);

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_beacon(struct ieee80211_hw *hw,
			 struct ieee80211_vif *vif, u8 *beacon, int len)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	mwl_fwcmd_parse_beacon(priv, mwl_vif, beacon, len);

	if (mwl_fwcmd_set_ies(priv, mwl_vif))
		goto err;

	if (mwl_fwcmd_set_ap_beacon(priv, mwl_vif, &vif->bss_conf))
		goto err;

	mwl_vif->beacon_info.valid = false;

	return 0;

err:

	mwl_vif->beacon_info.valid = false;

	return -EIO;
}

int mwl_fwcmd_set_new_stn_add(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif,
			      struct ieee80211_sta *sta)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_new_stn *pcmd;
	unsigned long flags;
	u32 rates;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_new_stn *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_NEW_STN);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action = cpu_to_le16(HOSTCMD_ACT_STA_ACTION_ADD);
	if (mwl_vif->is_sta) {
		pcmd->aid = 0;
		pcmd->stn_id = 0;
	} else {
		pcmd->aid = cpu_to_le16(sta->aid);
		pcmd->stn_id = cpu_to_le16(sta->aid);
	}
	ether_addr_copy(pcmd->mac_addr, sta->addr);

	if (hw->conf.chandef.chan->band == IEEE80211_BAND_2GHZ)
		rates = sta->supp_rates[IEEE80211_BAND_2GHZ];
	else
		rates = sta->supp_rates[IEEE80211_BAND_5GHZ] << 5;
	pcmd->peer_info.legacy_rate_bitmap = cpu_to_le32(rates);

	if (sta->ht_cap.ht_supported) {
		pcmd->peer_info.ht_rates[0] = sta->ht_cap.mcs.rx_mask[0];
		pcmd->peer_info.ht_rates[1] = sta->ht_cap.mcs.rx_mask[1];
		pcmd->peer_info.ht_rates[2] = sta->ht_cap.mcs.rx_mask[2];
		pcmd->peer_info.ht_rates[3] = sta->ht_cap.mcs.rx_mask[3];
		pcmd->peer_info.ht_cap_info = cpu_to_le16(sta->ht_cap.cap);
		pcmd->peer_info.mac_ht_param_info =
			(sta->ht_cap.ampdu_factor & 3) |
			((sta->ht_cap.ampdu_density & 7) << 2);
	}

	if (sta->vht_cap.vht_supported) {
		pcmd->peer_info.vht_max_rx_mcs =
			cpu_to_le32(*((u32 *)
			&sta->vht_cap.vht_mcs.rx_mcs_map));
		pcmd->peer_info.vht_cap = cpu_to_le32(sta->vht_cap.cap);
		pcmd->peer_info.vht_rx_channel_width = sta->bandwidth;
	}

	pcmd->is_qos_sta = sta->wme;
	pcmd->qos_info = ((sta->uapsd_queues << 4) | (sta->max_sp << 1));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_NEW_STN)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	if (mwl_vif->is_sta) {
		ether_addr_copy(pcmd->mac_addr, mwl_vif->sta_mac);

		if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_NEW_STN)) {
			spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
			wiphy_err(hw->wiphy, "failed execution");
			return -EIO;
		}
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_new_stn_add_self(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_new_stn *pcmd;
	unsigned long flags;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_new_stn *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_NEW_STN);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action = cpu_to_le16(HOSTCMD_ACT_STA_ACTION_ADD);
	ether_addr_copy(pcmd->mac_addr, vif->addr);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_NEW_STN)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_new_stn_del(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif, u8 *addr)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_new_stn *pcmd;
	unsigned long flags;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_new_stn *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_NEW_STN);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action = cpu_to_le16(HOSTCMD_ACT_STA_ACTION_REMOVE);
	ether_addr_copy(pcmd->mac_addr, addr);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_NEW_STN)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	if  (mwl_vif->is_sta) {
		ether_addr_copy(pcmd->mac_addr, mwl_vif->sta_mac);

		if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_NEW_STN)) {
			spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
			wiphy_err(hw->wiphy, "failed execution");
			return -EIO;
		}
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_apmode(struct ieee80211_hw *hw, u8 apmode)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_apmode *pcmd;
	unsigned long flags;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_set_apmode *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_APMODE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->apmode = apmode;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_APMODE)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_update_encryption_enable(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       u8 *addr, u8 encr_type)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_update_encryption *pcmd;
	unsigned long flags;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_update_encryption *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_UPDATE_ENCRYPTION);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action_type = cpu_to_le32(ENCR_ACTION_ENABLE_HW_ENCR);
	ether_addr_copy(pcmd->mac_addr, addr);
	pcmd->action_data[0] = encr_type;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_UPDATE_ENCRYPTION)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	if (mwl_vif->is_sta) {
		if (memcmp(mwl_vif->bssid, addr, ETH_ALEN) == 0)
			ether_addr_copy(pcmd->mac_addr, mwl_vif->sta_mac);
		else
			ether_addr_copy(pcmd->mac_addr, mwl_vif->bssid);

		if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_UPDATE_ENCRYPTION)) {
			spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
			wiphy_err(hw->wiphy, "failed execution");
			return -EIO;
		}
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_encryption_set_key(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif, u8 *addr,
				 struct ieee80211_key_conf *key)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_key *pcmd;
	unsigned long flags;
	int rc;
	int keymlen;
	u32 action;
	u8 idx;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_key *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_UPDATE_ENCRYPTION);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	rc = mwl_fwcmd_encryption_set_cmd_info(pcmd, addr, key);
	if (rc) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "encryption not support");
		return rc;
	}

	idx = key->keyidx;

	if (key->flags & IEEE80211_KEY_FLAG_PAIRWISE)
		action = ENCR_ACTION_TYPE_SET_KEY;
	else
		action = ENCR_ACTION_TYPE_SET_GROUP_KEY;

	switch (key->cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		if (!mwl_vif->wep_key_conf[idx].enabled) {
			memcpy(mwl_vif->wep_key_conf[idx].key, key,
			       sizeof(*key) + key->keylen);
			mwl_vif->wep_key_conf[idx].enabled = 1;
		}

		keymlen = key->keylen;
		action = ENCR_ACTION_TYPE_SET_KEY;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		keymlen = MAX_ENCR_KEY_LENGTH + 2 * MIC_KEY_LENGTH;
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		keymlen = key->keylen;
		break;
	default:
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "encryption not support");
		return -ENOTSUPP;
	}

	memcpy((void *)&pcmd->key_param.key, key->key, keymlen);
	pcmd->action_type = cpu_to_le32(action);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_UPDATE_ENCRYPTION)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	if (mwl_vif->is_sta) {
		if (memcmp(mwl_vif->bssid, addr, ETH_ALEN) == 0)
			ether_addr_copy(pcmd->key_param.mac_addr,
					mwl_vif->sta_mac);
		else
			ether_addr_copy(pcmd->key_param.mac_addr,
					mwl_vif->bssid);

		if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_UPDATE_ENCRYPTION)) {
			spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
			wiphy_err(hw->wiphy, "failed execution");
			return -EIO;
		}
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_encryption_remove_key(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif, u8 *addr,
				    struct ieee80211_key_conf *key)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_key *pcmd;
	unsigned long flags;
	int rc;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_key *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_UPDATE_ENCRYPTION);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	rc = mwl_fwcmd_encryption_set_cmd_info(pcmd, addr, key);
	if (rc) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "encryption not support");
		return rc;
	}

	pcmd->action_type = cpu_to_le32(ENCR_ACTION_TYPE_REMOVE_KEY);

	if (key->cipher == WLAN_CIPHER_SUITE_WEP40 ||
	    key->cipher == WLAN_CIPHER_SUITE_WEP104)
		mwl_vif->wep_key_conf[key->keyidx].enabled = 0;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_UPDATE_ENCRYPTION)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_check_ba(struct ieee80211_hw *hw,
		       struct mwl_ampdu_stream *stream,
		       struct ieee80211_vif *vif)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_bastream *pcmd;
	unsigned long flags;
	u32 ba_flags, ba_type, ba_direction;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_bastream *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_BASTREAM);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	pcmd->cmd_hdr.result = cpu_to_le16(0xffff);

	pcmd->action_type = cpu_to_le32(BA_CHECK_STREAM);
	memcpy(&pcmd->ba_info.create_params.peer_mac_addr[0],
	       stream->sta->addr, ETH_ALEN);
	pcmd->ba_info.create_params.tid = stream->tid;
	ba_type = BASTREAM_FLAG_IMMEDIATE_TYPE;
	ba_direction = BASTREAM_FLAG_DIRECTION_UPSTREAM;
	ba_flags = (ba_type & BA_TYPE_MASK) |
		((ba_direction << BA_DIRECTION_SHIFT) & BA_DIRECTION_MASK);
	pcmd->ba_info.create_params.flags = cpu_to_le32(ba_flags);
	pcmd->ba_info.create_params.queue_id = stream->idx;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_BASTREAM)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	if (pcmd->cmd_hdr.result != 0) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "result error");
		return -EINVAL;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_create_ba(struct ieee80211_hw *hw,
			struct mwl_ampdu_stream *stream,
			u8 buf_size, struct ieee80211_vif *vif)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_bastream *pcmd;
	unsigned long flags;
	u32 ba_flags, ba_type, ba_direction;

	priv = hw->priv;
	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_bastream *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_BASTREAM);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	pcmd->cmd_hdr.result = cpu_to_le16(0xffff);

	pcmd->action_type = cpu_to_le32(BA_CREATE_STREAM);
	pcmd->ba_info.create_params.bar_thrs = cpu_to_le32(buf_size);
	pcmd->ba_info.create_params.window_size = cpu_to_le32(buf_size);
	memcpy(&pcmd->ba_info.create_params.peer_mac_addr[0],
	       stream->sta->addr, ETH_ALEN);
	pcmd->ba_info.create_params.tid = stream->tid;
	ba_type = BASTREAM_FLAG_IMMEDIATE_TYPE;
	ba_direction = BASTREAM_FLAG_DIRECTION_UPSTREAM;
	ba_flags = (ba_type & BA_TYPE_MASK) |
		((ba_direction << BA_DIRECTION_SHIFT) & BA_DIRECTION_MASK);
	pcmd->ba_info.create_params.flags = cpu_to_le32(ba_flags);
	pcmd->ba_info.create_params.queue_id = stream->idx;
	pcmd->ba_info.create_params.param_info =
		(stream->sta->ht_cap.ampdu_factor &
		 IEEE80211_HT_AMPDU_PARM_FACTOR) |
		((stream->sta->ht_cap.ampdu_density << 2) &
		 IEEE80211_HT_AMPDU_PARM_DENSITY);
	pcmd->ba_info.create_params.reset_seq_no = 1;
	pcmd->ba_info.create_params.current_seq = cpu_to_le16(0);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_BASTREAM)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	if (pcmd->cmd_hdr.result != 0) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "result error");
		return -EINVAL;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_destroy_ba(struct ieee80211_hw *hw,
			 u8 idx)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_bastream *pcmd;
	unsigned long flags;
	u32 ba_flags, ba_type, ba_direction;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_bastream *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_BASTREAM);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));

	pcmd->action_type = cpu_to_le32(BA_DESTROY_STREAM);
	ba_type = BASTREAM_FLAG_IMMEDIATE_TYPE;
	ba_direction = BASTREAM_FLAG_DIRECTION_UPSTREAM;
	ba_flags = (ba_type & BA_TYPE_MASK) |
		((ba_direction << BA_DIRECTION_SHIFT) & BA_DIRECTION_MASK);
	pcmd->ba_info.destroy_params.flags = cpu_to_le32(ba_flags);
	pcmd->ba_info.destroy_params.fw_ba_context.context = cpu_to_le32(idx);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_BASTREAM)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

/* caller must hold priv->stream_lock when calling the stream functions */
struct mwl_ampdu_stream *mwl_fwcmd_add_stream(struct ieee80211_hw *hw,
					      struct ieee80211_sta *sta,
					      u8 tid)
{
	struct mwl_priv *priv;
	struct mwl_ampdu_stream *stream;
	int i;

	priv = hw->priv;

	for (i = 0; i < SYSADPT_TX_AMPDU_QUEUES; i++) {
		stream = &priv->ampdu[i];

		if (stream->state == AMPDU_NO_STREAM) {
			stream->sta = sta;
			stream->state = AMPDU_STREAM_NEW;
			stream->tid = tid;
			stream->idx = i;
			return stream;
		}
	}

	return NULL;
}

int mwl_fwcmd_start_stream(struct ieee80211_hw *hw,
			   struct mwl_ampdu_stream *stream)
{
	/* if the stream has already been started, don't start it again */
	if (stream->state != AMPDU_STREAM_NEW)
		return 0;

	return ieee80211_start_tx_ba_session(stream->sta, stream->tid, 0);
}

void mwl_fwcmd_remove_stream(struct ieee80211_hw *hw,
			     struct mwl_ampdu_stream *stream)
{
	memset(stream, 0, sizeof(*stream));
}

struct mwl_ampdu_stream *mwl_fwcmd_lookup_stream(struct ieee80211_hw *hw,
						 u8 *addr, u8 tid)
{
	struct mwl_priv *priv;
	struct mwl_ampdu_stream *stream;
	int i;

	priv = hw->priv;

	for (i = 0; i < SYSADPT_TX_AMPDU_QUEUES; i++) {
		stream = &priv->ampdu[i];

		if (stream->state == AMPDU_NO_STREAM)
			continue;

		if (!memcmp(stream->sta->addr, addr, ETH_ALEN) &&
		    stream->tid == tid)
			return stream;
	}

	return NULL;
}

bool mwl_fwcmd_ampdu_allowed(struct ieee80211_sta *sta, u8 tid)
{
	struct mwl_sta *sta_info;
	struct mwl_tx_info *tx_stats;

	sta_info = mwl_dev_get_sta(sta);

	BUG_ON(tid >= SYSADPT_MAX_TID);

	tx_stats = &sta_info->tx_stats[tid];

	return (sta_info->is_ampdu_allowed &&
		tx_stats->pkts > SYSADPT_AMPDU_PACKET_THRESHOLD);
}

int mwl_fwcmd_set_dwds_stamode(struct ieee80211_hw *hw, bool enable)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_dwds_enable *pcmd;
	unsigned long flags;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_dwds_enable *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_DWDS_ENABLE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->enable = cpu_to_le32(enable);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_DWDS_ENABLE)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_fw_flush_timer(struct ieee80211_hw *hw, u32 value)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_fw_flush_timer *pcmd;
	unsigned long flags;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_fw_flush_timer *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_FW_FLUSH_TIMER);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->value = cpu_to_le32(value);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_FW_FLUSH_TIMER)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}

int mwl_fwcmd_set_cdd(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_cdd *pcmd;
	unsigned long flags;

	priv = hw->priv;

	pcmd = (struct hostcmd_cmd_set_cdd *)&priv->pcmd_buf[0];

	spin_lock_irqsave(&priv->fwcmd_lock, flags);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_CDD);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->enable = cpu_to_le32(priv->cdd);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_CDD)) {
		spin_unlock_irqrestore(&priv->fwcmd_lock, flags);
		wiphy_err(hw->wiphy, "failed execution");
		return -EIO;
	}

	spin_unlock_irqrestore(&priv->fwcmd_lock, flags);

	return 0;
}
