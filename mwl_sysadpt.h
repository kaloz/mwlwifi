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
*   Description:  This file defines system adaptation related information.
*
*/

#ifndef _mwl_sysadpt_h_
#define _mwl_sysadpt_h_

/* CONSTANTS AND MACROS
*/

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

#define SYSADPT_TX_QUEUE_LIMIT         1024

#define SYSADPT_DELAY_FREE_Q_LIMIT     SYSADPT_MAX_NUM_TX_DESC

#define SYSADPT_MAX_NUM_RX_DESC        256

#define SYSADPT_RECEIVE_LIMIT          64

#define SYSADPT_MAX_AGGR_SIZE          4096

#define SYSADPT_MIN_BYTES_HEADROOM     64

#define SYSADPT_AMPDU_PACKET_THRESHOLD 64

#define SYSADPT_MAX_TID                8

#endif /* _mwl_sysadpt_h_ */
