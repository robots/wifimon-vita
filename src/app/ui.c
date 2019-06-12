#include "vitasdk.h"
#include "vita2d.h"

#include "ui.h"

struct ui_color_t ui_color;
vita2d_font *ui_font;
SceCtrlData ui_pad_last;

void ui_init(void)
{
	ui_color.bg            = RGBA8(0x00, 0x00, 0x00, 0xff);
	ui_color.text          = RGBA8(0xff, 0xff, 0xff, 0xff);
	ui_color.seleted_bg    = RGBA8(0x00, 0x07, 0xff, 0xff);
	ui_color.selected_text = RGBA8(0xff, 0xbe, 0x00, 0xff);


	memset(&ui_pad_last, 0, sizeof(SceCtrlData));

	vita2d_init();
	vita2d_set_clear_color(ui_color.bg);
	ui_font = vita2d_load_font_file("ux0:/app/WIFIMON01/DejaVuSansMono.ttf");
}

void ui_deinit(void)
{
	vita2d_free_font(ui_font);
	vita2d_fini();
}


void ui_main(void)
{

}

void ui_draw_sb(void)
{

}

uint32_t ui_get_input(void)
{
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(0, &pad, 1);

	uint32_t input = pad.buttons & ~ui_pad_last.buttons;

	ui_pad_last = pad;

	return input;
}
