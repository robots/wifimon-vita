#include <vitasdk.h>
#include <stdlib.h>
#include <taihen.h>

//#include <kwifimon.h>
#include "../common/kwifimon_export.h"
#include "uwifimon.h"

int (*sceNetSyscallGetIfList)(struct iface_t *, int c);
int (*sceNetSyscallControl)(int dev, int req, void *buf, int buf_len);

int uwifimon_get_iface_list(struct iface_t **list, int *cnt)
{
	// first get interface count (100 is arbitraty)
	int c = sceNetSyscallGetIfList(0, 100);
	*cnt = c;

	// allocate enough space
	*list = malloc(sizeof(struct iface_t) * c);
	if (*list == NULL) {
		*cnt = 0;
		return 1;
	}

	// get real interface list
	sceNetSyscallGetIfList(*list, c);

	return 0;
}

int uwifimon_ioctl(int dev, int req, void *buf, int buf_len)
{
	return sceNetSyscallControl(dev, req, buf, buf_len);
}

int uwifimon_cap_start(char *file)
{
	return kwifimon_cap_start(file, strlen(file));
}

int uwifimon_cap_stop(void)
{
	return kwifimon_cap_stop();
}

int uwifimon_cap_status(void)
{
	return kwifimon_cap_state();
}

void _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize args, void *argp) {
	taiGetModuleExportFunc("SceNetPs", 0x2CBED2C6, 0xEA0C1B71, (uintptr_t*)&sceNetSyscallControl);
	taiGetModuleExportFunc("SceNetPs", 0x2CBED2C6, 0x878274CE, (uintptr_t*)&sceNetSyscallGetIfList);
  return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, void *argp) {
  return SCE_KERNEL_STOP_SUCCESS;
}
