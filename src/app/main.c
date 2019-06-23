#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vitasdk.h>
#include <taihen.h>

#include "uwifimon.h"
#include "kwifimon_export.h"

#include "ui.h"


int wlan_idx = -1;

int sceNetSyscallGetIfList(struct iface_t *, int c);
int sceNetSyscallControl(int dev, int req, void *buf, int buf_len);

void find_wlan_idx(void)
{
	struct iface_t *iface_list = NULL;
	int iface_count = 0;

	// first get number of interfaces
	iface_count = sceNetSyscallGetIfList(0, 10);
	// alloc space
	iface_list = malloc(sizeof(struct iface_t)*iface_count);
	// and get real list
	sceNetSyscallGetIfList(iface_list, iface_count);

	if (iface_count >= 0) {
		for (int i = 0; i< iface_count; i ++) {
			struct iface_t *f = &iface_list[i];

			if (strcmp(f->name, "wlan0") == 0) {
				wlan_idx = f->dev_idx;
			}

		}
	}
	free(iface_list);
}

//void main(void) __attribute__ ((weak, alias ("module_start")));
void main(void)
{
	int x=20,y=0;
	int ret;

	ui_init();

	vita2d_start_drawing();
	vita2d_clear_screen();

	vita2d_font_draw_text(ui_font, 20, 20, ui_color.text, 10, "Wifimon");
/*
	tai_module_info_t minfo;
	if (!(taiGetModuleInfo("kwifimon", &minfo) < 0)) {
		ret = taiStopUnloadKernelModuleForUser(minfo.modid, NULL, NULL, NULL);
		x = vita2d_font_draw_textf(ui_font, 20, 30, ui_color.text, 10, "Old Kernel module unload %x %s ", ret, (ret < 0)?"failed":"ok");
	}
*/
	SceUID kmod = taiLoadStartKernelModule("ux0:app/WIFIMON01/kwifimon.skprx", 0, NULL, 0);
	vita2d_font_draw_textf(ui_font, x+20, 30, ui_color.text, 10, "Loading kernel module: %x %s", kmod, (kmod < 0)?"failed. Already loaded ?":"ok");

	SceUID umod = sceKernelLoadStartModule("ux0:app/WIFIMON01/uwifimon.suprx", 0, NULL, 0, NULL, NULL);
	vita2d_font_draw_textf(ui_font, 20, 40, ui_color.text, 10, "User module: %x %s", umod, (umod < 0)?"failed":"ok");

	find_wlan_idx();
	
	y= 50;


/*
	uint32_t random;
	ret = sceNetSyscallControl(wlan_idx, 0x14, &random, 4);
*/

/*
	{
	uint16_t mode = 0xdead;
	ret = 0;
	if (wlan_idx > 0) {
		ret = sceNetSyscallControl(wlan_idx, WLAN_IOCTL_GET_MAC_CONTROL, &mode, 2);
	}

	vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "Wlan0 idx: %d mac_control reg: 0x%x  uwifimon state 0x%x", wlan_idx, mode, uwifimon_mod_state());
	y+=10;
	}
*/
/*
	{
		uint32_t x = 0;
		if (wlan_idx > 0) {
			ret = sceNetSyscallControl(wlan_idx, WLAN_IOCTL_DISCONNECT, &x, 4);
		}
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "Disconnect ret: %x  %x", ret, x);
		y+=10;
	}
*/
/*
	{
		uint16_t mon = 1; 
		if (wlan_idx > 0) {
			ret = sceNetSyscallControl(wlan_idx, WLAN_IOCTL_SET_MONITOR_MODE, &mon, 2);
		}
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "Monitor mode ret: %x  %s", ret, (ret < 0)? "bad :-(": "good :-)");
		y+=10;
	}
*/
	{
		uint16_t mask = 0x0001; 
		if (wlan_idx > 0) {
			ret = sceNetSyscallControl(wlan_idx, WLAN_IOCTL_SET_MGMT_REG, &mask, 2);
		}
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "mgmt_reg ret: %x  %d", ret, uwifimon_mod_state());
		y+=10;
	}
	/*
	{

		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "dump: %d", dump(0));
		y+=10;
	}
	{

		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "dump: %d", dump(1));
		y+=10;
	}*/
	{
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "dump: %d", dump(2));
		y+=10;
	}
	{
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "dump: %d", dump(3));
		y+=10;
	}
/*	{
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "dump: %d", dump(4));
		y+=10;
	}*/
/*
	{
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "dump: %d", dump(5));
		y+=10;
	}
	*/
/*
	{
		uint16_t mac = mode | 0x0080; 
		if (wlan_idx > 0) {
			ret = sceNetSyscallControl(wlan_idx, WLAN_IOCTL_SET_MAC_CONTROL, &mac, 2);
		}
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "set mac to promisc ret: %x %x", ret, mode);
		y+=10;
	}
*/
/*
	{
		uint16_t mac = 0;
		if (wlan_idx > 0) {
			ret = sceNetSyscallControl(wlan_idx, WLAN_IOCTL_GET_MAC_CONTROL, &mac, 2);
		}
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "get mac  ret: %x  %x", ret, mac);
		y+=10;
	}
*/
	{
		struct wifimon_stats_t s;
		ret = uwifimon_mod_stats(&s, 1);
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "stats ret: 0x%x pkt:%d mgmt:%d amsdu:%d bar:%d", ret, s.pkt_cnt, s.mgmt_cnt, s.amsdu_cnt, s.bar_cnt);
		y+=10;
	}


	ret = sceKernelStopUnloadModule(umod, 0, NULL, 0, NULL, NULL);
	vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "User unload: %x", ret);
	y+=10;

	if (kmod > 0) {
		ret = taiStopUnloadKernelModule(kmod, 0, NULL, 0, NULL, NULL);
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "Kernel unload: %x", ret);
		y+=10;
	}


	vita2d_font_draw_text(ui_font, 20, y, ui_color.text, 10, "Press O to continue...");
	y+=10;

	vita2d_end_drawing();
	vita2d_swap_buffers();

	while (1) {
		sceKernelDelayThread(1);
		if (ui_get_input() & SCE_CTRL_CIRCLE) {
			break;
		}
	}


	ui_main();

	ui_deinit();

	sceKernelExitProcess(0);
}
