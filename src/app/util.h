#ifndef UTIL_h_
#define UTIL_h_

#include <stdint.h>

int mem_read(uint32_t addr, uint32_t *datA);
int mem_write(uint32_t addr, uint32_t datA);

#endif
