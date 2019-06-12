#ifndef KWIFIMON_h_
#define KWIFIMON_h_

#define PACK               __attribute__ ((packed))


// wlan commands
#define WLAN_CMD_802_11_RF_CHANNEL         0x001d
#define WLAN_CMD_MAC_CONTROL               0x0028
#define WLAN_CMD_802_11_MONITOR_MODE       0x0098

// action
#define HostCmd_ACT_GEN_GET                   0x0000
#define HostCmd_ACT_GEN_SET                   0x0001

// rf channel rf_type
#define WLAN_RADIO_TYPE_BG          0
#define WLAN_RADIO_TYPE_A           1


// mac control
#define HostCmd_ACT_MAC_RX_ON                     0x0001
#define HostCmd_ACT_MAC_TX_ON                     0x0002
#define HostCmd_ACT_MAC_LOOPBACK_ON               0x0004
#define HostCmd_ACT_MAC_WEP_ENABLE                0x0008
#define HostCmd_ACT_MAC_ETHERNETII_ENABLE         0x0010
#define HostCmd_ACT_MAC_PROMISCUOUS_ENABLE        0x0080
#define HostCmd_ACT_MAC_ALL_MULTICAST_ENABLE      0x0100
#define HostCmd_ACT_MAC_RTS_CTS_ENABLE            0x0200
#define HostCmd_ACT_MAC_STRICT_PROTECTION_ENABLE  0x0400
#define HostCmd_ACT_MAC_ADHOC_G_PROTECTION_ON     0x2000

struct wlan_mon_t {
	uint16_t action;
	uint16_t mode;
} PACK;

struct wlan_rf_channel_t {
	uint16_t action;
	uint16_t current_channel;
	uint16_t rf_type;
	uint16_t reserved;
	uint8_t reserved_1[32];
} PACK;

struct wlan_mac_control_t {
	uint32_t action;
} PACK;

struct sdio_rx_t {
	uint16_t xx;
	uint16_t pkt_type;
} PACK;

struct rxpd {
	uint8_t bss_type;
	uint8_t bss_num;
	uint16_t rx_pkt_length;
	uint16_t rx_pkt_offset;
	uint16_t rx_pkt_type;
	uint16_t seq_num;
	uint8_t priority;
	uint8_t rx_rate;
	int8_t snr;
	int8_t nf;

	/* For: Non-802.11 AC cards
	 *
	 * Ht Info [Bit 0] RxRate format: LG=0, HT=1
	 * [Bit 1]  HT Bandwidth: BW20 = 0, BW40 = 1
	 * [Bit 2]  HT Guard Interval: LGI = 0, SGI = 1
	 *
	 * For: 802.11 AC cards
	 * [Bit 1] [Bit 0] RxRate format: legacy rate = 00 HT = 01 VHT = 10
	 * [Bit 3] [Bit 2] HT/VHT Bandwidth BW20 = 00 BW40 = 01
	 *						BW80 = 10  BW160 = 11
	 * [Bit 4] HT/VHT Guard interval LGI = 0 SGI = 1
	 * [Bit 5] STBC support Enabled = 1
	 * [Bit 6] LDPC support Enabled = 1
	 * [Bit 7] Reserved
	 */
	uint8_t ht_info;
	uint8_t reserved[3];
	uint8_t flags;
} PACK;

#endif
