#ifndef M_h_
#define M_h_

#include <stdint.h>

int m_chan_valid(uint32_t num, int band);
int m_freq_valid(uint32_t freq);
uint32_t m_hwvalue_to_freq(uint32_t num, uint32_t band);
uint32_t m_freq_to_hwvalue(uint32_t freq);
uint16_t mwifiex_index_to_data_rate(uint8_t index, uint8_t ht_info);

#endif
