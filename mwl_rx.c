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
*   Description:  This file implements receive related functions.
*
*/

#include "mwl_sysadpt.h"
#include "mwl_dev.h"
#include "mwl_debug.h"
#include "mwl_rx.h"

/* CONSTANTS AND MACROS
*/

#define MAX_NUM_RX_RING_BYTES  SYSADPT_MAX_NUM_RX_DESC * sizeof(struct mwl_rx_desc)

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

/* PRIVATE FUNCTION DECLARATION
*/

static int mwl_rx_ring_alloc(struct mwl_priv *priv);
static int mwl_rx_ring_init(struct mwl_priv *priv);
static void mwl_rx_ring_cleanup(struct mwl_priv *priv);
static void mwl_rx_ring_free(struct mwl_priv *priv);
static inline void mwl_rx_prepare_status(struct mwl_rx_desc *pdesc,
					 struct ieee80211_rx_status *status);
static inline struct mwl_vif *mwl_rx_find_vif_bss(struct list_head *vif_list, u8 *bssid);
static inline void mwl_rx_remove_dma_header(struct sk_buff *skb, u16 qos);
static int mwl_rx_refill(struct mwl_priv *priv, struct mwl_rx_desc *pdesc);

/* PUBLIC FUNCTION DEFINITION
*/

int mwl_rx_init(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;
	int rc;

	WLDBG_ENTER(DBG_LEVEL_4);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	rc = mwl_rx_ring_alloc(priv);
	if (rc) {

		WLDBG_ERROR(DBG_LEVEL_4, "allocating RX ring failed");

	} else {

		rc = mwl_rx_ring_init(priv);
		if (rc) {

			mwl_rx_ring_free(priv);
			WLDBG_ERROR(DBG_LEVEL_4, "initializing RX ring failed");
		}
	}

	WLDBG_EXIT(DBG_LEVEL_4);

	return rc;
}

void mwl_rx_deinit(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;

	WLDBG_ENTER(DBG_LEVEL_4);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	mwl_rx_ring_cleanup(priv);
	mwl_rx_ring_free(priv);

	WLDBG_EXIT(DBG_LEVEL_4);
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

	WLDBG_ENTER(DBG_LEVEL_4);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	curr_desc = priv->desc_data[0].pnext_rx_desc;

	if (curr_desc == NULL) {

		status_mask = readl(priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);
		writel(status_mask | MACREG_A2HRIC_BIT_RX_RDY,
		       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);

		priv->is_rx_schedule = false;

		WLDBG_EXIT_INFO(DBG_LEVEL_4, "busy or no receiving packets");
		return;
	}

	while ((curr_desc->rx_control == EAGLE_RXD_CTRL_DMA_OWN)
		&& (work_done < priv->recv_limit)) {

		prx_skb = curr_desc->psk_buff;
		if (prx_skb == NULL)
			goto out;
		pci_unmap_single(priv->pdev,
				 ENDIAN_SWAP32(curr_desc->pphys_buff_data),
			priv->desc_data[0].rx_buf_size,
			PCI_DMA_FROMDEVICE);
		pkt_len = curr_desc->pkt_len;

		if (skb_tailroom(prx_skb) < pkt_len) {

			WLDBG_INFO(DBG_LEVEL_4, "Not enough tail room =%x pkt_len=%x, curr_desc=%x, curr_desc_data=%x",
				   skb_tailroom(prx_skb), pkt_len, curr_desc, curr_desc->pbuff_data);
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
			mwl_vif = mwl_rx_find_vif_bss(&priv->vif_list,
						      wh->addr1);

			if (mwl_vif != NULL &&
			    mwl_vif->is_hw_crypto_enabled) {
				/*
				 * When MMIC ERROR is encountered
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

					tr = (struct mwl_dma_data *)prx_skb->data;
					memset((void *)&(tr->data), 0, 4);
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

		mwl_rx_refill(priv, curr_desc);

out:
		curr_desc->rx_control = EAGLE_RXD_CTRL_DRIVER_OWN;
		curr_desc->qos_ctrl = 0;
		curr_desc = curr_desc->pnext;
		work_done++;
	}

	priv->desc_data[0].pnext_rx_desc = curr_desc;

	status_mask = readl(priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);
	writel(status_mask | MACREG_A2HRIC_BIT_RX_RDY,
	       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);

	priv->is_rx_schedule = false;

	WLDBG_EXIT(DBG_LEVEL_4);
}

/* PRIVATE FUNCTION DEFINITION
*/

static int mwl_rx_ring_alloc(struct mwl_priv *priv)
{
	WLDBG_ENTER_INFO(DBG_LEVEL_4, "allocating %i (0x%x) bytes",
			 MAX_NUM_RX_RING_BYTES, MAX_NUM_RX_RING_BYTES);

	BUG_ON(!priv);

	priv->desc_data[0].prx_ring =
		(struct mwl_rx_desc *)pci_alloc_consistent(priv->pdev,
		MAX_NUM_RX_RING_BYTES,
		&priv->desc_data[0].pphys_rx_ring);

	if (priv->desc_data[0].prx_ring == NULL) {

		WLDBG_ERROR(DBG_LEVEL_4, "can not alloc mem");
		WLDBG_EXIT_INFO(DBG_LEVEL_4, "no memory");
		return -ENOMEM;
	}

	memset(priv->desc_data[0].prx_ring, 0x00, MAX_NUM_RX_RING_BYTES);

	WLDBG_EXIT_INFO(DBG_LEVEL_4, "RX ring vaddr: 0x%x paddr: 0x%x",
			priv->desc_data[0].prx_ring, priv->desc_data[0].pphys_rx_ring);

	return 0;
}

static int mwl_rx_ring_init(struct mwl_priv *priv)
{
	int curr_desc;

	WLDBG_ENTER_INFO(DBG_LEVEL_4,  "initializing %i descriptors", SYSADPT_MAX_NUM_RX_DESC);

	if (priv->desc_data[0].prx_ring != NULL) {

		priv->desc_data[0].rx_buf_size = SYSADPT_MAX_AGGR_SIZE;

		for (curr_desc = 0; curr_desc < SYSADPT_MAX_NUM_RX_DESC; curr_desc++) {

			CURR_RXD.psk_buff = dev_alloc_skb(priv->desc_data[0].rx_buf_size);

			if (skb_linearize(CURR_RXD.psk_buff)) {

				dev_kfree_skb_any(CURR_RXD.psk_buff);
				WLDBG_ERROR(DBG_LEVEL_4, "need linearize memory");
				WLDBG_EXIT_INFO(DBG_LEVEL_4, "no suitable memory");
				return -ENOMEM;
			}

			skb_reserve(CURR_RXD.psk_buff, SYSADPT_MIN_BYTES_HEADROOM);
			CURR_RXD.rx_control = EAGLE_RXD_CTRL_DRIVER_OWN;
			CURR_RXD.status = EAGLE_RXD_STATUS_OK;
			CURR_RXD.qos_ctrl = 0x0000;
			CURR_RXD.channel = 0x00;
			CURR_RXD.rssi = 0x00;
			CURR_RXD.sq2 = 0x00;

			if (CURR_RXD.psk_buff != NULL) {

				CURR_RXD.pkt_len = SYSADPT_MAX_AGGR_SIZE;
				CURR_RXD.pbuff_data = CURR_RXD.psk_buff->data;
				CURR_RXD.pphys_buff_data =
					ENDIAN_SWAP32(pci_map_single(priv->pdev,
								     CURR_RXD.psk_buff->data,
						priv->desc_data[0].rx_buf_size,
						PCI_DMA_FROMDEVICE));
				CURR_RXD.pnext = &NEXT_RXD;
				CURR_RXD.pphys_next =
					ENDIAN_SWAP32((u32)priv->desc_data[0].pphys_rx_ring +
						((curr_desc + 1) * sizeof(struct mwl_rx_desc)));
				WLDBG_INFO(DBG_LEVEL_4,
					   "rxdesc: %i status: 0x%x (%i) len: 0x%x (%i)",
					curr_desc, EAGLE_TXD_STATUS_IDLE, EAGLE_TXD_STATUS_IDLE,
					priv->desc_data[0].rx_buf_size, priv->desc_data[0].rx_buf_size);
				WLDBG_INFO(DBG_LEVEL_4,
					   "rxdesc: %i vnext: 0x%p pnext: 0x%x", curr_desc,
					CURR_RXD.pnext, ENDIAN_SWAP32(CURR_RXD.pphys_next));
			} else {

				WLDBG_ERROR(DBG_LEVEL_4,
					    "rxdesc %i: no skbuff available", curr_desc);
				WLDBG_EXIT_INFO(DBG_LEVEL_4, "no socket buffer");
				return -ENOMEM;
			}
		}
		LAST_RXD.pphys_next =
			ENDIAN_SWAP32((u32)priv->desc_data[0].pphys_rx_ring);
		LAST_RXD.pnext = &FIRST_RXD;
		priv->desc_data[0].pnext_rx_desc = &FIRST_RXD;

		WLDBG_EXIT_INFO(DBG_LEVEL_4,
				"last rxdesc vnext: 0x%p pnext: 0x%x vfirst 0x%x",
			LAST_RXD.pnext, ENDIAN_SWAP32(LAST_RXD.pphys_next),
			priv->desc_data[0].pnext_rx_desc);

		return 0;
	}

	WLDBG_ERROR(DBG_LEVEL_4, "no valid RX mem");
	WLDBG_EXIT_INFO(DBG_LEVEL_4, "no valid RX mem");

	return -ENOMEM;
}

static void mwl_rx_ring_cleanup(struct mwl_priv *priv)
{
	int curr_desc;

	WLDBG_ENTER(DBG_LEVEL_4);

	BUG_ON(!priv);

	if (priv->desc_data[0].prx_ring != NULL) {

		for (curr_desc = 0; curr_desc < SYSADPT_MAX_NUM_RX_DESC; curr_desc++) {

			if (CURR_RXD.psk_buff != NULL) {

				if (skb_shinfo(CURR_RXD.psk_buff)->nr_frags)
					skb_shinfo(CURR_RXD.psk_buff)->nr_frags = 0;

				if (skb_shinfo(CURR_RXD.psk_buff)->frag_list)
					skb_shinfo(CURR_RXD.psk_buff)->frag_list = NULL;

				pci_unmap_single(priv->pdev,
						 ENDIAN_SWAP32(CURR_RXD.pphys_buff_data),
						priv->desc_data[0].rx_buf_size,
						PCI_DMA_FROMDEVICE);

				dev_kfree_skb_any(CURR_RXD.psk_buff);

				WLDBG_INFO(DBG_LEVEL_4,
					   "unmapped+free'd rxdesc %i vaddr: 0x%p paddr: 0x%x len: %i",
					curr_desc, CURR_RXD.pbuff_data,
					ENDIAN_SWAP32(CURR_RXD.pphys_buff_data),
						priv->desc_data[0].rx_buf_size);

				CURR_RXD.pbuff_data = NULL;
				CURR_RXD.psk_buff = NULL;
			}
		}
	}

	WLDBG_EXIT(DBG_LEVEL_4);
}

static void mwl_rx_ring_free(struct mwl_priv *priv)
{
	WLDBG_ENTER(DBG_LEVEL_4);

	BUG_ON(!priv);

	if (priv->desc_data[0].prx_ring != NULL) {

		mwl_rx_ring_cleanup(priv);

		pci_free_consistent(priv->pdev,
				    MAX_NUM_RX_RING_BYTES,
			priv->desc_data[0].prx_ring,
			priv->desc_data[0].pphys_rx_ring);

		priv->desc_data[0].prx_ring = NULL;
	}

	priv->desc_data[0].pnext_rx_desc = NULL;

	WLDBG_EXIT(DBG_LEVEL_4);
}

static inline void mwl_rx_prepare_status(struct mwl_rx_desc *pdesc,
					 struct ieee80211_rx_status *status)
{
	WLDBG_ENTER(DBG_LEVEL_4);

	BUG_ON(!pdesc);
	BUG_ON(!status);

	memset(status, 0, sizeof(*status));

	status->signal = -(pdesc->rssi + W836X_RSSI_OFFSET);

	/* TODO: rate & antenna
	*/

	if (pdesc->channel > 14)
		status->band = IEEE80211_BAND_5GHZ;
	else
		status->band = IEEE80211_BAND_2GHZ;

	status->freq = ieee80211_channel_to_frequency(pdesc->channel,
		status->band);

	/* check if status has a specific error bit (bit 7)set or indicates a general decrypt error
	*/
	if ((pdesc->status == GENERAL_DECRYPT_ERR) || (pdesc->status & DECRYPT_ERR_MASK)) {

		/* check if status is not equal to 0xFF
		 * the 0xFF check is for backward compatibility
		 */
		if (pdesc->status != GENERAL_DECRYPT_ERR) {

			if (((pdesc->status & (~DECRYPT_ERR_MASK)) & TKIP_DECRYPT_MIC_ERR) &&
			    !((pdesc->status & (WEP_DECRYPT_ICV_ERR | TKIP_DECRYPT_ICV_ERR)))) {

				status->flag |= RX_FLAG_MMIC_ERROR;
			}
		}
	}

	WLDBG_EXIT(DBG_LEVEL_4);
}

static inline struct mwl_vif *mwl_rx_find_vif_bss(struct list_head *vif_list, u8 *bssid)
{
	struct mwl_vif *mwl_vif;

	list_for_each_entry(mwl_vif, vif_list, list) {

		if (memcmp(bssid, mwl_vif->bssid, ETH_ALEN) == 0)
			return mwl_vif;
	}

	return NULL;
}

static inline void mwl_rx_remove_dma_header(struct sk_buff *skb, u16 qos)
{
	struct mwl_dma_data *tr;
	int hdrlen;

	tr = (struct mwl_dma_data *)skb->data;
	hdrlen = ieee80211_hdrlen(tr->wh.frame_control);

	if (hdrlen != sizeof(tr->wh)) {

		if (ieee80211_is_data_qos(tr->wh.frame_control)) {
			memmove(tr->data - hdrlen, &tr->wh, hdrlen - 2);
			*((u16 *)(tr->data - 2)) = qos;
		} else {
			memmove(tr->data - hdrlen, &tr->wh, hdrlen);
		}
	}

	if (hdrlen != sizeof(*tr))
		skb_pull(skb, sizeof(*tr) - hdrlen);
}

static int mwl_rx_refill(struct mwl_priv *priv, struct mwl_rx_desc *pdesc)
{
	WLDBG_ENTER(DBG_LEVEL_4);

	BUG_ON(!priv);
	BUG_ON(!pdesc);

	pdesc->psk_buff = dev_alloc_skb(priv->desc_data[0].rx_buf_size);

	if (pdesc->psk_buff == NULL)
		goto nomem;

	if (skb_linearize(pdesc->psk_buff)) {

		dev_kfree_skb_any(pdesc->psk_buff);
		WLDBG_ERROR(DBG_LEVEL_4, "need linearize memory");
		goto nomem;
	}

	skb_reserve(pdesc->psk_buff, SYSADPT_MIN_BYTES_HEADROOM);

	pdesc->status = EAGLE_RXD_STATUS_OK;
	pdesc->qos_ctrl = 0x0000;
	pdesc->channel = 0x00;
	pdesc->rssi = 0x00;
	pdesc->sq2 = 0x00;

	pdesc->pkt_len = priv->desc_data[0].rx_buf_size;
	pdesc->pbuff_data = pdesc->psk_buff->data;
	pdesc->pphys_buff_data =
		ENDIAN_SWAP32(pci_map_single(priv->pdev,
					     pdesc->psk_buff->data,
			priv->desc_data[0].rx_buf_size,
			PCI_DMA_BIDIRECTIONAL));

	WLDBG_EXIT(DBG_LEVEL_4);

	return 0;

nomem:

	WLDBG_EXIT_INFO(DBG_LEVEL_4, "no memory");

	return -ENOMEM;
}
