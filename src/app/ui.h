#ifndef UI_h_
#define UI_h_

#include <vita2d.h>

struct ui_color_t {
	unsigned int bg;
	unsigned int text;
	unsigned int seleted_bg;
	unsigned int selected_text;
};

extern struct ui_color_t ui_color;
extern vita2d_font *ui_font;

void ui_init(void);
void ui_deinit(void);
void ui_main(void);
void ui_draw_sb(void);
uint32_t ui_get_input(void);
#endif
