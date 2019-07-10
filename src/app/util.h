#ifndef UTIL_h_
#define UTIL_h_

#include <stdint.h>

int mem_read(uint32_t addr, uint32_t *datA);
int mem_write(uint32_t addr, uint32_t datA);
int wlan_cmd_func_shutdown(void);
int wlan_cmd_init(void);

#endif
