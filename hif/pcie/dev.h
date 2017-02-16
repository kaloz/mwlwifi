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

/* Description:  This file defines device related information. */

#ifndef _DEV_H_
#define _DEV_H_

#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <net/mac80211.h>

#define PCIE_DRV_NAME    KBUILD_MODNAME
#define PCIE_DRV_VERSION "10.3.2.0-20170110"

#define PCIE_MIN_BYTES_HEADROOM   64
#define PCIE_NUM_OF_DESC_DATA     (4 + SYSADPT_NUM_OF_AP)
#define PCIE_TOTAL_HW_QUEUES      (SYSADPT_TX_WMM_QUEUES + \
					SYSADPT_TX_AMPDU_QUEUES)
#define PCIE_MAX_NUM_TX_DESC      256
#define PCIE_TX_QUEUE_LIMIT       (3 * PCIE_MAX_NUM_TX_DESC)
#define PCIE_TX_WAKE_Q_THRESHOLD  (2 * PCIE_MAX_NUM_TX_DESC)
#define PCIE_DELAY_FREE_Q_LIMIT   PCIE_MAX_NUM_TX_DESC
#define PCIE_MAX_NUM_RX_DESC      256
#define PCIE_RECEIVE_LIMIT        64

#define MAC_REG_ADDR(offset)      (offset)
#define MAC_REG_ADDR_PCI(offset)  ((pcie_priv->iobase1 + 0xA000) + offset)

#define MCU_CCA_CNT               MAC_REG_ADDR(0x06A0)
#define MCU_TXPE_CNT              MAC_REG_ADDR(0x06A4)
#define MCU_LAST_READ             MAC_REG_ADDR(0x06A8)

/* Map to 0x80000000 (Bus control) on BAR0 */
#define MACREG_REG_H2A_INTERRUPT_EVENTS      0x00000C18 /* (From host to ARM) */
#define MACREG_REG_H2A_INTERRUPT_CAUSE       0x00000C1C /* (From host to ARM) */
#define MACREG_REG_H2A_INTERRUPT_MASK        0x00000C20 /* (From host to ARM) */
#define MACREG_REG_H2A_INTERRUPT_CLEAR_SEL   0x00000C24 /* (From host to ARM) */
#define MACREG_REG_H2A_INTERRUPT_STATUS_MASK 0x00000C28 /* (From host to ARM) */

#define MACREG_REG_A2H_INTERRUPT_EVENTS      0x00000C2C /* (From ARM to host) */
#define MACREG_REG_A2H_INTERRUPT_CAUSE       0x00000C30 /* (From ARM to host) */
#define MACREG_REG_A2H_INTERRUPT_MASK        0x00000C34 /* (From ARM to host) */
#define MACREG_REG_A2H_INTERRUPT_CLEAR_SEL   0x00000C38 /* (From ARM to host) */
#define MACREG_REG_A2H_INTERRUPT_STATUS_MASK 0x00000C3C /* (From ARM to host) */

/* Map to 0x80000000 on BAR1 */
#define MACREG_REG_GEN_PTR                  0x00000C10
#define MACREG_REG_INT_CODE                 0x00000C14

/* Bit definition for MACREG_REG_A2H_INTERRUPT_CAUSE (A2HRIC) */
#define MACREG_A2HRIC_BIT_TX_DONE           BIT(0)
#define MACREG_A2HRIC_BIT_RX_RDY            BIT(1)
#define MACREG_A2HRIC_BIT_OPC_DONE          BIT(2)
#define MACREG_A2HRIC_BIT_MAC_EVENT         BIT(3)
#define MACREG_A2HRIC_BIT_RX_PROBLEM        BIT(4)
#define MACREG_A2HRIC_BIT_RADIO_OFF         BIT(5)
#define MACREG_A2HRIC_BIT_RADIO_ON          BIT(6)
#define MACREG_A2HRIC_BIT_RADAR_DETECT      BIT(7)
#define MACREG_A2HRIC_BIT_ICV_ERROR         BIT(8)
#define MACREG_A2HRIC_BIT_WEAKIV_ERROR      BIT(9)
#define MACREG_A2HRIC_BIT_QUE_EMPTY         BIT(10)
#define MACREG_A2HRIC_BIT_QUE_FULL          BIT(11)
#define MACREG_A2HRIC_BIT_CHAN_SWITCH       BIT(12)
#define MACREG_A2HRIC_BIT_TX_WATCHDOG       BIT(13)
#define MACREG_A2HRIC_BA_WATCHDOG           BIT(14)
/* 15 taken by ISR_TXACK */
#define MACREG_A2HRIC_BIT_SSU_DONE          BIT(16)
#define MACREG_A2HRIC_CONSEC_TXFAIL         BIT(17)

#define ISR_SRC_BITS        (MACREG_A2HRIC_BIT_RX_RDY | \
			     MACREG_A2HRIC_BIT_TX_DONE | \
			     MACREG_A2HRIC_BIT_OPC_DONE | \
			     MACREG_A2HRIC_BIT_MAC_EVENT | \
			     MACREG_A2HRIC_BIT_WEAKIV_ERROR | \
			     MACREG_A2HRIC_BIT_ICV_ERROR | \
			     MACREG_A2HRIC_BIT_SSU_DONE | \
			     MACREG_A2HRIC_BIT_RADAR_DETECT | \
			     MACREG_A2HRIC_BIT_CHAN_SWITCH | \
			     MACREG_A2HRIC_BIT_TX_WATCHDOG | \
			     MACREG_A2HRIC_BIT_QUE_EMPTY | \
			     MACREG_A2HRIC_BA_WATCHDOG | \
			     MACREG_A2HRIC_CONSEC_TXFAIL)

#define MACREG_A2HRIC_BIT_MASK      ISR_SRC_BITS

/* Bit definition for MACREG_REG_H2A_INTERRUPT_CAUSE (H2ARIC) */
#define MACREG_H2ARIC_BIT_PPA_READY         BIT(0)
#define MACREG_H2ARIC_BIT_DOOR_BELL         BIT(1)
#define MACREG_H2ARIC_BIT_PS                BIT(2)
#define MACREG_H2ARIC_BIT_PSPOLL            BIT(3)
#define ISR_RESET                           BIT(15)
#define ISR_RESET_AP33                      BIT(26)

/* Data descriptor related constants */
#define EAGLE_RXD_CTRL_DRIVER_OWN           0x00
#define EAGLE_RXD_CTRL_DMA_OWN              0x80

#define EAGLE_RXD_STATUS_OK                 0x01

#define EAGLE_TXD_STATUS_IDLE               0x00000000
#define EAGLE_TXD_STATUS_OK                 0x00000001
#define EAGLE_TXD_STATUS_FW_OWNED           0x80000000

#define MWL_TX_RATE_FORMAT_MASK       0x00000003
#define MWL_TX_RATE_BANDWIDTH_MASK    0x00000030
#define MWL_TX_RATE_BANDWIDTH_SHIFT   4
#define MWL_TX_RATE_SHORTGI_MASK      0x00000040
#define MWL_TX_RATE_SHORTGI_SHIFT     6
#define MWL_TX_RATE_RATEIDMCS_MASK    0x00007F00
#define MWL_TX_RATE_RATEIDMCS_SHIFT   8

struct pcie_tx_desc {
	u8 data_rate;
	u8 tx_priority;
	__le16 qos_ctrl;
	__le32 pkt_ptr;
	__le16 pkt_len;
	u8 dest_addr[ETH_ALEN];
	__le32 pphys_next;
	__le32 sap_pkt_info;
	__le32 rate_info;
	u8 type;
	u8 xmit_control;     /* bit 0: use rateinfo, bit 1: disable ampdu */
	__le16 reserved;
	__le32 tcpack_sn;
	__le32 tcpack_src_dst;
	__le32 reserved1;
	__le32 reserved2;
	u8 reserved3[2];
	u8 packet_info;
	u8 packet_id;
	__le16 packet_len_and_retry;
	__le16 packet_rate_info;
	__le32 reserved4;
	__le32 status;
} __packed;

struct pcie_tx_hndl {
	struct sk_buff *psk_buff;
	struct pcie_tx_desc *pdesc;
	struct pcie_tx_hndl *pnext;
};

#define MWL_RX_RATE_FORMAT_MASK       0x0007
#define MWL_RX_RATE_NSS_MASK          0x0018
#define MWL_RX_RATE_NSS_SHIFT         3
#define MWL_RX_RATE_BW_MASK           0x0060
#define MWL_RX_RATE_BW_SHIFT          5
#define MWL_RX_RATE_GI_MASK           0x0080
#define MWL_RX_RATE_GI_SHIFT          7
#define MWL_RX_RATE_RT_MASK           0xFF00
#define MWL_RX_RATE_RT_SHIFT          8

struct pcie_rx_desc {
	__le16 pkt_len;              /* total length of received data      */
	__le16 rate;                 /* receive rate information           */
	__le32 pphys_buff_data;      /* physical address of payload data   */
	__le32 pphys_next;           /* physical address of next RX desc   */
	__le16 qos_ctrl;             /* received QosCtrl field variable    */
	__le16 ht_sig2;              /* like name states                   */
	__le32 hw_rssi_info;
	__le32 hw_noise_floor_info;
	u8 noise_floor;
	u8 reserved[3];
	u8 rssi;                     /* received signal strengt indication */
	u8 status;                   /* status field containing USED bit   */
	u8 channel;                  /* channel this pkt was received on   */
	u8 rx_control;               /* the control element of the desc    */
	__le32 reserved1[3];
} __packed;

struct pcie_rx_hndl {
	struct sk_buff *psk_buff;    /* associated sk_buff for Linux       */
	struct pcie_rx_desc *pdesc;
	struct pcie_rx_hndl *pnext;
};

struct pcie_desc_data {
	dma_addr_t pphys_tx_ring;          /* ptr to first TX desc (phys.)    */
	struct pcie_tx_desc *ptx_ring;     /* ptr to first TX desc (virt.)    */
	struct pcie_tx_hndl *tx_hndl;
	struct pcie_tx_hndl *pnext_tx_hndl;/* next TX handle that can be used */
	struct pcie_tx_hndl *pstale_tx_hndl;/* the staled TX handle           */
	dma_addr_t pphys_rx_ring;          /* ptr to first RX desc (phys.)    */
	struct pcie_rx_desc *prx_ring;     /* ptr to first RX desc (virt.)    */
	struct pcie_rx_hndl *rx_hndl;
	struct pcie_rx_hndl *pnext_rx_hndl;/* next RX handle that can be used */
	u32 wcb_base;                      /* FW base offset for registers    */
	u32 rx_desc_write;                 /* FW descriptor write position    */
	u32 rx_desc_read;                  /* FW descriptor read position     */
	u32 rx_buf_size;                   /* length of the RX buffers        */
};

struct pcie_priv {
	struct mwl_priv *mwl_priv;
	struct pci_dev *pdev;
	void __iomem *iobase0; /* MEM Base Address Register 0  */
	void __iomem *iobase1; /* MEM Base Address Register 1  */
	u32 next_bar_num;

	/* various descriptor data */
	/* for tx descriptor data  */
	spinlock_t tx_desc_lock ____cacheline_aligned_in_smp;
	struct pcie_desc_data desc_data[PCIE_NUM_OF_DESC_DATA];
	struct sk_buff_head txq[PCIE_NUM_OF_DESC_DATA];
	struct sk_buff_head delay_q;
	/* number of descriptors owned by fw at any one time */
	int fw_desc_cnt[PCIE_NUM_OF_DESC_DATA];

	struct tasklet_struct tx_task;
	struct tasklet_struct tx_done_task;
	struct tasklet_struct rx_task;
	struct tasklet_struct qe_task;
	int txq_limit;
	bool is_tx_done_schedule;
	int recv_limit;
	bool is_rx_schedule;
	bool is_qe_schedule;
	u32 qe_trig_num;
	unsigned long qe_trig_time;
};

/* DMA header used by firmware and hardware. */
struct pcie_dma_data {
	__le16 fwlen;
	struct ieee80211_hdr wh;
	char data[0];
} __packed;
#endif /* _DEV_H_ */
