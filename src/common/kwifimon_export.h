#ifndef KWIFIMON_EXPORT_H_
#define KWIFIMON_EXPORT_H_

#define KWIFIMON_NET_PORT 65111

struct wifimon_stats_t {
	uint32_t pkt_cnt;
	uint32_t mgmt_cnt;
	uint32_t amsdu_cnt;
	uint32_t bar_cnt;
};

struct iface_counter_t {
  unsigned int bytes1;
  unsigned int pkts1;
  unsigned int bytes2;
  unsigned int pkts2;
} __attribute__ ((packed));

struct iface_t
{
  char name[16];
  char field_10[16];
  int dev_idx;
  int field_10_len;
  int field_28;
  int16_t field_2C;
  int field_2E;
  int16_t field_32;
  int field_34;
  char field_38[16];
  int field_48;
  int dev_type;
  char field_50[8];
  int mtu;
  int mtu1;
  struct iface_counter_t counters[5];
  char field_B0[8];
  int field_B8;
  int field_BC;
  struct iface_counter_t field_C0;
  char field_D0[8];
  int field_D8;
  char field_DC[4];
  int field_E0;
  int field_E4;
  int field_E8;
  int field_EC;
  int field_F0;
  int field_F4;
  int field_F8;
  int field_FC;
  int field_100;
  char field_104[8];
  int field_10C;
  int field_110;
  char field_114[44];
} __attribute__ ((packed));

enum wifi_ioctl_t {
	WLAN_IOCTL_GET_RSSI          = 0x40110001, // 1+8bytes
	WLAN_IOCTL_GET_CMDf5_buf     = 0x40120003, // 0x108c bytes
	WLAN_IOCTL_GET_XXXX          = 0x40120004,
	WLAN_IOCTL_GET_RETRY_LIM_I   = 0x40180002,
	WLAN_IOCTL_IS_MODE_0x200     = 0x40180003,
	WLAN_IOCTL_GET_MACADDRESS    = 0x40180004,
	WLAN_IOCTL_BGSCAN_CONFIG     = 0x50100002,
	WLAN_IOCTL_SCAN_BEGIN        = 0x50100003,
	WLAN_IOCTL_SCAN_RESULT       = 0x50100004,
	WLAN_IOCTL_SET_WOL           = 0x50110001,
	WLAN_IOCTL_DISCONNECT        = 0x50110002,
	WLAN_IOCTL_ADHOC_STOP        = 0x50120002,
	WLAN_IOCTL_ADHOC_CREATE      = 0x50120003,
	WLAN_IOCTL_DO_CMD_0x00f2     = 0x60120002,
	WLAN_IOCTL_TXRATE_CFG        = 0x60180001,
	WLAN_IOCTL_SET_RETRY_LIM_I   = 0x60180002,
	WLAN_IOCTL_SET_XXXXXx        = 0x60180003,
	WLAN_IOCTL_SET_XXXXXy        = 0x60180005,

	WLAN_IOCTL_GET_MAC_CONTROL   = 0x4011FF01,
	WLAN_IOCTL_SET_MAC_CONTROL   = 0x5011FF01,
	WLAN_IOCTL_SET_RF_CHANNEL    = 0x5011FF02,
	WLAN_IOCTL_SET_MONITOR_MODE  = 0x5011FF03,
	WLAN_IOCTL_SET_MGMT_REG      = 0x5011FF04,
	WLAN_IOCTL_ANYCMD            = 0x5011FF05,
	WLAN_IOCTL_MEM               = 0x5011FF07,
};

enum kwifimon_state_t {
	STATE_IDLE      = 0,
	STATE_MONITOR   = 0x00000001,
	STATE_REC_FILE  = 0x00000002,
	STATE_REC_NET   = 0x00000003,

	STATE_ERROR     = 0x80000001,
	STATE_ERROR_1   = 0x80000002,

};

int kwifimon_mod_state(void);
int kwifimon_mod_stats(struct wifimon_stats_t *s, int reset);
int kwifimon_cap_start(char *file);
int kwifimon_cap_stop(void);
int kwifimon_net_start(void);
int kwifimon_net_stop(void);

#endif
