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

struct wlan_dev_t;

struct netdev_t {
  struct netdev_t *next;
  struct netdev_t *prev;
  void *(*alloc)(struct netdev_t *, unsigned int, int);
  int (*free)(struct netdev_t *, void *);
  int (*dev_pbuf_alloc)(struct netdev_t *, int a2, int a3);
  int (*xxx1)(struct netdev_t *, int a2, int a3);
  int (*dev_pbuf_free)(struct netdev_t *, int a2);
  int (*pkt_rx)(struct netdev_t *, int a2);
  int (*get_tx_pbuf)(struct netdev_t *);
  int (*xxx2)(struct netdev_t *, int a2, int a3);
  int (*xxx3)(struct netdev_t *, int a2);
  int field_2C;
  int field_30;
  void *netdev_2_ptr;
  void (*xxx4)(struct netdev_t *, int);
  void (*xxx5)(struct netdev_t *, int);
  void (*xxx6)(struct netdev_t *, int);
  void (*xxx7)(struct netdev_t *, int);
  void (*xxx8)(struct netdev_t *, int);
  void (*xxx9)(struct netdev_t *, int);
  void (*ioctl)(struct netdev_t *, int);
  int field_54;
  int dev_type;
  int field_5C;
  int field_60;
  int mtu;
  int field_68;
  struct wlan_dev_t *priv;
} PACK;

struct wlan_tx_buf_t {
  int cmd;
  int len;
  int buf;
  int field_C;
  int field_10;
  int field_14;
} PACK;

struct wlan_cmd_t {
	uint16_t cmd;
	uint16_t unk;
	void *hdr;
	void *out_data;
	void *in_data;
	uint32_t unk1;
	uint32_t unk2;
	int (*result_cb)(struct wlan_dev_t *, struct wlan_cmd_t *);
	uint32_t unk3;
	uint32_t unk4;
	uint32_t unk5;
	uint32_t unk6;
	uint32_t unk7;
	uint32_t unk8;
	uint32_t unk9;
} PACK;



struct wlan_dev_t {
  uint8_t gap0[12];
  int field_C;
  int field_10;
  uint8_t gap14[20];
  struct wlan_dev_t *wlandev_ptr;
  int cmd_mutex;
  uint8_t gap30[28];
  uint16_t current_maccontrl;
  uint8_t gap4E[30];
  int command_event_flag;
  uint8_t gap70[4];
  int sdio_dev_fd;
  uint8_t gap78[4];
  int some_reg;
  uint8_t gap80[8];
  int aggr_20;
  int cmd_mem;
  int robin_mem;
  int robin_mem2;
  int field_98;
  char field_9C;
  uint16_t field_9E;
  uint8_t gapA0[2];
  uint16_t field_A2;
	int sdasd;
  struct wlan_cmd_t *current_cmd;
  int field_AC;
  struct wlan_cmd_t wifi_cmds[8];
  struct wlan_tx_buf_t field_270[16];
  uint8_t gap3F0[340];
  int aggr_mem;
  uint8_t gap548[40];
  int aggr_memsize;
  int field_574;
  int sdio_aggr_rx;
	int fiels_xxx;
  struct wlan_dev_t *wlandev_ptr1;
  struct wlan_dev_t *wlandev_ptr2;
  int tcmmutex;
  uint8_t gap58C[60];
  int tcm_event_flag;
  uint16_t current_mac_control;
  uint8_t gap5CE[34];
  int mutex;
  uint8_t gap5F4[60];
  struct netdev_t netdev;
  int field_6A0;
  int field_6A4;
  int tx_bytes;
  int tx_pkts;
  int tx_bcast_bytes;
  int tx_bcast_pkts;
  int tx_mcast_bytes;
  int tx_mcast_pkts;
  int rx_bytes;
  int rx_pkts;
  int rx_bcast_bytes;
  int rx_bcast_pkts;
  int rx_mcast_bytes;
  int rx_mcast_pkts;
  int field_6D8;
  int field_6DC;
  int wlan_thread;
  int eventflag_robinworker;
  uint8_t gap6E8[24];
  int field_700;
  int some_net_flag;
  int sysevent_handler;
  int tx_flag;
  int field_710;
	int field_714;
  int wlan_lock;
  uint8_t gap71C[68];
  int ibss_mac;
  uint8_t gap764[88];
  int field_7BC;
  uint8_t gap7C0[8];
  int addba_active;
  int field_7CC;
  int field_7D0;
  int field_7D4;
  int field_7D8;
	int field_7DC;
  int field_7E0;
  int field_7E4;
  uint8_t gap7E8[5700];
  int field_0;
} PACK;



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
