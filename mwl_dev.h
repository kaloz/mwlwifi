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
*   Description:  This file defines device related information.
*
*/

#ifndef _mwl_dev_h_
#define _mwl_dev_h_

#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <net/mac80211.h>

/* CONSTANTS AND MACROS
*/

/* Map to 0x80000000 (Bus control) on BAR0
*/
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

/* Map to 0x80000000 on BAR1
*/
#define MACREG_REG_GEN_PTR                  0x00000C10
#define MACREG_REG_INT_CODE                 0x00000C14
#define MACREG_REG_SCRATCH                  0x00000C40
#define MACREG_REG_FW_PRESENT               0x0000BFFC

/* Bit definitio for MACREG_REG_A2H_INTERRUPT_CAUSE (A2HRIC)
*/
#define MACREG_A2HRIC_BIT_TX_DONE           0x00000001 /* bit 0 */
#define MACREG_A2HRIC_BIT_RX_RDY            0x00000002 /* bit 1 */
#define MACREG_A2HRIC_BIT_OPC_DONE          0x00000004 /* bit 2 */
#define MACREG_A2HRIC_BIT_MAC_EVENT         0x00000008 /* bit 3 */
#define MACREG_A2HRIC_BIT_RX_PROBLEM        0x00000010 /* bit 4 */
#define MACREG_A2HRIC_BIT_RADIO_OFF         0x00000020 /* bit 5 */
#define MACREG_A2HRIC_BIT_RADIO_ON          0x00000040 /* bit 6 */
#define MACREG_A2HRIC_BIT_RADAR_DETECT      0x00000080 /* bit 7 - IEEE80211_DH */
#define MACREG_A2HRIC_BIT_ICV_ERROR         0x00000100 /* bit 8 */
#define MACREG_A2HRIC_BIT_WEAKIV_ERROR      0x00000200 /* bit 9 */
#define MACREG_A2HRIC_BIT_QUEUE_EMPTY       (1<<10)
#define MACREG_A2HRIC_BIT_QUEUE_FULL        (1<<11)
#define MACREG_A2HRIC_BIT_CHAN_SWITCH       (1<<12) /* IEEE80211_DH */
#define MACREG_A2HRIC_BIT_TX_WATCHDOG       (1<<13)
#define MACREG_A2HRIC_BA_WATCHDOG           (1<<14)
#define MACREG_A2HRIC_BIT_SSU_DONE          (1<<16)
#define MACREG_A2HRIC_CONSEC_TXFAIL         (1<<17) /* 15 taken by ISR_TXACK */

#define ISR_SRC_BITS        ((MACREG_A2HRIC_BIT_RX_RDY)   | \
                             (MACREG_A2HRIC_BIT_TX_DONE)  | \
                             (MACREG_A2HRIC_BIT_OPC_DONE) | \
                             (MACREG_A2HRIC_BIT_MAC_EVENT)| \
                             (MACREG_A2HRIC_BIT_WEAKIV_ERROR)| \
                             (MACREG_A2HRIC_BIT_ICV_ERROR)| \
                             (MACREG_A2HRIC_BIT_SSU_DONE) | \
                             (MACREG_A2HRIC_BIT_RADAR_DETECT)| \
                             (MACREG_A2HRIC_BIT_CHAN_SWITCH)| \
                             (MACREG_A2HRIC_BIT_TX_WATCHDOG)| \
                             (MACREG_A2HRIC_BIT_QUEUE_EMPTY)| \
                             (MACREG_A2HRIC_BA_WATCHDOG)	| \
                             (MACREG_A2HRIC_CONSEC_TXFAIL))

#define MACREG_A2HRIC_BIT_MASK      ISR_SRC_BITS

/* Bit definitio for MACREG_REG_H2A_INTERRUPT_CAUSE (H2ARIC)
*/
#define MACREG_H2ARIC_BIT_PPA_READY         0x00000001 /* bit 0 */
#define MACREG_H2ARIC_BIT_DOOR_BELL         0x00000002 /* bit 1 */
#define MACREG_H2ARIC_BIT_PS                0x00000004 /* bit 2 */
#define MACREG_H2ARIC_BIT_PSPOLL            0x00000008 /* bit 3 */
#define ISR_RESET                           (1<<15)
#define ISR_RESET_AP33                      (1<<26)

/* Data descriptor related constants
*/
#define EAGLE_RXD_CTRL_DRIVER_OWN               0x00
#define EAGLE_RXD_CTRL_OS_OWN                   0x04
#define EAGLE_RXD_CTRL_DMA_OWN                  0x80

#define EAGLE_RXD_STATUS_IDLE                   0x00
#define EAGLE_RXD_STATUS_OK                     0x01
#define EAGLE_RXD_STATUS_MULTICAST_RX           0x02
#define EAGLE_RXD_STATUS_BROADCAST_RX           0x04
#define EAGLE_RXD_STATUS_FRAGMENT_RX            0x08

#define EAGLE_TXD_STATUS_IDLE                   0x00000000
#define EAGLE_TXD_STATUS_USED                   0x00000001
#define EAGLE_TXD_STATUS_OK                     0x00000001
#define EAGLE_TXD_STATUS_OK_RETRY               0x00000002
#define EAGLE_TXD_STATUS_OK_MORE_RETRY          0x00000004
#define EAGLE_TXD_STATUS_MULTICAST_TX           0x00000008
#define EAGLE_TXD_STATUS_BROADCAST_TX           0x00000010
#define EAGLE_TXD_STATUS_FAILED_LINK_ERROR      0x00000020
#define EAGLE_TXD_STATUS_FAILED_EXCEED_LIMIT    0x00000040
#define EAGLE_TXD_STATUS_FAILED_AGING           0x00000080
#define EAGLE_TXD_STATUS_FW_OWNED               0x80000000

#define EAGLE_TXD_XMITCTRL_USE_RATEINFO         0x1
#define EAGLE_TXD_XMITCTRL_DISABLE_AMPDU        0x2
#define EAGLE_TXD_XMITCTRL_ENABLE_AMPDU         0x4
#define EAGLE_TXD_XMITCTRL_USE_MC_RATE          0x8     /* Use multicast data rate */

#define NBR_BYTES_FW_RX_PREPEND_LEN             2
#define NBR_BYTES_FW_TX_PREPEND_LEN             2

/* Antenna control
*/
#define ANTENNA_TX_4_AUTO                       0
#define ANTENNA_TX_2                            3
#define ANTENNA_RX_4_AUTO                       0
#define ANTENNA_RX_2                            2

/* Band related constants
*/
#define BAND_24_CHANNEL_NUM                     14
#define BAND_24_RATE_NUM                        13
#define BAND_50_CHANNEL_NUM                     24
#define BAND_50_RATE_NUM                        8

/* Misc
*/
#define WL_SEC_SLEEP(num_secs)              mdelay(num_secs * 1000)
#define WL_MSEC_SLEEP(num_milli_secs)       mdelay(num_milli_secs)

#define ENDIAN_SWAP32(_val)                 (cpu_to_le32(_val))
#define ENDIAN_SWAP16(_val)                 (cpu_to_le16(_val))

#define DECLARE_LOCK(l)                     spinlock_t l
#define SPIN_LOCK_INIT(l)                   spin_lock_init(l)
#define SPIN_LOCK(l)                        spin_lock(l)
#define SPIN_UNLOCK(l)                      spin_unlock(l)
#define SPIN_LOCK_IRQSAVE(l, f)             spin_lock_irqsave(l, f)
#define SPIN_UNLOCK_IRQRESTORE(l, f)        spin_unlock_irqrestore(l, f)

/* vif and station
*/
#define MAX_WEP_KEY_LEN                     13
#define NUM_WEP_KEYS                        4
#define MWL_MAX_TID                         8
#define MWL_VIF(_vif)                       ((struct mwl_vif *)&((_vif)->drv_priv))
#define IEEE80211_KEY_CONF(_u8)             ((struct ieee80211_key_conf *)(_u8))
#define MWL_STA(_sta)                       ((struct mwl_sta *)&((_sta)->drv_priv))

/* TYPE DEFINITION
*/

enum {
	AP_MODE_11AC = 0x10,         /* generic 11ac indication mode */
	AP_MODE_2_4GHZ_11AC_MIXED = 0x17,
};

enum {
	AMPDU_NO_STREAM,
	AMPDU_STREAM_NEW,
	AMPDU_STREAM_IN_PROGRESS,
	AMPDU_STREAM_ACTIVE,
};

enum {
	IEEE_TYPE_MANAGEMENT = 0,
	IEEE_TYPE_CONTROL,
	IEEE_TYPE_DATA
};

struct mwl_tx_pwr_tbl {
	u8 channel;
	u8 setcap;
	u16 tx_power[SYSADPT_TX_POWER_LEVEL_TOTAL];
	u8 cdd;                      /* 0: off, 1: on */
	u16 txantenna2;
};

struct mwl_hw_data {
	u32 fw_release_num;          /* MajNbr:MinNbr:SubMin:PatchLevel */
	u8 hw_version;               /* plain number indicating version */
	u8 host_interface;           /* plain number of interface       */
	u16 max_num_tx_desc;         /* max number of TX descriptors    */
	u16 max_num_mc_addr;         /* max number multicast addresses  */
	u16 num_antennas;            /* number antennas used            */
	u16 region_code;             /* region (eg. 0x10 for USA FCC)   */
	unsigned char mac_addr[ETH_ALEN]; /* well known -> AA:BB:CC:DD:EE:FF */
};

struct mwl_rate_info {
	u32 format:1;                /* 0 = Legacy format, 1 = Hi-throughput format */
	u32 short_gi:1;              /* 0 = Use standard guard interval,1 = Use short guard interval */
	u32 bandwidth:1;             /* 0 = Use 20 MHz channel,1 = Use 40 MHz channel */
	u32 rate_id_mcs:7;           /* = RateID[3:0]; Legacy format,= MCS[5:0]; HT format */
	u32 adv_coding:1;            /* AdvCoding 0 = No AdvCoding,1 = LDPC,2 = RS,3 = Reserved */
	u32 ant_select:2;            /* Bitmap to select one of the transmit antennae */
	u32 act_sub_chan:2;          /* Active subchannel for 40 MHz mode 00:lower, 01= upper, 10= both on lower and upper */
	u32 preamble_type:1;         /* Preambletype 0= Long, 1= Short; */
	u32 pid:4;                   /* Power ID */
	u32 ant2:1;                  /* bit 2 of antenna selection field */
	u32 ant3:1;
	u32 bf:1;                    /* 0: beam forming off; 1: beam forming on */
	u32 gf:1;                    /* 0: green field off; 1, green field on */
	u32 count:4;
	u32 rsvd2:3;
	u32 drop:1;
} __packed;

struct mwl_tx_desc {
	u8 data_rate;
	u8 tx_priority;
	u16 qos_ctrl;
	u32 pkt_ptr;
	u16 pkt_len;
	u8 dest_addr[ETH_ALEN];
	u32 pphys_next;
	u32 sap_pkt_info;
	struct mwl_rate_info rate_info;
	u8 type;
	u8 xmit_control;             /* bit 0: use rateinfo, bit 1: disable ampdu */
	u16 reserved;
	u32 tcpack_sn;
	u32 tcpack_src_dst;
	struct sk_buff *psk_buff;
	struct mwl_tx_desc *pnext;
	u8 reserved1[2];
	u8 packet_info;
	u8 packet_id;
	u16 packet_len_and_retry;
	u16 packet_rate_info;
	u8 *sta_info;
	u32 status;
} __packed;

struct mwl_hw_rssi_info {
	u32 rssi_a:8;
	u32 rssi_b:8;
	u32 rssi_c:8;
	u32 rssi_d:8;
} __packed;

struct mwl_hw_noise_floor_info {
	u32 noise_floor_a:8;
	u32 noise_floor_b:8;
	u32 noise_floor_c:8;
	u32 noise_floor_d:8;
} __packed;

struct mwl_rxrate_info {
	u16 format:3;                /* 0: 11a, 1: 11b, 2: 11n, 4: 11ac    */
	u16 nss:2;                   /* number space spectrum              */
	u16 bw:2;                    /* 0: ht20, 1: ht40, 2: ht80          */
	u16 gi:1;                    /* 0: long interval, 1: short interval*/
	u16 rt:8;                    /* 11a/11b: 1,2,5,11,22,6,9,12,18,24,36,48,54,72*/
} __packed;                          /* 11n/11ac: MCS                      */

struct mwl_rx_desc {
	u16 pkt_len;                 /* total length of received data      */
	struct mwl_rxrate_info rate; /* receive rate information           */
	u32 pphys_buff_data;         /* physical address of payload data   */
	u32 pphys_next;              /* physical address of next RX desc   */
	u16 qos_ctrl;                /* received QosCtrl field variable    */
	u16 ht_sig2;                 /* like name states                   */
	struct mwl_hw_rssi_info hw_rssi_info;
	struct mwl_hw_noise_floor_info hw_noise_floor_info;
	u8 noise_floor;
	u8 reserved[3];
	u8 rssi;                     /* received signal strengt indication */
	u8 status;                   /* status field containing USED bit   */
	u8 channel;                  /* channel this pkt was received on   */
	u8 rx_control;               /* the control element of the desc    */
	/* above are 32bits aligned and is same as FW, RxControl put at end for sync */
	struct sk_buff *psk_buff;    /* associated sk_buff for Linux       */
	void *pbuff_data;            /* virtual address of payload data    */
	struct mwl_rx_desc *pnext;   /* virtual address of next RX desc    */
} __packed;

struct mwl_desc_data {
	dma_addr_t pphys_tx_ring;          /* ptr to first TX desc (phys.)    */
	struct mwl_tx_desc *ptx_ring;      /* ptr to first TX desc (virt.)    */
	struct mwl_tx_desc *pnext_tx_desc; /* next TX desc that can be used   */
	struct mwl_tx_desc *pstale_tx_desc;/* the staled TX descriptor        */
	dma_addr_t pphys_rx_ring;          /* ptr to first RX desc (phys.)    */
	struct mwl_rx_desc *prx_ring;      /* ptr to first RX desc (virt.)    */
	struct mwl_rx_desc *pnext_rx_desc; /* next RX desc that can be used   */
	unsigned int wcb_base;             /* FW base offset for registers    */
	unsigned int rx_desc_write;        /* FW descriptor write position    */
	unsigned int rx_desc_read;         /* FW descriptor read position     */
	unsigned int rx_buf_size;          /* length of the RX buffers        */
} __packed;

struct mwl_locks {
	DECLARE_LOCK(xmit_lock);           /* used to protect TX actions      */
	DECLARE_LOCK(fwcmd_lock);          /* used to protect FW commands     */
	DECLARE_LOCK(stream_lock);         /* used to protect stream          */
};

struct mwl_ampdu_stream {
	struct ieee80211_sta *sta;
	u8 tid;
	u8 state;
	u8 idx;
};

struct mwl_priv {
	struct ieee80211_hw *hw;
	const struct firmware *fw_ucode;
	struct device_node *dt_node;
	struct device_node *pwr_node;
	bool disable_2g;
	bool disable_5g;
	int antenna_tx;
	int antenna_rx;
	struct mwl_tx_pwr_tbl tx_pwr_tbl[SYSADPT_MAX_NUM_CHANNELS];
	u32 cdd;                     /* 0: off, 1: on */
	u16 txantenna2;
	u8 powinited;
	u16 max_tx_pow[SYSADPT_TX_POWER_LEVEL_TOTAL]; /* max tx power (dBm) */
	u16 target_powers[SYSADPT_TX_POWER_LEVEL_TOTAL]; /* target powers (dBm) */
	u8 cal_tbl[200];
	struct pci_dev *pdev;
	void *iobase0;               /* MEM Base Address Register 0  */
	void *iobase1;               /* MEM Base Address Register 1  */
	u32 next_bar_num;
	unsigned short *pcmd_buf;    /* pointer to CmdBuf (virtual)  */
	dma_addr_t pphys_cmd_buf;    /* pointer to CmdBuf (physical) */
	bool in_send_cmd;
	int irq;
	struct mwl_hw_data hw_data;  /* Adapter HW specific info	 */
	struct mwl_desc_data desc_data[SYSADPT_NUM_OF_DESC_DATA]; /* various descriptor data */
	struct sk_buff_head txq[SYSADPT_NUM_OF_DESC_DATA];
	struct sk_buff_head delay_freeq;
	int fw_desc_cnt[SYSADPT_NUM_OF_DESC_DATA]; /* number of descriptors owned by fw at any one time */
	struct mwl_locks locks;      /* various spinlocks			 */
	struct tasklet_struct tx_task;
	struct tasklet_struct rx_task;
	int txq_limit;
	bool is_tx_schedule;
	int recv_limit;
	bool is_rx_schedule;
	s8 noise;                    /* Most recently reported noise in dBm */
	struct ieee80211_supported_band band_24;
	struct ieee80211_channel channels_24[BAND_24_CHANNEL_NUM];
	struct ieee80211_rate rates_24[BAND_24_RATE_NUM];
	struct ieee80211_supported_band band_50;
	struct ieee80211_channel channels_50[BAND_50_CHANNEL_NUM];
	struct ieee80211_rate rates_50[BAND_50_RATE_NUM];
	u32 ap_macids_supported;
	u32 sta_macids_supported;
	u32 macids_used;
	struct list_head vif_list;   /* List of interfaces.          */
	u32 running_bsses;           /* bitmap of running BSSes      */
	bool radio_on;
	bool radio_short_preamble;
	bool wmm_enabled;
	struct ieee80211_tx_queue_params wmm_params[SYSADPT_TX_WMM_QUEUES];
	/* Ampdu stream information */
	u8 num_ampdu_queues;
	struct mwl_ampdu_stream ampdu[SYSADPT_TX_AMPDU_QUEUES];
	struct work_struct watchdog_ba_handle;
};

struct beacon_info {
	bool valid;
	u16 cap_info;
	u8 bss_basic_rate_set[SYSADPT_MAX_DATA_RATES_G];
	u8 op_rate_set[SYSADPT_MAX_DATA_RATES_G];
	u8 ie_wmm_len;               /* Keep WMM IE */
	u8 *ie_wmm_ptr;
	u8 ie_rsn_len;               /* Keep WPA IE */
	u8 *ie_rsn_ptr;
	u8 ie_rsn48_len;             /* Keep WPA2 IE */
	u8 *ie_rsn48_ptr;
	u8 ie_ht_len;                /* Keep HT IE */
	u8 *ie_ht_ptr;
	u8 ie_list_ht[148];
	u8 ie_vht_len;               /* Keep VHT IE */
	u8 *ie_vht_ptr;
	u8 ie_list_vht[24];
};

struct mwl_vif {
	struct list_head list;
	struct ieee80211_vif *vif;
	int macid;                   /* Firmware macid for this vif.  */
	u16 seqno;                   /* Non AMPDU sequence number assigned by driver.  */
	struct {                     /* Saved WEP keys */
		u8 enabled;
		u8 key[sizeof(struct ieee80211_key_conf) + MAX_WEP_KEY_LEN];
	} wep_key_conf[NUM_WEP_KEYS];
	u8 bssid[ETH_ALEN];          /* BSSID */
	u8 sta_mac[ETH_ALEN];        /* Station mac address */
	bool is_hw_crypto_enabled;   /* A flag to indicate is HW crypto is enabled for this bssid */
	bool is_sta;                 /* Indicate if this is station mode */
	struct beacon_info beacon_info;
	u16 iv16;
	u32 iv32;
	s8 keyidx;
};

struct mwl_tx_info {
	u32 start_time;
	u32 pkts;
};

struct mwl_sta {
	u8 is_ampdu_allowed;
	struct mwl_tx_info tx_stats[MWL_MAX_TID];
	u16 iv16;
	u32 iv32;
};

/* DMA header used by firmware and hardware.
*/
struct mwl_dma_data {
	u16 fwlen;
	struct ieee80211_hdr wh;
	char data[0];
} __packed;

/* Transmission information to transmit a socket buffer.
 */
struct mwl_tx_ctrl {
	u8 tx_priority;
	u16 qos_ctrl;
	u8 type;
	u8 xmit_control;
	u8 *sta_info;
	bool ccmp;
} __packed;

#endif /* _mwl_dev_h_ */
