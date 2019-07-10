#include <vitasdkkern.h>
#include <sys/time.h>

#include "pcap.h"

SceUID pcap_fd = -1;

int ksceKernelLibcGettimeofday(struct timeval *ptimeval, void *ptimezone);

int pcap_open(char *file)
{
	if (pcap_fd > 0) {
		pcap_close();
	}

	pcap_fd = ksceIoOpen(file, SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC, 0777);

	if (pcap_fd < 0) {
		return -1;
	}

	pcap_hdr_t hdr;

	hdr.magic_number = 0xa1b2c3d4;
	hdr.version_major = 2;
	hdr.version_minor = 4;
	hdr.thiszone = 0;
	hdr.sigfigs = 0;
	hdr.snaplen = 65535;
	hdr.network = 127;//LINKTYPE_IEEE802_11_RADIOTAP

	if (ksceIoWrite(pcap_fd, &hdr, sizeof(hdr)) < 0) {
		pcap_close();
		return -1;
	}

	return 0;
}

void pcap_close(void)
{
	if (pcap_fd >= 0) {
		ksceIoClose(pcap_fd);
		pcap_fd = -1;
	}
}

int pcap_write_raw(uint8_t *buf, uint32_t buf_len)
{
	pcaprec_hdr_t rec;
	struct timeval tv;
	struct timezone tz;

	if (pcap_fd < 0) {
		return 0;
	}

	ksceKernelLibcGettimeofday(&tv, &tz);

	rec.ts_sec = tv.tv_sec;
	rec.ts_usec = tv.tv_usec;
	rec.incl_len = buf_len;
	rec.orig_len = buf_len;

	if (ksceIoWrite(pcap_fd, &rec, sizeof(rec)) < 0) {
		pcap_close();
		return -1;
	}

	if (ksceIoWrite(pcap_fd, buf, buf_len) < 0) {
		pcap_close();
		return -1;
	}

	return 0;
}

int pcap_write_rt(struct ieee80211_radiotap_header *rtap, uint8_t *buf, uint32_t buf_len)
{
	pcaprec_hdr_t rec;
	struct timeval tv;
	struct timezone tz;

	if (pcap_fd < 0) {
		return 0;
	}

	ksceKernelLibcGettimeofday(&tv, &tz);

	rec.ts_sec = tv.tv_sec;
	rec.ts_usec = tv.tv_usec;
	rec.incl_len = buf_len;
	rec.orig_len = buf_len;

	if (ksceIoWriteAsync(pcap_fd, &rec, sizeof(rec)) < 0) {
		pcap_close();
		return -1;
	}

	if (ksceIoWriteAsync(pcap_fd, rtap, rtap->it_len) < 0) {
		pcap_close();
		return -1;
	}

	if (ksceIoWriteAsync(pcap_fd, buf, buf_len) < 0) {
		pcap_close();
		return -1;
	}

	return 0;
}

