#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <vitasdk.h>

#include "util.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

uint8_t p1_patch[] = {
	0xb1, 0x68, 0x31, 0x44, 0x70, 0x69, 0x00, 0x28, 0x00, 0xD1, 0x05, 0x48, 0xA0, 0xF7, 0x75, 0xFD, 0x00, 0x25, 0x02, 0xAA,
	0x03, 0xA9, 0x04, 0xA8, 0xAB, 0xF7, 0xB6, 0xFD, 0xAB, 0xF7, 0xDA, 0xFD, 0x00, 0xFF, 0x05, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

uint8_t p2_orig[] = {
	0x00, 0x25, 0x02, 0xaa,
};

/* bl 0x5fedc */
uint8_t p2_patch[] = {
	0x54, 0xf0, 0x1a, 0xfa,
};


uint8_t p3_orig[] = {
	0x20, 0x20, 0x20, 0x81,
};

uint8_t p3_patch[] = {
	0x20, 0x20, 0x20, 0x20,
};


uint8_t p4_orig[] = {
	0x00, 0x19, 0xC1, 0x78,
};

uint8_t p4_patch[] = {
	0x00, 0xf0, 0x12, 0xf8,
};

struct patch_t {
	uint32_t addr;
	uint32_t size;
	uint8_t *orig;
	uint8_t *patch;
} patches[] = {
	{ 0x0005fedc, sizeof(p1_patch), NULL, p1_patch},
//	{ 0x00000a3c, sizeof(p3_orig), p3_orig, p3_patch},
//	{ 0x000001f0, sizeof(p4_orig), p4_orig, p4_patch},
	{ 0x0000baa4, sizeof(p2_orig), p2_orig, p2_patch},
};

int patch_do(void)
{
	int cnt = 0;

	for (uint32_t i = 0; i < ARRAY_SIZE(patches); i++) {
		struct patch_t *p = &patches[i];

		for (uint32_t j = 0; j < (p->size+3)/4; j++) {
			uint32_t tmp, in;

			memcpy(&tmp, &p->patch[4*j], 4);
			int ret = mem_write(p->addr + 4*j, tmp);
			if (ret < 0) {
				// we broke wifi, sorry about that
				return -1;
			}

			ret = mem_read(p->addr + 4*j, &in);
			if (ret < 0) {
				// we broke wifi, sorry about that
				return -2;
			}

			if (tmp != in) {
				return -3;
			}
			cnt++;
		}
	}

	return cnt;
}

int patch_undo(void)
{
	for (uint32_t i = ARRAY_SIZE(patches)-1; i ; i--) {
		struct patch_t *p = &patches[i];

		if (p->orig == NULL) {
			continue;
		}

		for (uint32_t j = 0; j < p->size/4; j++) {
			uint32_t tmp, in;

			memcpy(&tmp, &p->orig[4*j], 4);
			int ret = mem_write(p->addr + 4*j, tmp);
			if (ret < 0) {
				// we broke wifi, sorry about that
				return -1;
			}

			ret = mem_read(p->addr + 4*j, &in);
			if (ret < 0) {
				// we broke wifi, sorry about that
				return -2;
			}

			if (tmp != in) {
				return -3;
			}
		}
	}

	return 0;
}
