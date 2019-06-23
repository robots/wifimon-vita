#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vitasdk.h>



#include "kwifimon_export.h"

extern int wlan_idx;

int sceNetSyscallControl(int dev, int req, void *buf, int buf_len);

int mem_read(uint32_t addr, uint32_t *data)
{
	uint8_t request[9];

	request[0] = 0;
	memcpy(&request[1], &addr, 4);

	int ret = sceNetSyscallControl(wlan_idx, WLAN_IOCTL_MEM, request, 9);

	memcpy(data, &request[5], 4);

	return ret;
}

int mem_write(uint32_t addr, uint32_t data)
{
	uint8_t request[9];

	request[0] = 1;
	memcpy(&request[1], &addr, 4);
	memcpy(&request[5], &data, 4);

	int ret = sceNetSyscallControl(wlan_idx, WLAN_IOCTL_MEM, request, 9);

	return ret;
}
