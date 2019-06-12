#ifndef WLAN_KERNEL_h_
#define WLAN_KERNEL_h_


struct wlan_dev_t;

struct netdev_t {
  struct netdev_t *next;
  struct netdev_t *prev;
  void *(*fnc_alloc)(struct netdev_t *, unsigned int, int);
  int (*fnc_free)(struct netdev_t *, void *);
  int (*fnc_dev_pbuf_alloc)(struct netdev_t *result, int a2, int a3);
  int (*field_14)(struct netdev_t *result, int a2, int a3);
  int (*fnc_dev_pbuf_free)(struct netdev_t *result, int a2);
  int (*fnc_pkt_rx)(struct netdev_t *result, int a2);
  int (*fnc_get_tx_pbuf)(struct netdev_t *result);
  int (*field_24)(struct netdev_t *result, int a2, int a3);
  int (*field_28)(struct netdev_t *result, int a2);
  int field_2C;
  int field_30;
  struct netdev_2_t *netdev_2_ptr;
  void (*field_38)(struct netdev_t *, int);
  void (*field_3C)(struct netdev_t *, int);
  void (*field_40)(struct netdev_t *, int);
  void (*field_44)(struct netdev_t *, int);
  void (*fnc_tx_pkt)(struct netdev_t *, int);
  void (*field_4C)(struct netdev_t *, int);
  void (*fnc_ioctl)(struct netdev_t *, int);
  int field_54;
  int dev_type;
  int field_5C;
  int field_60;
  int mtu;
  int field_68;
  struct wlan_dev_t *priv;
  int field_70;
  int field_74;
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
  int field_A8;
  int field_AC;
} __attribute__ ((packed));

struct wlan_tx_buf_t {
  int cmd;
  int len;
  int buf;
  int field_C;
  int field_10;
  int field_14;
} __attribute__ ((packed));

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
} __attribute__ ((packed));

struct wlan_lock_t {
	uint32_t x;
	uint32_t y;
	uint32_t z;
};


struct wlan_dev_t
{
  char field_1[12];
  int field_C;
  int field_10;
  char field_14[20];
  struct wlan_dev_t *wlandev_ptr;
  int cmd_mutex;
  char field_30[28];
  int16_t current_maccontrl;
  char field_4E[30];
  int command_event_flag;
  char field_70[4];
  int sdio_dev_fd;
  char field_78[4];
  int ioport;
  char field_80[8];
  int aggr_20;
  int cmd_mem;
  int robin_mem;
  int robin_mem2;
  int field_98;
  char field_9C;
  char field_9D;
  int16_t field_9E;
  int16_t field_A0;
  int16_t field_A2;
  int field_A4;
  struct wlan_cmd_t *current_cmd;
  int field_AC;
  struct wlan_cmd_t wifi_cmds[8];
  struct wlan_tx_buf_t field_270[16];
  char field_3F0[340];
  int aggr_mem;
  char field_548[40];
  int aggr_memsize;
  int field_574;
  int sdio_aggr_rx;
  char field_57C[4];
  struct wlan_dev_t *wlandev_ptr1;
  struct wlan_dev_t *wlandev_ptr2;
  int tcmmutex;
  char field_58C[60];
  int tcm_event_flag;
  uint16_t current_mac_control;
  char field_5CE[34];
  int mutex;
  char field_5F4[60];
  struct netdev_t netdev;
  int wlan_thread;
  int eventflag_robinworker;
  char field_6E8[24];
  int field_700;
  int some_net_flag;
  int sysevent_handler;
  int tx_flag;
  int field_710;
  int field_714;
  struct wlan_lock_t wlan_lock;
  char field_724[60];
  int ibss_mac;
  char field_764[88];
  int field_7BC;
  int field_7C0;
  int field_7C4;
  int addba_active;
  int field_7CC;
  int field_7D0;
  int field_7D4;
  int field_7D8;
  int field_7DC;
  int field_7E0;
  int field_7E4;
  char field_7E8[5700];
  int field_0;
} __attribute__ ((packed));


#endif
