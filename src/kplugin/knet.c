#include <vitasdkkern.h>
#include <psp2kern/net/net.h>

#include <string.h>

#include "knet.h"

int knet_fd = -1;
SceNetSockaddrIn knet_tgt;
uint8_t knet_pkt[2048];

int knet_start(int port)
{
	knet_fd = ksceNetSocket("kwifimon", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
	if (knet_fd < 0) {
		return -1;
	}

	memset(&knet_tgt, 0, sizeof(knet_tgt));
	knet_tgt.sin_family = SCE_NET_AF_INET;
	knet_tgt.sin_port = ksceNetHtons(port);
	knet_tgt.sin_addr.s_addr = SCE_NET_INADDR_LOOPBACK;

	return 0;
}


int knet_stop(void)
{
	ksceNetClose(knet_fd);
	knet_fd = -1;

	return 0;
}

int knet_write_rt(struct ieee80211_radiotap_header *rtap, uint8_t *buf, uint32_t buf_len)
{
	if (knet_fd < 0) {
		return -1;
	}

	memcpy(knet_pkt, rtap, rtap->it_len);
	memcpy(knet_pkt + rtap->it_len, buf, buf_len);

	return ksceNetSendto(knet_fd, knet_pkt, rtap->it_len+buf_len, SCE_NET_MSG_DONTWAIT, (SceNetSockaddr *)&knet_tgt, sizeof(knet_tgt));
}
