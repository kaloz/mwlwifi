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

/* Description:  This file implements firmware host command related
 * functions.
 */

#include <linux/etherdevice.h>

#include "sysadpt.h"
#include "dev.h"
#include "fwcmd.h"
#include "hostcmd.h"

#define MAX_WAIT_FW_COMPLETE_ITERATIONS         2000
#define MAX_WAIT_GET_HW_SPECS_ITERATONS         3

struct cmd_header {
	__le16 command;
	__le16 len;
} __packed;

static bool mwl_fwcmd_chk_adapter(struct mwl_priv *priv)
{
	u32 regval;

	regval = readl(priv->iobase1 + MACREG_REG_INT_CODE);

	if (regval == 0xffffffff) {
		wiphy_err(priv->hw->wiphy, "adapter does not exist\n");
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
		{ HOSTCMD_CMD_BBP_REG_ACCESS, "BBPRegAccess" },
		{ HOSTCMD_CMD_RF_REG_ACCESS, "RFRegAccess" },
		{ HOSTCMD_CMD_802_11_RADIO_CONTROL, "80211RadioControl" },
		{ HOSTCMD_CMD_MEM_ADDR_ACCESS, "MEMAddrAccess" },
		{ HOSTCMD_CMD_802_11_TX_POWER, "80211TxPower" },
		{ HOSTCMD_CMD_802_11_RF_ANTENNA, "80211RfAntenna" },
		{ HOSTCMD_CMD_BROADCAST_SSID_ENABLE, "broadcast_ssid_enable" },
		{ HOSTCMD_CMD_SET_RF_CHANNEL, "SetRfChannel" },
		{ HOSTCMD_CMD_SET_AID, "SetAid" },
		{ HOSTCMD_CMD_SET_INFRA_MODE, "SetInfraMode" },
		{ HOSTCMD_CMD_802_11_RTS_THSD, "80211RtsThreshold" },
		{ HOSTCMD_CMD_SET_EDCA_PARAMS, "SetEDCAParams" },
		{ HOSTCMD_CMD_802_11H_DETECT_RADAR, "80211hDetectRadar" },
		{ HOSTCMD_CMD_SET_WMM_MODE, "SetWMMMode" },
		{ HOSTCMD_CMD_HT_GUARD_INTERVAL, "HtGuardInterval" },
		{ HOSTCMD_CMD_SET_FIXED_RATE, "SetFixedRate" },
		{ HOSTCMD_CMD_SET_IES, "SetInformationElements" },
		{ HOSTCMD_CMD_SET_LINKADAPT_CS_MODE, "LinkAdaptCsMode" },
		{ HOSTCMD_CMD_SET_MAC_ADDR, "SetMacAddr" },
		{ HOSTCMD_CMD_SET_RATE_ADAPT_MODE, "SetRateAdaptationMode" },
		{ HOSTCMD_CMD_GET_WATCHDOG_BITMAP, "GetWatchdogBitMap" },
		{ HOSTCMD_CMD_DEL_MAC_ADDR, "DelMacAddr" },
		{ HOSTCMD_CMD_BSS_START, "BssStart" },
		{ HOSTCMD_CMD_AP_BEACON, "SetApBeacon" },
		{ HOSTCMD_CMD_SET_NEW_STN, "SetNewStation" },
		{ HOSTCMD_CMD_SET_APMODE, "SetApMode" },
		{ HOSTCMD_CMD_SET_SWITCH_CHANNEL, "SetSwitchChannel" },
		{ HOSTCMD_CMD_UPDATE_ENCRYPTION, "UpdateEncryption" },
		{ HOSTCMD_CMD_BASTREAM, "BAStream" },
		{ HOSTCMD_CMD_SET_SPECTRUM_MGMT, "SetSpectrumMgmt" },
		{ HOSTCMD_CMD_SET_POWER_CONSTRAINT, "SetPowerConstraint" },
		{ HOSTCMD_CMD_SET_COUNTRY_CODE, "SetCountryCode" },
		{ HOSTCMD_CMD_SET_OPTIMIZATION_LEVEL, "SetOptimizationLevel" },
		{ HOSTCMD_CMD_SET_WSC_IE, "SetWscIE" },
		{ HOSTCMD_CMD_DWDS_ENABLE, "DwdsEnable" },
		{ HOSTCMD_CMD_FW_FLUSH_TIMER, "FwFlushTimer" },
		{ HOSTCMD_CMD_SET_CDD, "SetCDD" },
		{ HOSTCMD_CMD_CAU_REG_ACCESS, "CAURegAccess" },
		{ HOSTCMD_CMD_GET_TEMP, "GetTemp" },
		{ HOSTCMD_CMD_GET_FW_REGION_CODE, "GetFwRegionCode" },
		{ HOSTCMD_CMD_GET_DEVICE_PWR_TBL, "GetDevicePwrTbl" },
		{ HOSTCMD_CMD_GET_FW_REGION_CODE_SC4, "GetFwRegionCodeSC4" },
		{ HOSTCMD_CMD_GET_DEVICE_PWR_TBL_SC4, "GetDevicePwrTblSC4" },
		{ HOSTCMD_CMD_QUIET_MODE, "QuietMode" },
	};

	max_entries = ARRAY_SIZE(cmds);

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
		usleep_range(1000, 2000);
	} while ((int_code != cmd) && (--curr_iteration));

	if (curr_iteration == 0) {
		wiphy_err(priv->hw->wiphy, "cmd 0x%04x=%s timed out\n",
			  cmd, mwl_fwcmd_get_cmd_string(cmd));
		wiphy_err(priv->hw->wiphy, "return code: 0x%04x\n", int_code);
		return -EIO;
	}

	usleep_range(3000, 5000);

	return 0;
}

static int mwl_fwcmd_exec_cmd(struct mwl_priv *priv, unsigned short cmd)
{
	bool busy = false;

	might_sleep();

	if (!mwl_fwcmd_chk_adapter(priv)) {
		wiphy_err(priv->hw->wiphy, "adapter does not exist\n");
		priv->in_send_cmd = false;
		return -EIO;
	}

	if (!priv->in_send_cmd) {
		priv->in_send_cmd = true;
		mwl_fwcmd_send_cmd(priv);
		if (mwl_fwcmd_wait_complete(priv, 0x8000 | cmd)) {
			wiphy_err(priv->hw->wiphy, "timeout: 0x%04x\n", cmd);
			priv->in_send_cmd = false;
			return -EIO;
		}
	} else {
		wiphy_warn(priv->hw->wiphy,
			   "previous command is still running\n");
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

	if (enable == priv->radio_on && !force)
		return 0;

	pcmd = (struct hostcmd_cmd_802_11_radio_control *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_802_11_RADIO_CONTROL);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le16(WL_SET);
	pcmd->control = cpu_to_le16(priv->radio_short_preamble ?
		WL_AUTO_PREAMBLE : WL_LONG_PREAMBLE);
	pcmd->radio_on = cpu_to_le16(enable ? WL_ENABLE : WL_DISABLE);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_RADIO_CONTROL)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(priv->hw->wiphy, "failed execution\n");
		return -EIO;
	}

	priv->radio_on = enable;

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

static int mwl_fwcmd_get_tx_powers(struct mwl_priv *priv, u16 *powlist, u16 ch,
				   u16 band, u16 width, u16 sub_ch)
{
	struct hostcmd_cmd_802_11_tx_power *pcmd;
	int i;

	pcmd = (struct hostcmd_cmd_802_11_tx_power *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_802_11_TX_POWER);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le16(HOSTCMD_ACT_GEN_GET_LIST);
	pcmd->ch = cpu_to_le16(ch);
	pcmd->bw = cpu_to_le16(width);
	pcmd->band = cpu_to_le16(band);
	pcmd->sub_ch = cpu_to_le16(sub_ch);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_TX_POWER)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(priv->hw->wiphy, "failed execution\n");
		return -EIO;
	}

	for (i = 0; i < SYSADPT_TX_POWER_LEVEL_TOTAL; i++)
		powlist[i] = le16_to_cpu(pcmd->power_level_list[i]);

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

static int mwl_fwcmd_set_tx_powers(struct mwl_priv *priv, u16 txpow[],
				   u8 action, u16 ch, u16 band,
				   u16 width, u16 sub_ch)
{
	struct hostcmd_cmd_802_11_tx_power *pcmd;
	int i;

	pcmd = (struct hostcmd_cmd_802_11_tx_power *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

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
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(priv->hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

static u8 mwl_fwcmd_get_80m_pri_chnl(u8 channel)
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

static u8 mwl_fwcmd_get_160m_pri_chnl(u8 channel)
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
		act_primary = ACT_PRIMARY_CHAN_4;
		break;
	case 56:
		act_primary = ACT_PRIMARY_CHAN_5;
		break;
	case 60:
		act_primary = ACT_PRIMARY_CHAN_6;
		break;
	case 64:
		act_primary = ACT_PRIMARY_CHAN_7;
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
		act_primary = ACT_PRIMARY_CHAN_4;
		break;
	case 120:
		act_primary = ACT_PRIMARY_CHAN_5;
		break;
	case 124:
		act_primary = ACT_PRIMARY_CHAN_6;
		break;
	case 128:
		act_primary = ACT_PRIMARY_CHAN_7;
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
	case 165:
		act_primary = ACT_PRIMARY_CHAN_4;
		break;
	case 169:
		act_primary = ACT_PRIMARY_CHAN_5;
		break;
	case 173:
		act_primary = ACT_PRIMARY_CHAN_6;
		break;
	case 177:
		act_primary = ACT_PRIMARY_CHAN_7;
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
	beacon_info->power_constraint = 0;

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
		case WLAN_EID_COUNTRY:
			beacon_info->ie_country_len = (elen + 2);
			beacon_info->ie_country_ptr = (pos - 2);
			break;
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
		case WLAN_EID_PWR_CONSTRAINT:
			if (elen == 1)
				beacon_info->power_constraint = *pos;
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

				if (pos[3] == 0x04) {
					beacon_info->ie_wsc_len = (elen + 2);
					beacon_info->ie_wsc_ptr = (pos - 2);
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
	}
}

static int mwl_fwcmd_set_ies(struct mwl_priv *priv, struct mwl_vif *mwl_vif)
{
	struct hostcmd_cmd_set_ies *pcmd;
	struct beacon_info *beacon = &mwl_vif->beacon_info;
	u16 ie_list_len_proprietary = 0;

	if (beacon->ie_ht_len > sizeof(pcmd->ie_list_ht))
		goto einval;

	if (beacon->ie_vht_len > sizeof(pcmd->ie_list_vht))
		goto einval;

	pcmd = (struct hostcmd_cmd_set_ies *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_IES);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action = cpu_to_le16(HOSTCMD_ACT_GEN_SET);

	memcpy(pcmd->ie_list_ht, beacon->ie_ht_ptr, beacon->ie_ht_len);
	pcmd->ie_list_len_ht = cpu_to_le16(beacon->ie_ht_len);

	memcpy(pcmd->ie_list_vht, beacon->ie_vht_ptr, beacon->ie_vht_len);
	pcmd->ie_list_len_vht = cpu_to_le16(beacon->ie_vht_len);

	if (priv->chip_type == MWL8897) {
		memcpy(pcmd->ie_list_proprietary + ie_list_len_proprietary,
		       beacon->ie_wmm_ptr, beacon->ie_wmm_len);
		ie_list_len_proprietary += mwl_vif->beacon_info.ie_wmm_len;
	}

	pcmd->ie_list_len_proprietary = cpu_to_le16(ie_list_len_proprietary);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_IES)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(priv->hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;

einval:

	wiphy_err(priv->hw->wiphy, "length of IE is too long\n");

	return -EINVAL;
}

static int mwl_fwcmd_set_ap_beacon(struct mwl_priv *priv,
				   struct mwl_vif *mwl_vif,
				   struct ieee80211_bss_conf *bss_conf)
{
	struct hostcmd_cmd_ap_beacon *pcmd;
	struct ds_params *phy_ds_param_set;

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

	mutex_lock(&priv->fwcmd_mutex);

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
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(priv->hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;

ielenerr:

	wiphy_err(priv->hw->wiphy, "length of IE is too long\n");

	return -EINVAL;
}

static int mwl_fwcmd_set_spectrum_mgmt(struct mwl_priv *priv, bool enable)
{
	struct hostcmd_cmd_set_spectrum_mgmt *pcmd;

	pcmd = (struct hostcmd_cmd_set_spectrum_mgmt *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_SPECTRUM_MGMT);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->spectrum_mgmt = cpu_to_le32(enable);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_SPECTRUM_MGMT)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(priv->hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

static int mwl_fwcmd_set_power_constraint(struct mwl_priv *priv,
					  u32 power_constraint)
{
	struct hostcmd_cmd_set_power_constraint *pcmd;

	pcmd = (struct hostcmd_cmd_set_power_constraint *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_POWER_CONSTRAINT);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->power_constraint = cpu_to_le32(power_constraint);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_POWER_CONSTRAINT)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(priv->hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

static int mwl_fwcmd_set_country_code(struct mwl_priv *priv,
				      struct mwl_vif *mwl_vif,
				      struct ieee80211_bss_conf *bss_conf)
{
	struct hostcmd_cmd_set_country_code *pcmd;
	struct beacon_info *b_inf = &mwl_vif->beacon_info;
	u8 chnl_len;
	bool a_band;
	bool enable = false;

	if (b_inf->ie_country_ptr) {
		if (bss_conf->chandef.chan->band == NL80211_BAND_2GHZ)
			a_band = false;
		else if (bss_conf->chandef.chan->band == NL80211_BAND_5GHZ)
			a_band = true;
		else
			return -EINVAL;

		chnl_len = b_inf->ie_country_len - 5;
		if (a_band) {
			if (chnl_len > sizeof(pcmd->domain_info.domain_entry_a))
				return -EINVAL;
		} else {
			if (chnl_len > sizeof(pcmd->domain_info.domain_entry_g))
				return -EINVAL;
		}

		enable = true;
	}

	pcmd = (struct hostcmd_cmd_set_country_code *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_COUNTRY_CODE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le32(enable);
	if (enable) {
		memcpy(pcmd->domain_info.country_string,
		       b_inf->ie_country_ptr + 2, 3);
		if (a_band) {
			pcmd->domain_info.g_chnl_len = 0;
			pcmd->domain_info.a_chnl_len = chnl_len;
			memcpy(pcmd->domain_info.domain_entry_a,
			       b_inf->ie_country_ptr + 5, chnl_len);
		} else {
			pcmd->domain_info.a_chnl_len = 0;
			pcmd->domain_info.g_chnl_len = chnl_len;
			memcpy(pcmd->domain_info.domain_entry_g,
			       b_inf->ie_country_ptr + 5, chnl_len);
		}
	}

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_COUNTRY_CODE)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(priv->hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
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
	case WLAN_CIPHER_SUITE_AES_CMAC:
		return 1;
	default:
		return -ENOTSUPP;
	}

	return 0;
}

static u32 pci_read_mac_reg(struct mwl_priv *priv, u32 offset)
{
	if (priv->chip_type == MWL8964) {
		u32 *addr_val = kmalloc(64 * sizeof(u32), GFP_ATOMIC);
		u32 val;

		if(addr_val)
		{
			mwl_fwcmd_get_addr_value(priv->hw,
						 0x8000a000 + offset, 4,
						 addr_val, 0);
			val = addr_val[0];
			kfree(addr_val);
			return val;
		}
		return 0;
	} else
		return le32_to_cpu(*(volatile unsigned long *)
		       (MAC_REG_ADDR_PCI(offset)));
}

void mwl_fwcmd_reset(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv = hw->priv;

	if (mwl_fwcmd_chk_adapter(priv))
		writel(ISR_RESET,
		       priv->iobase1 + MACREG_REG_H2A_INTERRUPT_EVENTS);
}

void mwl_fwcmd_int_enable(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv = hw->priv;

	if (mwl_fwcmd_chk_adapter(priv)) {
		writel(0x00,
		       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_MASK);
		writel(MACREG_A2HRIC_BIT_MASK,
		       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_MASK);
	}
}

void mwl_fwcmd_int_disable(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv = hw->priv;

	if (mwl_fwcmd_chk_adapter(priv))
		writel(0x00,
		       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_MASK);
}

int mwl_fwcmd_get_hw_specs(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_get_hw_spec *pcmd;
	int retry;
	int i;

	pcmd = (struct hostcmd_cmd_get_hw_spec *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	wiphy_debug(hw->wiphy, "pcmd = %p\n", pcmd);
	memset(pcmd, 0x00, sizeof(*pcmd));
	eth_broadcast_addr(pcmd->permanent_addr);
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_GET_HW_SPEC);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->fw_awake_cookie = cpu_to_le32(priv->pphys_cmd_buf + 2048);

	retry = 0;
	while (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_GET_HW_SPEC)) {
		if (retry++ > MAX_WAIT_GET_HW_SPECS_ITERATONS) {
			wiphy_err(hw->wiphy, "can't get hw specs\n");
			mutex_unlock(&priv->fwcmd_mutex);
			return -EIO;
		}

		msleep(1000);
		wiphy_debug(hw->wiphy,
			    "repeat command = %p\n", pcmd);
	}

	ether_addr_copy(&priv->hw_data.mac_addr[0], pcmd->permanent_addr);
	priv->desc_data[0].wcb_base =
		le32_to_cpu(pcmd->wcb_base0) & 0x0000ffff;
	for (i = 1; i < SYSADPT_TOTAL_TX_QUEUES; i++)
		priv->desc_data[i].wcb_base =
			le32_to_cpu(pcmd->wcb_base[i - 1]) & 0x0000ffff;
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

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_hw_specs(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_set_hw_spec *pcmd;
	int i;

	pcmd = (struct hostcmd_cmd_set_hw_spec *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_HW_SPEC);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->wcb_base[0] = cpu_to_le32(priv->desc_data[0].pphys_tx_ring);
	for (i = 1; i < SYSADPT_TOTAL_TX_QUEUES; i++)
		pcmd->wcb_base[i] =
			cpu_to_le32(priv->desc_data[i].pphys_tx_ring);
	pcmd->tx_wcb_num_per_queue = cpu_to_le32(SYSADPT_MAX_NUM_TX_DESC);
	pcmd->num_tx_queues = cpu_to_le32(SYSADPT_NUM_OF_DESC_DATA);
	pcmd->total_rx_wcb = cpu_to_le32(SYSADPT_MAX_NUM_RX_DESC);
	pcmd->rxpd_wr_ptr = cpu_to_le32(priv->desc_data[0].pphys_rx_ring);
	pcmd->features = 0;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_HW_SPEC)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_get_stat(struct ieee80211_hw *hw,
		       struct ieee80211_low_level_stats *stats)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_802_11_get_stat *pcmd;

	pcmd = (struct hostcmd_cmd_802_11_get_stat *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_802_11_GET_STAT);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_GET_STAT)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
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

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_reg_bb(struct ieee80211_hw *hw, u8 flag, u32 reg, u32 *val)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_bbp_reg_access *pcmd;

	pcmd = (struct hostcmd_cmd_bbp_reg_access *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_BBP_REG_ACCESS);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->offset = cpu_to_le16(reg);
	pcmd->action = cpu_to_le16(flag);
	pcmd->value = *val;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_BBP_REG_ACCESS)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	*val = pcmd->value;

	mutex_unlock(&priv->fwcmd_mutex);
	return 0;
}

int mwl_fwcmd_reg_rf(struct ieee80211_hw *hw, u8 flag, u32 reg, u32 *val)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_rf_reg_access *pcmd;

	pcmd = (struct hostcmd_cmd_rf_reg_access *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_RF_REG_ACCESS);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->offset = cpu_to_le16(reg);
	pcmd->action = cpu_to_le16(flag);
	pcmd->value = *val;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_RF_REG_ACCESS)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	*val = pcmd->value;

	mutex_unlock(&priv->fwcmd_mutex);

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
	struct mwl_priv *priv = hw->priv;
	int rc;

	priv->radio_short_preamble = short_preamble;
	rc = mwl_fwcmd_802_11_radio_control(priv, true, true);

	return rc;
}

int mwl_fwcmd_get_addr_value(struct ieee80211_hw *hw, u32 addr, u32 len,
			     u32 *val, u16 set)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_mem_addr_access *pcmd;
	int i;

	pcmd = (struct hostcmd_cmd_mem_addr_access *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_MEM_ADDR_ACCESS);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->address = cpu_to_le32(addr);
	pcmd->length = cpu_to_le16(len);
	pcmd->value[0] = cpu_to_le32(*val);
	pcmd->reserved = cpu_to_le16(set);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_MEM_ADDR_ACCESS)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	for (i = 0; i < len; i++)
		val[i] = le32_to_cpu(pcmd->value[i]);

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_max_tx_power(struct ieee80211_hw *hw,
			   struct ieee80211_conf *conf, u8 fraction)
{
	struct ieee80211_channel *channel = conf->chandef.chan;
	struct mwl_priv *priv = hw->priv;
	int reduce_val = 0;
	u16 band = 0, width = 0, sub_ch = 0;
	u16 maxtxpow[SYSADPT_TX_POWER_LEVEL_TOTAL];
	int i, tmp;
	int rc = 0;

	if (priv->forbidden_setting)
		return rc;

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

	if (channel->band == NL80211_BAND_2GHZ)
		band = FREQ_BAND_2DOT4GHZ;
	else if (channel->band == NL80211_BAND_5GHZ)
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
	struct mwl_priv *priv = hw->priv;
	int reduce_val = 0;
	u16 band = 0, width = 0, sub_ch = 0;
	u16 txpow[SYSADPT_TX_POWER_LEVEL_TOTAL];
	int index, found = 0;
	int i, tmp;
	int rc = 0;

	if (priv->forbidden_setting)
		return rc;

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

	if (channel->band == NL80211_BAND_2GHZ)
		band = FREQ_BAND_2DOT4GHZ;
	else if (channel->band == NL80211_BAND_5GHZ)
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
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_802_11_rf_antenna *pcmd;

	pcmd = (struct hostcmd_cmd_802_11_rf_antenna *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

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
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_broadcast_ssid_enable(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif, bool enable)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_broadcast_ssid_enable *pcmd;

	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_broadcast_ssid_enable *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_BROADCAST_SSID_ENABLE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	pcmd->enable = cpu_to_le32(enable);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_BROADCAST_SSID_ENABLE)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_rf_channel(struct ieee80211_hw *hw,
			     struct ieee80211_conf *conf)
{
	struct ieee80211_channel *channel = conf->chandef.chan;
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_set_rf_channel *pcmd;
	u32 chnl_flags, freq_band, chnl_width, act_primary;

	pcmd = (struct hostcmd_cmd_set_rf_channel *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_RF_CHANNEL);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le16(WL_SET);
	pcmd->curr_chnl = channel->hw_value;

	if (channel->band == NL80211_BAND_2GHZ) {
		freq_band = FREQ_BAND_2DOT4GHZ;
	} else if (channel->band == NL80211_BAND_5GHZ) {
		freq_band = FREQ_BAND_5GHZ;
	} else {
		mutex_unlock(&priv->fwcmd_mutex);
		return -EINVAL;
	}

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
			mwl_fwcmd_get_80m_pri_chnl(pcmd->curr_chnl);
		break;
	case NL80211_CHAN_WIDTH_160:
		chnl_width = CH_160_MHZ_WIDTH;
		act_primary =
			mwl_fwcmd_get_160m_pri_chnl(pcmd->curr_chnl);
		break;
	default:
		mutex_unlock(&priv->fwcmd_mutex);
		return -EINVAL;
	}

	chnl_flags = (freq_band & FREQ_BAND_MASK) |
		((chnl_width << CHNL_WIDTH_SHIFT) & CHNL_WIDTH_MASK) |
		((act_primary << ACT_PRIMARY_SHIFT) & ACT_PRIMARY_MASK);

	pcmd->chnl_flags = cpu_to_le32(chnl_flags);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_RF_CHANNEL)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	if (pcmd->cmd_hdr.result != 0) {
		mutex_unlock(&priv->fwcmd_mutex);
		return -EINVAL;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	if (priv->sw_scanning) {
		priv->survey_info_idx++;
		mwl_fwcmd_get_survey(hw, priv->survey_info_idx);
	} else {
		mwl_fwcmd_get_survey(hw, 0);
		memset(&priv->cur_survey_info, 0, sizeof(struct mwl_survey_info));
	}

	return 0;
}

int mwl_fwcmd_set_aid(struct ieee80211_hw *hw,
		      struct ieee80211_vif *vif, u8 *bssid, u16 aid)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_aid *pcmd;

	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_aid *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_AID);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	pcmd->aid = cpu_to_le16(aid);
	ether_addr_copy(pcmd->mac_addr, bssid);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_AID)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_infra_mode(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_infra_mode *pcmd;

	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_infra_mode *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_INFRA_MODE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_INFRA_MODE)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_rts_threshold(struct ieee80211_hw *hw, int threshold)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_802_11_rts_thsd *pcmd;

	pcmd = (struct hostcmd_cmd_802_11_rts_thsd *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_802_11_RTS_THSD);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action  = cpu_to_le16(WL_SET);
	pcmd->threshold = cpu_to_le16(threshold);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11_RTS_THSD)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_edca_params(struct ieee80211_hw *hw, u8 index,
			      u16 cw_min, u16 cw_max, u8 aifs, u16 txop)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_set_edca_params *pcmd;

	pcmd = (struct hostcmd_cmd_set_edca_params *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_EDCA_PARAMS);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));

	pcmd->action = cpu_to_le16(0xffff);
	pcmd->txop = cpu_to_le16(txop);
	pcmd->cw_max = cpu_to_le32(cw_max);
	pcmd->cw_min = cpu_to_le32(cw_min);
	pcmd->aifsn = aifs;
	pcmd->txq_num = index;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_EDCA_PARAMS)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_radar_detect(struct ieee80211_hw *hw, u16 action)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_802_11h_detect_radar *pcmd;
	u16 radar_type = RADAR_TYPE_CODE_0;
	u8 channel = hw->conf.chandef.chan->hw_value;

	pcmd = (struct hostcmd_cmd_802_11h_detect_radar *)&priv->pcmd_buf[0];

	if (priv->dfs_region == NL80211_DFS_JP) {
		if (channel >= 52 && channel <= 64)
			radar_type = RADAR_TYPE_CODE_53;
		else if (channel >= 100 && channel <= 140)
			radar_type = RADAR_TYPE_CODE_56;
		else
			radar_type = RADAR_TYPE_CODE_0;
	} else if (priv->dfs_region == NL80211_DFS_ETSI) {
		radar_type = RADAR_TYPE_CODE_ETSI;
	}

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_802_11H_DETECT_RADAR);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le16(action);
	pcmd->radar_type_code = cpu_to_le16(radar_type);
	pcmd->min_chirp_cnt = cpu_to_le16(priv->dfs_chirp_count_min);
	pcmd->chirp_time_intvl = cpu_to_le16(priv->dfs_chirp_time_interval);
	pcmd->pw_filter = cpu_to_le16(priv->dfs_pw_filter);
	pcmd->min_num_radar = cpu_to_le16(priv->dfs_min_num_radar);
	pcmd->pri_min_num = cpu_to_le16(priv->dfs_min_pri_count);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_802_11H_DETECT_RADAR)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_wmm_mode(struct ieee80211_hw *hw, bool enable)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_set_wmm_mode *pcmd;

	pcmd = (struct hostcmd_cmd_set_wmm_mode *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_WMM_MODE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le16(enable ? WL_ENABLE : WL_DISABLE);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_WMM_MODE)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_ht_guard_interval(struct ieee80211_hw *hw, u32 gi_type)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_ht_guard_interval *pcmd;

	pcmd = (struct hostcmd_cmd_ht_guard_interval *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_HT_GUARD_INTERVAL);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le32(WL_SET);
	pcmd->gi_type = cpu_to_le32(gi_type);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_HT_GUARD_INTERVAL)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_use_fixed_rate(struct ieee80211_hw *hw, int mcast, int mgmt)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_set_fixed_rate *pcmd;

	pcmd = (struct hostcmd_cmd_set_fixed_rate *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_FIXED_RATE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));

	pcmd->action = cpu_to_le32(HOSTCMD_ACT_NOT_USE_FIXED_RATE);
	pcmd->multicast_rate = mcast;
	pcmd->management_rate = mgmt;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_FIXED_RATE)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_linkadapt_cs_mode(struct ieee80211_hw *hw, u16 cs_mode)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_set_linkadapt_cs_mode *pcmd;

	pcmd = (struct hostcmd_cmd_set_linkadapt_cs_mode *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_LINKADAPT_CS_MODE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action  = cpu_to_le16(HOSTCMD_ACT_GEN_SET);
	pcmd->cs_mode = cpu_to_le16(cs_mode);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_LINKADAPT_CS_MODE)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_rate_adapt_mode(struct ieee80211_hw *hw, u16 mode)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_set_rate_adapt_mode *pcmd;

	pcmd = (struct hostcmd_cmd_set_rate_adapt_mode *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_RATE_ADAPT_MODE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le16(WL_SET);
	pcmd->rate_adapt_mode = cpu_to_le16(mode);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_RATE_ADAPT_MODE)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_mac_addr_client(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif, u8 *mac_addr)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_mac_addr *pcmd;

	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_mac_addr *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_MAC_ADDR);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	pcmd->mac_type = cpu_to_le16(WL_MAC_TYPE_SECONDARY_CLIENT);
	ether_addr_copy(pcmd->mac_addr, mac_addr);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_MAC_ADDR)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_get_watchdog_bitmap(struct ieee80211_hw *hw, u8 *bitmap)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_get_watchdog_bitmap *pcmd;

	pcmd = (struct hostcmd_cmd_get_watchdog_bitmap *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_GET_WATCHDOG_BITMAP);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_GET_WATCHDOG_BITMAP)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	*bitmap = pcmd->watchdog_bitmap;

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_remove_mac_addr(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif, u8 *mac_addr)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_mac_addr *pcmd;

	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_mac_addr *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_DEL_MAC_ADDR);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	ether_addr_copy(pcmd->mac_addr, mac_addr);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_DEL_MAC_ADDR)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_bss_start(struct ieee80211_hw *hw,
			struct ieee80211_vif *vif, bool enable)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_bss_start *pcmd;

	mwl_vif = mwl_dev_get_vif(vif);

	if (enable && (priv->running_bsses & (1 << mwl_vif->macid)))
		return 0;

	if (!enable && !(priv->running_bsses & (1 << mwl_vif->macid)))
		return 0;

	pcmd = (struct hostcmd_cmd_bss_start *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

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
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	if (enable)
		priv->running_bsses |= (1 << mwl_vif->macid);
	else
		priv->running_bsses &= ~(1 << mwl_vif->macid);

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_beacon(struct ieee80211_hw *hw,
			 struct ieee80211_vif *vif, u8 *beacon, int len)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct beacon_info *b_inf;
	int rc;

	mwl_vif = mwl_dev_get_vif(vif);
	b_inf = &mwl_vif->beacon_info;

	mwl_fwcmd_parse_beacon(priv, mwl_vif, beacon, len);

	if (!b_inf->valid)
		goto err;

	if (mwl_fwcmd_set_ies(priv, mwl_vif))
		goto err;

	if (mwl_fwcmd_set_wsc_ie(hw, b_inf->ie_wsc_len, b_inf->ie_wsc_ptr))
		goto err;

	if (mwl_fwcmd_set_ap_beacon(priv, mwl_vif, &vif->bss_conf))
		goto err;

	if (mwl_fwcmd_bss_start(hw, vif, true))
		goto err;

	if (b_inf->cap_info & WLAN_CAPABILITY_SPECTRUM_MGMT)
		rc = mwl_fwcmd_set_spectrum_mgmt(priv, true);
	else
		rc = mwl_fwcmd_set_spectrum_mgmt(priv, false);
	if (rc)
		goto err;

	if (b_inf->power_constraint)
		rc = mwl_fwcmd_set_power_constraint(priv,
						    b_inf->power_constraint);
	if (rc)
		goto err;

	if (mwl_fwcmd_set_country_code(priv, mwl_vif, &vif->bss_conf))
		goto err;

	b_inf->valid = false;

	return 0;

err:

	b_inf->valid = false;

	return -EIO;
}

int mwl_fwcmd_set_new_stn_add(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif,
			      struct ieee80211_sta *sta)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_new_stn *pcmd;
	u32 rates;

	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_new_stn *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_NEW_STN);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action = cpu_to_le16(HOSTCMD_ACT_STA_ACTION_ADD);
	if (vif->type == NL80211_IFTYPE_STATION) {
		pcmd->aid = 1;
		pcmd->stn_id = 1;
		pcmd->reserved = 1;
	} else {
		pcmd->aid = cpu_to_le16(sta->aid);
		pcmd->stn_id = cpu_to_le16(sta->aid);
	}
	ether_addr_copy(pcmd->mac_addr, sta->addr);

	if (hw->conf.chandef.chan->band == NL80211_BAND_2GHZ)
		rates = sta->supp_rates[NL80211_BAND_2GHZ];
	else
		rates = sta->supp_rates[NL80211_BAND_5GHZ] << 5;
	pcmd->peer_info.legacy_rate_bitmap = cpu_to_le32(rates);

	if (sta->ht_cap.ht_supported) {
		int i;

		for (i = 0; i < 4; i++) {
			if (i < sta->rx_nss) {
				pcmd->peer_info.ht_rates[i] =
					sta->ht_cap.mcs.rx_mask[i];
			} else {
				pcmd->peer_info.ht_rates[i] = 0;
			}
		}
		pcmd->peer_info.ht_cap_info = cpu_to_le16(sta->ht_cap.cap);
		pcmd->peer_info.mac_ht_param_info =
			(sta->ht_cap.ampdu_factor & 3) |
			((sta->ht_cap.ampdu_density & 7) << 2);
	}

	if (sta->vht_cap.vht_supported) {
		u32 rx_mcs_map_mask = 0;

		rx_mcs_map_mask = ((0x0000FFFF) >> (sta->rx_nss * 2))
			<< (sta->rx_nss * 2);
		pcmd->peer_info.vht_max_rx_mcs =
			cpu_to_le32((*((u32 *)
			&sta->vht_cap.vht_mcs.rx_mcs_map)) | rx_mcs_map_mask);
		pcmd->peer_info.vht_cap = cpu_to_le32(sta->vht_cap.cap);
		pcmd->peer_info.vht_rx_channel_width = sta->bandwidth;
	}

	pcmd->is_qos_sta = sta->wme;
	pcmd->qos_info = ((sta->uapsd_queues << 4) | (sta->max_sp << 1));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_NEW_STN)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	if (vif->type == NL80211_IFTYPE_STATION) {
		ether_addr_copy(pcmd->mac_addr, mwl_vif->sta_mac);
		pcmd->aid = 2;
		pcmd->stn_id = 2;
		pcmd->reserved = 0;
		if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_NEW_STN)) {
			mutex_unlock(&priv->fwcmd_mutex);
			wiphy_err(hw->wiphy, "failed execution\n");
			return -EIO;
		}
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_new_stn_add_self(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_new_stn *pcmd;

	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_new_stn *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_NEW_STN);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action = cpu_to_le16(HOSTCMD_ACT_STA_ACTION_ADD);
	ether_addr_copy(pcmd->mac_addr, vif->addr);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_NEW_STN)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_new_stn_del(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif, u8 *addr)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_new_stn *pcmd;

	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_new_stn *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_NEW_STN);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action = cpu_to_le16(HOSTCMD_ACT_STA_ACTION_REMOVE);
	ether_addr_copy(pcmd->mac_addr, addr);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_NEW_STN)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	if (vif->type == NL80211_IFTYPE_STATION) {
		ether_addr_copy(pcmd->mac_addr, mwl_vif->sta_mac);

		if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_NEW_STN)) {
			mutex_unlock(&priv->fwcmd_mutex);
			wiphy_err(hw->wiphy, "failed execution\n");
			return -EIO;
		}
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_apmode(struct ieee80211_hw *hw, u8 apmode)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_set_apmode *pcmd;

	pcmd = (struct hostcmd_cmd_set_apmode *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_APMODE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->apmode = apmode;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_APMODE)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_switch_channel(struct mwl_priv *priv,
				 struct ieee80211_channel_switch *ch_switch)
{
	struct hostcmd_cmd_set_switch_channel *pcmd;
	struct cfg80211_chan_def *chandef = &ch_switch->chandef;
	struct ieee80211_channel *channel = chandef->chan;
	u32 chnl_flags, freq_band, chnl_width, act_primary, sec_chnl_offset;

	if (priv->csa_active)
		return 0;

	if (channel->band == NL80211_BAND_2GHZ)
		freq_band = FREQ_BAND_2DOT4GHZ;
	else if (channel->band == NL80211_BAND_5GHZ)
		freq_band = FREQ_BAND_5GHZ;
	else
		return -EINVAL;

	switch (chandef->width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		chnl_width = CH_20_MHZ_WIDTH;
		act_primary = ACT_PRIMARY_CHAN_0;
		sec_chnl_offset = IEEE80211_HT_PARAM_CHA_SEC_NONE;
		break;
	case NL80211_CHAN_WIDTH_40:
		chnl_width = CH_40_MHZ_WIDTH;
		if (chandef->center_freq1 > channel->center_freq) {
			act_primary = ACT_PRIMARY_CHAN_0;
			sec_chnl_offset = IEEE80211_HT_PARAM_CHA_SEC_ABOVE;
		} else {
			act_primary = ACT_PRIMARY_CHAN_1;
			sec_chnl_offset = IEEE80211_HT_PARAM_CHA_SEC_BELOW;
		}
		break;
	case NL80211_CHAN_WIDTH_80:
		chnl_width = CH_80_MHZ_WIDTH;
		act_primary =
			mwl_fwcmd_get_80m_pri_chnl(channel->hw_value);
		if ((act_primary == ACT_PRIMARY_CHAN_0) ||
		    (act_primary == ACT_PRIMARY_CHAN_2))
			sec_chnl_offset = IEEE80211_HT_PARAM_CHA_SEC_ABOVE;
		else
			sec_chnl_offset = IEEE80211_HT_PARAM_CHA_SEC_BELOW;
		break;
	default:
		return -EINVAL;
	}

	chnl_flags = (freq_band & FREQ_BAND_MASK) |
		((chnl_width << CHNL_WIDTH_SHIFT) & CHNL_WIDTH_MASK) |
		((act_primary << ACT_PRIMARY_SHIFT) & ACT_PRIMARY_MASK);

	pcmd = (struct hostcmd_cmd_set_switch_channel *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_SWITCH_CHANNEL);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->next_11h_chnl = cpu_to_le32(channel->hw_value);
	pcmd->mode = cpu_to_le32(ch_switch->block_tx);
	pcmd->init_count = cpu_to_le32(ch_switch->count + 1);
	pcmd->chnl_flags = cpu_to_le32(chnl_flags);
	pcmd->next_ht_extchnl_offset = cpu_to_le32(sec_chnl_offset);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_SWITCH_CHANNEL)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(priv->hw->wiphy, "failed execution\n");
		return -EIO;
	}

	priv->csa_active = true;

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_update_encryption_enable(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       u8 *addr, u8 encr_type)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_update_encryption *pcmd;

	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_update_encryption *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_UPDATE_ENCRYPTION);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	pcmd->action_type = cpu_to_le32(ENCR_ACTION_ENABLE_HW_ENCR);
	ether_addr_copy(pcmd->mac_addr, addr);
	pcmd->action_data[0] = encr_type;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_UPDATE_ENCRYPTION)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	if (vif->type == NL80211_IFTYPE_STATION) {
		if (ether_addr_equal(mwl_vif->bssid, addr))
			ether_addr_copy(pcmd->mac_addr, mwl_vif->sta_mac);
		else
			ether_addr_copy(pcmd->mac_addr, mwl_vif->bssid);

		if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_UPDATE_ENCRYPTION)) {
			mutex_unlock(&priv->fwcmd_mutex);
			wiphy_err(hw->wiphy, "failed execution\n");
			return -EIO;
		}
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_encryption_set_key(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif, u8 *addr,
				 struct ieee80211_key_conf *key)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_key *pcmd;
	int rc;
	int keymlen;
	u32 action;
	u8 idx;

	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_key *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_UPDATE_ENCRYPTION);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	rc = mwl_fwcmd_encryption_set_cmd_info(pcmd, addr, key);
	if (rc) {
		mutex_unlock(&priv->fwcmd_mutex);
		if (rc != 1)
			wiphy_err(hw->wiphy, "encryption not support\n");
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
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "encryption not support\n");
		return -ENOTSUPP;
	}

	memcpy((void *)&pcmd->key_param.key, key->key, keymlen);
	pcmd->action_type = cpu_to_le32(action);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_UPDATE_ENCRYPTION)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	if (vif->type == NL80211_IFTYPE_STATION) {
		if (ether_addr_equal(mwl_vif->bssid, addr))
			ether_addr_copy(pcmd->key_param.mac_addr,
					mwl_vif->sta_mac);
		else
			ether_addr_copy(pcmd->key_param.mac_addr,
					mwl_vif->bssid);

		if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_UPDATE_ENCRYPTION)) {
			mutex_unlock(&priv->fwcmd_mutex);
			wiphy_err(hw->wiphy, "failed execution\n");
			return -EIO;
		}
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_encryption_remove_key(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif, u8 *addr,
				    struct ieee80211_key_conf *key)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_set_key *pcmd;
	int rc;

	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_set_key *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_UPDATE_ENCRYPTION);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;

	rc = mwl_fwcmd_encryption_set_cmd_info(pcmd, addr, key);
	if (rc) {
		mutex_unlock(&priv->fwcmd_mutex);
		if (rc != 1)
			wiphy_err(hw->wiphy, "encryption not support\n");
		return rc;
	}

	pcmd->action_type = cpu_to_le32(ENCR_ACTION_TYPE_REMOVE_KEY);

	if (key->cipher == WLAN_CIPHER_SUITE_WEP40 ||
	    key->cipher == WLAN_CIPHER_SUITE_WEP104)
		mwl_vif->wep_key_conf[key->keyidx].enabled = 0;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_UPDATE_ENCRYPTION)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_check_ba(struct ieee80211_hw *hw,
		       struct mwl_ampdu_stream *stream,
		       struct ieee80211_vif *vif)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_bastream *pcmd;
	u32 ba_flags, ba_type, ba_direction;

	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_bastream *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_BASTREAM);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	pcmd->cmd_hdr.result = cpu_to_le16(0xffff);

	pcmd->action_type = cpu_to_le32(BA_CHECK_STREAM);
	ether_addr_copy(&pcmd->ba_info.create_params.peer_mac_addr[0],
			stream->sta->addr);
	pcmd->ba_info.create_params.tid = stream->tid;
	ba_type = BASTREAM_FLAG_IMMEDIATE_TYPE;
	ba_direction = BASTREAM_FLAG_DIRECTION_UPSTREAM;
	ba_flags = (ba_type & BA_TYPE_MASK) |
		((ba_direction << BA_DIRECTION_SHIFT) & BA_DIRECTION_MASK);
	pcmd->ba_info.create_params.flags = cpu_to_le32(ba_flags);
	pcmd->ba_info.create_params.queue_id = stream->idx;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_BASTREAM)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "check ba failed execution\n");
		return -EIO;
	}

	if (pcmd->cmd_hdr.result != 0) {
		mutex_unlock(&priv->fwcmd_mutex);
		return -EINVAL;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_create_ba(struct ieee80211_hw *hw,
			struct mwl_ampdu_stream *stream,
			u8 buf_size, struct ieee80211_vif *vif)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_vif *mwl_vif;
	struct hostcmd_cmd_bastream *pcmd;
	u32 ba_flags, ba_type, ba_direction;

	mwl_vif = mwl_dev_get_vif(vif);

	pcmd = (struct hostcmd_cmd_bastream *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_BASTREAM);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->cmd_hdr.macid = mwl_vif->macid;
	pcmd->cmd_hdr.result = cpu_to_le16(0xffff);

	pcmd->action_type = cpu_to_le32(BA_CREATE_STREAM);
	pcmd->ba_info.create_params.bar_thrs = cpu_to_le32(buf_size);
	pcmd->ba_info.create_params.window_size = cpu_to_le32(buf_size);
	ether_addr_copy(&pcmd->ba_info.create_params.peer_mac_addr[0],
			stream->sta->addr);
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
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "create ba failed execution\n");
		return -EIO;
	}

	if (pcmd->cmd_hdr.result != 0) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "create ba result error %d\n",
			  le16_to_cpu(pcmd->cmd_hdr.result));
		return -EINVAL;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_destroy_ba(struct ieee80211_hw *hw,
			 u8 idx)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_bastream *pcmd;
	u32 ba_flags, ba_type, ba_direction;

	pcmd = (struct hostcmd_cmd_bastream *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

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
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "destroy ba failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

/* caller must hold priv->stream_lock when calling the stream functions */
struct mwl_ampdu_stream *mwl_fwcmd_add_stream(struct ieee80211_hw *hw,
					      struct ieee80211_sta *sta,
					      u8 tid)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_ampdu_stream *stream;
	int i;

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

void mwl_fwcmd_del_sta_streams(struct ieee80211_hw *hw,
			       struct ieee80211_sta *sta)
{
	struct mwl_priv *priv = hw->priv;
	struct mwl_ampdu_stream *stream;
	int i;

	for (i = 0; i < SYSADPT_TX_AMPDU_QUEUES; i++) {
		stream = &priv->ampdu[i];

		if (stream->sta == sta) {
			mwl_fwcmd_destroy_ba(hw, stream->idx);
			mwl_fwcmd_remove_stream(hw, stream);
		}
	}
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
	struct mwl_priv *priv = hw->priv;
	struct mwl_ampdu_stream *stream;
	int i;

	for (i = 0; i < SYSADPT_TX_AMPDU_QUEUES; i++) {
		stream = &priv->ampdu[i];

		if (stream->state == AMPDU_NO_STREAM)
			continue;

		if (ether_addr_equal(stream->sta->addr, addr) &&
		    stream->tid == tid)
			return stream;
	}

	return NULL;
}

bool mwl_fwcmd_ampdu_allowed(struct ieee80211_sta *sta, u8 tid)
{
	struct mwl_sta *sta_info;
	struct mwl_tx_info *tx_stats;

	if (WARN_ON(tid >= SYSADPT_MAX_TID))
		return false;

	sta_info = mwl_dev_get_sta(sta);

	tx_stats = &sta_info->tx_stats[tid];

	return (sta_info->is_ampdu_allowed &&
		tx_stats->pkts > SYSADPT_AMPDU_PACKET_THRESHOLD);
}

int mwl_fwcmd_set_optimization_level(struct ieee80211_hw *hw, u8 opt_level)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_set_optimization_level *pcmd;

	pcmd = (struct hostcmd_cmd_set_optimization_level *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_OPTIMIZATION_LEVEL);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->opt_level = opt_level;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_OPTIMIZATION_LEVEL)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_wsc_ie(struct ieee80211_hw *hw, u8 len, u8 *data)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_set_wsc_ie *pcmd;

	pcmd = (struct hostcmd_cmd_set_wsc_ie *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_WSC_IE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->len = cpu_to_le16(len);
	memcpy(pcmd->data, data, len);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_WSC_IE)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	pcmd->ie_type = cpu_to_le16(WSC_IE_SET_PROBE_RESPONSE);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_WSC_IE)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_dwds_stamode(struct ieee80211_hw *hw, bool enable)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_dwds_enable *pcmd;

	pcmd = (struct hostcmd_cmd_dwds_enable *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_DWDS_ENABLE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->enable = cpu_to_le32(enable);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_DWDS_ENABLE)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_fw_flush_timer(struct ieee80211_hw *hw, u32 value)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_fw_flush_timer *pcmd;

	pcmd = (struct hostcmd_cmd_fw_flush_timer *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_FW_FLUSH_TIMER);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->value = cpu_to_le32(value);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_FW_FLUSH_TIMER)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_set_cdd(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_set_cdd *pcmd;

	pcmd = (struct hostcmd_cmd_set_cdd *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_SET_CDD);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->enable = cpu_to_le32(priv->cdd);

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_SET_CDD)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_reg_cau(struct ieee80211_hw *hw, u8 flag, u32 reg, u32 *val)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_bbp_reg_access *pcmd;

	pcmd = (struct hostcmd_cmd_bbp_reg_access *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_CAU_REG_ACCESS);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->offset = cpu_to_le16(reg);
	pcmd->action = cpu_to_le16(flag);
	pcmd->value = *val;

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_CAU_REG_ACCESS)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	*val = pcmd->value;

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_get_temp(struct ieee80211_hw *hw, u32 *temp)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_get_temp *pcmd;

	pcmd = (struct hostcmd_cmd_get_temp *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_GET_TEMP);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_GET_TEMP)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	*temp = le32_to_cpu(pcmd->celcius);

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_get_fw_region_code(struct ieee80211_hw *hw,
				 u32 *fw_region_code)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_get_fw_region_code *pcmd;
	u16 cmd;
	int status;

	pcmd = (struct hostcmd_cmd_get_fw_region_code *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	cmd = HOSTCMD_CMD_GET_FW_REGION_CODE;
	pcmd->cmd_hdr.cmd = cpu_to_le16(cmd);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, cmd)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	if (pcmd->cmd_hdr.result != 0) {
		mutex_unlock(&priv->fwcmd_mutex);
		return -EINVAL;
	}

	status = le32_to_cpu(pcmd->status);

	if (!status)
		*fw_region_code = le32_to_cpu(pcmd->fw_region_code);

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_get_device_pwr_tbl(struct ieee80211_hw *hw,
				 struct mwl_device_pwr_tbl *device_ch_pwrtbl,
				 u8 *region_code,
				 u8 *number_of_channels,
				 u32 channel_index)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_get_device_pwr_tbl *pcmd;
	int status;
	u16 cmd;

	pcmd = (struct hostcmd_cmd_get_device_pwr_tbl *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	cmd = HOSTCMD_CMD_GET_DEVICE_PWR_TBL;
	pcmd->cmd_hdr.cmd = cpu_to_le16(cmd);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->status = cpu_to_le16(cmd);
	pcmd->current_channel_index = cpu_to_le32(channel_index);

	if (mwl_fwcmd_exec_cmd(priv, cmd)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	device_ch_pwrtbl->channel = pcmd->channel_pwr_tbl.channel;
	memcpy(device_ch_pwrtbl->tx_pwr, pcmd->channel_pwr_tbl.tx_pwr,
	       SYSADPT_TX_POWER_LEVEL_TOTAL);
	device_ch_pwrtbl->dfs_capable = pcmd->channel_pwr_tbl.dfs_capable;
	device_ch_pwrtbl->ax_ant = pcmd->channel_pwr_tbl.ax_ant;
	device_ch_pwrtbl->cdd = pcmd->channel_pwr_tbl.cdd;
	*region_code = pcmd->region_code;
	*number_of_channels = pcmd->number_of_channels;
	status = le16_to_cpu(pcmd->status);

	mutex_unlock(&priv->fwcmd_mutex);

	return status;
}

int mwl_fwcmd_get_fw_region_code_sc4(struct ieee80211_hw *hw,
				     u32 *fw_region_code)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_get_fw_region_code_sc4 *pcmd;
	u16 cmd;

	pcmd = (struct hostcmd_cmd_get_fw_region_code_sc4 *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	cmd = HOSTCMD_CMD_GET_FW_REGION_CODE_SC4;
	pcmd->cmd_hdr.cmd = cpu_to_le16(cmd);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));

	if (mwl_fwcmd_exec_cmd(priv, cmd)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	if (pcmd->cmd_hdr.result != 0) {
		mutex_unlock(&priv->fwcmd_mutex);
		return -EINVAL;
	}

	*fw_region_code = le32_to_cpu(pcmd->fw_region_code);

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

int mwl_fwcmd_get_device_pwr_tbl_sc4(struct ieee80211_hw *hw,
				     struct mwl_device_pwr_tbl *device_ch_pwrtbl,
				     u8 *region_code,
				     u8 *number_of_channels,
				     u32 channel_index)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_get_device_pwr_tbl_sc4 *pcmd;
	int status;
	u16 cmd;

	pcmd = (struct hostcmd_cmd_get_device_pwr_tbl_sc4 *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	cmd = HOSTCMD_CMD_GET_DEVICE_PWR_TBL_SC4;
	pcmd->cmd_hdr.cmd = cpu_to_le16(cmd);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->status = cpu_to_le16(cmd);
	pcmd->current_channel_index = cpu_to_le32(channel_index);

	if (mwl_fwcmd_exec_cmd(priv, cmd)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	device_ch_pwrtbl->channel = pcmd->channel_pwr_tbl.channel;
	memcpy(device_ch_pwrtbl->tx_pwr, pcmd->channel_pwr_tbl.tx_pwr,
	       SYSADPT_TX_PWR_LEVEL_TOTAL_SC4);
	device_ch_pwrtbl->dfs_capable = pcmd->channel_pwr_tbl.dfs_capable;
	device_ch_pwrtbl->ax_ant = pcmd->channel_pwr_tbl.ax_ant;
	device_ch_pwrtbl->cdd = pcmd->channel_pwr_tbl.cdd;
	*region_code = pcmd->region_code;
	*number_of_channels = pcmd->number_of_channels;
	status = le16_to_cpu(pcmd->status);

	mutex_unlock(&priv->fwcmd_mutex);

	return status;
}

int mwl_fwcmd_quiet_mode(struct ieee80211_hw *hw, bool enable, u32 period,
			 u32 duration, u32 next_offset)
{
	struct mwl_priv *priv = hw->priv;
	struct hostcmd_cmd_quiet_mode *pcmd;

	pcmd = (struct hostcmd_cmd_quiet_mode *)&priv->pcmd_buf[0];

	mutex_lock(&priv->fwcmd_mutex);

	memset(pcmd, 0x00, sizeof(*pcmd));
	pcmd->cmd_hdr.cmd = cpu_to_le16(HOSTCMD_CMD_QUIET_MODE);
	pcmd->cmd_hdr.len = cpu_to_le16(sizeof(*pcmd));
	pcmd->action = cpu_to_le16(WL_SET);
	pcmd->enable = cpu_to_le32(enable);
	if (enable) {
		pcmd->period = cpu_to_le32(period);
		pcmd->duration = cpu_to_le32(duration);
		pcmd->next_offset = cpu_to_le32(next_offset);
	}

	if (mwl_fwcmd_exec_cmd(priv, HOSTCMD_CMD_QUIET_MODE)) {
		mutex_unlock(&priv->fwcmd_mutex);
		wiphy_err(hw->wiphy, "failed execution\n");
		return -EIO;
	}

	mutex_unlock(&priv->fwcmd_mutex);

	return 0;
}

void mwl_fwcmd_get_survey(struct ieee80211_hw *hw, int idx)
{
	struct mwl_priv *priv = hw->priv;
	struct ieee80211_conf *conf = &hw->conf;
	struct mwl_survey_info *survey_info;

	if (idx)
		survey_info = &priv->survey_info[idx - 1];
	else
		survey_info = &priv->cur_survey_info;

	memcpy(&survey_info->channel, conf->chandef.chan,
	       sizeof(struct ieee80211_channel));
	survey_info->filled = SURVEY_INFO_TIME |
			      SURVEY_INFO_TIME_BUSY |
			      SURVEY_INFO_TIME_TX |
			      SURVEY_INFO_NOISE_DBM;
	survey_info->time_period += pci_read_mac_reg(priv, MCU_LAST_READ);
	survey_info->time_busy += pci_read_mac_reg(priv, MCU_CCA_CNT);
	survey_info->time_tx += pci_read_mac_reg(priv, MCU_TXPE_CNT);
	survey_info->noise = priv->noise;
}