/*
 * Copyright (C) 2006-2015, Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Description:  This file implements receive related functions.
 */

#include <linux/skbuff.h>

#include "sysadpt.h"
#include "dev.h"
#include "rx.h"

#define MAX_NUM_RX_RING_BYTES  (SYSADPT_MAX_NUM_RX_DESC * \
				sizeof(struct mwl_rx_desc))

#define FIRST_RXD priv->desc_data[0].prx_ring[0]
#define CURR_RXD  priv->desc_data[0].prx_ring[curr_desc]
#define NEXT_RXD  priv->desc_data[0].prx_ring[curr_desc + 1]
#define LAST_RXD  priv->desc_data[0].prx_ring[SYSADPT_MAX_NUM_RX_DESC - 1]

#define DECRYPT_ERR_MASK        0x80
#define GENERAL_DECRYPT_ERR     0xFF
#define TKIP_DECRYPT_MIC_ERR    0x02
#define WEP_DECRYPT_ICV_ERR     0x04
#define TKIP_DECRYPT_ICV_ERR    0x08

#define W836X_RSSI_OFFSET       8

/* Receive rate information constants */
#define RX_RATE_INFO_FORMAT_11A       0
#define RX_RATE_INFO_FORMAT_11B       1
#define RX_RATE_INFO_FORMAT_11N       2
#define RX_RATE_INFO_FORMAT_11AC      4

#define RX_RATE_INFO_HT20             0
#define RX_RATE_INFO_HT40             1
#define RX_RATE_INFO_HT80             2

#define RX_RATE_INFO_LONG_INTERVAL    0
#define RX_RATE_INFO_SHORT_INTERVAL   1

static int mwl_rx_ring_alloc(struct mwl_priv *priv)
{
	priv->desc_data[0].prx_ring =
		(struct mwl_rx_desc *)
		dma_alloc_coherent(&priv->pdev->dev,
				   MAX_NUM_RX_RING_BYTES,
				   &priv->desc_data[0].pphys_rx_ring,
				   GFP_KERNEL);

	if (!priv->desc_data[0].prx_ring) {
		wiphy_err(priv->hw->wiphy, "can not alloc mem");
		return -ENOMEM;
	}

	memset(priv->desc_data[0].prx_ring, 0x00, MAX_NUM_RX_RING_BYTES);

	return 0;
}

static int mwl_rx_ring_init(struct mwl_priv *priv)
{
	int curr_desc;
	struct mwl_desc_data *desc;

	desc = &priv->desc_data[0];

	if (desc->prx_ring) {
		desc->rx_buf_size = SYSADPT_MAX_AGGR_SIZE;

		for (curr_desc = 0; curr_desc < SYSADPT_MAX_NUM_RX_DESC;
		     curr_desc++) {
			CURR_RXD.psk_buff =
				dev_alloc_skb(desc->rx_buf_size);

			if (skb_linearize(CURR_RXD.psk_buff)) {
				dev_kfree_skb_any(CURR_RXD.psk_buff);
				wiphy_err(priv->hw->wiphy,
					  "need linearize memory");
				return -ENOMEM;
			}

			skb_reserve(CURR_RXD.psk_buff,
				    SYSADPT_MIN_BYTES_HEADROOM);
			CURR_RXD.rx_control = EAGLE_RXD_CTRL_DRIVER_OWN;
			CURR_RXD.status = EAGLE_RXD_STATUS_OK;
			CURR_RXD.qos_ctrl = 0x0000;
			CURR_RXD.channel = 0x00;
			CURR_RXD.rssi = 0x00;

			if (CURR_RXD.psk_buff) {
				dma_addr_t dma;
				u32 val;

				CURR_RXD.pkt_len =
					cpu_to_le16(SYSADPT_MAX_AGGR_SIZE);
				CURR_RXD.pbuff_data = CURR_RXD.psk_buff->data;
				dma = pci_map_single(priv->pdev,
						     CURR_RXD.psk_buff->data,
						     desc->rx_buf_size,
						     PCI_DMA_FROMDEVICE);
				CURR_RXD.pphys_buff_data =
					cpu_to_le32(dma);
				CURR_RXD.pnext = &NEXT_RXD;
				val = (u32)desc->pphys_rx_ring +
				      ((curr_desc + 1) *
				      sizeof(struct mwl_rx_desc));
				CURR_RXD.pphys_next =
					cpu_to_le32(val);
			} else {
				wiphy_err(priv->hw->wiphy,
					  "rxdesc %i: no skbuff available",
					  curr_desc);
				return -ENOMEM;
			}
		}
		LAST_RXD.pphys_next =
			cpu_to_le32((u32)desc->pphys_rx_ring);
		LAST_RXD.pnext = &FIRST_RXD;
		priv->desc_data[0].pnext_rx_desc = &FIRST_RXD;

		return 0;
	}

	wiphy_err(priv->hw->wiphy, "no valid RX mem");

	return -ENOMEM;
}

static void mwl_rx_ring_cleanup(struct mwl_priv *priv)
{
	int curr_desc;

	if (priv->desc_data[0].prx_ring) {
		for (curr_desc = 0; curr_desc < SYSADPT_MAX_NUM_RX_DESC;
		     curr_desc++) {
			if (!CURR_RXD.psk_buff)
				continue;

			if (skb_shinfo(CURR_RXD.psk_buff)->nr_frags)
				skb_shinfo(CURR_RXD.psk_buff)->nr_frags = 0;

			if (skb_shinfo(CURR_RXD.psk_buff)->frag_list)
				skb_shinfo(CURR_RXD.psk_buff)->frag_list = NULL;

			pci_unmap_single(priv->pdev,
					 le32_to_cpu
					 (CURR_RXD.pphys_buff_data),
					 priv->desc_data[0].rx_buf_size,
					 PCI_DMA_FROMDEVICE);

			dev_kfree_skb_any(CURR_RXD.psk_buff);

			wiphy_info(priv->hw->wiphy,
				   "unmapped+free'd %i 0x%p 0x%x %i",
				   curr_desc, CURR_RXD.pbuff_data,
				   le32_to_cpu(CURR_RXD.pphys_buff_data),
				   priv->desc_data[0].rx_buf_size);

			CURR_RXD.pbuff_data = NULL;
			CURR_RXD.psk_buff = NULL;
		}
	}
}

static void mwl_rx_ring_free(struct mwl_priv *priv)
{
	if (priv->desc_data[0].prx_ring) {
		mwl_rx_ring_cleanup(priv);

		dma_free_coherent(&priv->pdev->dev,
				  MAX_NUM_RX_RING_BYTES,
				  priv->desc_data[0].prx_ring,
				  priv->desc_data[0].pphys_rx_ring);

		priv->desc_data[0].prx_ring = NULL;
	}

	priv->desc_data[0].pnext_rx_desc = NULL;
}

static inline void mwl_rx_prepare_status(struct mwl_rx_desc *pdesc,
					 struct ieee80211_rx_status *status)
{
	u16 rate, format, nss, bw, gi, rt;

	memset(status, 0, sizeof(*status));

	status->signal = -(pdesc->rssi + W836X_RSSI_OFFSET);

	rate = le16_to_cpu(pdesc->rate);
	format = rate & MWL_RX_RATE_FORMAT_MASK;
	nss = (rate & MWL_RX_RATE_NSS_MASK) >> MWL_RX_RATE_NSS_SHIFT;
	bw = (rate & MWL_RX_RATE_BW_MASK) >> MWL_RX_RATE_BW_SHIFT;
	gi = (rate & MWL_RX_RATE_GI_MASK) >> MWL_RX_RATE_GI_SHIFT;
	rt = (rate & MWL_RX_RATE_RT_MASK) >> MWL_RX_RATE_RT_SHIFT;

	switch (format) {
	case RX_RATE_INFO_FORMAT_11N:
		status->flag |= RX_FLAG_HT;
		if (bw == RX_RATE_INFO_HT40)
			status->flag |= RX_FLAG_40MHZ;
		if (gi == RX_RATE_INFO_SHORT_INTERVAL)
			status->flag |= RX_FLAG_SHORT_GI;
		break;
	case RX_RATE_INFO_FORMAT_11AC:
		status->flag |= RX_FLAG_VHT;
		if (bw == RX_RATE_INFO_HT40)
			status->flag |= RX_FLAG_40MHZ;
		if (bw == RX_RATE_INFO_HT80)
			status->vht_flag |= RX_VHT_FLAG_80MHZ;
		if (gi == RX_RATE_INFO_SHORT_INTERVAL)
			status->flag |= RX_FLAG_SHORT_GI;
		status->vht_nss = (nss + 1);
		break;
	}

	status->rate_idx = rt;

	if (pdesc->channel > BAND_24_CHANNEL_NUM) {
		status->band = IEEE80211_BAND_5GHZ;
		if ((!(status->flag & RX_FLAG_HT)) &&
		    (!(status->flag & RX_FLAG_VHT))) {
			status->rate_idx -= 5;
			if (status->rate_idx >= BAND_50_RATE_NUM)
				status->rate_idx = BAND_50_RATE_NUM - 1;
		}
	} else {
		status->band = IEEE80211_BAND_2GHZ;
		if ((!(status->flag & RX_FLAG_HT)) &&
		    (!(status->flag & RX_FLAG_VHT))) {
			if (status->rate_idx >= BAND_24_RATE_NUM)
				status->rate_idx = BAND_24_RATE_NUM - 1;
		}
	}

	status->freq = ieee80211_channel_to_frequency(pdesc->channel,
						      status->band);

	/* check if status has a specific error bit (bit 7) set or indicates
	 * a general decrypt error
	 */
	if ((pdesc->status == GENERAL_DECRYPT_ERR) ||
	    (pdesc->status & DECRYPT_ERR_MASK)) {
		/* check if status is not equal to 0xFF
		 * the 0xFF check is for backward compatibility
		 */
		if (pdesc->status != GENERAL_DECRYPT_ERR) {
			if (((pdesc->status & (~DECRYPT_ERR_MASK)) &
			    TKIP_DECRYPT_MIC_ERR) && !((pdesc->status &
			    (WEP_DECRYPT_ICV_ERR | TKIP_DECRYPT_ICV_ERR)))) {
				status->flag |= RX_FLAG_MMIC_ERROR;
			}
		}
	}
}

static inline struct mwl_vif *mwl_rx_find_vif_bss(struct mwl_priv *priv,
						  u8 *bssid)
{
	struct mwl_vif *mwl_vif;
	unsigned long flags;

	spin_lock_irqsave(&priv->vif_lock, flags);
	list_for_each_entry(mwl_vif, &priv->vif_list, list) {
		if (memcmp(bssid, mwl_vif->bssid, ETH_ALEN) == 0) {
			spin_unlock_irqrestore(&priv->vif_lock, flags);
			return mwl_vif;
		}
	}
	spin_unlock_irqrestore(&priv->vif_lock, flags);

	return NULL;
}

static inline void mwl_rx_remove_dma_header(struct sk_buff *skb, __le16 qos)
{
	struct mwl_dma_data *tr;
	int hdrlen;

	tr = (struct mwl_dma_data *)skb->data;
	hdrlen = ieee80211_hdrlen(tr->wh.frame_control);

	if (hdrlen != sizeof(tr->wh)) {
		if (ieee80211_is_data_qos(tr->wh.frame_control)) {
			memmove(tr->data - hdrlen, &tr->wh, hdrlen - 2);
			*((__le16 *)(tr->data - 2)) = qos;
		} else {
			memmove(tr->data - hdrlen, &tr->wh, hdrlen);
		}
	}

	if (hdrlen != sizeof(*tr))
		skb_pull(skb, sizeof(*tr) - hdrlen);
}

static int mwl_rx_refill(struct mwl_priv *priv, struct mwl_rx_desc *pdesc)
{
	pdesc->psk_buff = dev_alloc_skb(priv->desc_data[0].rx_buf_size);

	if (!pdesc->psk_buff)
		goto nomem;

	if (skb_linearize(pdesc->psk_buff)) {
		dev_kfree_skb_any(pdesc->psk_buff);
		wiphy_err(priv->hw->wiphy, "need linearize memory");
		goto nomem;
	}

	skb_reserve(pdesc->psk_buff, SYSADPT_MIN_BYTES_HEADROOM);

	pdesc->status = EAGLE_RXD_STATUS_OK;
	pdesc->qos_ctrl = 0x0000;
	pdesc->channel = 0x00;
	pdesc->rssi = 0x00;

	pdesc->pkt_len = cpu_to_le16(priv->desc_data[0].rx_buf_size);
	pdesc->pbuff_data = pdesc->psk_buff->data;
	pdesc->pphys_buff_data =
		cpu_to_le32(pci_map_single(priv->pdev,
					   pdesc->psk_buff->data,
					   priv->desc_data[0].rx_buf_size,
					   PCI_DMA_BIDIRECTIONAL));

	return 0;

nomem:

	wiphy_err(priv->hw->wiphy, "no memory");

	return -ENOMEM;
}

int mwl_rx_init(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;
	int rc;

	priv = hw->priv;

	rc = mwl_rx_ring_alloc(priv);
	if (rc) {
		wiphy_err(hw->wiphy, "allocating RX ring failed");
	} else {
		rc = mwl_rx_ring_init(priv);
		if (rc) {
			mwl_rx_ring_free(priv);
			wiphy_err(hw->wiphy,
				  "initializing RX ring failed");
		}
	}

	return rc;
}

void mwl_rx_deinit(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;

	priv = hw->priv;

	mwl_rx_ring_cleanup(priv);
	mwl_rx_ring_free(priv);
}

void mwl_rx_recv(unsigned long data)
{
	struct ieee80211_hw *hw = (struct ieee80211_hw *)data;
	struct mwl_priv *priv;
	struct mwl_rx_desc *curr_desc;
	int work_done = 0;
	struct sk_buff *prx_skb = NULL;
	int pkt_len;
	struct ieee80211_rx_status status;
	struct mwl_vif *mwl_vif = NULL;
	struct ieee80211_hdr *wh;
	u32 status_mask;

	priv = hw->priv;

	curr_desc = priv->desc_data[0].pnext_rx_desc;

	if (!curr_desc) {
		status_mask = readl(priv->iobase1 +
				    MACREG_REG_A2H_INTERRUPT_STATUS_MASK);
		writel(status_mask | MACREG_A2HRIC_BIT_RX_RDY,
		       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);

		priv->is_rx_schedule = false;

		wiphy_warn(hw->wiphy, "busy or no receiving packets");
		return;
	}

	while ((curr_desc->rx_control == EAGLE_RXD_CTRL_DMA_OWN) &&
	       (work_done < priv->recv_limit)) {
		prx_skb = curr_desc->psk_buff;
		if (!prx_skb)
			goto out;
		pci_unmap_single(priv->pdev,
				 le32_to_cpu(curr_desc->pphys_buff_data),
				 priv->desc_data[0].rx_buf_size,
				 PCI_DMA_FROMDEVICE);
		pkt_len = le16_to_cpu(curr_desc->pkt_len);

		if (skb_tailroom(prx_skb) < pkt_len) {
			dev_kfree_skb_any(prx_skb);
			goto out;
		}

		if (curr_desc->channel != hw->conf.chandef.chan->hw_value) {
			dev_kfree_skb_any(prx_skb);
			goto out;
		}

		mwl_rx_prepare_status(curr_desc, &status);

		priv->noise = -curr_desc->noise_floor;

		wh = &((struct mwl_dma_data *)prx_skb->data)->wh;

		if (ieee80211_has_protected(wh->frame_control)) {
			/* Check if hw crypto has been enabled for
			 * this bss. If yes, set the status flags
			 * accordingly
			 */
			if (ieee80211_has_tods(wh->frame_control))
				mwl_vif = mwl_rx_find_vif_bss(priv, wh->addr1);
			else
				mwl_vif = mwl_rx_find_vif_bss(priv, wh->addr2);

			if (mwl_vif && mwl_vif->is_hw_crypto_enabled) {
				/* When MMIC ERROR is encountered
				 * by the firmware, payload is
				 * dropped and only 32 bytes of
				 * mwl8k Firmware header is sent
				 * to the host.
				 *
				 * We need to add four bytes of
				 * key information.  In it
				 * MAC80211 expects keyidx set to
				 * 0 for triggering Counter
				 * Measure of MMIC failure.
				 */
				if (status.flag & RX_FLAG_MMIC_ERROR) {
					struct mwl_dma_data *tr;

					tr = (struct mwl_dma_data *)
					     prx_skb->data;
					memset((void *)&tr->data, 0, 4);
					pkt_len += 4;
				}

				if (!ieee80211_is_auth(wh->frame_control))
					status.flag |= RX_FLAG_IV_STRIPPED |
						       RX_FLAG_DECRYPTED |
						       RX_FLAG_MMIC_STRIPPED;
			}
		}

		skb_put(prx_skb, pkt_len);
		mwl_rx_remove_dma_header(prx_skb, curr_desc->qos_ctrl);
		memcpy(IEEE80211_SKB_RXCB(prx_skb), &status, sizeof(status));
		ieee80211_rx(hw, prx_skb);
out:
		mwl_rx_refill(priv, curr_desc);
		curr_desc->rx_control = EAGLE_RXD_CTRL_DRIVER_OWN;
		curr_desc->qos_ctrl = 0;
		curr_desc = curr_desc->pnext;
		work_done++;
	}

	priv->desc_data[0].pnext_rx_desc = curr_desc;

	status_mask = readl(priv->iobase1 +
			    MACREG_REG_A2H_INTERRUPT_STATUS_MASK);
	writel(status_mask | MACREG_A2HRIC_BIT_RX_RDY,
	       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);

	priv->is_rx_schedule = false;
}
