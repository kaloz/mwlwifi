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

/* Description:  This file implements main functions of this module. */

#include <linux/module.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif

#include "sysadpt.h"
#include "dev.h"
#include "fwdl.h"
#include "fwcmd.h"
#include "tx.h"
#include "rx.h"
#include "isr.h"
#include "thermal.h"
#ifdef CONFIG_DEBUG_FS
#include "debugfs.h"
#endif

#define MWL_DESC         "Marvell 802.11ac Wireless Network Driver"
#define MWL_DEV_NAME     "Marvell 802.11ac Adapter"

#define FILE_PATH_LEN    64
#define CMD_BUF_SIZE     0x4000

static struct pci_device_id mwl_pci_id_tbl[] = {
	{ PCI_VDEVICE(MARVELL, 0x2a55), .driver_data = MWL8864, },
	{ PCI_VDEVICE(MARVELL, 0x2b38), .driver_data = MWL8897, },
	{ },
};

static struct mwl_chip_info mwl_chip_tbl[] = {
	[MWL8864] = {
		.part_name	= "88W8864",
		.fw_image	= "mwlwifi/88W8864.bin",
		.antenna_tx	= ANTENNA_TX_4_AUTO,
		.antenna_rx	= ANTENNA_RX_4_AUTO,
	},
	[MWL8897] = {
		.part_name	= "88W8897",
		.fw_image	= "mwlwifi/88W8897.bin",
		.antenna_tx	= ANTENNA_TX_2,
		.antenna_rx	= ANTENNA_RX_2,
	},
};

static const struct ieee80211_channel mwl_channels_24[] = {
	{ .band = NL80211_BAND_2GHZ, .center_freq = 2412, .hw_value = 1, },
	{ .band = NL80211_BAND_2GHZ, .center_freq = 2417, .hw_value = 2, },
	{ .band = NL80211_BAND_2GHZ, .center_freq = 2422, .hw_value = 3, },
	{ .band = NL80211_BAND_2GHZ, .center_freq = 2427, .hw_value = 4, },
	{ .band = NL80211_BAND_2GHZ, .center_freq = 2432, .hw_value = 5, },
	{ .band = NL80211_BAND_2GHZ, .center_freq = 2437, .hw_value = 6, },
	{ .band = NL80211_BAND_2GHZ, .center_freq = 2442, .hw_value = 7, },
	{ .band = NL80211_BAND_2GHZ, .center_freq = 2447, .hw_value = 8, },
	{ .band = NL80211_BAND_2GHZ, .center_freq = 2452, .hw_value = 9, },
	{ .band = NL80211_BAND_2GHZ, .center_freq = 2457, .hw_value = 10, },
	{ .band = NL80211_BAND_2GHZ, .center_freq = 2462, .hw_value = 11, },
	{ .band = NL80211_BAND_2GHZ, .center_freq = 2467, .hw_value = 12, },
	{ .band = NL80211_BAND_2GHZ, .center_freq = 2472, .hw_value = 13, },
	{ .band = NL80211_BAND_2GHZ, .center_freq = 2484, .hw_value = 14, },
};

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

static const struct ieee80211_channel mwl_channels_50[] = {
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5180, .hw_value = 36, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5200, .hw_value = 40, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5220, .hw_value = 44, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5240, .hw_value = 48, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5260, .hw_value = 52, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5280, .hw_value = 56, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5300, .hw_value = 60, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5320, .hw_value = 64, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5500, .hw_value = 100, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5520, .hw_value = 104, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5540, .hw_value = 108, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5560, .hw_value = 112, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5580, .hw_value = 116, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5600, .hw_value = 120, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5620, .hw_value = 124, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5640, .hw_value = 128, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5660, .hw_value = 132, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5680, .hw_value = 136, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5700, .hw_value = 140, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5720, .hw_value = 144, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5745, .hw_value = 149, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5765, .hw_value = 153, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5785, .hw_value = 157, },
	{ .band = NL80211_BAND_5GHZ, .center_freq = 5805, .hw_value = 161, },
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

static const struct ieee80211_iface_limit ap_if_limits[] = {
	{ .max = SYSADPT_NUM_OF_AP,	.types = BIT(NL80211_IFTYPE_AP) },
	{ .max = 1,	.types = BIT(NL80211_IFTYPE_STATION) },
};

static const struct ieee80211_iface_combination ap_if_comb = {
	.limits = ap_if_limits,
	.n_limits = ARRAY_SIZE(ap_if_limits),
	.max_interfaces = SYSADPT_NUM_OF_AP,
	.num_different_channels = 1,
	.radar_detect_widths =	BIT(NL80211_CHAN_WIDTH_20_NOHT) |
				BIT(NL80211_CHAN_WIDTH_20) |
				BIT(NL80211_CHAN_WIDTH_40) |
				BIT(NL80211_CHAN_WIDTH_80),
};

struct region_code_mapping {
	const char *alpha2;
	u32 region_code;
};

static const struct region_code_mapping regmap[] = {
	{"US", 0x10}, /* US FCC */
	{"CA", 0x20}, /* Canada */
	{"EU", 0x30}, /* ETSI   */
	{"ES", 0x31}, /* Spain  */
	{"FR", 0x32}, /* France */
	{"JP", 0x40}, /* Japan  */
	{"TW", 0x80}, /* Taiwan */
	{"AU", 0x81}, /* Australia */
	{"CN", 0x90}, /* China (Asia) */
};

static int mwl_alloc_pci_resource(struct mwl_priv *priv)
{
	struct pci_dev *pdev = priv->pdev;
	void __iomem *addr;

	priv->next_bar_num = 1;	/* 32-bit */
	if (pci_resource_flags(pdev, 0) & 0x04)
		priv->next_bar_num = 2;	/* 64-bit */

	addr = devm_ioremap_resource(priv->dev, &pdev->resource[0]);
	if (IS_ERR(addr)) {
		wiphy_err(priv->hw->wiphy,
			  "%s: cannot reserve PCI memory region 0\n",
			  MWL_DRV_NAME);
		goto err;
	}
	priv->iobase0 = addr;
	wiphy_debug(priv->hw->wiphy, "priv->iobase0 = %p\n", priv->iobase0);

	addr = devm_ioremap_resource(priv->dev,
				     &pdev->resource[priv->next_bar_num]);
	if (IS_ERR(addr)) {
		wiphy_err(priv->hw->wiphy,
			  "%s: cannot reserve PCI memory region 1\n",
			  MWL_DRV_NAME);
		goto err;
	}
	priv->iobase1 = addr;
	wiphy_debug(priv->hw->wiphy, "priv->iobase1 = %p\n", priv->iobase1);

	priv->pcmd_buf =
		(unsigned short *)dmam_alloc_coherent(priv->dev,
						      CMD_BUF_SIZE,
						      &priv->pphys_cmd_buf,
						      GFP_KERNEL);
	if (!priv->pcmd_buf) {
		wiphy_err(priv->hw->wiphy,
			  "%s: cannot alloc memory for command buffer\n",
			  MWL_DRV_NAME);
		goto err;
	}
	wiphy_debug(priv->hw->wiphy,
		    "priv->pcmd_buf = %p  priv->pphys_cmd_buf = %p\n",
		    priv->pcmd_buf,
		    (void *)priv->pphys_cmd_buf);
	memset(priv->pcmd_buf, 0x00, CMD_BUF_SIZE);

	return 0;

err:
	wiphy_err(priv->hw->wiphy, "pci alloc fail\n");

	return -EIO;
}

static int mwl_init_firmware(struct mwl_priv *priv, const char *fw_name)
{
	int rc = 0;

	rc = request_firmware((const struct firmware **)&priv->fw_ucode,
			      fw_name, priv->dev);

	if (rc) {
		wiphy_err(priv->hw->wiphy,
			  "%s: cannot find firmware image <%s>\n",
				  MWL_DRV_NAME, fw_name);
		goto err_load_fw;
	}

	rc = mwl_fwdl_download_firmware(priv->hw);
	if (rc) {
		wiphy_err(priv->hw->wiphy,
			  "%s: cannot download firmware image <%s>\n",
			  MWL_DRV_NAME, fw_name);
		goto err_download_fw;
	}

	return rc;

err_download_fw:

	release_firmware(priv->fw_ucode);

err_load_fw:

	wiphy_err(priv->hw->wiphy, "firmware init fail\n");

	return rc;
}

static void mwl_reg_notifier(struct wiphy *wiphy,
			     struct regulatory_request *request)
{
	struct ieee80211_hw *hw;
	struct mwl_priv *priv;
#ifdef CONFIG_OF
	struct property *prop;
	struct property *fcc_prop = NULL;
	struct property *etsi_prop = NULL;
	struct property *specific_prop = NULL;
	u32 prop_value;
	int i, j, k;
#endif

	hw = (struct ieee80211_hw *)wiphy_priv(wiphy);
	priv = hw->priv;

	if (priv->forbidden_setting) {
		if (!priv->regulatory_set) {
			regulatory_hint(wiphy, priv->fw_alpha2);
			priv->regulatory_set = true;
		} else {
			if (memcmp(priv->fw_alpha2, request->alpha2, 2))
				regulatory_hint(wiphy, priv->fw_alpha2);
		}
		return;
	}

	priv->dfs_region = request->dfs_region;

#ifdef CONFIG_OF
	if (priv->pwr_node) {
		for_each_property_of_node(priv->pwr_node, prop) {
			if (strcmp(prop->name, "FCC") == 0)
				fcc_prop = prop;
			if (strcmp(prop->name, "ETSI") == 0)
				etsi_prop = prop;
			if ((prop->name[0] == request->alpha2[0]) &&
			    (prop->name[1] == request->alpha2[1]))
				specific_prop = prop;
		}

		prop = NULL;

		if (specific_prop) {
			prop = specific_prop;
		} else {
			if (priv->dfs_region == NL80211_DFS_ETSI)
				prop = etsi_prop;
			else
				prop = fcc_prop;
		}

		if (prop) {
			/* Reset the whole table */
			for (i = 0; i < SYSADPT_MAX_NUM_CHANNELS; i++)
				memset(&priv->tx_pwr_tbl[i], 0,
				       sizeof(struct mwl_tx_pwr_tbl));

			/* Load related power table */
			i = 0;
			j = 0;
			while (i < prop->length) {
				prop_value =
					be32_to_cpu(*(__be32 *)
						    (prop->value + i));
				priv->tx_pwr_tbl[j].channel = prop_value;
				i += 4;
				prop_value =
					be32_to_cpu(*(__be32 *)
						    (prop->value + i));
				priv->tx_pwr_tbl[j].setcap = prop_value;
				i += 4;
				for (k = 0; k < SYSADPT_TX_POWER_LEVEL_TOTAL;
				     k++) {
					prop_value =
						be32_to_cpu(*(__be32 *)
							    (prop->value + i));
					priv->tx_pwr_tbl[j].tx_power[k] =
						prop_value;
					i += 4;
				}
				prop_value =
					be32_to_cpu(*(__be32 *)
						    (prop->value + i));
				priv->tx_pwr_tbl[j].cdd =
					(prop_value == 0) ? false : true;
				i += 4;
				prop_value =
					be32_to_cpu(*(__be32 *)
						    (prop->value + i));
				priv->tx_pwr_tbl[j].txantenna2 = prop_value;
				i += 4;
				j++;
			}

			/* Dump loaded power tabel */
			wiphy_debug(hw->wiphy, "regdomain: %s\n", prop->name);
			for (i = 0; i < SYSADPT_MAX_NUM_CHANNELS; i++) {
				struct mwl_tx_pwr_tbl *pwr_tbl;
				char disp_buf[64];
				char *disp_ptr;

				pwr_tbl = &priv->tx_pwr_tbl[i];
				if (pwr_tbl->channel == 0)
					break;
				wiphy_debug(hw->wiphy,
					    "Channel: %d: 0x%x 0x%x 0x%x\n",
					    pwr_tbl->channel,
					    pwr_tbl->setcap,
					    pwr_tbl->cdd,
					    pwr_tbl->txantenna2);
				disp_ptr = disp_buf;
				for (j = 0; j < SYSADPT_TX_POWER_LEVEL_TOTAL;
				     j++) {
					disp_ptr +=
						sprintf(disp_ptr, "%x ",
							pwr_tbl->tx_power[j]);
				}
				wiphy_debug(hw->wiphy, "%s\n", disp_buf);
			}
		}
	}
#endif
}

static void mwl_process_of_dts(struct mwl_priv *priv)
{
#ifdef CONFIG_OF
	struct property *prop;
	u32 prop_value;

	priv->dt_node =
		of_find_node_by_name(pci_bus_to_OF_node(priv->pdev->bus),
				     "mwlwifi");
	if (!priv->dt_node)
		return;

	/* look for all matching property names */
	for_each_property_of_node(priv->dt_node, prop) {
		if (strcmp(prop->name, "marvell,2ghz") == 0)
			priv->disable_2g = true;
		if (strcmp(prop->name, "marvell,5ghz") == 0)
			priv->disable_5g = true;
		if (strcmp(prop->name, "marvell,chainmask") == 0) {
			prop_value = be32_to_cpu(*((__be32 *)prop->value));
			if (prop_value == 2)
				priv->antenna_tx = ANTENNA_TX_2;

			prop_value = be32_to_cpu(*((__be32 *)
						 (prop->value + 4)));
			if (prop_value == 2)
				priv->antenna_rx = ANTENNA_RX_2;
		}
	}

	priv->pwr_node = of_find_node_by_name(priv->dt_node,
					      "marvell,powertable");
#endif
}

static void mwl_set_ht_caps(struct mwl_priv *priv,
			    struct ieee80211_supported_band *band)
{
	struct ieee80211_hw *hw;

	hw = priv->hw;

	band->ht_cap.ht_supported = 1;

	band->ht_cap.cap |= IEEE80211_HT_CAP_LDPC_CODING;
	band->ht_cap.cap |= IEEE80211_HT_CAP_SUP_WIDTH_20_40;
	band->ht_cap.cap |= IEEE80211_HT_CAP_SM_PS;
	band->ht_cap.cap |= IEEE80211_HT_CAP_SGI_20;
	band->ht_cap.cap |= IEEE80211_HT_CAP_SGI_40;

	ieee80211_hw_set(hw, AMPDU_AGGREGATION);
	ieee80211_hw_set(hw, SUPPORTS_AMSDU_IN_AMPDU);
	band->ht_cap.ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K;
	band->ht_cap.ampdu_density = IEEE80211_HT_MPDU_DENSITY_4;

	band->ht_cap.mcs.rx_mask[0] = 0xff;
	band->ht_cap.mcs.rx_mask[1] = 0xff;
	if (priv->antenna_rx == ANTENNA_RX_4_AUTO)
		band->ht_cap.mcs.rx_mask[2] = 0xff;
	band->ht_cap.mcs.rx_mask[4] = 0x01;

	band->ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;
}

static void mwl_set_vht_caps(struct mwl_priv *priv,
			     struct ieee80211_supported_band *band)
{
	band->vht_cap.vht_supported = 1;

	band->vht_cap.cap |= IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_3895;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_RXLDPC;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_SHORT_GI_80;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_RXSTBC_1;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_RX_ANTENNA_PATTERN;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_TX_ANTENNA_PATTERN;

	if (priv->antenna_rx == ANTENNA_RX_2)
		band->vht_cap.vht_mcs.rx_mcs_map = cpu_to_le16(0xfffa);
	else
		band->vht_cap.vht_mcs.rx_mcs_map = cpu_to_le16(0xffea);

	if (priv->antenna_tx == ANTENNA_TX_2)
		band->vht_cap.vht_mcs.tx_mcs_map = cpu_to_le16(0xfffa);
	else
		band->vht_cap.vht_mcs.tx_mcs_map = cpu_to_le16(0xffea);
}

static void mwl_set_caps(struct mwl_priv *priv)
{
	struct ieee80211_hw *hw;

	hw = priv->hw;

	/* set up band information for 2.4G */
	if (!priv->disable_2g) {
		BUILD_BUG_ON(sizeof(priv->channels_24) !=
			     sizeof(mwl_channels_24));
		memcpy(priv->channels_24, mwl_channels_24,
		       sizeof(mwl_channels_24));

		BUILD_BUG_ON(sizeof(priv->rates_24) != sizeof(mwl_rates_24));
		memcpy(priv->rates_24, mwl_rates_24, sizeof(mwl_rates_24));

		priv->band_24.band = NL80211_BAND_2GHZ;
		priv->band_24.channels = priv->channels_24;
		priv->band_24.n_channels = ARRAY_SIZE(mwl_channels_24);
		priv->band_24.bitrates = priv->rates_24;
		priv->band_24.n_bitrates = ARRAY_SIZE(mwl_rates_24);

		mwl_set_ht_caps(priv, &priv->band_24);
		mwl_set_vht_caps(priv, &priv->band_24);

		hw->wiphy->bands[NL80211_BAND_2GHZ] = &priv->band_24;
	}

	/* set up band information for 5G */
	if (!priv->disable_5g) {
		BUILD_BUG_ON(sizeof(priv->channels_50) !=
			     sizeof(mwl_channels_50));
		memcpy(priv->channels_50, mwl_channels_50,
		       sizeof(mwl_channels_50));

		BUILD_BUG_ON(sizeof(priv->rates_50) != sizeof(mwl_rates_50));
		memcpy(priv->rates_50, mwl_rates_50, sizeof(mwl_rates_50));

		priv->band_50.band = NL80211_BAND_5GHZ;
		priv->band_50.channels = priv->channels_50;
		priv->band_50.n_channels = ARRAY_SIZE(mwl_channels_50);
		priv->band_50.bitrates = priv->rates_50;
		priv->band_50.n_bitrates = ARRAY_SIZE(mwl_rates_50);

		mwl_set_ht_caps(priv, &priv->band_50);
		mwl_set_vht_caps(priv, &priv->band_50);

		hw->wiphy->bands[NL80211_BAND_5GHZ] = &priv->band_50;
	}
}

static void mwl_regd_init(struct mwl_priv *priv)
{
	u8 region_code;
	int i;

	/* hook regulatory domain change notification */
	priv->hw->wiphy->reg_notifier = mwl_reg_notifier;

	if (mwl_fwcmd_get_device_pwr_tbl(priv->hw,
					 &priv->device_pwr_tbl[0],
					 &region_code,
					 &priv->number_of_channels,
					 0))
		return;

	priv->forbidden_setting = true;

	for (i = 0; i < priv->number_of_channels; i++)
		mwl_fwcmd_get_device_pwr_tbl(priv->hw,
					     &priv->device_pwr_tbl[i],
					     &region_code,
					     &priv->number_of_channels,
					     i);

	for (i = 0; i < ARRAY_SIZE(regmap); i++)
		if (regmap[i].region_code == priv->fw_region_code) {
			memcpy(priv->fw_alpha2, regmap[i].alpha2, 2);
			break;
		}
}

static void timer_routine(unsigned long data)
{
	struct mwl_priv *priv = (struct mwl_priv *)data;
	struct mwl_ampdu_stream *stream;
	struct mwl_sta *sta_info;
	struct mwl_tx_info *tx_stats;
	int i;

	spin_lock_bh(&priv->stream_lock);
	for (i = 0; i < SYSADPT_TX_AMPDU_QUEUES; i++) {
		stream = &priv->ampdu[i];

		if (stream->state == AMPDU_STREAM_ACTIVE) {
			sta_info = mwl_dev_get_sta(stream->sta);
			tx_stats = &sta_info->tx_stats[stream->tid];

			if ((jiffies - tx_stats->start_time > HZ) &&
			    (tx_stats->pkts < SYSADPT_AMPDU_PACKET_THRESHOLD)) {
				ieee80211_stop_tx_ba_session(stream->sta,
							     stream->tid);
			}

			if (jiffies - tx_stats->start_time > HZ) {
				tx_stats->pkts = 0;
				tx_stats->start_time = jiffies;
			}
		}
	}
	spin_unlock_bh(&priv->stream_lock);

	mod_timer(&priv->period_timer, jiffies +
		  msecs_to_jiffies(SYSADPT_TIMER_WAKEUP_TIME));
}

static int mwl_wl_init(struct mwl_priv *priv)
{
	struct ieee80211_hw *hw;
	int rc;
	int i;

	hw = priv->hw;

	hw->extra_tx_headroom = SYSADPT_MIN_BYTES_HEADROOM;
	hw->queues = SYSADPT_TX_WMM_QUEUES;

	/* Set rssi values to dBm */
	ieee80211_hw_set(hw, SIGNAL_DBM);
	ieee80211_hw_set(hw, HAS_RATE_CONTROL);

	/* Ask mac80211 not to trigger PS mode
	 * based on PM bit of incoming frames.
	 */
	ieee80211_hw_set(hw, AP_LINK_PS);

	ieee80211_hw_set(hw, SUPPORTS_PER_STA_GTK);
	ieee80211_hw_set(hw, MFP_CAPABLE);

	hw->wiphy->flags |= WIPHY_FLAG_IBSS_RSN;
	hw->wiphy->flags |= WIPHY_FLAG_HAS_CHANNEL_SWITCH;

	hw->wiphy->flags |= WIPHY_FLAG_SUPPORTS_TDLS;

	hw->vif_data_size = sizeof(struct mwl_vif);
	hw->sta_data_size = sizeof(struct mwl_sta);

	priv->ap_macids_supported = 0x0000ffff;
	priv->sta_macids_supported = 0x00010000;
	priv->macids_used = 0;
	INIT_LIST_HEAD(&priv->vif_list);
	INIT_LIST_HEAD(&priv->sta_list);

	/* Set default radio state, preamble and wmm */
	priv->radio_on = false;
	priv->radio_short_preamble = false;
	priv->wmm_enabled = false;

	priv->powinited = 0;

	priv->csa_active = false;
	priv->dfs_chirp_count_min = 5;
	priv->dfs_chirp_time_interval = 1000;
	priv->dfs_pw_filter = 0;
	priv->dfs_min_num_radar = 5;
	priv->dfs_min_pri_count = 4;

	/* Handle watchdog ba events */
	INIT_WORK(&priv->watchdog_ba_handle, mwl_watchdog_ba_events);
	INIT_WORK(&priv->chnl_switch_handle, mwl_chnl_switch_event);

	tasklet_init(&priv->tx_task, (void *)mwl_tx_skbs, (unsigned long)hw);
	tasklet_disable(&priv->tx_task);
	tasklet_init(&priv->tx_done_task,
		     (void *)mwl_tx_done, (unsigned long)hw);
	tasklet_disable(&priv->tx_done_task);
	tasklet_init(&priv->rx_task, (void *)mwl_rx_recv, (unsigned long)hw);
	tasklet_disable(&priv->rx_task);
	tasklet_init(&priv->qe_task,
		     (void *)mwl_tx_flush_amsdu, (unsigned long)hw);
	tasklet_disable(&priv->qe_task);
	priv->txq_limit = SYSADPT_TX_QUEUE_LIMIT;
	priv->is_tx_done_schedule = false;
	priv->recv_limit = SYSADPT_RECEIVE_LIMIT;
	priv->is_rx_schedule = false;
	priv->is_qe_schedule = false;
	priv->qe_trigger_num = 0;
	priv->qe_trigger_time = jiffies;

	mutex_init(&priv->fwcmd_mutex);
	spin_lock_init(&priv->tx_desc_lock);
	spin_lock_init(&priv->vif_lock);
	spin_lock_init(&priv->sta_lock);
	spin_lock_init(&priv->stream_lock);

	rc = mwl_thermal_register(priv);
	if (rc) {
		wiphy_err(hw->wiphy, "%s: fail to register thermal framework\n",
			  MWL_DRV_NAME);
		goto err_thermal_register;
	}

	rc = mwl_tx_init(hw);
	if (rc) {
		wiphy_err(hw->wiphy, "%s: fail to initialize TX\n",
			  MWL_DRV_NAME);
		goto err_mwl_tx_init;
	}

	rc = mwl_rx_init(hw);
	if (rc) {
		wiphy_err(hw->wiphy, "%s: fail to initialize RX\n",
			  MWL_DRV_NAME);
		goto err_mwl_rx_init;
	}

	rc = mwl_fwcmd_get_hw_specs(hw);
	if (rc) {
		wiphy_err(hw->wiphy, "%s: fail to get HW specifications\n",
			  MWL_DRV_NAME);
		goto err_get_hw_specs;
	}

	SET_IEEE80211_PERM_ADDR(hw, priv->hw_data.mac_addr);

	writel(priv->desc_data[0].pphys_tx_ring,
	       priv->iobase0 + priv->desc_data[0].wcb_base);
	for (i = 1; i < SYSADPT_TOTAL_TX_QUEUES; i++)
		writel(priv->desc_data[i].pphys_tx_ring,
		       priv->iobase0 + priv->desc_data[i].wcb_base);
	writel(priv->desc_data[0].pphys_rx_ring,
	       priv->iobase0 + priv->desc_data[0].rx_desc_read);
	writel(priv->desc_data[0].pphys_rx_ring,
	       priv->iobase0 + priv->desc_data[0].rx_desc_write);

	rc = mwl_fwcmd_set_hw_specs(hw);
	if (rc) {
		wiphy_err(hw->wiphy, "%s: fail to set HW specifications\n",
			  MWL_DRV_NAME);
		goto err_set_hw_specs;
	}

	wiphy_info(hw->wiphy,
		   "firmware version: 0x%x\n", priv->hw_data.fw_release_num);

	if (!mwl_fwcmd_get_fw_region_code(hw, &priv->fw_region_code)) {
		priv->fw_device_pwrtbl = true;
		mwl_regd_init(priv);
		wiphy_info(hw->wiphy,
			   "firmware region code: %x\n", priv->fw_region_code);
	}

	mwl_fwcmd_radio_disable(hw);

	mwl_fwcmd_rf_antenna(hw, WL_ANTENNATYPE_TX, priv->antenna_tx);

	mwl_fwcmd_rf_antenna(hw, WL_ANTENNATYPE_RX, priv->antenna_rx);

	hw->wiphy->interface_modes = 0;
	hw->wiphy->interface_modes |= BIT(NL80211_IFTYPE_AP);
	hw->wiphy->interface_modes |= BIT(NL80211_IFTYPE_STATION);
	hw->wiphy->iface_combinations = &ap_if_comb;
	hw->wiphy->n_iface_combinations = 1;

	mwl_set_caps(priv);

	rc = ieee80211_register_hw(hw);
	if (rc) {
		wiphy_err(hw->wiphy, "%s: fail to register device\n",
			  MWL_DRV_NAME);
		goto err_register_hw;
	}

	rc = request_irq(priv->pdev->irq, mwl_isr,
			 IRQF_SHARED, MWL_DRV_NAME, hw);
	if (rc) {
		priv->irq = -1;
		wiphy_err(hw->wiphy, "fail to register IRQ handler\n");
		goto err_register_irq;
	}
	priv->irq = priv->pdev->irq;

	setup_timer(&priv->period_timer, timer_routine, (unsigned long)priv);
	mod_timer(&priv->period_timer, jiffies +
		  msecs_to_jiffies(SYSADPT_TIMER_WAKEUP_TIME));

	return rc;

err_register_irq:
err_register_hw:
err_set_hw_specs:
err_get_hw_specs:

	mwl_rx_deinit(hw);

err_mwl_rx_init:

	mwl_tx_deinit(hw);

err_mwl_tx_init:
err_thermal_register:

	wiphy_err(hw->wiphy, "init fail\n");

	return rc;
}

static void mwl_wl_deinit(struct mwl_priv *priv)
{
	struct ieee80211_hw *hw = priv->hw;

	del_timer_sync(&priv->period_timer);

	if (priv->irq != -1) {
		free_irq(priv->pdev->irq, hw);
		priv->irq = -1;
	}

	ieee80211_unregister_hw(hw);
	mwl_thermal_unregister(priv);
	mwl_rx_deinit(hw);
	mwl_tx_deinit(hw);
	tasklet_kill(&priv->qe_task);
	tasklet_kill(&priv->rx_task);
	tasklet_kill(&priv->tx_done_task);
	tasklet_kill(&priv->tx_task);
	cancel_work_sync(&priv->watchdog_ba_handle);
	mwl_fwcmd_reset(hw);
}

static int mwl_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	static bool printed_version;
	struct ieee80211_hw *hw;
	struct mwl_priv *priv;
	const char *fw_name;
	int rc = 0;

	if (id->driver_data >= MWLUNKNOWN)
		return -ENODEV;

	if (!printed_version) {
		pr_info("<<%s version %s>>",
			MWL_DESC, MWL_DRV_VERSION);
		printed_version = true;
	}

	rc = pci_enable_device(pdev);
	if (rc) {
		pr_err("%s: cannot enable new PCI device",
		       MWL_DRV_NAME);
		return rc;
	}

	rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
	if (rc) {
		pr_err("%s: 32-bit PCI DMA not supported",
		       MWL_DRV_NAME);
		goto err_pci_disable_device;
	}

	pci_set_master(pdev);

	hw = ieee80211_alloc_hw(sizeof(*priv), &mwl_mac80211_ops);
	if (!hw) {
		pr_err("%s: ieee80211 alloc failed",
		       MWL_DRV_NAME);
		rc = -ENOMEM;
		goto err_pci_disable_device;
	}

	pci_set_drvdata(pdev, hw);

	priv = hw->priv;
	priv->hw = hw;
	priv->pdev = pdev;
	priv->dev = &pdev->dev;
	priv->chip_type = id->driver_data;
	priv->fw_device_pwrtbl = false;
	priv->forbidden_setting = false;
	priv->regulatory_set = false;
	priv->disable_2g = false;
	priv->disable_5g = false;
	priv->antenna_tx = mwl_chip_tbl[priv->chip_type].antenna_tx;
	priv->antenna_rx = mwl_chip_tbl[priv->chip_type].antenna_rx;

	SET_IEEE80211_DEV(hw, priv->dev);

	rc = mwl_alloc_pci_resource(priv);
	if (rc)
		goto err_alloc_pci_resource;

	fw_name = mwl_chip_tbl[priv->chip_type].fw_image;

	rc = mwl_init_firmware(priv, fw_name);

	if (rc) {
		wiphy_err(hw->wiphy, "%s: fail to initialize firmware\n",
			  MWL_DRV_NAME);
		goto err_init_firmware;
	}

	/* firmware is loaded to H/W, it can be released now */
	release_firmware(priv->fw_ucode);

	mwl_process_of_dts(priv);

	rc = mwl_wl_init(priv);
	if (rc) {
		wiphy_err(hw->wiphy, "%s: fail to initialize wireless lan\n",
			  MWL_DRV_NAME);
		goto err_wl_init;
	}

	wiphy_info(priv->hw->wiphy, "2G %s, 5G %s\n",
		   priv->disable_2g ? "disabled" : "enabled",
		   priv->disable_5g ? "disabled" : "enabled");

	wiphy_info(priv->hw->wiphy, "%s TX antennas, %s RX antennas\n",
		   (priv->antenna_tx == ANTENNA_TX_4_AUTO) ? "4" : "2",
		   (priv->antenna_rx == ANTENNA_RX_4_AUTO) ? "4" : "2");

#ifdef CONFIG_DEBUG_FS
	mwl_debugfs_init(hw);
#endif

	return rc;

err_wl_init:
err_init_firmware:

	mwl_fwcmd_reset(hw);

err_alloc_pci_resource:

	pci_set_drvdata(pdev, NULL);
	ieee80211_free_hw(hw);

err_pci_disable_device:

	pci_disable_device(pdev);

	return rc;
}

static void mwl_remove(struct pci_dev *pdev)
{
	struct ieee80211_hw *hw = pci_get_drvdata(pdev);
	struct mwl_priv *priv;

	if (!hw)
		return;

	priv = hw->priv;

	mwl_wl_deinit(priv);
	pci_set_drvdata(pdev, NULL);
	ieee80211_free_hw(hw);
	pci_disable_device(pdev);

#ifdef CONFIG_DEBUG_FS
	mwl_debugfs_remove(hw);
#endif
}

static struct pci_driver mwl_pci_driver = {
	.name     = MWL_DRV_NAME,
	.id_table = mwl_pci_id_tbl,
	.probe    = mwl_probe,
	.remove   = mwl_remove
};

module_pci_driver(mwl_pci_driver);

MODULE_DESCRIPTION(MWL_DESC);
MODULE_VERSION(MWL_DRV_VERSION);
MODULE_AUTHOR("Marvell Semiconductor, Inc.");
MODULE_LICENSE("GPL v2");
MODULE_SUPPORTED_DEVICE(MWL_DEV_NAME);
MODULE_DEVICE_TABLE(pci, mwl_pci_id_tbl);
