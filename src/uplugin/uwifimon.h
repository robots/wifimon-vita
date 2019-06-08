#ifndef UWIFIMON_h_
#define UWIFIMON_h_

struct iface_counter_t
{
  uint32_t unicast_bytes;
  uint32_t unicast_pkts;
  uint32_t multicast_bytes;
  uint32_t multicast_pkts;
};

struct iface_t
{
  char name[16];
  char field_10[16];
  int field_20;
  int field_24;
  int field_28;
  uint16_t field_2C;
  int field_2E;
  uint16_t field_32;
  int field_34;
  char field_38[16];
  uint64_t field_48;
  char field_50[8];
  uint64_t field_58;
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
  uint64_t field_FC;
  char field_104[8];
  int field_10C;
  int field_110;
  char pad114[44];
  char field_A0;
} __attribute__((packed));


int uwifimon_iface_list(struct iface_t **list, int *cnt);
void uwifimon_iface_control(int dev, int req, void *buf, int buf_len);
void uwifimon_getlog(char *dest);

#endif
