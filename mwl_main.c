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
*   Description:  This file implements main functions of this module.
*
*/

#include <linux/module.h>
#include <linux/moduleparam.h>

#include "mwl_sysadpt.h"
#include "mwl_dev.h"
#include "mwl_debug.h"
#include "mwl_fwdl.h"
#include "mwl_fwcmd.h"
#include "mwl_tx.h"
#include "mwl_rx.h"
#include "mwl_mac80211.h"

/* CONSTANTS AND MACROS
*/

#define MWL_DESC         "Marvell 802.11ac Wireless Network Driver"
#define MWL_DEV_NAME     "Marvell 88W8864 802.11ac Adapter"
#define MWL_DRV_NAME     KBUILD_MODNAME
#define MWL_DRV_VERSION	 "10.2.8.5.p0"

#define FILE_PATH_LEN    64
#define CMD_BUF_SIZE     0x4000

#define INVALID_WATCHDOG 0xAA


/* PRIVATE FUNCTION DECLARATION
*/

static int mwl_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static void mwl_remove(struct pci_dev *pdev);
static int mwl_alloc_pci_resource(struct mwl_priv *priv);
static void mwl_free_pci_resource(struct mwl_priv *priv);
static int mwl_init_firmware(struct mwl_priv *priv, char *fw_image);
static void mwl_reg_notifier(struct wiphy *wiphy,
			     struct regulatory_request *request);
static int mwl_process_of_dts(struct mwl_priv *priv);
static void mwl_set_ht_caps(struct mwl_priv *priv,
			    struct ieee80211_supported_band *band);
static void mwl_set_vht_caps(struct mwl_priv *priv,
			     struct ieee80211_supported_band *band);
static void mwl_set_caps(struct mwl_priv *priv);
static int mwl_wl_init(struct mwl_priv *priv);
static void mwl_wl_deinit(struct mwl_priv *priv);
static void mwl_watchdog_ba_events(struct work_struct *work);
static irqreturn_t mwl_interrupt(int irq, void *dev_id);


/* PRIVATE VARIABLES
*/

static char fw_image_path[FILE_PATH_LEN] = "mwlwifi/88W8864.bin";

static struct pci_device_id mwl_pci_id_tbl[SYSADPT_MAX_CARDS_SUPPORT + 1] = {
	{ 0x11ab, 0x2a55, PCI_ANY_ID, PCI_ANY_ID, 0, 0, (unsigned long)MWL_DEV_NAME },
	{ 0, 0, 0, 0, 0, 0, 0 }
};

static struct pci_driver mwl_pci_driver = {
	.name     = MWL_DRV_NAME,
	.id_table = mwl_pci_id_tbl,
	.probe    = mwl_probe,
	.remove   = mwl_remove
};

static const struct ieee80211_channel mwl_channels_24[] = {
	{ .band = IEEE80211_BAND_2GHZ, .center_freq = 2412, .hw_value = 1, },
	{ .band = IEEE80211_BAND_2GHZ, .center_freq = 2417, .hw_value = 2, },
	{ .band = IEEE80211_BAND_2GHZ, .center_freq = 2422, .hw_value = 3, },
	{ .band = IEEE80211_BAND_2GHZ, .center_freq = 2427, .hw_value = 4, },
	{ .band = IEEE80211_BAND_2GHZ, .center_freq = 2432, .hw_value = 5, },
	{ .band = IEEE80211_BAND_2GHZ, .center_freq = 2437, .hw_value = 6, },
	{ .band = IEEE80211_BAND_2GHZ, .center_freq = 2442, .hw_value = 7, },
	{ .band = IEEE80211_BAND_2GHZ, .center_freq = 2447, .hw_value = 8, },
	{ .band = IEEE80211_BAND_2GHZ, .center_freq = 2452, .hw_value = 9, },
	{ .band = IEEE80211_BAND_2GHZ, .center_freq = 2457, .hw_value = 10, },
	{ .band = IEEE80211_BAND_2GHZ, .center_freq = 2462, .hw_value = 11, },
	{ .band = IEEE80211_BAND_2GHZ, .center_freq = 2467, .hw_value = 12, },
	{ .band = IEEE80211_BAND_2GHZ, .center_freq = 2472, .hw_value = 13, },
	{ .band = IEEE80211_BAND_2GHZ, .center_freq = 2484, .hw_value = 14, },
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
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5180, .hw_value = 36, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5200, .hw_value = 40, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5220, .hw_value = 44, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5240, .hw_value = 48, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5260, .hw_value = 52, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5280, .hw_value = 56, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5300, .hw_value = 60, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5320, .hw_value = 64, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5500, .hw_value = 100, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5520, .hw_value = 104, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5540, .hw_value = 108, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5560, .hw_value = 112, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5580, .hw_value = 116, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5600, .hw_value = 120, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5620, .hw_value = 124, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5640, .hw_value = 128, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5660, .hw_value = 132, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5680, .hw_value = 136, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5700, .hw_value = 140, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5720, .hw_value = 144, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5745, .hw_value = 149, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5765, .hw_value = 153, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5785, .hw_value = 157, },
	{ .band = IEEE80211_BAND_5GHZ, .center_freq = 5805, .hw_value = 161, },
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
};


/* PRIVATE FUNCTION DEFINITION
*/

static int mwl_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	static bool printed_version = false;
	struct ieee80211_hw *hw;
	struct mwl_priv *priv;
	int rc = 0;

	WLDBG_ENTER(DBG_LEVEL_0);

	if (!printed_version) {
		WLDBG_PRINT("<<%s version %s>>", MWL_DESC, MWL_DRV_VERSION);
		printed_version = true;
	}

	rc = pci_enable_device(pdev);
	if (rc) {
		WLDBG_PRINT("%s: cannot enable new PCI device",
			    MWL_DRV_NAME);
		WLDBG_EXIT_INFO(DBG_LEVEL_0, "init error");
		return rc;
	}

	rc = pci_set_dma_mask(pdev, 0xffffffff);
	if (rc) {
		WLDBG_PRINT("%s: 32-bit PCI DMA not supported",
			    MWL_DRV_NAME);
		goto err_pci_disable_device;
	}

	pci_set_master(pdev);

	hw = ieee80211_alloc_hw(sizeof(*priv), mwl_mac80211_get_ops());
	if (hw == NULL) {
		WLDBG_PRINT("%s: ieee80211 alloc failed",
			    MWL_DRV_NAME);
		rc = -ENOMEM;
		goto err_pci_disable_device;
	}

	/* hook regulatory domain change notification
	*/
	hw->wiphy->reg_notifier = mwl_reg_notifier;

	/* set interrupt service routine to mac80211 module
	*/
	mwl_mac80211_set_isr(mwl_interrupt);

	SET_IEEE80211_DEV(hw, &pdev->dev);
	pci_set_drvdata(pdev, hw);

	priv = hw->priv;
	priv->hw = hw;
	priv->pdev = pdev;

	rc = mwl_alloc_pci_resource(priv);
	if (rc)
		goto err_alloc_pci_resource;

	rc = mwl_init_firmware(priv, fw_image_path);
	if (rc) {
		WLDBG_PRINT("%s: fail to initialize firmware",
			    MWL_DRV_NAME);
		goto err_init_firmware;
	}

	/* firmware is loaded to H/W, it can be released now
	*/
	release_firmware(priv->fw_ucode);

	rc = mwl_process_of_dts(priv);
	if (rc) {
		WLDBG_PRINT("%s: fail to load dts mwlwifi parameters",
			    MWL_DRV_NAME);
		goto err_process_of_dts;
	}

	rc = mwl_wl_init(priv);
	if (rc) {
		WLDBG_PRINT("%s: fail to initialize wireless lan",
			    MWL_DRV_NAME);
		goto err_wl_init;
	}

	WLDBG_EXIT(DBG_LEVEL_0);

	return rc;

err_wl_init:
err_process_of_dts:
err_init_firmware:

	mwl_fwcmd_reset(hw);

err_alloc_pci_resource:

	pci_set_drvdata(pdev, NULL);
	ieee80211_free_hw(hw);

err_pci_disable_device:

	pci_disable_device(pdev);

	WLDBG_EXIT_INFO(DBG_LEVEL_0, "init error");

	return rc;
}

static void mwl_remove(struct pci_dev *pdev)
{
	struct ieee80211_hw *hw = pci_get_drvdata(pdev);
	struct mwl_priv *priv;

	WLDBG_ENTER(DBG_LEVEL_0);

	if (hw == NULL) {
		WLDBG_EXIT_INFO(DBG_LEVEL_0, "ieee80211 hw is null");
		return;
	}

	priv = hw->priv;
	BUG_ON(!priv);

	mwl_wl_deinit(priv);
	mwl_free_pci_resource(priv);
	pci_set_drvdata(pdev, NULL);
	ieee80211_free_hw(hw);
	pci_disable_device(pdev);

	WLDBG_EXIT(DBG_LEVEL_0);
}

static int mwl_alloc_pci_resource(struct mwl_priv *priv)
{
	struct pci_dev *pdev;
	u32 phys_addr = 0;
	u32 flags;
	void *phys_addr1[2];
	void *phys_addr2[2];

	WLDBG_ENTER(DBG_LEVEL_0);

	BUG_ON(!priv);
	pdev = priv->pdev;
	BUG_ON(!pdev);

	phys_addr = pci_resource_start(pdev, 0);
	flags = pci_resource_flags(pdev, 0);

	priv->next_bar_num = 1;	/* 32-bit */

	if (flags & 0x04)
		priv->next_bar_num = 2;	/* 64-bit */

	if (!request_mem_region(phys_addr, pci_resource_len(pdev, 0), MWL_DRV_NAME)) {
		WLDBG_ERROR(DBG_LEVEL_0, "%s: cannot reserve PCI memory region 0",
			    MWL_DRV_NAME);
		goto err_reserve_mem_region_bar0;
	}

	phys_addr1[0] = ioremap(phys_addr, pci_resource_len(pdev, 0));
	phys_addr1[1] = 0;

	priv->iobase0 = phys_addr1[0];
	if (!priv->iobase0) {
		WLDBG_ERROR(DBG_LEVEL_0, "%s: cannot remap PCI memory region 0",
			    MWL_DRV_NAME);
		goto err_release_mem_region_bar0;
	}

	WLDBG_PRINT("priv->iobase0 = %x", (unsigned int)priv->iobase0);

	phys_addr = pci_resource_start(pdev, priv->next_bar_num);

	if (!request_mem_region(phys_addr, pci_resource_len(pdev, priv->next_bar_num), MWL_DRV_NAME)) {
		WLDBG_ERROR(DBG_LEVEL_0, "%s: cannot reserve PCI memory region 1",
			    MWL_DRV_NAME);
		goto err_iounmap_iobase0;
	}

	phys_addr2[0] = ioremap(phys_addr, pci_resource_len(pdev, priv->next_bar_num));
	phys_addr2[1] = 0;
	priv->iobase1 = phys_addr2[0];

	if (!priv->iobase1) {
		WLDBG_ERROR(DBG_LEVEL_0, "%s: cannot remap PCI memory region 1",
			    MWL_DRV_NAME);
		goto err_release_mem_region_bar1;
	}

	WLDBG_PRINT("priv->iobase1 = %x", (unsigned int)priv->iobase1);

	priv->pcmd_buf = (unsigned short *)
		dma_alloc_coherent(&priv->pdev->dev, CMD_BUF_SIZE, &priv->pphys_cmd_buf, GFP_KERNEL);

	if (priv->pcmd_buf == NULL) {
		WLDBG_ERROR(DBG_LEVEL_0, "%s: cannot alloc memory for command buffer",
			    MWL_DRV_NAME);
		goto err_iounmap_iobase1;
	}

	WLDBG_PRINT("priv->pcmd_buf = %x  priv->pphys_cmd_buf = %x",
		    (unsigned int)priv->pcmd_buf, (unsigned int)priv->pphys_cmd_buf);

	memset(priv->pcmd_buf, 0x00, CMD_BUF_SIZE);

	WLDBG_EXIT(DBG_LEVEL_0);

	return 0;

err_iounmap_iobase1:

	iounmap(priv->iobase1);

err_release_mem_region_bar1:

	release_mem_region(pci_resource_start(pdev, 1), pci_resource_len(pdev, 1));

err_iounmap_iobase0:

	iounmap(priv->iobase0);

err_release_mem_region_bar0:

	release_mem_region(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));

err_reserve_mem_region_bar0:

	WLDBG_EXIT_INFO(DBG_LEVEL_0, "pci alloc fail");

	return -EIO;

}

static void mwl_free_pci_resource(struct mwl_priv *priv)
{
	struct pci_dev *pdev;

	WLDBG_ENTER(DBG_LEVEL_0);

	BUG_ON(!priv);
	pdev = priv->pdev;
	BUG_ON(!pdev);

	iounmap(priv->iobase0);
	iounmap(priv->iobase1);
	release_mem_region(pci_resource_start(pdev, priv->next_bar_num), pci_resource_len(pdev, priv->next_bar_num));
	release_mem_region(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
	dma_free_coherent(&priv->pdev->dev, CMD_BUF_SIZE, priv->pcmd_buf, priv->pphys_cmd_buf);

	WLDBG_EXIT(DBG_LEVEL_0);
}

static int mwl_init_firmware(struct mwl_priv *priv, char *fw_name)
{
	struct pci_dev *pdev;
	int rc = 0;

	WLDBG_ENTER(DBG_LEVEL_0);

	BUG_ON(!priv);
	pdev = priv->pdev;
	BUG_ON(!pdev);

	rc = request_firmware(&priv->fw_ucode, fw_name, &priv->pdev->dev);
	if (rc) {
		WLDBG_ERROR(DBG_LEVEL_0, "%s: cannot load firmware image <%s>",
			    MWL_DRV_NAME, fw_name);
		goto err_load_fw;
	}

	rc = mwl_fwdl_download_firmware(priv->hw);
	if (rc) {
		WLDBG_ERROR(DBG_LEVEL_0, "%s: cannot download firmware image <%s>",
			    MWL_DRV_NAME, fw_name);
		goto err_download_fw;
	}

	WLDBG_EXIT(DBG_LEVEL_0);

	return rc;

err_download_fw:

	release_firmware(priv->fw_ucode);

err_load_fw:

	WLDBG_EXIT_INFO(DBG_LEVEL_0, "firmware init fail");

	return rc;
}

static void mwl_reg_notifier(struct wiphy *wiphy,
			     struct regulatory_request *request)
{
	struct ieee80211_hw *hw;
	struct mwl_priv *priv;
	struct property *prop;
	struct property *fcc_prop = NULL;
	struct property *etsi_prop = NULL;
	struct property *specific_prop = NULL;
	u32 prop_value;
	int i, j, k;

	WLDBG_ENTER(DBG_LEVEL_0);

	BUG_ON(!wiphy);
	hw = (struct ieee80211_hw *) wiphy_priv(wiphy);
	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	if (priv->pwr_node != NULL) {
		for_each_property_of_node(priv->pwr_node, prop) {
			if(strcmp(prop->name, "FCC") == 0)
				fcc_prop = prop;
			if (strcmp(prop->name, "ETSI") == 0)
				etsi_prop = prop;
			if ((prop->name[0] == request->alpha2[0]) &&
			    (prop->name[1] == request->alpha2[1]))
			    	specific_prop = prop;
		}

		prop = NULL;

		if (specific_prop != NULL) {
			prop = specific_prop;
		} else {
			if (request->dfs_region == NL80211_DFS_ETSI)
				prop = etsi_prop;
			else
				prop = fcc_prop;
		}

		if (prop != NULL) {
			/* Reset the whole table
			*/
			for (i = 0; i < SYSADPT_MAX_NUM_CHANNELS; i++)
				memset(&priv->tx_pwr_tbl[i], 0,
				       sizeof(struct mwl_tx_pwr_tbl));
			
			/* Load related power table
			*/
			i = 0;
			j = 0;
			while (i < prop->length) {
				prop_value = be32_to_cpu(*(u32 *)(prop->value + i));
				priv->tx_pwr_tbl[j].channel = prop_value;
				i += 4;
				prop_value = be32_to_cpu(*(u32 *)(prop->value + i));
				priv->tx_pwr_tbl[j].setcap = prop_value;
				i += 4;
				for (k = 0; k < SYSADPT_TX_POWER_LEVEL_TOTAL; k++) {
					prop_value = be32_to_cpu(*(u32 *)(prop->value + i));
					priv->tx_pwr_tbl[j].tx_power[k] = prop_value;
					i += 4;
				}
				prop_value = be32_to_cpu(*(u32 *)(prop->value + i));
				priv->tx_pwr_tbl[j].cdd = prop_value;
				i += 4;
				prop_value = be32_to_cpu(*(u32 *)(prop->value + i));
				priv->tx_pwr_tbl[j].txantenna2 = prop_value;
				i += 4;
				j++;
			}

			/* Dump loaded power tabel
			*/
			WLDBG_PRINT("%s: %s\n", dev_name(&wiphy->dev), prop->name);
			for (i = 0; i < SYSADPT_MAX_NUM_CHANNELS; i++) {
				char disp_buf[64];
				char *disp_ptr;
				
				if (priv->tx_pwr_tbl[i].channel == 0)
					break;
				WLDBG_PRINT("Channel: %d: 0x%x 0x%x 0x%x",
					    priv->tx_pwr_tbl[i].channel,
					    priv->tx_pwr_tbl[i].setcap,
					    priv->tx_pwr_tbl[i].cdd,
					    priv->tx_pwr_tbl[i].txantenna2);
				disp_ptr = disp_buf;
				for (j = 0; j < SYSADPT_TX_POWER_LEVEL_TOTAL; j++) {
					disp_ptr += 
						sprintf(disp_ptr, "%x ",
							priv->tx_pwr_tbl[i].tx_power[j]);
				}
				WLDBG_PRINT("%s", disp_buf);
			}
		}
	}

	WLDBG_EXIT(DBG_LEVEL_0);
}

static int mwl_process_of_dts(struct mwl_priv *priv)
{
	struct property *prop;
	u32 prop_value;

	WLDBG_ENTER(DBG_LEVEL_0);

	BUG_ON(!priv);

	priv->disable_2g = false;
	priv->disable_5g = false;
	priv->antenna_tx = ANTENNA_TX_4_AUTO;
	priv->antenna_rx = ANTENNA_RX_4_AUTO;

	priv->dt_node =
		of_find_node_by_name(pci_bus_to_OF_node(priv->pdev->bus),
				     "mwlwifi");
	if (priv->dt_node == NULL)
		return -EPERM;

	/* look for all matching property names
	*/	
	for_each_property_of_node(priv->dt_node, prop) {
		if (strcmp(prop->name, "marvell,2ghz") == 0)
			priv->disable_2g = true;
		if (strcmp(prop->name, "marvell,5ghz") == 0)
			priv->disable_5g = true;
		if (strcmp(prop->name, "marvell,chainmask") == 0) {
			prop_value = be32_to_cpu(*((u32 *)prop->value));
			if (prop_value == 2)
				priv->antenna_tx = ANTENNA_TX_2;

			prop_value = be32_to_cpu(*((u32 *)(prop->value + 4)));
			if (prop_value == 2)
				priv->antenna_rx = ANTENNA_RX_2;
		}
	}

	priv->pwr_node = of_find_node_by_name(priv->dt_node,
					      "marvell,powertable");

	WLDBG_PRINT("2G: %s\n", priv->disable_2g ? "disable" : "enable");
	WLDBG_PRINT("5G: %s\n", priv->disable_5g ? "disable" : "enable");

	if (priv->antenna_tx == ANTENNA_TX_4_AUTO)
		WLDBG_PRINT("TX: 4 antennas\n");
	else if (priv->antenna_tx == ANTENNA_TX_2)
		WLDBG_PRINT("TX: 2 antennas\n");
	else
		WLDBG_PRINT("TX: unknown\n");
	if (priv->antenna_rx == ANTENNA_RX_4_AUTO)
		WLDBG_PRINT("RX: 4 antennas\n");
	else if (priv->antenna_rx == ANTENNA_RX_2)
		WLDBG_PRINT("RX: 2 antennas\n");
	else
		WLDBG_PRINT("RX: unknown\n");

	WLDBG_EXIT(DBG_LEVEL_0);

	return 0;
}

static void mwl_set_ht_caps(struct mwl_priv *priv,
			    struct ieee80211_supported_band *band)
{
	struct ieee80211_hw *hw;

	WLDBG_ENTER(DBG_LEVEL_0);

	BUG_ON(!priv);
	hw = priv->hw;
	BUG_ON(!hw);

	band->ht_cap.ht_supported = 1;

	band->ht_cap.cap |= IEEE80211_HT_CAP_LDPC_CODING;
	band->ht_cap.cap |= IEEE80211_HT_CAP_SUP_WIDTH_20_40;
	band->ht_cap.cap |= IEEE80211_HT_CAP_SM_PS;
	band->ht_cap.cap |= IEEE80211_HT_CAP_SGI_20;
	band->ht_cap.cap |= IEEE80211_HT_CAP_SGI_40;

	hw->flags |= IEEE80211_HW_AMPDU_AGGREGATION;
	band->ht_cap.ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K;
	band->ht_cap.ampdu_density = IEEE80211_HT_MPDU_DENSITY_4;

	band->ht_cap.mcs.rx_mask[0] = 0xff;
	band->ht_cap.mcs.rx_mask[1] = 0xff;
	if (priv->antenna_rx == ANTENNA_RX_4_AUTO)
		band->ht_cap.mcs.rx_mask[2] = 0xff;
	band->ht_cap.mcs.rx_mask[4] = 0x01;

	band->ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;

	WLDBG_EXIT(DBG_LEVEL_0);
}

static void mwl_set_vht_caps(struct mwl_priv *priv,
			     struct ieee80211_supported_band *band)
{
	WLDBG_ENTER(DBG_LEVEL_0);

	band->vht_cap.vht_supported = 1;

	band->vht_cap.cap |= IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_RXLDPC;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_SHORT_GI_80;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_RXSTBC_1;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_MU_BEAMFORMER_CAPABLE;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_RX_ANTENNA_PATTERN;
	band->vht_cap.cap |= IEEE80211_VHT_CAP_TX_ANTENNA_PATTERN;

	if (priv->antenna_rx == ANTENNA_RX_2)
		band->vht_cap.vht_mcs.rx_mcs_map = 0xfffa;
	else
		band->vht_cap.vht_mcs.rx_mcs_map = 0xffea;

	if (priv->antenna_tx == ANTENNA_TX_2)
		band->vht_cap.vht_mcs.tx_mcs_map = 0xfffa;
	else
		band->vht_cap.vht_mcs.tx_mcs_map = 0xffea;

	WLDBG_EXIT(DBG_LEVEL_0);
}

static void mwl_set_caps(struct mwl_priv *priv)
{
	struct ieee80211_hw *hw;

	WLDBG_ENTER(DBG_LEVEL_0);

	BUG_ON(!priv);
	hw = priv->hw;
	BUG_ON(!hw);

	/* set up band information for 2.4G
	*/
	if (priv->disable_2g == false) {
		BUILD_BUG_ON(sizeof(priv->channels_24) != sizeof(mwl_channels_24));
		memcpy(priv->channels_24, mwl_channels_24, sizeof(mwl_channels_24));

		BUILD_BUG_ON(sizeof(priv->rates_24) != sizeof(mwl_rates_24));
		memcpy(priv->rates_24, mwl_rates_24, sizeof(mwl_rates_24));

		priv->band_24.band = IEEE80211_BAND_2GHZ;
		priv->band_24.channels = priv->channels_24;
		priv->band_24.n_channels = ARRAY_SIZE(mwl_channels_24);
		priv->band_24.bitrates = priv->rates_24;
		priv->band_24.n_bitrates = ARRAY_SIZE(mwl_rates_24);

		mwl_set_ht_caps(priv, &priv->band_24);
		mwl_set_vht_caps(priv, &priv->band_24);

		hw->wiphy->bands[IEEE80211_BAND_2GHZ] = &priv->band_24;
	}

	/* set up band information for 5G
	*/
	if (priv->disable_5g == false) {
		BUILD_BUG_ON(sizeof(priv->channels_50) != sizeof(mwl_channels_50));
		memcpy(priv->channels_50, mwl_channels_50, sizeof(mwl_channels_50));

		BUILD_BUG_ON(sizeof(priv->rates_50) != sizeof(mwl_rates_50));
		memcpy(priv->rates_50, mwl_rates_50, sizeof(mwl_rates_50));

		priv->band_50.band = IEEE80211_BAND_5GHZ;
		priv->band_50.channels = priv->channels_50;
		priv->band_50.n_channels = ARRAY_SIZE(mwl_channels_50);
		priv->band_50.bitrates = priv->rates_50;
		priv->band_50.n_bitrates = ARRAY_SIZE(mwl_rates_50);

		mwl_set_ht_caps(priv, &priv->band_50);
		mwl_set_vht_caps(priv, &priv->band_50);

		hw->wiphy->bands[IEEE80211_BAND_5GHZ] = &priv->band_50;
	}

	WLDBG_EXIT(DBG_LEVEL_0);
}

static int mwl_wl_init(struct mwl_priv *priv)
{
	struct ieee80211_hw *hw;
	int rc;
	int i;

	WLDBG_ENTER(DBG_LEVEL_0);

	BUG_ON(!priv);
	hw = priv->hw;
	BUG_ON(!hw);

	/*
	 * Extra headroom is the size of the required DMA header
	 * minus the size of the smallest 802.11 frame (CTS frame).
	 */
	hw->extra_tx_headroom =
		sizeof(struct mwl_dma_data) - sizeof(struct ieee80211_cts);
	hw->queues = SYSADPT_TX_WMM_QUEUES;

	/* Set rssi values to dBm
	*/
	hw->flags |= IEEE80211_HW_SIGNAL_DBM | IEEE80211_HW_HAS_RATE_CONTROL;

	/*
	 * Ask mac80211 to not to trigger PS mode
	 * based on PM bit of incoming frames.
	 */
	hw->flags |= IEEE80211_HW_AP_LINK_PS;

	hw->vif_data_size = sizeof(struct mwl_vif);
	hw->sta_data_size = sizeof(struct mwl_sta);

	priv->ap_macids_supported = 0x0000ffff;
	priv->sta_macids_supported = 0x00010000;
	priv->macids_used = 0;
	INIT_LIST_HEAD(&priv->vif_list);

	/* Set default radio state, preamble and wmm
	*/
	priv->radio_on = false;
	priv->radio_short_preamble = false;
	priv->wmm_enabled = false;

	priv->powinited = 0;

	/* Handle watchdog ba events
	*/
	INIT_WORK(&priv->watchdog_ba_handle, mwl_watchdog_ba_events);

	tasklet_init(&priv->tx_task, (void *)mwl_tx_done, (unsigned long)hw);
	tasklet_disable(&priv->tx_task);
	tasklet_init(&priv->rx_task, (void *)mwl_rx_recv, (unsigned long)hw);
	tasklet_disable(&priv->rx_task);
	priv->txq_limit = SYSADPT_TX_QUEUE_LIMIT;
	priv->is_tx_schedule = false;
	priv->recv_limit = SYSADPT_RECEIVE_LIMIT;
	priv->is_rx_schedule = false;

	SPIN_LOCK_INIT(&priv->locks.xmit_lock);
	SPIN_LOCK_INIT(&priv->locks.fwcmd_lock);
	SPIN_LOCK_INIT(&priv->locks.stream_lock);

	rc = mwl_tx_init(hw);
	if (rc) {
		WLDBG_ERROR(DBG_LEVEL_0, "%s: fail to initialize TX",
			    MWL_DRV_NAME);
		goto err_mwl_tx_init;
	}

	rc = mwl_rx_init(hw);
	if (rc) {
		WLDBG_ERROR(DBG_LEVEL_0, "%s: fail to initialize RX",
			    MWL_DRV_NAME);
		goto err_mwl_rx_init;
	}

	rc = mwl_fwcmd_get_hw_specs(hw);
	if (rc) {
		WLDBG_ERROR(DBG_LEVEL_0, "%s: fail to get HW specifications",
			    MWL_DRV_NAME);
		goto err_get_hw_specs;
	}

	SET_IEEE80211_PERM_ADDR(hw, priv->hw_data.mac_addr);

	writel(priv->desc_data[0].pphys_tx_ring,
	       priv->iobase0 + priv->desc_data[0].wcb_base);
#if SYSADPT_NUM_OF_DESC_DATA > 3
	for (i = 1; i < SYSADPT_TOTAL_TX_QUEUES; i++)
		writel(priv->desc_data[i].pphys_tx_ring,
		       priv->iobase0 + priv->desc_data[i].wcb_base);
#endif
	writel(priv->desc_data[0].pphys_rx_ring,
	       priv->iobase0 + priv->desc_data[0].rx_desc_read);
	writel(priv->desc_data[0].pphys_rx_ring,
	       priv->iobase0 + priv->desc_data[0].rx_desc_write);

	rc = mwl_fwcmd_set_hw_specs(hw);
	if (rc) {
		WLDBG_ERROR(DBG_LEVEL_0, "%s: fail to set HW specifications",
			    MWL_DRV_NAME);
		goto err_set_hw_specs;
	}

	WLDBG_PRINT("firmware version: 0x%x", priv->hw_data.fw_release_num);

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
		WLDBG_ERROR(DBG_LEVEL_0, "%s: fail to register device",
			    MWL_DRV_NAME);
		goto err_register_hw;
	}

	WLDBG_EXIT(DBG_LEVEL_0);

	return rc;

err_register_hw:
err_set_hw_specs:
err_get_hw_specs:

	mwl_rx_deinit(hw);

err_mwl_rx_init:

	mwl_tx_deinit(hw);

err_mwl_tx_init:

	WLDBG_EXIT_INFO(DBG_LEVEL_0, "init fail");

	return rc;
}

static void mwl_wl_deinit(struct mwl_priv *priv)
{
	struct ieee80211_hw *hw;

	WLDBG_ENTER(DBG_LEVEL_0);

	BUG_ON(!priv);
	hw = priv->hw;
	BUG_ON(!hw);

	ieee80211_unregister_hw(hw);
	mwl_rx_deinit(hw);
	mwl_tx_deinit(hw);
	tasklet_kill(&priv->rx_task);
	tasklet_kill(&priv->tx_task);
	mwl_fwcmd_reset(hw);

	WLDBG_EXIT(DBG_LEVEL_0);
}

static void mwl_watchdog_ba_events(struct work_struct *work)
{
	int rc;
	u8 bitmap = 0, stream_index;
	struct mwl_ampdu_stream *streams;
	struct mwl_priv *priv =
		container_of(work, struct mwl_priv, watchdog_ba_handle);
	struct ieee80211_hw *hw = priv->hw;
	u32 status;

	rc = mwl_fwcmd_get_watchdog_bitmap(priv->hw, &bitmap);

	if (rc)
		goto done;

	SPIN_LOCK(&priv->locks.stream_lock);

	/* the bitmap is the hw queue number.  Map it to the ampdu queue.
	*/
	if (bitmap != INVALID_WATCHDOG) {
		if (bitmap == SYSADPT_TX_AMPDU_QUEUES)
			stream_index = 0;
		else if (bitmap > SYSADPT_TX_AMPDU_QUEUES)
			stream_index = bitmap - SYSADPT_TX_AMPDU_QUEUES;
		else
			stream_index = bitmap + 3; /** queue 0 is stream 3*/

		if (bitmap != 0xFF) {
			/* Check if the stream is in use before disabling it
			*/
			streams = &priv->ampdu[stream_index];

			if (streams->state == AMPDU_STREAM_ACTIVE) {
				ieee80211_stop_tx_ba_session(streams->sta,
							     streams->tid);
				SPIN_UNLOCK(&priv->locks.stream_lock);
				mwl_fwcmd_destroy_ba(hw, stream_index);
				SPIN_LOCK(&priv->locks.stream_lock);
			}
		} else {
			for (stream_index = 0; stream_index < SYSADPT_TX_AMPDU_QUEUES; stream_index++) {
				streams = &priv->ampdu[stream_index];

				if (streams->state == AMPDU_STREAM_ACTIVE) {
					ieee80211_stop_tx_ba_session(streams->sta,
								     streams->tid);
					SPIN_UNLOCK(&priv->locks.stream_lock);
					mwl_fwcmd_destroy_ba(hw, stream_index);
					SPIN_LOCK(&priv->locks.stream_lock);
				}
			}
		}
	}

	SPIN_UNLOCK(&priv->locks.stream_lock);

done:

	status = readl(priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);
	writel(status | MACREG_A2HRIC_BA_WATCHDOG,
	       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);

	return;
}

static irqreturn_t mwl_interrupt(int irq, void *dev_id)
{
	struct ieee80211_hw *hw = dev_id;
	struct mwl_priv *priv;
	unsigned int int_status, clr_status;
	u32 status;

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	int_status = readl(priv->iobase1 + MACREG_REG_A2H_INTERRUPT_CAUSE);

	if (int_status == 0x00000000)
		return IRQ_NONE;

	if (int_status == 0xffffffff) {
		WLDBG_INFO(DBG_LEVEL_0, "card plugged out???");
	} else {
		clr_status = int_status;

		if (int_status & MACREG_A2HRIC_BIT_TX_DONE) {
			int_status &= ~MACREG_A2HRIC_BIT_TX_DONE;

			if (priv->is_tx_schedule == false) {
				status = readl(priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);
				writel((status & ~MACREG_A2HRIC_BIT_TX_DONE),
				       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);
				tasklet_schedule(&priv->tx_task);
				priv->is_tx_schedule = true;
			}
		}

		if (int_status & MACREG_A2HRIC_BIT_RX_RDY) {
			int_status &= ~MACREG_A2HRIC_BIT_RX_RDY;

			if (priv->is_rx_schedule == false) {
				status = readl(priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);
				writel((status & ~MACREG_A2HRIC_BIT_RX_RDY),
				       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);
				tasklet_schedule(&priv->rx_task);
				priv->is_rx_schedule = true;
			}
		}

		if (int_status & MACREG_A2HRIC_BA_WATCHDOG) {
			status = readl(priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);
			writel((status & ~MACREG_A2HRIC_BA_WATCHDOG),
			       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);
			int_status &= ~MACREG_A2HRIC_BA_WATCHDOG;
			ieee80211_queue_work(hw, &priv->watchdog_ba_handle);
		}

		writel(~clr_status,
		       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_CAUSE);
	}

	return IRQ_HANDLED;
}


module_pci_driver(mwl_pci_driver);

MODULE_DESCRIPTION(MWL_DESC);
MODULE_VERSION(MWL_DRV_VERSION);
MODULE_AUTHOR("Marvell Semiconductor, Inc.");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_SUPPORTED_DEVICE(MWL_DEV_NAME);
MODULE_DEVICE_TABLE(pci, mwl_pci_id_tbl);
