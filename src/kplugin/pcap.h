#ifndef PCAP_h_
#define PCAP_h_

#include <stdint.h>
#include "radiotap.h"

typedef struct pcap_hdr_s {
	uint32_t magic_number;   /* magic number */
	uint16_t version_major;  /* major version number */
	uint16_t version_minor;  /* minor version number */
	int32_t  thiszone;       /* GMT to local correction */
	uint32_t sigfigs;        /* accuracy of timestamps */
	uint32_t snaplen;        /* max length of captured packets, in octets */
	uint32_t network;        /* data link type */
} __attribute__ ((packed)) pcap_hdr_t;

typedef struct pcaprec_hdr_s {
	uint32_t ts_sec;         /* timestamp seconds */
	uint32_t ts_usec;        /* timestamp microseconds */
	uint32_t incl_len;       /* number of octets of packet saved in file */
	uint32_t orig_len;       /* actual length of packet */
} __attribute__ ((packed)) pcaprec_hdr_t;

int pcap_open(char *file);
void pcap_close(void);
int pcap_write_rt(struct ieee80211_radiotap_header *rtap, uint8_t *buf, uint32_t buf_len);

#endif
