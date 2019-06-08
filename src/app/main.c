#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vitasdk.h>
#include <vita2d.h>
#include <taihen.h>





//void main(void) __attribute__ ((weak, alias ("module_start")));
void main(void)
{
	sceCtrlData pad;
	sceCtrlData pad_last;
	
	memset(&pad_last, 0, sizeof(sceCtrlData));
	memset(&pad, 0, sizeof(sceCtrlData));

	vita2d_init();
	vita2d_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
	vita2d_pgf *font = vita2d_load_default_pgf();


	while (1) {
		sceCtrlPeekBufferPositive(0, &pad, 1);
		unsigned int input = pad.buttons & ~pad_last.buttons;
		pad_last = pad;

		vita2d_start_drawing();
		vita2d_clear_screen();

		if (input & SCE_CTRL_CIRCLE) {
			break;
		}

		vita2d_end_drawing();
		vita2d_swap_buffers();

		sceKernelDelayThread(1);
	}

	vita2d_fini();
	vita2d_free_pgf(font);
				
	sceKernelExitProcess(0);
	return 0;
}
