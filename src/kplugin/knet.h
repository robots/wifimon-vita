#ifndef KNET_h_
#define KNET_h_

#include <stdint.h>
#include "radiotap.h"

int knet_start(int port);
int knet_stop(void);
int knet_write_rt(struct ieee80211_radiotap_header *rtap, uint8_t * buf, uint32_t buf_len);

#endif
