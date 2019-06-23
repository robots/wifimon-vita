#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vitasdk.h>

#include "util.h"


struct {
	uint32_t base;
	uint32_t size;
	char *name
} segs[] = {
	{ 0x00000000, 0x00080000, "00seg" },// code ram (tcim)
	{ 0x04000000, 0x00010000, "04seg" },// data ram (tcdm)
	{ 0xc0000000, 0x00040000, "c0seg" },// ram
	{ 0x03f00000, 0x00050000, "03seg" },// code rom
	{ 0x80000000, 0x00010000, "80seg" },// peripherals
	{ 0x90000000, 0x00001000, "90seg" },// this one is not dumpable somehow
};


int dump(int i)
{
	int ret = 0;
	uint32_t addr = segs[i].base;
	uint32_t addrmax = segs[i].base + segs[i].size;


	uint8_t *seg = malloc(segs[i].size);
	if (seg == NULL) {
		return -1;
	}
	memset(seg, 0, segs[i].size);

	uint8_t *segp = seg;
	uint32_t size = 0;

	while (addr < addrmax) {
		int ret = mem_read(addr, segp);
		if (ret < 0) {
			ret = -2;
			break;
		}

		segp += 4;
		addr += 4;
		size += 4;
	}

	char name[200];
	SceUID fd;

	sprintf(name, "ux0:data/dump-%08x-%08x.bin", segs[i].base, size);

	if ((fd = sceIoOpen(name, SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC, 0777)) >= 0) {
		if (sceIoWrite(fd, seg, size) >= 0) {
			sceIoClose(fd);
		}
	}

	free(seg);

	return ret;
}
