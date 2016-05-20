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

/* Description:  This file defines system adaptation related information. */

#ifndef _SYSADPT_H_
#define _SYSADPT_H_

#define SYSADPT_MAX_NUM_CHANNELS       64

#define SYSADPT_MAX_DATA_RATES_G       14

#define SYSADPT_TX_POWER_LEVEL_TOTAL   16

#define SYSADPT_TX_WMM_QUEUES          4

#define SYSADPT_TX_AMPDU_QUEUES        4

#define SYSADPT_NUM_OF_AP              16

#define SYSADPT_TOTAL_TX_QUEUES        (SYSADPT_TX_WMM_QUEUES + \
					SYSADPT_NUM_OF_AP)

#define SYSADPT_TOTAL_HW_QUEUES        (SYSADPT_TX_WMM_QUEUES + \
					SYSADPT_TX_AMPDU_QUEUES)

#define SYSADPT_NUM_OF_DESC_DATA       (4 + SYSADPT_NUM_OF_AP)

#define SYSADPT_MAX_NUM_TX_DESC        256

#define SYSADPT_TX_QUEUE_LIMIT         (3 * SYSADPT_MAX_NUM_TX_DESC)

#define SYSADPT_TX_WAKE_Q_THRESHOLD    (2 * SYSADPT_MAX_NUM_TX_DESC)

#define SYSADPT_DELAY_FREE_Q_LIMIT     SYSADPT_MAX_NUM_TX_DESC

#define SYSADPT_MAX_NUM_RX_DESC        256

#define SYSADPT_RECEIVE_LIMIT          64

#define SYSADPT_MAX_AGGR_SIZE          8192

#define SYSADPT_MIN_BYTES_HEADROOM     64

#define SYSADPT_AMPDU_PACKET_THRESHOLD 64

#define SYSADPT_AMSDU_FW_MAX_SIZE      3300

#define SYSADPT_AMSDU_4K_MAX_SIZE      SYSADPT_AMSDU_FW_MAX_SIZE

#define SYSADPT_AMSDU_8K_MAX_SIZE      SYSADPT_AMSDU_FW_MAX_SIZE

#define SYSADPT_AMSDU_ALLOW_SIZE       1600

#define SYSADPT_AMSDU_FLUSH_TIME       500

#define SYSADPT_AMSDU_PACKET_THRESHOLD 10

#define SYSADPT_MAX_TID                8

#define SYSADPT_QUIET_PERIOD_DEFAULT   100

#define SYSADPT_QUIET_PERIOD_MIN       25

#define SYSADPT_QUIET_START_OFFSET     10

#define SYSADPT_THERMAL_THROTTLE_MAX   100

#define SYSADPT_TIMER_WAKEUP_TIME      300 /* ms */

#endif /* _SYSADPT_H_ */
