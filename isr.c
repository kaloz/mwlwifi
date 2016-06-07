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

/* Description:  This file implements interrupt related functions. */

#include "sysadpt.h"
#include "dev.h"
#include "fwcmd.h"
#include "isr.h"

#define INVALID_WATCHDOG 0xAA

irqreturn_t mwl_isr(int irq, void *dev_id)
{
	struct ieee80211_hw *hw = dev_id;
	struct mwl_priv *priv = hw->priv;
	void __iomem *int_status_mask;
	u32 int_status;
	u32 status;

	int_status_mask = priv->iobase1 + MACREG_REG_A2H_INTERRUPT_STATUS_MASK;

	int_status = readl(priv->iobase1 + MACREG_REG_A2H_INTERRUPT_CAUSE);

	if (int_status == 0x00000000)
		return IRQ_NONE;

	if (int_status == 0xffffffff) {
		wiphy_warn(hw->wiphy, "card unplugged?\n");
	} else {
		writel(~int_status,
		       priv->iobase1 + MACREG_REG_A2H_INTERRUPT_CAUSE);

		if (int_status & MACREG_A2HRIC_BIT_TX_DONE) {
			if (!priv->is_tx_done_schedule) {
				status = readl(int_status_mask);
				writel((status & ~MACREG_A2HRIC_BIT_TX_DONE),
				       int_status_mask);
				tasklet_schedule(&priv->tx_done_task);
				priv->is_tx_done_schedule = true;
			}
		}

		if (int_status & MACREG_A2HRIC_BIT_RX_RDY) {
			if (!priv->is_rx_schedule) {
				status = readl(int_status_mask);
				writel((status & ~MACREG_A2HRIC_BIT_RX_RDY),
				       int_status_mask);
				tasklet_schedule(&priv->rx_task);
				priv->is_rx_schedule = true;
			}
		}

		if (int_status & MACREG_A2HRIC_BIT_RADAR_DETECT) {
			wiphy_info(hw->wiphy, "radar detected by firmware\n");
			ieee80211_radar_detected(hw);
		}

		if (int_status & MACREG_A2HRIC_BIT_QUE_EMPTY) {
			if (!priv->is_qe_schedule) {
				if (time_after(jiffies,
					       (priv->qe_trigger_time + 1))) {
					status = readl(int_status_mask);
					writel((status &
					       ~MACREG_A2HRIC_BIT_QUE_EMPTY),
					       int_status_mask);
					tasklet_schedule(&priv->qe_task);
					priv->qe_trigger_num++;
					priv->is_qe_schedule = true;
					priv->qe_trigger_time = jiffies;
				}
			}
		}

		if (int_status & MACREG_A2HRIC_BIT_CHAN_SWITCH)
			ieee80211_queue_work(hw, &priv->chnl_switch_handle);

		if (int_status & MACREG_A2HRIC_BA_WATCHDOG)
			ieee80211_queue_work(hw, &priv->watchdog_ba_handle);
	}

	return IRQ_HANDLED;
}

void mwl_chnl_switch_event(struct work_struct *work)
{
	struct mwl_priv *priv =
		container_of(work, struct mwl_priv, chnl_switch_handle);
	struct mwl_vif *mwl_vif;
	struct ieee80211_vif *vif;

	if (!priv->csa_active) {
		wiphy_err(priv->hw->wiphy,
			  "csa is not active (got channel switch event)\n");
		return;
	}

	spin_lock_bh(&priv->vif_lock);
	list_for_each_entry(mwl_vif, &priv->vif_list, list) {
		vif = container_of((char *)mwl_vif, struct ieee80211_vif,
				   drv_priv[0]);

		if (vif->csa_active)
			ieee80211_csa_finish(vif);
	}
	spin_unlock_bh(&priv->vif_lock);

	wiphy_info(priv->hw->wiphy, "channel switch is done\n");

	priv->csa_active = false;
}

void mwl_watchdog_ba_events(struct work_struct *work)
{
	int rc;
	u8 bitmap = 0, stream_index;
	struct mwl_ampdu_stream *streams;
	struct mwl_priv *priv =
		container_of(work, struct mwl_priv, watchdog_ba_handle);

	rc = mwl_fwcmd_get_watchdog_bitmap(priv->hw, &bitmap);

	if (rc)
		return;

	spin_lock_bh(&priv->stream_lock);

	/* the bitmap is the hw queue number.  Map it to the ampdu queue. */
	if (bitmap != INVALID_WATCHDOG) {
		if (bitmap == SYSADPT_TX_AMPDU_QUEUES)
			stream_index = 0;
		else if (bitmap > SYSADPT_TX_AMPDU_QUEUES)
			stream_index = bitmap - SYSADPT_TX_AMPDU_QUEUES;
		else
			stream_index = bitmap + 3; /** queue 0 is stream 3*/

		if (bitmap != 0xFF) {
			/* Check if the stream is in use before disabling it */
			streams = &priv->ampdu[stream_index];

			if (streams->state == AMPDU_STREAM_ACTIVE)
				ieee80211_stop_tx_ba_session(streams->sta,
							     streams->tid);
		} else {
			for (stream_index = 0;
			     stream_index < SYSADPT_TX_AMPDU_QUEUES;
			     stream_index++) {
				streams = &priv->ampdu[stream_index];

				if (streams->state != AMPDU_STREAM_ACTIVE)
					continue;

				ieee80211_stop_tx_ba_session(streams->sta,
							     streams->tid);
			}
		}
	}

	spin_unlock_bh(&priv->stream_lock);
}
