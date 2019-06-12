#ifndef RADIOTAP_h_
#define RADIOTAP_h_

/* SPDX-License-Identifier: GPL-2.0 */
#include "ieee80211_radiotap.h"

struct tx_radiotap_hdr {
	struct ieee80211_radiotap_header hdr;
	uint8_t rate;
	uint8_t txpower;
	uint8_t rts_retries;
	uint8_t data_retries;
} __attribute__ ((packed));

#define TX_RADIOTAP_PRESENT (				\
	(1 << IEEE80211_RADIOTAP_RATE) |		\
	(1 << IEEE80211_RADIOTAP_DBM_TX_POWER) |	\
	(1 << IEEE80211_RADIOTAP_RTS_RETRIES) |		\
	(1 << IEEE80211_RADIOTAP_DATA_RETRIES)  |	\
	0)

#define IEEE80211_FC_VERSION_MASK    0x0003
#define IEEE80211_FC_TYPE_MASK       0x000c
#define IEEE80211_FC_TYPE_MGT        0x0000
#define IEEE80211_FC_TYPE_CTL        0x0004
#define IEEE80211_FC_TYPE_DATA       0x0008
#define IEEE80211_FC_SUBTYPE_MASK    0x00f0
#define IEEE80211_FC_TOFROMDS_MASK   0x0300
#define IEEE80211_FC_TODS_MASK       0x0100
#define IEEE80211_FC_FROMDS_MASK     0x0200
#define IEEE80211_FC_NODS            0x0000
#define IEEE80211_FC_TODS            0x0100
#define IEEE80211_FC_FROMDS          0x0200
#define IEEE80211_FC_DSTODS          0x0300

struct rx_radiotap_hdr {
	struct ieee80211_radiotap_header hdr;
	uint8_t flags;
	uint8_t rate;
	// channel
	uint16_t ch_freq;
	uint16_t ch_flags;

	int8_t antsignal;
	int8_t antnoise;
	//mcs
	uint8_t mcs_known;
	uint8_t mcs_flags;
	uint8_t mcs;
} __attribute__ ((packed));

#define RX_RADIOTAP_PRESENT (			\
	(1 << IEEE80211_RADIOTAP_FLAGS) |	\
	(1 << IEEE80211_RADIOTAP_RATE) |	\
	(1 << IEEE80211_RADIOTAP_CHANNEL) |	\
	(1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL) |\
	(1 << IEEE80211_RADIOTAP_DB_ANTNOISE) |\
	(1 << IEEE80211_RADIOTAP_MCS) |\
	0)

#endif
