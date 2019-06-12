#include <vitasdk.h>
#include <stdlib.h>
#include <taihen.h>

//#include <kwifimon.h>
#include "../common/kwifimon_export.h"
#include "uwifimon.h"

int uwifimon_cap_start(char *file)
{
	return kwifimon_cap_start(file);
	//return 0;
}

int uwifimon_cap_stop(void)
{
	return kwifimon_cap_stop();
}

int uwifimon_net_start(void)
{
	return kwifimon_net_start();
}

int uwifimon_net_stop(void)
{
	return kwifimon_net_stop();
}

int uwifimon_mod_state(void)
{
	return kwifimon_mod_state();
}

void _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize args, void *argp) {
  return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, void *argp) {
  return SCE_KERNEL_STOP_SUCCESS;
}
