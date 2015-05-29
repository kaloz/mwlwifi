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
*   Description:  This file implements transmit related functions.
*
*/

#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include "mwl_sysadpt.h"
#include "mwl_dev.h"
#include "mwl_debug.h"
#include "mwl_fwcmd.h"
#include "mwl_tx.h"

/* CONSTANTS AND MACROS
*/

#define MAX_NUM_TX_RING_BYTES  (SYSADPT_MAX_NUM_TX_DESC * \
				sizeof(struct mwl_tx_desc))

#define FIRST_TXD(i) priv->desc_data[i].ptx_ring[0]
#define CURR_TXD(i)  priv->desc_data[i].ptx_ring[curr_desc]
#define NEXT_TXD(i)  priv->desc_data[i].ptx_ring[curr_desc + 1]
#define LAST_TXD(i)  priv->desc_data[i].ptx_ring[SYSADPT_MAX_NUM_TX_DESC - 1]

#define STALE_TXD(i) priv->desc_data[i].pstale_tx_desc

#define EAGLE_TXD_XMITCTRL_USE_MC_RATE     0x8     /* Use multicast data rate */

#define MWL_QOS_ACK_POLICY_MASK	           0x0060
#define MWL_QOS_ACK_POLICY_NORMAL          0x0000
#define MWL_QOS_ACK_POLICY_BLOCKACK        0x0060

#define EXT_IV                             0x20
#define INCREASE_IV(iv16, iv32) \
{ \
	(iv16)++; \
	if ((iv16) == 0) \
		(iv32)++; \
}

/* Transmit rate information constants
*/
#define TX_RATE_FORMAT_LEGACY         0
#define TX_RATE_FORMAT_11N            1
#define TX_RATE_FORMAT_11AC           2

#define TX_RATE_BANDWIDTH_20          0
#define TX_RATE_BANDWIDTH_40          1
#define TX_RATE_BANDWIDTH_80          2

#define TX_RATE_INFO_STD_GI           0
#define TX_RATE_INFO_SHORT_GI         1

/* PRIVATE FUNCTION DECLARATION
*/

static int mwl_tx_ring_alloc(struct mwl_priv *priv);
static int mwl_tx_ring_init(struct mwl_priv *priv);
static void mwl_tx_ring_cleanup(struct mwl_priv *priv);
static void mwl_tx_ring_free(struct mwl_priv *priv);
static inline void mwl_tx_add_dma_header(struct mwl_priv *priv,
					 struct sk_buff *skb,
					 int head_pad,
					 int tail_pad);
static inline void mwl_tx_encapsulate_frame(struct mwl_priv *priv,
					    struct sk_buff *skb, bool *ccmp);
static inline void mwl_tx_insert_ccmp_hdr(u8 *pccmp_hdr,
					  u8 key_id, u16 iv16, u32 iv32);
static inline int mwl_tx_tid_queue_mapping(u8 tid);
static inline void mwl_tx_count_packet(struct ieee80211_sta *sta, u8 tid);
static inline void mwl_tx_skbs(struct ieee80211_hw *hw);
/* static void mwl_tx_descriptor_dump(struct mwl_priv *priv); */

/* PUBLIC FUNCTION DEFINITION
*/

int mwl_tx_init(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;
	int rc;

	WLDBG_ENTER(DBG_LEVEL_3);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	skb_queue_head_init(&priv->delay_q);

	rc = mwl_tx_ring_alloc(priv);
	if (rc) {
		WLDBG_ERROR(DBG_LEVEL_3, "allocating TX ring failed");
	} else {
		rc = mwl_tx_ring_init(priv);
		if (rc) {
			mwl_tx_ring_free(priv);
			WLDBG_ERROR(DBG_LEVEL_3, "initializing TX ring failed");
		}
	}

	WLDBG_EXIT(DBG_LEVEL_3);

	return rc;
}

void mwl_tx_deinit(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;

	WLDBG_ENTER(DBG_LEVEL_3);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	skb_queue_purge(&priv->delay_q);

	mwl_tx_ring_cleanup(priv);
	mwl_tx_ring_free(priv);

	WLDBG_EXIT(DBG_LEVEL_3);
}

void mwl_tx_xmit(struct ieee80211_hw *hw,
		 int index,
	struct ieee80211_sta *sta,
	struct sk_buff *skb)
{
	struct mwl_priv *priv;
	struct ieee80211_tx_info *tx_info;
	struct mwl_vif *mwl_vif;
	struct ieee80211_hdr *wh;
	u8 xmitcontrol;
	u16 qos;
	int txpriority;
	u8 tid = 0;
	struct mwl_ampdu_stream *stream = NULL;
	bool start_ba_session = false;
	bool mgmtframe = false;
	struct ieee80211_mgmt *mgmt;
	bool eapol_frame = false;
	bool ccmp = false;
	struct mwl_dma_data *dma_data;
	struct mwl_tx_ctrl *tx_ctrl;

	WLDBG_ENTER(DBG_LEVEL_3);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	BUG_ON(!skb);

	wh = (struct ieee80211_hdr *)skb->data;

	if (ieee80211_is_data_qos(wh->frame_control))
		qos = ENDIAN_SWAP16(*((u16 *)ieee80211_get_qos_ctl(wh)));
	else
		qos = 0;

	if (skb->protocol == cpu_to_be16(ETH_P_PAE)) {
		index = IEEE80211_AC_VO;
		eapol_frame = true;
	}

	if (ieee80211_is_mgmt(wh->frame_control)) {
		mgmtframe = true;
		mgmt = (struct ieee80211_mgmt *)skb->data;
	}

	mwl_tx_encapsulate_frame(priv, skb, &ccmp);

	dma_data = (struct mwl_dma_data *)skb->data;
	wh = &dma_data->wh;

	tx_info = IEEE80211_SKB_CB(skb);
	mwl_vif = MWL_VIF(tx_info->control.vif);

	if (tx_info->flags & IEEE80211_TX_CTL_ASSIGN_SEQ) {
		wh->seq_ctrl &= ENDIAN_SWAP16(IEEE80211_SCTL_FRAG);
		wh->seq_ctrl |= ENDIAN_SWAP16(mwl_vif->seqno);
		mwl_vif->seqno += 0x10;
	}

	/* Setup firmware control bit fields for each frame type.
	*/
	xmitcontrol = 0;

	if (mgmtframe || ieee80211_is_ctl(wh->frame_control)) {
		qos = 0;
	} else if (ieee80211_is_data(wh->frame_control)) {
		qos &= ~MWL_QOS_ACK_POLICY_MASK;

		if (tx_info->flags & IEEE80211_TX_CTL_AMPDU) {
			xmitcontrol &= 0xfb;
			qos |= MWL_QOS_ACK_POLICY_BLOCKACK;
		} else {
			xmitcontrol |= 0x4;
			qos |= MWL_QOS_ACK_POLICY_NORMAL;
		}

		if (is_multicast_ether_addr(wh->addr1)) {
			xmitcontrol |= EAGLE_TXD_XMITCTRL_USE_MC_RATE;

			if (ccmp) {
				mwl_tx_insert_ccmp_hdr(dma_data->data,
						       mwl_vif->keyidx,
						       mwl_vif->iv16,
						       mwl_vif->iv32);
				INCREASE_IV(mwl_vif->iv16, mwl_vif->iv32);
			}
		} else {
			if (ccmp) {
				if (mwl_vif->is_sta == true) {
					mwl_tx_insert_ccmp_hdr(dma_data->data,
							       mwl_vif->keyidx,
							       mwl_vif->iv16,
							       mwl_vif->iv32);
					INCREASE_IV(mwl_vif->iv16,
						    mwl_vif->iv32);
				} else {
					struct mwl_sta *sta_info = MWL_STA(sta);

					mwl_tx_insert_ccmp_hdr(dma_data->data,
							       0,
							       sta_info->iv16,
							       sta_info->iv32);
					INCREASE_IV(sta_info->iv16,
						    sta_info->iv32);
				}
			}
		}
	}

	/* Queue ADDBA request in the respective data queue.  While setting up
	 * the ampdu stream, mac80211 queues further packets for that
	 * particular ra/tid pair.  However, packets piled up in the hardware
	 * for that ra/tid pair will still go out. ADDBA request and the
	 * related data packets going out from different queues asynchronously
	 * will cause a shift in the receiver window which might result in
	 * ampdu packets getting dropped at the receiver after the stream has
	 * been setup.
	 */
	if (mgmtframe) {
		if (unlikely(ieee80211_is_action(wh->frame_control) &&
		    mgmt->u.action.category == WLAN_CATEGORY_BACK &&
		    mgmt->u.action.u.addba_req.action_code ==
		    WLAN_ACTION_ADDBA_REQ)) {
			u16 capab =
				ENDIAN_SWAP16(mgmt->u.action.u.addba_req.capab);
			tid = (capab & IEEE80211_ADDBA_PARAM_TID_MASK) >> 2;
			index = mwl_tx_tid_queue_mapping(tid);
		}
	}

	txpriority = SYSADPT_TX_WMM_QUEUES - index - 1;

	if (sta && sta->ht_cap.ht_supported && !eapol_frame &&
	    ieee80211_is_data_qos(wh->frame_control)) {
		tid = qos & 0xf;
		mwl_tx_count_packet(sta, tid);

		SPIN_LOCK(&priv->locks.stream_lock);
		stream = mwl_fwcmd_lookup_stream(hw, sta->addr, tid);

		if (stream != NULL) {
			if (stream->state == AMPDU_STREAM_ACTIVE) {
				WARN_ON(!(qos & MWL_QOS_ACK_POLICY_BLOCKACK));

				txpriority =
					(SYSADPT_TX_WMM_QUEUES + stream->idx) %
					SYSADPT_TOTAL_HW_QUEUES;
			} else if (stream->state == AMPDU_STREAM_NEW) {
				/* We get here if the driver sends us packets
				 * after we've initiated a stream, but before
				 * our ampdu_action routine has been called
				 * with IEEE80211_AMPDU_TX_START to get the SSN
				 * for the ADDBA request.  So this packet can
				 * go out with no risk of sequence number
				 * mismatch.  No special handling is required.
				 */
			} else {
				/* Drop packets that would go out after the
				 * ADDBA request was sent but before the ADDBA
				 * response is received.  If we don't do this,
				 * the recipient would probably receive it
				 * after the ADDBA request with SSN 0.  This
				 * will cause the recipient's BA receive window
				 * to shift, which would cause the subsequent
				 * packets in the BA stream to be discarded.
				 * mac80211 queues our packets for us in this
				 * case, so this is really just a safety check.
				 */
				WLDBG_WARNING(DBG_LEVEL_3,
					      "can't send packet during ADDBA");
				SPIN_UNLOCK(&priv->locks.stream_lock);
				dev_kfree_skb_any(skb);
				return;
			}
		} else {
			/* Defer calling mwl8k_start_stream so that the current
			 * skb can go out before the ADDBA request.  This
			 * prevents sequence number mismatch at the recepient
			 * as described above.
			 */
			if (mwl_fwcmd_ampdu_allowed(sta, tid)) {
				stream = mwl_fwcmd_add_stream(hw, sta, tid);

				if (stream != NULL)
					start_ba_session = true;
			}
		}

		SPIN_UNLOCK(&priv->locks.stream_lock);
	} else {
		qos &= ~MWL_QOS_ACK_POLICY_MASK;
		qos |= MWL_QOS_ACK_POLICY_NORMAL;
	}

	tx_ctrl = (struct mwl_tx_ctrl *)&tx_info->status;
	tx_ctrl->tx_priority = txpriority;
	tx_ctrl->qos_ctrl = qos;
	tx_ctrl->type = (mgmtframe ? IEEE_TYPE_MANAGEMENT : IEEE_TYPE_DATA);
	tx_ctrl->xmit_control = xmitcontrol;
	tx_ctrl->sta_info = (u8 *)sta;
	tx_ctrl->ccmp = ccmp;

	if (skb_queue_len(&priv->txq[index]) > priv->txq_limit) {
		dev_kfree_skb_any(skb);
		WLDBG_INFO(DBG_LEVEL_3, "queue len > limit");
	} else {
		skb_queue_tail(&priv->txq[index], skb);
	}

	mwl_tx_skbs(hw);

	/* Initiate the ampdu session here
	*/
	if (start_ba_session) {
		SPIN_LOCK(&priv->locks.stream_lock);
		if (mwl_fwcmd_start_stream(hw, stream))
			mwl_fwcmd_remove_stream(hw, stream);
		SPIN_UNLOCK(&priv->locks.stream_lock);
	}

	WLDBG_EXIT(DBG_LEVEL_3);
}

void mwl_tx_done(unsigned long data)
{
	struct ieee80211_hw *hw = (struct ieee80211_hw *)data;
	struct mwl_priv *priv;
	unsigned long flags;
	int num;
	struct sk_buff *done_skb;
	struct mwl_rate_info rate_info;
	struct mwl_dma_data *tr;
	struct ieee80211_tx_info *info;
	int hdrlen;

	WLDBG_ENTER(DBG_LEVEL_3);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	SPIN_LOCK_IRQSAVE(&priv->locks.xmit_lock, flags);

	for (num = 0; num < SYSADPT_NUM_OF_DESC_DATA; num++) {
		while (STALE_TXD(num)
		       && (STALE_TXD(num)->status &
		       ENDIAN_SWAP32(EAGLE_TXD_STATUS_OK))
		       && (!(STALE_TXD(num)->status &
		       ENDIAN_SWAP32(EAGLE_TXD_STATUS_FW_OWNED)))) {
			pci_unmap_single(priv->pdev,
				ENDIAN_SWAP32(STALE_TXD(num)->pkt_ptr),
				STALE_TXD(num)->psk_buff->len,
				PCI_DMA_TODEVICE);
			done_skb = STALE_TXD(num)->psk_buff;
			rate_info = STALE_TXD(num)->rate_info;
			STALE_TXD(num)->pkt_len = 0;
			STALE_TXD(num)->psk_buff = NULL;
			STALE_TXD(num)->status =
				ENDIAN_SWAP32(EAGLE_TXD_STATUS_IDLE);
			priv->fw_desc_cnt[num]--;
			STALE_TXD(num) = STALE_TXD(num)->pnext;
			wmb(); /* memory barrier */

			tr = (struct mwl_dma_data *)done_skb->data;
			info = IEEE80211_SKB_CB(done_skb);
			ieee80211_tx_info_clear_status(info);

			info->status.rates[0].idx = -1;

			if (ieee80211_is_data(tr->wh.frame_control) ||
			    ieee80211_is_data_qos(tr->wh.frame_control)) {
				skb_get(done_skb);
				skb_queue_tail(&priv->delay_q, done_skb);

				if (skb_queue_len(&priv->delay_q) >
				    SYSADPT_DELAY_FREE_Q_LIMIT)
					dev_kfree_skb_any(
						skb_dequeue(&priv->delay_q));

				/* Prepare rate information
				*/
				info->status.rates[0].idx =
					rate_info.rate_id_mcs;
				if (rate_info.format == TX_RATE_FORMAT_LEGACY) {
					if (hw->conf.chandef.chan->hw_value >
					    BAND_24_CHANNEL_NUM) {
						info->status.rates[0].idx -= 5;
					}
				}
				if (rate_info.format == TX_RATE_FORMAT_11N)
					info->status.rates[0].flags |=
						IEEE80211_TX_RC_MCS;
				if (rate_info.format == TX_RATE_FORMAT_11AC)
					info->status.rates[0].flags |=
						IEEE80211_TX_RC_VHT_MCS;
				if (rate_info.bandwidth == TX_RATE_BANDWIDTH_40)
					info->status.rates[0].flags |=
						IEEE80211_TX_RC_40_MHZ_WIDTH;
				if (rate_info.bandwidth == TX_RATE_BANDWIDTH_80)
					info->status.rates[0].flags |=
						IEEE80211_TX_RC_80_MHZ_WIDTH;
				if (rate_info.short_gi == TX_RATE_INFO_SHORT_GI)
					info->status.rates[0].flags |=
						IEEE80211_TX_RC_SHORT_GI;
				info->status.rates[0].count = 1;

				info->status.rates[1].idx = -1;
			}

			/* Remove H/W dma header
			*/
			hdrlen = ieee80211_hdrlen(tr->wh.frame_control);
			memmove(tr->data - hdrlen, &tr->wh, hdrlen);
			skb_pull(done_skb, sizeof(*tr) - hdrlen);

			info->flags |= IEEE80211_TX_STAT_ACK;
			ieee80211_tx_status(hw, done_skb);
		}
	}

	SPIN_UNLOCK_IRQRESTORE(&priv->locks.xmit_lock, flags);

	if (priv->irq != -1) {
		u32 status;

		status = readl(priv->iobase1 +
			       MACREG_REG_A2H_INTERRUPT_STATUS_MASK);
		writel(status | MACREG_A2HRIC_BIT_TX_DONE,
		       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK);

		mwl_tx_skbs(hw);
	}

	priv->is_tx_schedule = false;

	WLDBG_EXIT(DBG_LEVEL_3);
}

/* PRIVATE FUNCTION DEFINITION
*/

static int mwl_tx_ring_alloc(struct mwl_priv *priv)
{
	int num;
	u8 *mem;

	WLDBG_ENTER(DBG_LEVEL_3);

	BUG_ON(!priv);

	mem = (u8 *)dma_alloc_coherent(&priv->pdev->dev,
		MAX_NUM_TX_RING_BYTES * SYSADPT_NUM_OF_DESC_DATA,
		&priv->desc_data[0].pphys_tx_ring, GFP_KERNEL);

	if (mem == NULL) {
		WLDBG_ERROR(DBG_LEVEL_3, "can not alloc mem");
		WLDBG_EXIT_INFO(DBG_LEVEL_3, "no memory");
		return -ENOMEM;
	}

	for (num = 0; num < SYSADPT_NUM_OF_DESC_DATA; num++) {
		WLDBG_INFO(DBG_LEVEL_3, "allocating %i (0x%x) bytes",
			   MAX_NUM_TX_RING_BYTES, MAX_NUM_TX_RING_BYTES);

		priv->desc_data[num].ptx_ring = (struct mwl_tx_desc *)
			(mem + num * MAX_NUM_TX_RING_BYTES);

		priv->desc_data[num].pphys_tx_ring = (dma_addr_t)
			((u32)priv->desc_data[0].pphys_tx_ring +
			num * MAX_NUM_TX_RING_BYTES);

		memset(priv->desc_data[num].ptx_ring, 0x00,
		       MAX_NUM_TX_RING_BYTES);

		WLDBG_INFO(DBG_LEVEL_3, "TX ring vaddr: 0x%x paddr: 0x%x",
			   priv->desc_data[num].ptx_ring,
			   priv->desc_data[num].pphys_tx_ring);
	}

	WLDBG_EXIT(DBG_LEVEL_3);

	return 0;
}

static int mwl_tx_ring_init(struct mwl_priv *priv)
{
	int curr_desc;
	struct mwl_desc_data *desc;
	int num;

	WLDBG_ENTER(DBG_LEVEL_3);

	BUG_ON(!priv);

	for (num = 0; num < SYSADPT_NUM_OF_DESC_DATA; num++) {
		skb_queue_head_init(&priv->txq[num]);
		priv->fw_desc_cnt[num] = 0;

		desc = &priv->desc_data[num];

		if (desc->ptx_ring != NULL) {
			WLDBG_INFO(DBG_LEVEL_3, "initializing %i descriptors",
				   SYSADPT_MAX_NUM_TX_DESC);

			for (curr_desc = 0; curr_desc < SYSADPT_MAX_NUM_TX_DESC;
			     curr_desc++) {
				CURR_TXD(num).status =
					ENDIAN_SWAP32(EAGLE_TXD_STATUS_IDLE);
				CURR_TXD(num).pnext = &NEXT_TXD(num);
				CURR_TXD(num).pphys_next =
					ENDIAN_SWAP32((u32)desc->pphys_tx_ring +
					((curr_desc + 1) *
					sizeof(struct mwl_tx_desc)));
				WLDBG_INFO(DBG_LEVEL_3,
					   "txdesc: %i 0x%x (%i) 0x%p 0x%x",
					   curr_desc, EAGLE_TXD_STATUS_IDLE,
					   EAGLE_TXD_STATUS_IDLE,
					   CURR_TXD(num).pnext,
					   ENDIAN_SWAP32(
					   CURR_TXD(num).pphys_next));
			}
			LAST_TXD(num).pnext = &FIRST_TXD(num);
			LAST_TXD(num).pphys_next =
				ENDIAN_SWAP32((u32)desc->pphys_tx_ring);
			desc->pstale_tx_desc = &FIRST_TXD(num);
			desc->pnext_tx_desc  = &FIRST_TXD(num);

			WLDBG_INFO(DBG_LEVEL_3,
				   "last txdesc vnext: 0x%p 0x%x 0x%x 0x%x",
				   LAST_TXD(num).pnext,
				   ENDIAN_SWAP32(LAST_TXD(num).pphys_next),
				   desc->pstale_tx_desc, desc->pnext_tx_desc);
		} else {
			WLDBG_ERROR(DBG_LEVEL_3, "no valid TX mem");
			WLDBG_EXIT_INFO(DBG_LEVEL_3, "no valid memory");
			return -ENOMEM;
		}
	}

	WLDBG_EXIT(DBG_LEVEL_3);

	return 0;
}

static void mwl_tx_ring_cleanup(struct mwl_priv *priv)
{
	int cleaned_tx_desc = 0;
	int curr_desc;
	int num;

	WLDBG_ENTER(DBG_LEVEL_3);

	BUG_ON(!priv);

	for (num = 0; num < SYSADPT_NUM_OF_DESC_DATA; num++) {
		skb_queue_purge(&priv->txq[num]);
		priv->fw_desc_cnt[num] = 0;
		if (priv->desc_data[num].ptx_ring != NULL) {
			for (curr_desc = 0; curr_desc < SYSADPT_MAX_NUM_TX_DESC;
			     curr_desc++) {
				if (CURR_TXD(num).psk_buff == NULL)
					continue;

				WLDBG_INFO(DBG_LEVEL_3,
					   "unmapped and free'd %i 0x%p 0x%x",
					   curr_desc,
					   CURR_TXD(num).psk_buff->data,
					   ENDIAN_SWAP32(
					   CURR_TXD(num).pkt_ptr));
				pci_unmap_single(priv->pdev,
						 ENDIAN_SWAP32(
						 CURR_TXD(num).pkt_ptr),
						 CURR_TXD(num).psk_buff->len,
						 PCI_DMA_TODEVICE);
				dev_kfree_skb_any(CURR_TXD(num).psk_buff);
				CURR_TXD(num).status =
					ENDIAN_SWAP32(EAGLE_TXD_STATUS_IDLE);
				CURR_TXD(num).psk_buff = NULL;
				CURR_TXD(num).pkt_ptr = 0;
				CURR_TXD(num).pkt_len = 0;
				cleaned_tx_desc++;
			}
		}
	}

	WLDBG_EXIT_INFO(DBG_LEVEL_3, "cleaned %i TX descr", cleaned_tx_desc);
}

static void mwl_tx_ring_free(struct mwl_priv *priv)
{
	int num;

	WLDBG_ENTER(DBG_LEVEL_3);

	BUG_ON(!priv);

	if (priv->desc_data[0].ptx_ring != NULL) {
		dma_free_coherent(&priv->pdev->dev,
			MAX_NUM_TX_RING_BYTES * SYSADPT_NUM_OF_DESC_DATA,
			priv->desc_data[0].ptx_ring,
			priv->desc_data[0].pphys_tx_ring);
	}

	for (num = 0; num < SYSADPT_NUM_OF_DESC_DATA; num++) {
		if (priv->desc_data[num].ptx_ring != NULL)
			priv->desc_data[num].ptx_ring = NULL;
		priv->desc_data[num].pstale_tx_desc = NULL;
		priv->desc_data[num].pnext_tx_desc  = NULL;
	}

	WLDBG_EXIT(DBG_LEVEL_3);
}

static inline void mwl_tx_add_dma_header(struct mwl_priv *priv,
					 struct sk_buff *skb,
					 int head_pad,
					 int tail_pad)
{
	struct ieee80211_hdr *wh;
	int hdrlen;
	int reqd_hdrlen;
	struct mwl_dma_data *tr;

	/*
	 * Add a firmware DMA header; the firmware requires that we
	 * present a 2-byte payload length followed by a 4-address
	 * header (without QoS field), followed (optionally) by any
	 * WEP/ExtIV header (but only filled in for CCMP).
	 */
	wh = (struct ieee80211_hdr *)skb->data;

	hdrlen = ieee80211_hdrlen(wh->frame_control);

	reqd_hdrlen = sizeof(*tr) + head_pad;

	if (hdrlen != reqd_hdrlen)
		skb_push(skb, reqd_hdrlen - hdrlen);

	if (ieee80211_is_data_qos(wh->frame_control))
		hdrlen -= IEEE80211_QOS_CTL_LEN;

	tr = (struct mwl_dma_data *)skb->data;

	if (wh != &tr->wh)
		memmove(&tr->wh, wh, hdrlen);

	if (hdrlen != sizeof(tr->wh))
		memset(((void *)&tr->wh) + hdrlen, 0, sizeof(tr->wh) - hdrlen);

	/*
	 * Firmware length is the length of the fully formed "802.11
	 * payload".  That is, everything except for the 802.11 header.
	 * This includes all crypto material including the MIC.
	 */
	tr->fwlen = ENDIAN_SWAP16(skb->len - sizeof(*tr) + tail_pad);
}

static inline void mwl_tx_encapsulate_frame(struct mwl_priv *priv,
					    struct sk_buff *skb, bool *ccmp)
{
	struct ieee80211_hdr *wh;
	struct ieee80211_tx_info *tx_info;
	struct ieee80211_key_conf *key_conf;
	int data_pad;
	int head_pad = 0;

	wh = (struct ieee80211_hdr *)skb->data;

	tx_info = IEEE80211_SKB_CB(skb);

	key_conf = NULL;

	if (ieee80211_is_data(wh->frame_control))
		key_conf = tx_info->control.hw_key;

	/*
	 * Make sure the packet header is in the DMA header format (4-address
	 * without QoS), and add head & tail padding when HW crypto is enabled.
	 *
	 * We have the following trailer padding requirements:
	 * - WEP: 4 trailer bytes (ICV)
	 * - TKIP: 12 trailer bytes (8 MIC + 4 ICV)
	 * - CCMP: 8 trailer bytes (MIC)
	 */
	data_pad = 0;

	if (key_conf != NULL) {
		head_pad = key_conf->iv_len;

		switch (key_conf->cipher) {
		case WLAN_CIPHER_SUITE_WEP40:
		case WLAN_CIPHER_SUITE_WEP104:
			data_pad = 4;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			data_pad = 12;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			data_pad = 8;
			*ccmp = true;
			break;
		}
	}

	mwl_tx_add_dma_header(priv, skb, head_pad, data_pad);
}

static inline void mwl_tx_insert_ccmp_hdr(u8 *pccmp_hdr,
					  u8 key_id, u16 iv16, u32 iv32)
{
	*((u16 *)pccmp_hdr) = iv16;
	pccmp_hdr[2] = 0;
	pccmp_hdr[3] = EXT_IV | (key_id << 6);
	*((u32 *)&pccmp_hdr[4]) = iv32;
}

static inline int mwl_tx_tid_queue_mapping(u8 tid)
{
	BUG_ON(tid > 7);

	switch (tid) {
	case 0:
	case 3:
		return IEEE80211_AC_BE;
	case 1:
	case 2:
		return IEEE80211_AC_BK;
	case 4:
	case 5:
		return IEEE80211_AC_VI;
	case 6:
	case 7:
		return IEEE80211_AC_VO;
	default:
		break;
	}

	return -1;
}

static inline void mwl_tx_count_packet(struct ieee80211_sta *sta, u8 tid)
{
	struct mwl_sta *sta_info = MWL_STA(sta);
	struct mwl_tx_info *tx_stats;

	BUG_ON(!sta);
	sta_info = MWL_STA(sta);
	BUG_ON(!sta_info);

	BUG_ON(tid >= SYSADPT_MAX_TID);

	tx_stats = &sta_info->tx_stats[tid];

	if (tx_stats->start_time == 0)
		tx_stats->start_time = jiffies;

	/* reset the packet count after each second elapses.  If the number of
	 * packets ever exceeds the ampdu_min_traffic threshold, we will allow
	 * an ampdu stream to be started.
	 */
	if (jiffies - tx_stats->start_time > HZ) {
		tx_stats->pkts = 0;
		tx_stats->start_time = 0;
	} else
		tx_stats->pkts++;
}

static inline void mwl_tx_skbs(struct ieee80211_hw *hw)
{
	struct mwl_priv *priv;
	unsigned long flags;
	int num = SYSADPT_NUM_OF_DESC_DATA;
	struct mwl_desc_data *desc;
	struct sk_buff *tx_skb;
	struct ieee80211_tx_info *tx_info;
	struct mwl_tx_ctrl *tx_ctrl;

	WLDBG_ENTER(DBG_LEVEL_3);

	BUG_ON(!hw);
	priv = hw->priv;
	BUG_ON(!priv);

	SPIN_LOCK_IRQSAVE(&priv->locks.xmit_lock, flags);

	while (num--) {
		while (skb_queue_len(&priv->txq[num]) > 0) {
			desc = &priv->desc_data[num];

			if (desc->pnext_tx_desc == NULL)
				break;

			/* Only queue to tx desc when Status is 0 (not when 0x1
			 * or 0x80000000). If we queue even when Status==0x1
			 * (DMA'd to fw but txdone haven't change Status to 0),
			 * mismatch of fwDescCnt with actual number of desc
			 * with Status==0 could happen. E.g fwDescCnt 256
			 * instead of 255 when there is one desc with Status==0.
			 * This can cause Tx to stall when fwDescCnt==256 and
			 * pStaleTxDesc->Status==0.
			 */
			if (desc->pnext_tx_desc->status !=
			    EAGLE_TXD_STATUS_IDLE) {
				/* Interrupt F/W anyway
				*/
				if (desc->pnext_tx_desc->status &
				    ENDIAN_SWAP32(EAGLE_TXD_STATUS_FW_OWNED))
					writel(MACREG_H2ARIC_BIT_PPA_READY,
					       priv->iobase1 +
					       MACREG_REG_H2A_INTERRUPT_EVENTS);
				break;
			}

			tx_skb = skb_dequeue(&priv->txq[num]);

			BUG_ON(!tx_skb);

			tx_info = IEEE80211_SKB_CB(tx_skb);
			tx_ctrl = (struct mwl_tx_ctrl *)&tx_info->status;

			desc->pnext_tx_desc->tx_priority = tx_ctrl->tx_priority;
			desc->pnext_tx_desc->qos_ctrl = tx_ctrl->qos_ctrl;
			desc->pnext_tx_desc->psk_buff = tx_skb;
			desc->pnext_tx_desc->pkt_len =
				ENDIAN_SWAP16(tx_skb->len);
			desc->pnext_tx_desc->packet_info = 0;
			desc->pnext_tx_desc->data_rate = 0;
			desc->pnext_tx_desc->sta_info = tx_ctrl->sta_info;
			desc->pnext_tx_desc->type = tx_ctrl->type;
			desc->pnext_tx_desc->xmit_control =
				tx_ctrl->xmit_control;
			desc->pnext_tx_desc->sap_pkt_info = 0;
			desc->pnext_tx_desc->pkt_ptr =
				ENDIAN_SWAP32(pci_map_single(priv->pdev,
							     tx_skb->data,
							     tx_skb->len,
							     PCI_DMA_TODEVICE));
			desc->pnext_tx_desc->status =
				ENDIAN_SWAP32(EAGLE_TXD_STATUS_FW_OWNED);
			desc->pnext_tx_desc = desc->pnext_tx_desc->pnext;
			/* make sure all the memory transactions done by cpu
			 * were completed
			 */
			wmb();	/*Data Memory Barrier*/
			writel(MACREG_H2ARIC_BIT_PPA_READY,
			       priv->iobase1 + MACREG_REG_H2A_INTERRUPT_EVENTS);
			priv->fw_desc_cnt[num]++;
		}
	}

	SPIN_UNLOCK_IRQRESTORE(&priv->locks.xmit_lock, flags);

	WLDBG_EXIT(DBG_LEVEL_3);
}
