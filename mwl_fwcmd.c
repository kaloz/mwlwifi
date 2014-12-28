/*
* Copyright (c) 2006-2014 Marvell International Ltd.
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
*   Description:  This file implements frimware host command related functions.
*
*/

#include "mwl_sysadpt.h"
#include "mwl_dev.h"
#include "mwl_debug.h"
#include "mwl_fwcmd.h"

/* CONSTANTS AND MACROS
*/

#define MAX_WAIT_FW_COMPLETE_ITERATIONS         10000

/*
 *      16 bit host command code
 */
#define HOSTCMD_CMD_GET_HW_SPEC                 0x0003
#define HOSTCMD_CMD_SET_HW_SPEC				0x0004
#define HOSTCMD_CMD_802_11_GET_STAT             0x0014
#define HOSTCMD_CMD_802_11_RADIO_CONTROL        0x001c
#define HOSTCMD_CMD_802_11_TX_POWER             0x001f
#define HOSTCMD_CMD_802_11_RF_ANTENNA           0x0020
#define HOSTCMD_CMD_BROADCAST_SSID_ENABLE       0x0050 /* per-vif */
#define HOSTCMD_CMD_SET_RF_CHANNEL              0x010a
#define HOSTCMD_CMD_802_11_RTS_THSD             0x0113
#define HOSTCMD_CMD_SET_EDCA_PARAMS             0x0115
#define HOSTCMD_CMD_SET_WMM_MODE                0x0123
#define HOSTCMD_CMD_SET_FIXED_RATE              0x0126
#define HOSTCMD_CMD_SET_IES                     0x0127
#define HOSTCMD_CMD_SET_RATE_ADAPT_MODE			0x0203
#define HOSTCMD_CMD_GET_WATCHDOG_BITMAP         0x0205
#define HOSTCMD_CMD_BSS_START                   0x1100 /* per-vif */
#define HOSTCMD_CMD_AP_BEACON                   0x1101 /* per-vif */
#define HOSTCMD_CMD_SET_NEW_STN                 0x1111 /* per-vif */
#define HOSTCMD_CMD_SET_APMODE                  0x1114
#define HOSTCMD_CMD_UPDATE_ENCRYPTION			0x1122 /* per-vif */
#define HOSTCMD_CMD_BASTREAM					0x1125
#define HOSTCMD_CMD_DWDS_ENABLE					0x1144
#define HOSTCMD_CMD_FW_FLUSH_TIMER				0x1148
#define HOSTCMD_CMD_SET_CDD                     0x1150

/*
 *      Define general result code for each command
 */
#define HOSTCMD_RESULT_OK                       0x0000 /* OK */
#define HOSTCMD_RESULT_ERROR                    0x0001 /* Genenral error */
#define HOSTCMD_RESULT_NOT_SUPPORT              0x0002 /* Command is not valid */
#define HOSTCMD_RESULT_PENDING                  0x0003 /* Command is pending (will be processed) */
#define HOSTCMD_RESULT_BUSY                     0x0004 /* System is busy (command ignored) */
#define HOSTCMD_RESULT_PARTIAL_DATA             0x0005 /* Data buffer is not big enough */

/*
 *      Define channel related constants
 */
#define FREQ_BAND_2DOT4GHZ	                    0x1
#define FREQ_BAND_4DOT9GHZ	                    0x2
#define FREQ_BAND_5GHZ                          0x4
#define FREQ_BAND_5DOT2GHZ                      0x8
#define CH_AUTO_WIDTH	                        0
#define CH_10_MHz_WIDTH                         0x1
#define CH_20_MHz_WIDTH                         0x2
#define CH_40_MHz_WIDTH                         0x4
#define CH_80_MHz_WIDTH                         0x5
#define EXT_CH_ABOVE_CTRL_CH                    0x1
#define EXT_CH_AUTO                             0x2
#define EXT_CH_BELOW_CTRL_CH                    0x3
#define NO_EXT_CHANNEL                          0x0

#define ACT_PRIMARY_CHAN_0                      0 /* active primary 1st 20MHz channel */
#define ACT_PRIMARY_CHAN_1                      1 /* active primary 2nd 20MHz channel */
#define ACT_PRIMARY_CHAN_2                      2 /* active primary 3rd 20MHz channel */
#define ACT_PRIMARY_CHAN_3                      3 /* active primary 4th 20MHz channel */
#define ACT_PRIMARY_CHAN_4                      4 /* active primary 5th 20MHz channel */
#define ACT_PRIMARY_CHAN_5                      5 /* active primary 6th 20MHz channel */
#define ACT_PRIMARY_CHAN_6                      6 /* active primary 7th 20MHz channel */
#define ACT_PRIMARY_CHAN_7                      7 /* active primary 8th 20MHz channel */

/*
 *      Define rate related constants
 */
#define HOSTCMD_ACT_NOT_USE_FIXED_RATE          0x0002

/*
 *      Define station related constants
 */
#define HOSTCMD_ACT_STA_ACTION_ADD              0
#define HOSTCMD_ACT_STA_ACTION_REMOVE           2

/*
 *      Define key related constants
 */
#define MAX_ENCR_KEY_LENGTH                     16
#define MIC_KEY_LENGTH                          8

#define KEY_TYPE_ID_WEP							0x00       /* Key type is WEP		*/
#define KEY_TYPE_ID_TKIP						0x01       /* Key type is TKIP		*/
#define KEY_TYPE_ID_AES							0x02       /* Key type is AES-CCMP	*/

#define ENCR_KEY_FLAG_TXGROUPKEY				0x00000004 /* Group key for TX */
#define ENCR_KEY_FLAG_PAIRWISE					0x00000008 /* pairwise */
#define ENCR_KEY_FLAG_TSC_VALID					0x00000040 /* Sequence counters are valid */
#define ENCR_KEY_FLAG_WEP_TXKEY					0x01000000 /* Tx key for WEP */
#define ENCR_KEY_FLAG_MICKEY_VALID				0x02000000 /* Tx/Rx MIC keys are valid */

/*
 *      Define block ack related constants
 */
#define BASTREAM_FLAG_IMMEDIATE_TYPE            1
#define BASTREAM_FLAG_DIRECTION_UPSTREAM        0

/*
 *      Define general purpose action
 */
#define HOSTCMD_ACT_GEN_SET                     0x0001
#define HOSTCMD_ACT_GEN_SET_LIST                0x0002
#define HOSTCMD_ACT_GEN_GET_LIST                0x0003

/*      Misc
*/
#define MWL_SPIN_LOCK(X)                        SPIN_LOCK_IRQSAVE(X, flags)
#define MWL_SPIN_UNLOCK(X)	                    SPIN_UNLOCK_IRQRESTORE(X, flags)

#define MAX_ENCR_KEY_LENGTH                     16
#define MIC_KEY_LENGTH                          8

/* TYPE DEFINITION
*/

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

/*
 *      General host command header
 */

struct hostcmd_header {

	u16 cmd;
	u16 len;
	u8 seq_num;
	u8 macid;
	u16 result;

} __packed;

/*
 *      HOSTCMD_CMD_GET_HW_SPEC
 */

struct hostcmd_cmd_get_hw_spec {

	struct hostcmd_header cmd_hdr;
	u8 version;                  /* version of the HW                    */
	u8 host_if;                  /* host interface                       */
	u16 num_wcb;                 /* Max. number of WCB FW can handle     */
	u16 num_mcast_addr;          /* MaxNbr of MC addresses FW can handle */
	u8 permanent_addr[ETH_ALEN]; /* MAC address programmed in HW         */
	u16 region_code;
	u16 num_antenna;             /* Number of antenna used      */
	u32 fw_release_num;          /* 4 byte of FW release number */
	u32 wcb_base0;
	u32 rxpd_wr_ptr;
	u32 rxpd_rd_ptr;
	u32 fw_awake_cookie;
	u32 wcb_base[SYSADPT_TOTAL_TX_QUEUES - 1];

} __packed;

/*
 *      HOSTCMD_CMD_SET_HW_SPEC
 */

struct hostcmd_cmd_set_hw_spec {

	struct hostcmd_header cmd_hdr;
	u8 version;                             /* HW revision */
	u8 host_if;                             /* Host interface */
	u16 num_mcast_addr;                     /* Max. number of Multicast address FW can handle */
	u8 permanent_addr[ETH_ALEN];            /* MAC address */
	u16 region_code;                        /* Region Code */
	u32 fw_release_num;                     /* 4 byte of FW release number, example 0x1234=1.2.3.4 */
	u32 fw_awake_cookie;                    /* Firmware awake cookie - used to ensure that the device is not in sleep mode */
	u32 device_caps;                        /* Device capabilities (see above) */
	u32 rxpd_wr_ptr;                        /* Rx shared memory queue */
	u32 num_tx_queues;                      /* Actual number of TX queues in WcbBase array */
	u32 wcb_base[SYSADPT_NUM_OF_DESC_DATA]; /* TX WCB Rings */
	u32 max_amsdu_size:2;                   /* Max AMSDU size (00 - AMSDU Disabled, 01 - 4K, 10 - 8K, 11 - not defined) */
	u32 implicit_ampdu_ba:1;                /* Indicates supported AMPDU type (1 = implicit, 0 = explicit (default)) */
	u32 disablembss:1;                      /* indicates mbss features disable in FW */
	u32 host_form_beacon:1;
	u32 host_form_probe_response:1;
	u32 host_power_save:1;
	u32 host_encr_decr_mgt:1;
	u32 host_intra_bss_offload:1;
	u32 host_iv_offload:1;
	u32 host_encr_decr_frame:1;
	u32 reserved: 21;                       /* Reserved */
	u32 tx_wcb_num_per_queue;
	u32 total_rx_wcb;

} __packed;

/*
 *      HOSTCMD_CMD_802_11_GET_STAT
 */

struct hostcmd_cmd_802_11_get_stat {

	struct hostcmd_header cmd_hdr;
	u32 tx_retry_successes;
	u32 tx_multiple_retry_successes;
	u32 tx_failures;
	u32 rts_successes;
	u32 rts_failures;
	u32 ack_failures;
	u32 rx_duplicate_frames;
	u32 rx_fcs_errors;
	u32 tx_watchdog_timeouts;
	u32 rx_overflows;
	u32 rx_frag_errors;
	u32 rx_mem_errors;
	u32 pointer_errors;
	u32 tx_underflows;
	u32 tx_done;
	u32 tx_done_buf_try_put;
	u32 tx_done_buf_put;
	u32 wait_for_tx_buf;                    /* Put size of requested buffer in here */
	u32 tx_attempts;
	u32 tx_successes;
	u32 tx_fragments;
	u32 tx_multicasts;
	u32 rx_non_ctl_pkts;
	u32 rx_multicasts;
	u32 rx_undecryptable_frames;
	u32 rx_icv_errors;
	u32 rx_excluded_frames;
	u32 rx_weak_iv_count;
	u32 rx_unicasts;
	u32 rx_bytes;
	u32 rx_errors;
	u32 rx_rts_count;
	u32 tx_cts_count;

} __packed;

/*
 *      HOSTCMD_CMD_802_11_RADIO_CONTROL
 */

struct hostcmd_cmd_802_11_radio_control {

	struct hostcmd_header cmd_hdr;
	u16 action;
	u16 control;	             /* @bit0: 1/0,on/off, @bit1: 1/0, long/short @bit2: 1/0,auto/fix */
	u16 radio_on;

} __packed;

/*
 *     HOSTCMD_CMD_802_11_TX_POWER
 */

struct hostcmd_cmd_802_11_tx_power {

	struct hostcmd_header cmd_hdr;
	u16 action;
	u16 band;
	u16 ch;
	u16 bw;
	u16 sub_ch;
	u16 power_level_list[SYSADPT_TX_POWER_LEVEL_TOTAL];

} __packed;

/*
 *     HOSTCMD_CMD_802_11_RF_ANTENNA
 */

struct hostcmd_cmd_802_11_rf_antenna {

	struct hostcmd_header cmd_hdr;
	u16 action;
	u16 antenna_mode;            /* Number of antennas or 0xffff(diversity) */

} __packed;

/*
 *     HOSTCMD_CMD_BROADCAST_SSID_ENABLE
 */

struct hostcmd_cmd_broadcast_ssid_enable {

	struct hostcmd_header cmd_hdr;
	u32 enable;

} __packed;

/*
 *     HOSTCMD_CMD_SET_RF_CHANNEL
 */

struct chnl_flags_11ac {

	u32	freq_band:6;             /* bit0=1: 2.4GHz,bit1=1: 4.9GHz,bit2=1: 5GHz,bit3=1: 5.2GHz, */
	u32	chnl_width:5;            /* bit6=1:10MHz, bit7=1:20MHz, bit8=1:40MHz */
	u32	act_primary:3;           /* 000: 1st 20MHz chan, 001:2nd 20MHz chan, 011:3rd 20MHz chan, 100:4th 20MHz chan */
	u32	reserved:18;

} __packed;

struct hostcmd_cmd_set_rf_channel {

	struct hostcmd_header cmd_hdr;
	u16 action;
	u8 curr_chnl;
	struct chnl_flags_11ac chnl_flags;

} __packed;

/*
 *     HOSTCMD_CMD_802_11_RTS_THSD
 */

struct hostcmd_cmd_802_11_rts_thsd {

	struct hostcmd_header cmd_hdr;
	u16 action;
	u16	threshold;

} __packed;

/*
 *     HOSTCMD_CMD_SET_EDCA_PARAMS
 */

struct hostcmd_cmd_set_edca_params {

	struct hostcmd_header cmd_hdr;
	u16 action;                  /* 0 = get all, 0x1 =set CWMin/Max,  0x2 = set TXOP , 0x4 =set AIFSN */
	u16 txop;                    /* in unit of 32 us */
	u32 cw_max;                  /* 0~15 */
	u32 cw_min;                  /* 0~15 */
	u8 aifsn;
	u8 txq_num;                  /* Tx Queue number. */

} __packed;

/*
 *      HOSTCMD_CMD_SET_WMM_MODE
 */

struct hostcmd_cmd_set_wmm_mode {

	struct hostcmd_header cmd_hdr;
	u16 action;                  /* 0->unset, 1->set */

} __packed;

/*
 *     HOSTCMD_CMD_SET_FIXED_RATE
 */

struct fix_rate_flag {
                                 /* lower rate after the retry count */
	u32 fix_rate_type;           /* 0: legacy, 1: HT */
	u32 retry_count_valid;       /* 0: retry count is not valid, 1: use retry count specified */

} __packed;

struct fix_rate_entry {

	struct fix_rate_flag fix_rate_type_flags;
	u32 fixed_rate;              /* depending on the flags above, this can be either a legacy rate(not index) or an MCS code. */
	u32 retry_count;

} __packed;

struct hostcmd_cmd_set_fixed_rate {

	struct hostcmd_header cmd_hdr;
	u32 action;                  /* HOSTCMD_ACT_NOT_USE_FIXED_RATE 0x0002 */
	u32 allow_rate_drop;         /* use fixed rate specified but firmware can drop to */
	u32 entry_count;
	struct fix_rate_entry fixed_rate_table[4];
	u8 multicast_rate;
	u8 multi_rate_tx_type;
	u8 management_rate;

} __packed;

/*
 *     HOSTCMD_CMD_SET_IES
 */

struct hostcmd_cmd_set_ies {

	struct hostcmd_header cmd_hdr;
	u16 action;                  /* 0->unset, 1->set */
	u16 ie_list_len_ht;
	u16 ie_list_len_vht;
	u16 ie_list_len_proprietary;
	/*Buffer size same as Generic_Beacon*/
	u8 ie_list_ht[148];
	u8 ie_list_vht[24];
	u8 ie_list_proprietary[112];

} __packed;

/*
 *      HOSTCMD_CMD_SET_RATE_ADAPT_MODE
 */

struct hostcmd_cmd_set_rate_adapt_mode {

	struct hostcmd_header cmd_hdr;
	u16 action;
	u16 rate_adapt_mode;         /* 0:Indoor, 1:Outdoor */

} __packed;

/*
 *     HOSTCMD_CMD_GET_WATCHDOG_BITMAP
 */

struct hostcmd_cmd_get_watchdog_bitmap {

	struct hostcmd_header cmd_hdr;
	u8 watchdog_bitmap;          /* for SW/BA */

} __packed;

/*
 *     HOSTCMD_CMD_BSS_START
 */

struct hostcmd_cmd_bss_start {

	struct hostcmd_header cmd_hdr;
	u32 enable;                  /* FALSE: Disable or TRUE: Enable */

} __packed;

/*
 *     HOSTCMD_CMD_AP_BEACON
 */

struct cf_params {

	u8 elem_id;
	u8 len;
	u8 cfp_cnt;
	u8 cfp_period;
	u16 cfp_max_duration;
	u16 cfp_duration_remaining;

} __packed;

struct ibss_params {

	u8 elem_id;
	u8 len;
	u16	atim_window;

} __packed;

union ss_params {

	struct cf_params cf_param_set;
	struct ibss_params ibss_param_set;

} __packed;

struct fh_params {

	u8 elem_id;
	u8 len;
	u16 dwell_time;
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

} __packed;

struct aci_aifsn_field {

	u8 aifsn:4;
	u8 acm:1;
	u8 aci:2;
	u8 rsvd:1;

} __packed;

struct ecw_min_max_field {

	u8 ecw_min:4;
	u8 ecw_max:4;

} __packed;

struct ac_param_rcd {

	struct aci_aifsn_field aci_aifsn;
	struct ecw_min_max_field ecw_min_max;
	u16 txop_lim;

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

} __packetd;

struct start_cmd {

	u8 sta_mac_addr[ETH_ALEN];
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 bss_type;
	u16 bcn_period;
	u8 dtim_period;
	union ss_params ss_param_set;
	union phy_params phy_param_set;
	u16 probe_delay;
	u16 cap_info;
	u8 bss_basic_rate_set[SYSADPT_MAX_DATA_RATES_G];
	u8 op_rate_set[SYSADPT_MAX_DATA_RATES_G];
	struct rsn_ie rsn_ie;
	struct rsn48_ie rsn48_ie;
	struct wmm_param_elem wmm_param;
	struct country country;
	u32 ap_rf_type;              /* 0->B, 1->G, 2->Mixed, 3->A, 4->11J */

} __packed;

struct hostcmd_cmd_ap_beacon {

	struct hostcmd_header cmd_hdr;
	struct start_cmd start_cmd;

} __packed;

/*
 *     HOSTCMD_CMD_SET_NEW_STN
 */

struct cap_info {

	u16 ess:1;
	u16 ibss:1;
	u16 cf_pollable:1;
	u16 cf_poll_rqst:1;
	u16 privacy:1;
	u16 short_preamble:1;
	u16 pbcc:1;
	u16 chan_agility:1;
	u16 spectrum_mgmt:1;
	u16 qoS:1;
	u16 short_slot_time:1;
	u16 apsd:1;
	u16 rsrvd1:1;
	u16 dsss_ofdm:1;
	u16 block_ack:1;
	u16 rsrvd2:1;

} __packed;

struct add_ht_chnl {

	u8 ext_chnl_offset:2;
	u8 sta_chnl_width:1;
	u8 rifs_mode:1;
	u8 psmp_stas_only:1;
	u8 srvc_intvl_gran:3;

} __packed;

struct add_ht_op_mode {

	u16 op_mode:2;
	u16 non_gf_sta_present:1;
	u16 rrans_burst_limit:1;
	u16 non_ht_sta_present:1;
	u16 rsrv:11;

} __packed;

struct add_ht_stbc {

	u16 bsc_stbc:7;
	u16 dual_stbc_proc:1;
	u16 scd_bcn:1;
	u16 lsig_txop_proc_full_sup:1;
	u16 pco_active:1;
	u16 pco_phase:1;
	u16 rsrv:4;

} __packed;

struct add_ht_info {

	u8 control_chnl;
	struct add_ht_chnl add_chnl;
	struct add_ht_op_mode op_mode;
	struct add_ht_stbc stbc;

} __packed;

struct peer_info {

	u32 legacy_rate_bitmap;
	u8 ht_rates[4];
	struct cap_info cap_info;
	u16 ht_cap_info;
	u8 mac_ht_param_info;
	u8 mrvl_sta;
	struct add_ht_info add_ht_info;
	u32 tx_bf_capabilities;      /* EXBF_SUPPORT */
	u32 vht_max_rx_mcs;
	u32 vht_cap;
	u8 vht_rx_channel_width;     /* 0:20Mhz, 1:40Mhz, 2:80Mhz, 3:160 or 80+80Mhz */

} __packed;

struct hostcmd_cmd_set_new_stn {

	struct hostcmd_header cmd_hdr;
	u16 aid;
	u8 mac_addr[ETH_ALEN];
	u16 stn_id;
	u16 action;
	u16 reserved;
	struct peer_info peer_info;
	/* UAPSD_SUPPORT */
	u8 qos_info;
	u8 is_qos_sta;
	u32 fw_sta_ptr;

} __packed;

/*
 *     HOSTCMD_CMD_SET_APMODE
 */

struct hostcmd_cmd_set_apmode {

	struct hostcmd_header cmd_hdr;
	u8 apmode;

} __packed;

/*
 *     HOSTCMD_CMD_UPDATE_ENCRYPTION
 */

struct hostcmd_cmd_update_encryptoin {

	struct hostcmd_header cmd_hdr;
	/* Action type - see encr_action_type */
	u32 action_type;             /* encr_action_type */
	/* size of the data buffer attached. */
	u32 data_length;
	u8 mac_addr[ETH_ALEN];
	u8 action_data[1];

} __packed;

struct wep_type_key {

	/* WEP key material (max 128bit) */
	u8 key_material[MAX_ENCR_KEY_LENGTH];

} __packed;

struct encr_tkip_seqcnt {

	u16 low;
	u32 high;

} __packed;

struct tkip_type_key {

	/* TKIP Key material. Key type (group or pairwise key) is determined by flags */
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

	struct wep_type_key	wep_key;
	struct tkip_type_key tkip_key;
	struct aes_type_key	aes_key;

} __packed;

struct key_param_set {

	/* Total length of this structure (Key is variable size array) */
	u16 length;
	/* Key type - WEP, TKIP or AES-CCMP. */
	/* See definitions above */
	u16 key_type_id;
	/* key flags (ENCR_KEY_FLAG_XXX_ */
	u32 key_info;
	/* For WEP only - actual key index */
	u32 key_index;
	/* Size of the key */
	u16 key_len;
	/* Key material (variable size array) */
	union mwl_key_type key;
	u8 mac_addr[ETH_ALEN];

} __packed;

struct hostcmd_cmd_set_key {

	struct hostcmd_header cmd_hdr;
	/* Action type - see encr_action_type */
	u32 action_type;             /* encr_action_type */
	/* size of the data buffer attached. */
	u32 data_length;
	/* data buffer - maps to one KEY_PARAM_SET structure */
	struct key_param_set key_param;

} __packed;

/*
 *     HOSTCMD_CMD_BASTREAM
 */

struct ba_stream_flags {

	u32	ba_type:1;
	u32 ba_direction:3;
	u32 reserved:24;

} __packed;

struct ba_context {

	u32 context;

} __packed;

/* parameters for block ack creation */
struct create_ba_params {
	/* BA Creation flags - see above */
	struct ba_stream_flags flags;
	/* idle threshold */
	u32	idle_thrs;
	/* block ack transmit threshold (after how many pkts should we send BAR?) */
	u32	bar_thrs;
	/* receiver window size */
	u32 window_size;
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
	/* this context pointer will be passed to firmware for all future commands. */
	struct ba_context fw_ba_context;
	u8 reset_seq_no;             /** 0 or 1**/
	u16 current_seq;
	u8 sta_src_mac_addr[ETH_ALEN]; /* This is for virtual station in Sta proxy mode for V6FW */

} __packed;

/* new transmit sequence number information */
struct ba_update_seq_num {

	/* BA flags - see above */
	struct ba_stream_flags flags;
	/* returned by firmware in the create ba stream response */
	struct ba_context fw_ba_context;
	/* new sequence number for this block ack stream */
	u16 ba_seq_num;

} __packed;

struct ba_stream_context {

	/* BA Stream flags */
	struct ba_stream_flags flags;
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
	u32 action_type;
	union ba_info ba_info;

} __packed;

/*
 *     HOSTCMD_CMD_DWDS_ENABLE
 */

struct hostcmd_cmd_dwds_enable {

	struct hostcmd_header cmd_hdr;
	u32 enable;                  /* 0 -- Disable. or 1 -- Enable. */

} __packed;

/*
 *     HOSTCMD_CMD_FW_FLUSH_TIMER
 */

struct hostcmd_cmd_fw_flush_timer {

	struct hostcmd_header cmd_hdr;
	u32 value;                   /* 0 -- Disable. > 0 -- holds time value in usecs. */

} __packed;

/*
 *     HOSTCMD_CMD_SET_CDD
 */

struct hostcmd_cmd_set_cdd {

	struct hostcmd_header cmd_hdr;
	u32 enable;

} __packed;

/* PRIVATE FUNCTION DECLARATION
*/

static bool mwl_fwcmd_chk_adapter(struct mwl_priv *priv);
static int mwl_fwcmd_exec_cmd(struct mwl_priv *priv, unsigned short cmd);
static void mwl_fwcmd_send_cmd(struct mwl_priv *priv);
static int mwl_fwcmd_wait_complete(struct mwl_priv *priv, unsigned short cmd);

static int mwl_fwcmd_802_11_radio_control(struct mwl_priv *priv, bool enable, bool force);
static int mwl_fwcmd_get_tx_powers(struct mwl_priv *priv, u16 *powlist, u16 ch,
				   u16 band, u16 width, u16 sub_ch);
static int mwl_fwcmd_set_tx_powers(struct mwl_priv *priv, u16 txpow[], u8 action,
				   u16 ch, u16 band, u16 width, u16 sub_ch);
static u8 mwl_fwcmd_get_80m_pri_chnl_offset(u8 channel);
static void mwl_fwcmd_parse_beacon(struct mwl_priv *priv,
				   struct mwl_vif *vif, u8 *beacon, int len);
static int mwl_fwcmd_set_ies(struct mwl_priv *priv, struct mwl_vif *mwl_vif);
static int mwl_fwcmd_set_ap_beacon(struct mwl_priv *priv, struct mwl_vif *mwl_vif,
				   struct ieee80211_bss_conf *bss_conf);
static int mwl_fwcmd_encryption_set_cmd_info(struct hostcmd_cmd_set_key *cmd,
					     u8 *addr, struct ieee80211_key_conf *key);

#ifdef MWL_DEBUG
static char *mwl_fwcmd_get_cmd_string(unsigned short cmd);
#endif

/* PUBLIC FUNCTION DEFINITION
*/

void mwl_fwcmd_reset(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	if (mwl_fwcmd_chk_adapter(priv)) {

		writel(ISR_RESET, priv->iobase1 + MACREG_REG_H2A_INTERRUPT_EVENTS);
	}

	WLDBG_EXIT(DBG_LEVEL_2);
}

void mwl_fwcmd_int_enable(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	if (mwl_fwcmd_chk_adapter(priv)) {

		writel(0x00, priv->iobase1 + MACREG_REG_A2H_INTERRUPT_MASK);

		writel((MACREG_A2HRIC_BIT_MASK),
		       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_MASK);
	}

	WLDBG_EXIT(DBG_LEVEL_2);
}

void mwl_fwcmd_int_disable(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	if (mwl_fwcmd_chk_adapter(priv)) {

		writel(0x00, priv->iobase1 + MACREG_REG_A2H_INTERRUPT_MASK);
	}

	WLDBG_EXIT(DBG_LEVEL_2);
}

int mwl_fwcmd_get_hw_specs(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_get_hw_spec *pcmd;
	unsigned long flags;
	int i;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_get_hw_spec *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	WLDBG_PRINT("pcmd = %x", (unsigned int)pcmd);
	memset(pcmd, 0x00, sizeof(*pcmd));
	memset(&pcmd->permanent_addr[0], 0xff, ETH_ALEN);
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_GET_HW_SPEC);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->fw_awake_cookie = ENDIAN_SWAP32(priv->pphys_cmd_buf + 2048);

	WLDBG_DUMP_DATA(DBG_LEVEL_2, (void *)pcmd, sizeof(*pcmd));

	while (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_GET_HW_SPEC)) {

		WLDBG_PRINT("failed execution");
		WL_MSEC_SLEEP(1000);
		WLDBG_PRINT("repeat command = %x", (unsigned int)pcmd);
	}

	memcpy(&priv->hw_data.mac_addr[0], pcmd->permanent_addr, ETH_ALEN);
	priv->desc_data[0].wcb_base = ENDIAN_SWAP32(pcmd->wcb_base0) & 0x0000ffff;
#if SYSADPT_NUM_OF_DESC_DATA > 3
	for (i = 1; i < SYSADPT_TOTAL_TX_QUEUES; i++)
		priv->desc_data[i].wcb_base = ENDIAN_SWAP32(pcmd->wcb_base[i - 1]) & 0x0000ffff;
#endif
	priv->desc_data[0].rx_desc_read = ENDIAN_SWAP32(pcmd->rxpd_rd_ptr) & 0x0000ffff;
	priv->desc_data[0].rx_desc_write = ENDIAN_SWAP32(pcmd->rxpd_wr_ptr) & 0x0000ffff;
	priv->hw_data.region_code = ENDIAN_SWAP16(pcmd->region_code) & 0x00ff;
	priv->hw_data.fw_release_num = ENDIAN_SWAP32(pcmd->fw_release_num);
	priv->hw_data.max_num_tx_desc = ENDIAN_SWAP16(pcmd->num_wcb);
	priv->hw_data.max_num_mc_addr = ENDIAN_SWAP16(pcmd->num_mcast_addr);
	priv->hw_data.num_antennas = ENDIAN_SWAP16(pcmd->num_antenna);
	priv->hw_data.hw_version = pcmd->version;
	priv->hw_data.host_interface = pcmd->host_if;

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);

	WLDBG_EXIT_INFO(DBG_LEVEL_2,
			"region code is %i (0x%x), HW version is %i (0x%x)",
		priv->hw_data.region_code, priv->hw_data.region_code,
		priv->hw_data.hw_version, priv->hw_data.hw_version);

	return 0;
}

int mwl_fwcmd_set_hw_specs(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_hw_spec *pcmd;
	unsigned long flags;
	int i;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	/* Info for debugging
	*/
    WLDBG_PRINT("%s ...", __FUNCTION__);
    WLDBG_PRINT("  -->pPhysTxRing[0] = %x", priv->desc_data[0].pphys_tx_ring);
    WLDBG_PRINT("  -->pPhysTxRing[1] = %x", priv->desc_data[1].pphys_tx_ring);
    WLDBG_PRINT("  -->pPhysTxRing[2] = %x", priv->desc_data[2].pphys_tx_ring);
    WLDBG_PRINT("  -->pPhysTxRing[3] = %x", priv->desc_data[3].pphys_tx_ring);
    WLDBG_PRINT("  -->pPhysRxRing    = %x", priv->desc_data[0].pphys_rx_ring);
    WLDBG_PRINT("  -->numtxq %d wcbperq %d totalrxwcb %d",
		SYSADPT_NUM_OF_DESC_DATA, SYSADPT_MAX_NUM_TX_DESC, SYSADPT_MAX_NUM_RX_DESC);

	pcmd = (struct hostcmd_cmd_set_hw_spec *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_SET_HW_SPEC);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->wcb_base[0] = ENDIAN_SWAP32(priv->desc_data[0].pphys_tx_ring);
#if SYSADPT_NUM_OF_DESC_DATA > 3
	for (i = 1; i < SYSADPT_TOTAL_TX_QUEUES; i++)
		pcmd->wcb_base[i] = ENDIAN_SWAP32(priv->desc_data[i].pphys_tx_ring);
#endif
	pcmd->tx_wcb_num_per_queue = ENDIAN_SWAP32(SYSADPT_MAX_NUM_TX_DESC);
	pcmd->num_tx_queues = ENDIAN_SWAP32(SYSADPT_NUM_OF_DESC_DATA);
	pcmd->total_rx_wcb = ENDIAN_SWAP32(SYSADPT_MAX_NUM_RX_DESC);
	pcmd->rxpd_wr_ptr = ENDIAN_SWAP32(priv->desc_data[0].pphys_rx_ring);
	pcmd->disablembss = 0;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_HW_SPEC)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_get_stat(struct ieee80211_hw *hw,
		       struct ieee80211_low_level_stats *stats)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_802_11_get_stat *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_802_11_get_stat *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_802_11_GET_STAT);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_GET_STAT)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	stats->dot11ACKFailureCount =
		ENDIAN_SWAP32(pcmd->ack_failures);
	stats->dot11RTSFailureCount =
		ENDIAN_SWAP32(pcmd->rts_failures);
	stats->dot11FCSErrorCount =
		ENDIAN_SWAP32(pcmd->rx_fcs_errors);
	stats->dot11RTSSuccessCount =
		ENDIAN_SWAP32(pcmd->rts_successes);

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_radio_enable(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;
	int rc;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	rc = mwl_fwcmd_802_11_radio_control(priv, true, false);

	WLDBG_EXIT(DBG_LEVEL_2);

	return rc;
}

int mwl_fwcmd_radio_disable(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;
	int rc;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	rc = mwl_fwcmd_802_11_radio_control(priv, false, false);

	WLDBG_EXIT(DBG_LEVEL_2);

	return rc;
}

int mwl_fwcmd_set_radio_preamble(struct ieee80211_hw *hw, bool short_preamble)
{
	struct mwl_priv *priv;
	int rc;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	priv->radio_short_preamble = short_preamble;
	rc = mwl_fwcmd_802_11_radio_control(priv, true, true);

	WLDBG_EXIT(DBG_LEVEL_2);

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

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

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
		reduce_val = 0xff; /* larger than case 3,  pCmd->MaxPowerLevel is min */
		break;
	}

	if (channel->band == IEEE80211_BAND_2GHZ)
		band = FREQ_BAND_2DOT4GHZ;
	else if (channel->band == IEEE80211_BAND_5GHZ)
		band = FREQ_BAND_5GHZ;

	if ((conf->chandef.width == NL80211_CHAN_WIDTH_20_NOHT) ||
	    (conf->chandef.width == NL80211_CHAN_WIDTH_20)) {

		width = CH_20_MHz_WIDTH;
		sub_ch = NO_EXT_CHANNEL;

	} else if (conf->chandef.width == NL80211_CHAN_WIDTH_40) {

		width = CH_40_MHz_WIDTH;

		if (conf->chandef.center_freq1 > channel->center_freq)
			sub_ch = EXT_CH_ABOVE_CTRL_CH;
		else
			sub_ch = EXT_CH_BELOW_CTRL_CH;

	} else if (conf->chandef.width == NL80211_CHAN_WIDTH_80) {

		width = CH_80_MHz_WIDTH;

		if (conf->chandef.center_freq1 > channel->center_freq)
			sub_ch = EXT_CH_ABOVE_CTRL_CH;
		else
			sub_ch = EXT_CH_BELOW_CTRL_CH;
	}

	if ((priv->powinited & 2) == 0) {

		mwl_fwcmd_get_tx_powers(priv, priv->max_tx_pow,
					channel->hw_value, band, width, sub_ch);

		priv->powinited |= 2;
	}

	if ((priv->powinited & 1) == 0) {

		mwl_fwcmd_get_tx_powers(priv, priv->target_powers,
					channel->hw_value, band, width, sub_ch);

		priv->powinited |= 1;
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

	WLDBG_EXIT_INFO(DBG_LEVEL_2, "return code: %d", rc);

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

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

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
		reduce_val = 0xff; /* larger than case 3,  pCmd->MaxPowerLevel is min */
		break;
	}

	if (channel->band == IEEE80211_BAND_2GHZ)
		band = FREQ_BAND_2DOT4GHZ;
	else if (channel->band == IEEE80211_BAND_5GHZ)
		band = FREQ_BAND_5GHZ;

	if ((conf->chandef.width == NL80211_CHAN_WIDTH_20_NOHT) ||
	    (conf->chandef.width == NL80211_CHAN_WIDTH_20)) {

		width = CH_20_MHz_WIDTH;
		sub_ch = NO_EXT_CHANNEL;

	} else if (conf->chandef.width == NL80211_CHAN_WIDTH_40) {

		width = CH_40_MHz_WIDTH;

		if (conf->chandef.center_freq1 > channel->center_freq)
			sub_ch = EXT_CH_ABOVE_CTRL_CH;
		else
			sub_ch = EXT_CH_BELOW_CTRL_CH;

	} else if (conf->chandef.width == NL80211_CHAN_WIDTH_80) {

		width = CH_80_MHz_WIDTH;

		if (conf->chandef.center_freq1 > channel->center_freq)
			sub_ch = EXT_CH_ABOVE_CTRL_CH;
		else
			sub_ch = EXT_CH_BELOW_CTRL_CH;
	}

	/* search tx power table if exist
	*/
	for (index = 0; index < SYSADPT_MAX_NUM_CHANNELS; index++) {

		/* do nothing if table is not loaded
		*/
		if (priv->tx_pwr_tbl[index].channel == 0)
			break;

		if (priv->tx_pwr_tbl[index].channel == channel->hw_value) {

			priv->cdd = priv->tx_pwr_tbl[index].cdd;
			priv->txantenna2 = priv->tx_pwr_tbl[index].txantenna2;

			if (priv->tx_pwr_tbl[index].setcap)
				priv->powinited = 0x01;
			else
				priv->powinited = 0x02;

			for (i = 0; i < SYSADPT_TX_POWER_LEVEL_TOTAL; i++) {

				if (priv->tx_pwr_tbl[index].setcap)
					priv->max_tx_pow[i] = priv->tx_pwr_tbl[index].tx_power[i];
				else
					priv->target_powers[i] = priv->tx_pwr_tbl[index].tx_power[i];
			}

			found = 1;
			break;
		}
	}

	if ((priv->powinited & 2) == 0) {

		mwl_fwcmd_get_tx_powers(priv, priv->max_tx_pow,
					channel->hw_value, band, width, sub_ch);

		priv->powinited |= 2;
	}

	if ((priv->powinited & 1) == 0) {

		mwl_fwcmd_get_tx_powers(priv, priv->target_powers,
					channel->hw_value, band, width, sub_ch);

		priv->powinited |= 1;
	}

	for (i = 0; i < SYSADPT_TX_POWER_LEVEL_TOTAL; i++) {

		if (found) {

			if ((priv->tx_pwr_tbl[index].setcap)
				&& (priv->tx_pwr_tbl[index].tx_power[i] > priv->max_tx_pow[i]))
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

	WLDBG_EXIT_INFO(DBG_LEVEL_2, "return code: %d", rc);

	return rc;
}

int mwl_fwcmd_rf_antenna(struct ieee80211_hw *hw, int dir, int antenna)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_802_11_rf_antenna *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_802_11_rf_antenna *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_802_11_RF_ANTENNA);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));

	pcmd->action = ENDIAN_SWAP16(dir);

	if (dir == WL_ANTENNATYPE_RX) {

		u8 rx_antenna = 4; /* if auto, set 4 rx antennas in SC2 */

		if (antenna != 0)
			pcmd->antenna_mode = ENDIAN_SWAP16(antenna);
		else
			pcmd->antenna_mode = ENDIAN_SWAP16(rx_antenna);

	} else {

		u8 tx_antenna = 0xf; /* if auto, set 4 tx antennas in SC2 */

		if (antenna != 0)
			pcmd->antenna_mode = ENDIAN_SWAP16(antenna);
		else
			pcmd->antenna_mode = ENDIAN_SWAP16(tx_antenna);

	}

	WLDBG_DUMP_DATA(DBG_LEVEL_2, (void *)pcmd, sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_RF_ANTENNA)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_broadcast_ssid_enable(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif, bool enable)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_broadcast_ssid_enable *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	BUG_ON(!vif);
	mwl_vif = MWL_VIF(vif);
	BUG_ON(!mwl_vif);

	pcmd = (struct hostcmd_cmd_broadcast_ssid_enable *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_BROADCAST_SSID_ENABLE);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	pcmd->enable = ENDIAN_SWAP32(enable);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_BROADCAST_SSID_ENABLE)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_set_rf_channel(struct ieee80211_hw *hw,
			     struct ieee80211_conf *conf)
{
	struct ieee80211_channel *channel = conf->chandef.chan;
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_rf_channel *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_set_rf_channel *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_SET_RF_CHANNEL);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->action = ENDIAN_SWAP16(WL_SET);
	pcmd->curr_chnl = channel->hw_value;

	if (channel->band == IEEE80211_BAND_2GHZ)
		pcmd->chnl_flags.freq_band = FREQ_BAND_2DOT4GHZ;
	else if (channel->band == IEEE80211_BAND_5GHZ)
		pcmd->chnl_flags.freq_band = FREQ_BAND_5GHZ;

	if ((conf->chandef.width == NL80211_CHAN_WIDTH_20_NOHT) ||
	    (conf->chandef.width == NL80211_CHAN_WIDTH_20)) {

		pcmd->chnl_flags.chnl_width = CH_20_MHz_WIDTH;
		pcmd->chnl_flags.act_primary = ACT_PRIMARY_CHAN_0;

	} else if (conf->chandef.width == NL80211_CHAN_WIDTH_40) {

		pcmd->chnl_flags.chnl_width = CH_40_MHz_WIDTH;

		if (conf->chandef.center_freq1 > channel->center_freq)
			pcmd->chnl_flags.act_primary = ACT_PRIMARY_CHAN_0;
		else
			pcmd->chnl_flags.act_primary = ACT_PRIMARY_CHAN_1;

	} else if (conf->chandef.width == NL80211_CHAN_WIDTH_80) {

		pcmd->chnl_flags.chnl_width = CH_80_MHz_WIDTH;
		pcmd->chnl_flags.act_primary =
			mwl_fwcmd_get_80m_pri_chnl_offset(pcmd->curr_chnl);

	}

	WLDBG_DUMP_DATA(DBG_LEVEL_2, (void *)pcmd, sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_RF_CHANNEL)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_set_rts_threshold(struct ieee80211_hw *hw, int threshold)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_802_11_rts_thsd *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_802_11_rts_thsd *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_802_11_RTS_THSD);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->action  = ENDIAN_SWAP16(WL_SET);
	pcmd->threshold = ENDIAN_SWAP16(threshold);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_RTS_THSD)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_set_edca_params(struct ieee80211_hw *hw, u8 index,
			      u16 cw_min, u16 cw_max, u8 aifs, u16 txop)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_edca_params *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_set_edca_params *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_SET_EDCA_PARAMS);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));

	pcmd->action = ENDIAN_SWAP16(0xffff);;
	pcmd->txop = ENDIAN_SWAP16(txop);
	pcmd->cw_max = ENDIAN_SWAP32(ilog2(cw_max + 1));
	pcmd->cw_min = ENDIAN_SWAP32(ilog2(cw_min + 1));
	pcmd->aifsn = aifs;
	pcmd->txq_num = index;

	/* The array index defined in qos.h has a reversed bk and be.
	 * The HW queue was not used this way; the qos code needs to be changed or
	 * checked
	 */
	if (index == 0)
		pcmd->txq_num = 1;
	else if (index == 1)
		pcmd->txq_num = 0;

	WLDBG_DUMP_DATA(DBG_LEVEL_2, (void *)pcmd, sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_EDCA_PARAMS)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_set_wmm_mode(struct ieee80211_hw *hw, bool enable)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_wmm_mode *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_set_wmm_mode *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_SET_WMM_MODE);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->action = ENDIAN_SWAP16(enable ? WL_ENABLE : WL_DISABLE);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_WMM_MODE)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_use_fixed_rate(struct ieee80211_hw *hw, int mcast, int mgmt)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_fixed_rate *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_set_fixed_rate *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_SET_FIXED_RATE);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));

	pcmd->action = ENDIAN_SWAP32(HOSTCMD_ACT_NOT_USE_FIXED_RATE);
	pcmd->multicast_rate = mcast;
	pcmd->management_rate = mgmt;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_FIXED_RATE)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_set_rate_adapt_mode(struct ieee80211_hw *hw, u16 mode)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_rate_adapt_mode *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_set_rate_adapt_mode *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_SET_RATE_ADAPT_MODE);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->action = ENDIAN_SWAP16(WL_SET);
	pcmd->rate_adapt_mode = ENDIAN_SWAP16(mode);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_RATE_ADAPT_MODE)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_get_watchdog_bitmap(struct ieee80211_hw *hw, u8 *bitmap)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_get_watchdog_bitmap *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_get_watchdog_bitmap *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_GET_WATCHDOG_BITMAP);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_GET_WATCHDOG_BITMAP)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	*bitmap = pcmd->watchdog_bitmap;

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_bss_start(struct ieee80211_hw *hw,
			struct ieee80211_vif *vif, bool enable)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_bss_start *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	BUG_ON(!vif);
	mwl_vif = MWL_VIF(vif);
	BUG_ON(!mwl_vif);

	if (enable && (priv->running_bsses & (1 << mwl_vif->macid)))
		return 0;

	if (!enable && !(priv->running_bsses & (1 << mwl_vif->macid)))
		return 0;

	pcmd = (struct hostcmd_cmd_bss_start *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_BSS_START);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	if (enable) {

		pcmd->enable = ENDIAN_SWAP32(WL_ENABLE);

	} else {

		if (mwl_vif->macid == 0)
			pcmd->enable = ENDIAN_SWAP32(WL_DISABLE);
		else
			pcmd->enable = ENDIAN_SWAP32(WL_DISABLE_VMAC);

	}

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_BSS_START)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	if (enable)
		priv->running_bsses |= (1 << mwl_vif->macid);
	else
		priv->running_bsses &= ~(1 << mwl_vif->macid);

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_set_beacon(struct ieee80211_hw *hw,
			 struct ieee80211_vif *vif, u8 *beacon, int len)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	BUG_ON(!vif);
	mwl_vif = MWL_VIF(vif);
	BUG_ON(!mwl_vif);

	mwl_fwcmd_parse_beacon(priv, mwl_vif, beacon, len);

	if (mwl_fwcmd_set_ies(priv, mwl_vif))
		goto err;

	if (mwl_fwcmd_set_ap_beacon(priv, mwl_vif, &vif->bss_conf))
		goto err;

	mwl_vif->beacon_info.valid = false;

	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;

err:

	mwl_vif->beacon_info.valid = false;

	WLDBG_EXIT_INFO(DBG_LEVEL_2, "set beacon failed");

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

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	BUG_ON(!vif);
	mwl_vif = MWL_VIF(vif);
	BUG_ON(!mwl_vif);

	BUG_ON(!sta);

	pcmd = (struct hostcmd_cmd_set_new_stn *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_SET_NEW_STN);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action = ENDIAN_SWAP16(HOSTCMD_ACT_STA_ACTION_ADD);
	pcmd->aid = ENDIAN_SWAP16(sta->aid);
	memcpy(pcmd->mac_addr, sta->addr, ETH_ALEN);
	pcmd->stn_id = ENDIAN_SWAP16(sta->aid);

	if (hw->conf.chandef.chan->band == IEEE80211_BAND_2GHZ)
		rates = sta->supp_rates[IEEE80211_BAND_2GHZ];
	else
		rates = sta->supp_rates[IEEE80211_BAND_5GHZ] << 5;
	pcmd->peer_info.legacy_rate_bitmap = ENDIAN_SWAP32(rates);

	if (sta->ht_cap.ht_supported) {

		pcmd->peer_info.ht_rates[0] = sta->ht_cap.mcs.rx_mask[0];
		pcmd->peer_info.ht_rates[1] = sta->ht_cap.mcs.rx_mask[1];
		pcmd->peer_info.ht_rates[2] = sta->ht_cap.mcs.rx_mask[2];
		pcmd->peer_info.ht_rates[3] = sta->ht_cap.mcs.rx_mask[3];
		pcmd->peer_info.ht_cap_info = ENDIAN_SWAP16(sta->ht_cap.cap);
		pcmd->peer_info.mac_ht_param_info = (sta->ht_cap.ampdu_factor & 3) |
			((sta->ht_cap.ampdu_density & 7) << 2);
	}

	if (sta->vht_cap.vht_supported) {

		pcmd->peer_info.vht_max_rx_mcs = ENDIAN_SWAP32(*((u32 *)&sta->vht_cap.vht_mcs.rx_mcs_map));
		pcmd->peer_info.vht_cap = ENDIAN_SWAP32(sta->vht_cap.cap);
		pcmd->peer_info.vht_rx_channel_width = sta->bandwidth;
	}

	pcmd->is_qos_sta = sta->wme;
	pcmd->qos_info = ((sta->uapsd_queues << 4) | (sta->max_sp << 1));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_NEW_STN)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_set_new_stn_add_self(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_new_stn *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	BUG_ON(!vif);
	mwl_vif = MWL_VIF(vif);
	BUG_ON(!mwl_vif);

	pcmd = (struct hostcmd_cmd_set_new_stn *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_SET_NEW_STN);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action = ENDIAN_SWAP16(HOSTCMD_ACT_STA_ACTION_ADD);
	memcpy(pcmd->mac_addr, vif->addr, ETH_ALEN);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_NEW_STN)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_set_new_stn_del(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif, u8 *addr)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_new_stn *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	BUG_ON(!vif);
	mwl_vif = MWL_VIF(vif);
	BUG_ON(!mwl_vif);

	pcmd = (struct hostcmd_cmd_set_new_stn *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_SET_NEW_STN);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action = ENDIAN_SWAP16(HOSTCMD_ACT_STA_ACTION_REMOVE);
	memcpy(pcmd->mac_addr, addr, ETH_ALEN);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_NEW_STN)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_set_apmode(struct ieee80211_hw *hw, u8 apmode)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_apmode *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_set_apmode *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_SET_APMODE);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->apmode = apmode;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_APMODE)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_update_encryption_enable(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif, u8 *addr, u8 encr_type)
{
	struct mwl_priv *priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_update_encryptoin *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	BUG_ON(!vif);
	mwl_vif = MWL_VIF(vif);
	BUG_ON(!mwl_vif);

	pcmd = (struct hostcmd_cmd_update_encryptoin *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_UPDATE_ENCRYPTION);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action_type = ENDIAN_SWAP32(ENCR_ACTION_ENABLE_HW_ENCR);
	memcpy(pcmd->mac_addr, addr, ETH_ALEN);
	pcmd->action_data[0] = encr_type;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_UPDATE_ENCRYPTION)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

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

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	BUG_ON(!vif);
	mwl_vif = MWL_VIF(vif);
	BUG_ON(!mwl_vif);

	pcmd = (struct hostcmd_cmd_set_key *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_UPDATE_ENCRYPTION);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	rc = mwl_fwcmd_encryption_set_cmd_info(pcmd, addr, key);
	if (rc) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "encryption not support");
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

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "encryption not support");
		return -ENOTSUPP;
	}

	memcpy((void *)&pcmd->key_param.key, key->key, keymlen);
	pcmd->action_type = ENDIAN_SWAP32(action);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_UPDATE_ENCRYPTION)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

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

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	BUG_ON(!vif);
	mwl_vif = MWL_VIF(vif);
	BUG_ON(!mwl_vif);

	pcmd = (struct hostcmd_cmd_set_key *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_UPDATE_ENCRYPTION);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	rc = mwl_fwcmd_encryption_set_cmd_info(pcmd, addr, key);
	if (rc) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "encryption not support");
		return rc;
	}

	pcmd->action_type = ENDIAN_SWAP32(ENCR_ACTION_TYPE_REMOVE_KEY);

	if (key->cipher == WLAN_CIPHER_SUITE_WEP40 ||
	    key->cipher == WLAN_CIPHER_SUITE_WEP104)
		mwl_vif->wep_key_conf[key->keyidx].enabled = 0;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_UPDATE_ENCRYPTION)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

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

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	BUG_ON(!stream);

	BUG_ON(!vif);
	mwl_vif = MWL_VIF(vif);
	BUG_ON(!mwl_vif);

	pcmd = (struct hostcmd_cmd_bastream *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_BASTREAM);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	pcmd->cmd_hdr.result = ENDIAN_SWAP16(0xffff);

	pcmd->action_type = ENDIAN_SWAP32(BA_CHECK_STREAM);
	memcpy(&pcmd->ba_info.create_params.peer_mac_addr[0],
	       stream->sta->addr, ETH_ALEN);
	pcmd->ba_info.create_params.tid = stream->tid;
	pcmd->ba_info.create_params.flags.ba_type =
		BASTREAM_FLAG_IMMEDIATE_TYPE;
	pcmd->ba_info.create_params.flags.ba_direction =
		BASTREAM_FLAG_DIRECTION_UPSTREAM;
	pcmd->ba_info.create_params.queue_id = stream->idx;

	WLDBG_DUMP_DATA(DBG_LEVEL_2, (void *)pcmd, sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_BASTREAM)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	if (pcmd->cmd_hdr.result != 0) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "result error");
		return -EINVAL;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

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

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	BUG_ON(!stream);

	BUG_ON(!vif);
	mwl_vif = MWL_VIF(vif);
	BUG_ON(!mwl_vif);

	pcmd = (struct hostcmd_cmd_bastream *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_BASTREAM);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	pcmd->cmd_hdr.result = ENDIAN_SWAP16(0xffff);

	pcmd->action_type = ENDIAN_SWAP32(BA_CREATE_STREAM);
	pcmd->ba_info.create_params.bar_thrs = ENDIAN_SWAP32(buf_size);
	pcmd->ba_info.create_params.window_size = ENDIAN_SWAP32(buf_size);
	memcpy(&pcmd->ba_info.create_params.peer_mac_addr[0],
	       stream->sta->addr, ETH_ALEN);
	pcmd->ba_info.create_params.tid = stream->tid;
	pcmd->ba_info.create_params.flags.ba_type =
		BASTREAM_FLAG_IMMEDIATE_TYPE;
	pcmd->ba_info.create_params.flags.ba_direction =
		BASTREAM_FLAG_DIRECTION_UPSTREAM;
	pcmd->ba_info.create_params.queue_id = stream->idx;
	pcmd->ba_info.create_params.param_info =
		(stream->sta->ht_cap.ampdu_factor &
		 IEEE80211_HT_AMPDU_PARM_FACTOR) |
		((stream->sta->ht_cap.ampdu_density << 2) &
		 IEEE80211_HT_AMPDU_PARM_DENSITY);
	pcmd->ba_info.create_params.reset_seq_no = 1;
	pcmd->ba_info.create_params.current_seq = ENDIAN_SWAP16(0);

	WLDBG_DUMP_DATA(DBG_LEVEL_2, (void *)pcmd, sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_BASTREAM)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	if (pcmd->cmd_hdr.result != 0) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "result error");
		return -EINVAL;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_destroy_ba(struct ieee80211_hw *hw,
			 u8 idx)
{

	struct mwl_priv *priv;
	struct hostcmd_cmd_bastream *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_bastream *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_BASTREAM);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));

	pcmd->action_type = ENDIAN_SWAP32(BA_DESTROY_STREAM);
	pcmd->ba_info.destroy_params.flags.ba_type =
		BASTREAM_FLAG_IMMEDIATE_TYPE;
	pcmd->ba_info.destroy_params.flags.ba_direction =
		BASTREAM_FLAG_DIRECTION_UPSTREAM;
	pcmd->ba_info.destroy_params.fw_ba_context.context = ENDIAN_SWAP32(idx);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_BASTREAM)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

/* caller must hold priv->locks.stream_lock when calling the stream functions
*/
struct mwl_ampdu_stream *mwl_fwcmd_add_stream(struct ieee80211_hw *hw,
					      struct ieee80211_sta *sta, u8 tid)
{
	struct mwl_priv *priv;
	struct mwl_ampdu_stream *stream;
	int i;

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	BUG_ON(!sta);

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
	BUG_ON(!hw);
	BUG_ON(!stream);

	/* if the stream has already been started, don't start it again
	*/
	if (stream->state != AMPDU_STREAM_NEW)
		return 0;

	return (ieee80211_start_tx_ba_session(stream->sta, stream->tid, 0));
}

void mwl_fwcmd_remove_stream(struct ieee80211_hw *hw,
			     struct mwl_ampdu_stream *stream)
{
	BUG_ON(!hw);
	BUG_ON(!stream);

	memset(stream, 0, sizeof(*stream));
}

struct mwl_ampdu_stream *mwl_fwcmd_lookup_stream(struct ieee80211_hw *hw,
						 u8 *addr, u8 tid)
{
	struct mwl_priv *priv;
	struct mwl_ampdu_stream *stream;
	int i;

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

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

	BUG_ON(!sta);
	sta_info = MWL_STA(sta);
	BUG_ON(!sta_info);

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

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_dwds_enable *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_DWDS_ENABLE);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->enable = ENDIAN_SWAP32(enable);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_DWDS_ENABLE)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_set_fw_flush_timer(struct ieee80211_hw *hw, u32 value)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_fw_flush_timer *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_fw_flush_timer *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_FW_FLUSH_TIMER);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->value = ENDIAN_SWAP32(value);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_FW_FLUSH_TIMER)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

int mwl_fwcmd_set_cdd(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;
	struct hostcmd_cmd_set_cdd *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_set_cdd *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_SET_CDD);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->enable = ENDIAN_SWAP32(priv->cdd);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_CDD)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

/* PRIVATE FUNCTION DEFINITION
*/

static bool mwl_fwcmd_chk_adapter(struct mwl_priv *priv)
{
	u32 regval;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!priv);

	regval = readl(priv->iobase1 + MACREG_REG_INT_CODE);

	if (regval == 0xffffffff) {

		WLDBG_ERROR(DBG_LEVEL_2, "adapter is not existed");
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "adapter is not existed");

		return false;
	}

	WLDBG_EXIT(DBG_LEVEL_2);

	return true;
}

static int mwl_fwcmd_exec_cmd(struct mwl_priv *priv, unsigned short cmd)
{
	bool busy = false;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!priv);

	if (!mwl_fwcmd_chk_adapter(priv)) {

		WLDBG_ERROR(DBG_LEVEL_2, "no adapter existed");
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "no adapter plugged in");
		priv->in_send_cmd = false;
		return -EIO;
	}

	if (!priv->in_send_cmd) {

		priv->in_send_cmd = true;

		mwl_fwcmd_send_cmd(priv);

		if (mwl_fwcmd_wait_complete(priv, 0x8000 | cmd)) {

			WLDBG_ERROR(DBG_LEVEL_2, "timeout");
			WLDBG_EXIT_INFO(DBG_LEVEL_2, "timeout");
			priv->in_send_cmd = false;
			return -EIO;
		}

	} else {

		WLDBG_WARNING(DBG_LEVEL_2, "previous command is still running");
		busy = true;
	}

	WLDBG_EXIT(DBG_LEVEL_2);
	if (!busy)
		priv->in_send_cmd = false;

	return 0;
}

static void mwl_fwcmd_send_cmd(struct mwl_priv *priv)
{
	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!priv);

	writel(priv->pphys_cmd_buf, priv->iobase1 + MACREG_REG_GEN_PTR);
	writel(MACREG_H2ARIC_BIT_DOOR_BELL,
	       priv->iobase1 + MACREG_REG_H2A_INTERRUPT_EVENTS);

	WLDBG_EXIT(DBG_LEVEL_2);
}

static int mwl_fwcmd_wait_complete(struct mwl_priv *priv, unsigned short cmd)
{
	unsigned int curr_iteration = MAX_WAIT_FW_COMPLETE_ITERATIONS;

	volatile unsigned short int_code = 0;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!priv);

	do {

		int_code = ENDIAN_SWAP16(priv->pcmd_buf[0]);
		WL_MSEC_SLEEP(1);

	} while ((int_code != cmd) && (--curr_iteration));

	if (curr_iteration == 0)
	{
		WLDBG_ERROR(DBG_LEVEL_2, "cmd 0x%04x=%s timed out",
			    cmd, mwl_fwcmd_get_cmd_string(cmd));

		WLDBG_EXIT_INFO(DBG_LEVEL_2, "timeout");

		return -EIO;
	}

	WL_MSEC_SLEEP(3);

	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

static int mwl_fwcmd_802_11_radio_control(struct mwl_priv *priv, bool enable, bool force)
{
	struct hostcmd_cmd_802_11_radio_control *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!priv);

	if (enable == priv->radio_on && !force)
		return 0;

	pcmd = (struct hostcmd_cmd_802_11_radio_control *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_802_11_RADIO_CONTROL);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->action = ENDIAN_SWAP16(WL_SET);
	pcmd->control = ENDIAN_SWAP16(priv->radio_short_preamble ?
		WL_AUTO_PREAMBLE : WL_LONG_PREAMBLE);
	pcmd->radio_on = ENDIAN_SWAP16(enable ? WL_ENABLE : WL_DISABLE);

	WLDBG_DUMP_DATA(DBG_LEVEL_2, (void *)pcmd, sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_RADIO_CONTROL)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	priv->radio_on = enable;

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

static int mwl_fwcmd_get_tx_powers(struct mwl_priv *priv, u16 *powlist, u16 ch,
				   u16 band, u16 width, u16 sub_ch)
{
	struct hostcmd_cmd_802_11_tx_power *pcmd;
	unsigned long flags;
	int i;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_802_11_tx_power *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_802_11_TX_POWER);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->action = ENDIAN_SWAP16(HOSTCMD_ACT_GEN_GET_LIST);
	pcmd->ch = ENDIAN_SWAP16(ch);
	pcmd->bw = ENDIAN_SWAP16(width);
	pcmd->band = ENDIAN_SWAP16(band);
	pcmd->sub_ch = ENDIAN_SWAP16(sub_ch);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_TX_POWER)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	for (i = 0; i < SYSADPT_TX_POWER_LEVEL_TOTAL; i++)
		powlist[i] = (u8)ENDIAN_SWAP16(pcmd->power_level_list[i]);

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;
}

static int mwl_fwcmd_set_tx_powers(struct mwl_priv *priv, u16 txpow[], u8 action,
				   u16 ch, u16 band, u16 width, u16 sub_ch)
{
	struct hostcmd_cmd_802_11_tx_power *pcmd;
	unsigned long flags;
	int i;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!priv);

	pcmd = (struct hostcmd_cmd_802_11_tx_power *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_802_11_TX_POWER);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->action = ENDIAN_SWAP16(action);
	pcmd->ch = ENDIAN_SWAP16(ch);
	pcmd->bw = ENDIAN_SWAP16(width);
	pcmd->band = ENDIAN_SWAP16(band);
	pcmd->sub_ch = ENDIAN_SWAP16(sub_ch);

	for (i = 0; i < SYSADPT_TX_POWER_LEVEL_TOTAL; i++)
		pcmd->power_level_list[i] = ENDIAN_SWAP16(txpow[i]);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_TX_POWER)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

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

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!vif);
	BUG_ON(!beacon);

	mgmt = (struct ieee80211_mgmt *)beacon;

	baselen = (u8 *)mgmt->u.beacon.variable - (u8 *)mgmt;
	if (baselen > len)
		return;

	beacon_info = &vif->beacon_info;
	memset(beacon_info, 0, sizeof(struct beacon_info));
	beacon_info->valid = false;
	beacon_info->ie_ht_ptr = &beacon_info->ie_list_ht[0];
	beacon_info->ie_vht_ptr = &beacon_info->ie_list_vht[0];

	beacon_info->cap_info = mgmt->u.beacon.capab_info;

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
				int idx, basic_idx, oprate_idx;
				u8 rate;

				for (basic_idx = 0; basic_idx < SYSADPT_MAX_DATA_RATES_G; basic_idx++) {

					if (beacon_info->bss_basic_rate_set[basic_idx] == 0)
						break;
				}

				for (oprate_idx = 0; oprate_idx < SYSADPT_MAX_DATA_RATES_G; oprate_idx++) {

					if (beacon_info->op_rate_set[oprate_idx] == 0)
						break;
				}

				for (idx = 0; idx < elen; idx++) {

					rate = pos[idx];

					if ((rate & 0x80) != 0) {

						if (basic_idx < SYSADPT_MAX_DATA_RATES_G)
							beacon_info->bss_basic_rate_set[basic_idx++] = rate & 0x7f;
						else {
							elem_parse_failed = true;
							break;
						}
					}

					if (oprate_idx < SYSADPT_MAX_DATA_RATES_G)
						beacon_info->op_rate_set[oprate_idx++] = rate & 0x7f;
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
			if (beacon_info->ie_ht_len > sizeof(beacon_info->ie_list_ht)) {
				elem_parse_failed = true;
				break;
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
			if (beacon_info->ie_vht_len > sizeof(beacon_info->ie_list_vht)) {
				elem_parse_failed = true;
				break;
			} else {
				*beacon_info->ie_vht_ptr++ = id;
				*beacon_info->ie_vht_ptr++ = elen;
				memcpy(beacon_info->ie_vht_ptr, pos, elen);
				beacon_info->ie_vht_ptr += elen;
			}
			break;

		case WLAN_EID_VENDOR_SPECIFIC:
			if ((pos[0] == 0x00) && (pos[1] == 0x50) && (pos[2] == 0xf2)) {

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

	if (elem_parse_failed == false) {

		beacon_info->ie_ht_ptr = &beacon_info->ie_list_ht[0];
		beacon_info->ie_vht_ptr = &beacon_info->ie_list_vht[0];
		beacon_info->valid = true;

		WLDBG_INFO(DBG_LEVEL_2, "wmm:%d, rsn:%d, rsn48:%d, ht:%d, vht:%d",
			   beacon_info->ie_wmm_len, beacon_info->ie_rsn_len, beacon_info->ie_rsn48_len,
			beacon_info->ie_ht_len, beacon_info->ie_vht_len);

		WLDBG_DUMP_DATA(DBG_LEVEL_2, beacon_info->bss_basic_rate_set,
				SYSADPT_MAX_DATA_RATES_G);

		WLDBG_DUMP_DATA(DBG_LEVEL_2, beacon_info->op_rate_set,
				SYSADPT_MAX_DATA_RATES_G);
	}

	WLDBG_EXIT_INFO(DBG_LEVEL_2, "parse valid:%d", beacon_info->valid);

	return;
}

static int mwl_fwcmd_set_ies(struct mwl_priv *priv, struct mwl_vif *mwl_vif)
{
	struct hostcmd_cmd_set_ies *pcmd;
	unsigned long flags;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!priv);
	BUG_ON(!mwl_vif);

	if (!mwl_vif->beacon_info.valid)
		return -EINVAL;

	if (mwl_vif->beacon_info.ie_ht_len > sizeof(pcmd->ie_list_ht))
		goto einval;

	if (mwl_vif->beacon_info.ie_vht_len > sizeof(pcmd->ie_list_vht))
		goto einval;

	pcmd = (struct hostcmd_cmd_set_ies *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_SET_IES);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action = ENDIAN_SWAP16(HOSTCMD_ACT_GEN_SET);

	pcmd->ie_list_len_ht = mwl_vif->beacon_info.ie_ht_len;
	memcpy(pcmd->ie_list_ht, mwl_vif->beacon_info.ie_ht_ptr,
	       pcmd->ie_list_len_ht);

	pcmd->ie_list_len_vht = mwl_vif->beacon_info.ie_vht_len;
	memcpy(pcmd->ie_list_vht, mwl_vif->beacon_info.ie_vht_ptr,
	       pcmd->ie_list_len_vht);

	WLDBG_DUMP_DATA(DBG_LEVEL_2, (void *)pcmd, sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_IES)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;

einval:

	WLDBG_EXIT_INFO(DBG_LEVEL_2, "length of IE is too long");

	return -EINVAL;
}

static int mwl_fwcmd_set_ap_beacon(struct mwl_priv *priv, struct mwl_vif *mwl_vif,
				   struct ieee80211_bss_conf *bss_conf)
{
	struct hostcmd_cmd_ap_beacon *pcmd;
	unsigned long flags;
	struct ds_params *phy_ds_param_set;

	WLDBG_ENTER(DBG_LEVEL_2);

	BUG_ON(!priv);
	BUG_ON(!mwl_vif);
	BUG_ON(!bss_conf);

	if (!mwl_vif->beacon_info.valid)
		return -EINVAL;

	/* wmm structure of start command is defined less one byte, due to following
	 * field country is not used, add byte one to bypass the check.
	 */
	if (mwl_vif->beacon_info.ie_wmm_len > (sizeof(pcmd->start_cmd.wmm_param) + 1))
		goto ielenerr;

	if (mwl_vif->beacon_info.ie_rsn_len > sizeof(pcmd->start_cmd.rsn_ie))
		goto ielenerr;

	if (mwl_vif->beacon_info.ie_rsn48_len > sizeof(pcmd->start_cmd.rsn48_ie))
		goto ielenerr;

	pcmd = (struct hostcmd_cmd_ap_beacon *)&priv->pcmd_buf[0];

	MWL_SPIN_LOCK(&priv->locks.fwcmd_lock);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_AP_BEACON);
	pcmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	memcpy(pcmd->start_cmd.sta_mac_addr, mwl_vif->bssid, ETH_ALEN);
	memcpy(pcmd->start_cmd.ssid, bss_conf->ssid, bss_conf->ssid_len);
	pcmd->start_cmd.bss_type = 1;
	pcmd->start_cmd.bcn_period  = ENDIAN_SWAP16(bss_conf->beacon_int);
	pcmd->start_cmd.dtim_period = bss_conf->dtim_period; /* 8bit */

	phy_ds_param_set = &pcmd->start_cmd.phy_param_set.ds_param_set;
	phy_ds_param_set->elem_id = WLAN_EID_DS_PARAMS;
	phy_ds_param_set->len = sizeof(phy_ds_param_set->current_chnl);
	phy_ds_param_set->current_chnl = bss_conf->chandef.chan->hw_value;

	pcmd->start_cmd.probe_delay = ENDIAN_SWAP16(10);
	pcmd->start_cmd.cap_info = ENDIAN_SWAP16(mwl_vif->beacon_info.cap_info);

	memcpy(&pcmd->start_cmd.wmm_param, mwl_vif->beacon_info.ie_wmm_ptr,
	       mwl_vif->beacon_info.ie_wmm_len);

	memcpy(&pcmd->start_cmd.rsn_ie, mwl_vif->beacon_info.ie_rsn_ptr,
	       mwl_vif->beacon_info.ie_rsn_len);

	memcpy(&pcmd->start_cmd.rsn48_ie, mwl_vif->beacon_info.ie_rsn48_ptr,
	       mwl_vif->beacon_info.ie_rsn48_len);

	memcpy(pcmd->start_cmd.bss_basic_rate_set, mwl_vif->beacon_info.bss_basic_rate_set,
	       SYSADPT_MAX_DATA_RATES_G);

	memcpy(pcmd->start_cmd.op_rate_set, mwl_vif->beacon_info.op_rate_set,
	       SYSADPT_MAX_DATA_RATES_G);

	WLDBG_DUMP_DATA(DBG_LEVEL_2, (void *)pcmd, sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_AP_BEACON)) {

		MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
		WLDBG_EXIT_INFO(DBG_LEVEL_2, "failed execution");
		return -EIO;
	}

	MWL_SPIN_UNLOCK(&priv->locks.fwcmd_lock);
	WLDBG_EXIT(DBG_LEVEL_2);

	return 0;

ielenerr:

	WLDBG_EXIT_INFO(DBG_LEVEL_2, "length of IE is too long");

	return -EINVAL;
}

static int mwl_fwcmd_encryption_set_cmd_info(struct hostcmd_cmd_set_key *cmd,
					     u8 *addr, struct ieee80211_key_conf *key)
{
	cmd->cmd_hdr.cmd = ENDIAN_SWAP16(HOSTCMD_CMD_UPDATE_ENCRYPTION);
	cmd->cmd_hdr.len = ENDIAN_SWAP16(sizeof(*cmd));
	cmd->key_param.length = ENDIAN_SWAP16(sizeof(*cmd) -
		offsetof(struct hostcmd_cmd_set_key, key_param));
	cmd->key_param.key_index = ENDIAN_SWAP32(key->keyidx);
	cmd->key_param.key_len = ENDIAN_SWAP16(key->keylen);
	memcpy(cmd->key_param.mac_addr, addr, ETH_ALEN);

	switch (key->cipher) {

	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:

		cmd->key_param.key_type_id = ENDIAN_SWAP16(KEY_TYPE_ID_WEP);
		if (key->keyidx == 0)
			cmd->key_param.key_info = ENDIAN_SWAP32(ENCR_KEY_FLAG_WEP_TXKEY);

		break;

	case WLAN_CIPHER_SUITE_TKIP:

		cmd->key_param.key_type_id = ENDIAN_SWAP16(KEY_TYPE_ID_TKIP);
		cmd->key_param.key_info = (key->flags & IEEE80211_KEY_FLAG_PAIRWISE)
			? ENDIAN_SWAP32(ENCR_KEY_FLAG_PAIRWISE)
			: ENDIAN_SWAP32(ENCR_KEY_FLAG_TXGROUPKEY);
		cmd->key_param.key_info |= ENDIAN_SWAP32(ENCR_KEY_FLAG_MICKEY_VALID
			| ENCR_KEY_FLAG_TSC_VALID);
		break;

	case WLAN_CIPHER_SUITE_CCMP:

		cmd->key_param.key_type_id = ENDIAN_SWAP16(KEY_TYPE_ID_AES);
		cmd->key_param.key_info = (key->flags & IEEE80211_KEY_FLAG_PAIRWISE)
			? ENDIAN_SWAP32(ENCR_KEY_FLAG_PAIRWISE)
			: ENDIAN_SWAP32(ENCR_KEY_FLAG_TXGROUPKEY);
		break;

	default:

		return -ENOTSUPP;
	}

	return 0;
}

#ifdef MWL_DEBUG
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
        { HOSTCMD_CMD_802_11_RTS_THSD, "80211RtsThreshold" },
        { HOSTCMD_CMD_SET_EDCA_PARAMS, "SetEDCAParams" },
		{ HOSTCMD_CMD_SET_WMM_MODE, "SetWMMMode" },
		{ HOSTCMD_CMD_SET_FIXED_RATE, "SetFixedRate" },
		{ HOSTCMD_CMD_SET_IES, "SetInformationElements" },
		{ HOSTCMD_CMD_SET_RATE_ADAPT_MODE, "SetRateAdaptationMode" },
		{ HOSTCMD_CMD_GET_WATCHDOG_BITMAP, "GetWatchdogBitMap" },
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

	WLDBG_ENTER(DBG_LEVEL_2);

	max_entries = sizeof(cmds) / sizeof(cmds[0]);

	for (curr_cmd = 0; curr_cmd < max_entries; curr_cmd++) {

		if ((cmd & 0x7fff) == cmds[curr_cmd].cmd) {

			WLDBG_EXIT(DBG_LEVEL_2);

			return cmds[curr_cmd].cmd_string;
		}
	}

	WLDBG_EXIT_INFO(DBG_LEVEL_2, "unknown");

	return "unknown";
}
#endif
