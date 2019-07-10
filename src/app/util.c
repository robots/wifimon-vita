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

int wlan_disconnect(void)
{
	int ret = -1;

	if (wlan_idx > 0) {
		uint32_t x = 0;
		ret = sceNetSyscallControl(wlan_idx, WLAN_IOCTL_DISCONNECT, &x, 4);
	}

	return ret;

}

int wlan_set_maccontrol(uint16_t mac)
{
	int ret = -1;

	if (wlan_idx > 0) {
		ret = sceNetSyscallControl(wlan_idx, WLAN_IOCTL_SET_MAC_CONTROL, &mac, 2);
	}

	return ret;
}

int wlan_get_maccontrol(uint16_t *mac)
{
	int ret = -1;

	if (wlan_idx > 0) {
		ret = sceNetSyscallControl(wlan_idx, WLAN_IOCTL_GET_MAC_CONTROL, mac, 2);
	}

	return ret;
}

int wlan_cmd_func_shutdown(void)
{
	int ret = -1;
	uint16_t cmd = 0xaa;
	uint16_t len = 0;

	uint8_t request[6];

	memcpy(&request[0], &cmd, 2);
	memcpy(&request[2], &len, 2);
	memcpy(&request[4], &len, 2);

	if (wlan_idx > 0) {
		ret = sceNetSyscallControl(wlan_idx, WLAN_IOCTL_ANYCMD, request, 6);
	}

	return ret;
}

int wlan_cmd_init(void)
{
	int ret = -1;

	if (wlan_idx > 0) {
		ret = sceNetSyscallControl(wlan_idx, WLAN_IOCTL_INIT, 0, 0);
	}

	return ret;
}

uint32_t get_random(void)
{
	uint32_t random = 4;

	if (wlan_idx > 0) {
		sceNetSyscallControl(wlan_idx, 0x14, &random, 4);
	}

	return random;
}
