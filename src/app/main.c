#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vitasdk.h>
#include <taihen.h>

#include "uwifimon.h"
#include "kwifimon_export.h"

#include "ui.h"
#include "patch.h"


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
	SceUID kmod, umod;

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

	{
		int search_unk[2];
		SceUID modid = _vshKernelSearchModuleByName("kwifimon", search_unk);
		if (modid < 0) {
			kmod = taiLoadStartKernelModule("ux0:app/WIFIMON01/kwifimon.skprx", 0, NULL, 0);
			vita2d_font_draw_textf(ui_font, x+20, 30, ui_color.text, 10, "Loading kernel module: %x %s", kmod, (kmod < 0)?"failed.":"ok");
		} else {
			vita2d_font_draw_textf(ui_font, x+20, 30, ui_color.text, 10, "Loading kernel module: already loaded");
		}

		umod = sceKernelLoadStartModule("ux0:app/WIFIMON01/uwifimon.suprx", 0, NULL, 0, NULL, NULL);
		vita2d_font_draw_textf(ui_font, 20, 40, ui_color.text, 10, "User module: %x %s", umod, (umod < 0)?"failed":"ok");
	}

	find_wlan_idx();
	
	y= 50;



#if 0
	{

		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "dump: %d", dump(0));
		y+=10;
	}
	{

		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "dump: %d", dump(1));
		y+=10;
	}
	{
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "dump: %d", dump(2));
		y+=10;
	}
	{
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "dump: %d", dump(3));
		y+=10;
	}
	{
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "dump: %d", dump(4));
		y+=10;
	}
/*
	{
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "dump: %d", dump(5));
		y+=10;
	}
	*/
#endif

	{
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "Disconnect %d", uwifimon_mod_state());
		y+=10;
	}

	vita2d_end_drawing();
	vita2d_swap_buffers();

	while (1) {
		uint32_t in = ui_get_input();

		if (in & SCE_CTRL_CIRCLE) {
			break;
		}
		if (in & SCE_CTRL_TRIANGLE) {
			struct wifimon_stats_t s;
			ret = uwifimon_mod_stats(&s, 1);
			vita2d_start_drawing();
			vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "stats ret: 0x%x pkt:%d mgmt:%d amsdu:%d bar:%d ev:%d", ret, s.pkt_cnt, s.mgmt_cnt, s.amsdu_cnt, s.bar_cnt, s.evt_cnt);
			vita2d_end_drawing();
			vita2d_swap_buffers();
			y+=10;
		}
		if (in & SCE_CTRL_SQUARE) {
			vita2d_start_drawing();
			vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "Patching %08x", patch_do());
			vita2d_end_drawing();
			vita2d_swap_buffers();
			y+=10;
		}

		if (in & SCE_CTRL_LEFT) {
			vita2d_start_drawing();
			vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "Start recording", uwifimon_cap_start("ux0:/data/test.cap"));
			vita2d_end_drawing();
			vita2d_swap_buffers();
			y+=10;
		}

		if (in & SCE_CTRL_RIGHT) {
			vita2d_start_drawing();
			vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "Stop recording", uwifimon_cap_stop());
			vita2d_end_drawing();
			vita2d_swap_buffers();
			y+=10;
		}

		sceKernelDelayThread(1);
	}
/*
	{
		vita2d_font_draw_textf(ui_font, 20, y, ui_color.text, 10, "UnPatching %d", patch_undo());
		y+=10;
	}
*/

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
