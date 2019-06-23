
#include "m.h"

#define MWIFIEX_RATE_BITMAP_MCS0   32

struct ieee80211_channel {
	uint32_t center_freq;
	uint32_t hw_value;
};

static uint16_t mwifiex_data_rates[] = { 0x02, 0x04,
					0x0B, 0x16, 0x00, 0x0C, 0x12, 0x18,
					0x24, 0x30, 0x48, 0x60, 0x6C, 0x90,
					0x0D, 0x1A, 0x27, 0x34, 0x4E, 0x68,
					0x75, 0x82, 0x0C, 0x1B, 0x36, 0x51,
					0x6C, 0xA2, 0xD8, 0xF3, 0x10E, 0x00 };

static const uint16_t mcs_rate[4][16] = {
	/* LGI 40M */
	{ 0x1b, 0x36, 0x51, 0x6c, 0xa2, 0xd8, 0xf3, 0x10e,
	  0x36, 0x6c, 0xa2, 0xd8, 0x144, 0x1b0, 0x1e6, 0x21c },

	/* SGI 40M */
	{ 0x1e, 0x3c, 0x5a, 0x78, 0xb4, 0xf0, 0x10e, 0x12c,
	  0x3c, 0x78, 0xb4, 0xf0, 0x168, 0x1e0, 0x21c, 0x258 },

	/* LGI 20M */
	{ 0x0d, 0x1a, 0x27, 0x34, 0x4e, 0x68, 0x75, 0x82,
	  0x1a, 0x34, 0x4e, 0x68, 0x9c, 0xd0, 0xea, 0x104 },

	/* SGI 20M */
	{ 0x0e, 0x1c, 0x2b, 0x39, 0x56, 0x73, 0x82, 0x90,
	  0x1c, 0x39, 0x56, 0x73, 0xad, 0xe7, 0x104, 0x120 }
};


/* Channel definitions to be advertised to cfg80211 */
static struct ieee80211_channel mwifiex_channels_2ghz[] = {
	{.center_freq = 2412, .hw_value = 1, },
	{.center_freq = 2417, .hw_value = 2, },
	{.center_freq = 2422, .hw_value = 3, },
	{.center_freq = 2427, .hw_value = 4, },
	{.center_freq = 2432, .hw_value = 5, },
	{.center_freq = 2437, .hw_value = 6, },
	{.center_freq = 2442, .hw_value = 7, },
	{.center_freq = 2447, .hw_value = 8, },
	{.center_freq = 2452, .hw_value = 9, },
	{.center_freq = 2457, .hw_value = 10, },
	{.center_freq = 2462, .hw_value = 11, },
	{.center_freq = 2467, .hw_value = 12, },
	{.center_freq = 2472, .hw_value = 13, },
	{.center_freq = 2484, .hw_value = 14, },
};

static struct ieee80211_channel mwifiex_channels_5ghz[] = {
	{.center_freq = 5040, .hw_value = 8, },
	{.center_freq = 5060, .hw_value = 12, },
	{.center_freq = 5080, .hw_value = 16, },
	{.center_freq = 5170, .hw_value = 34, },
	{.center_freq = 5190, .hw_value = 38, },
	{.center_freq = 5210, .hw_value = 42, },
	{.center_freq = 5230, .hw_value = 46, },
	{.center_freq = 5180, .hw_value = 36, },
	{.center_freq = 5200, .hw_value = 40, },
	{.center_freq = 5220, .hw_value = 44, },
	{.center_freq = 5240, .hw_value = 48, },
	{.center_freq = 5260, .hw_value = 52, },
	{.center_freq = 5280, .hw_value = 56, },
	{.center_freq = 5300, .hw_value = 60, },
	{.center_freq = 5320, .hw_value = 64, },
	{.center_freq = 5500, .hw_value = 100, },
	{.center_freq = 5520, .hw_value = 104, },
	{.center_freq = 5540, .hw_value = 108, },
	{.center_freq = 5560, .hw_value = 112, },
	{.center_freq = 5580, .hw_value = 116, },
	{.center_freq = 5600, .hw_value = 120, },
	{.center_freq = 5620, .hw_value = 124, },
	{.center_freq = 5640, .hw_value = 128, },
	{.center_freq = 5660, .hw_value = 132, },
};

int m_chan_valid(uint32_t num, int band)
{
	int i;
	if (band) {
		for (i = 0; i < sizeof(mwifiex_channels_5ghz)/sizeof(mwifiex_channels_5ghz[0]); i++) {
			if (num == mwifiex_channels_5ghz[i].hw_value) {
				return 1;
			}
		}
	} else {
		for (i = 0; i < sizeof(mwifiex_channels_2ghz)/sizeof(mwifiex_channels_2ghz[0]); i++) {
			if (num == mwifiex_channels_2ghz[i].hw_value) {
				return 1;
			}
		}
	}

	return 0;
}

int m_freq_valid(uint32_t freq)
{
	int i;
	if (freq > 5000) {
		for (i = 0; i < sizeof(mwifiex_channels_5ghz)/sizeof(mwifiex_channels_5ghz[0]); i++) {
			if (freq == mwifiex_channels_5ghz[i].center_freq) {
				return 1;
			}
		}
	} else {
		for (i = 0; i < sizeof(mwifiex_channels_2ghz)/sizeof(mwifiex_channels_2ghz[0]); i++) {
			if (freq == mwifiex_channels_2ghz[i].center_freq) {
				return 1;
			}
		}
	}
	return 0;
}

uint32_t m_hwvalue_to_freq(uint32_t num, uint32_t band)
{
	int i;
	if (band) {
		for (i = 0; i < sizeof(mwifiex_channels_5ghz)/sizeof(mwifiex_channels_5ghz[0]); i++) {
			if (num == mwifiex_channels_5ghz[i].hw_value) {
				return mwifiex_channels_5ghz[i].center_freq;
			}
		}
	} else {
		for (i = 0; i < sizeof(mwifiex_channels_2ghz)/sizeof(mwifiex_channels_2ghz[0]); i++) {
			if (num == mwifiex_channels_2ghz[i].hw_value) {
				return mwifiex_channels_2ghz[i].center_freq;
			}
		}
	}
	return 0;
}

uint32_t m_freq_to_hwvalue(uint32_t freq)
{
	int i;
	if (freq > 5000) {
		for (i = 0; i < sizeof(mwifiex_channels_5ghz)/sizeof(mwifiex_channels_5ghz[0]); i++) {
			if (freq == mwifiex_channels_5ghz[i].center_freq) {
				return mwifiex_channels_5ghz[i].hw_value;
			}
		}
	} else {
		for (i = 0; i < sizeof(mwifiex_channels_2ghz)/sizeof(mwifiex_channels_2ghz[0]); i++) {
			if (freq == mwifiex_channels_2ghz[i].center_freq) {
				return mwifiex_channels_2ghz[i].hw_value;
			}
		}
	}
	return 0;
}

uint16_t mwifiex_index_to_data_rate(uint8_t index, uint8_t ht_info)
{
	uint16_t rate;

	if (ht_info & 1) {
		if (index == MWIFIEX_RATE_BITMAP_MCS0) {
			if (ht_info & 4)
				rate = 0x0D;	/* MCS 32 SGI rate */
			else
				rate = 0x0C;	/* MCS 32 LGI rate */
		} else if (index < 8) {
			if (ht_info & 2) {
				if (ht_info & 4)
					/* SGI, 40M */
					rate = mcs_rate[1][index];
				else
					/* LGI, 40M */
					rate = mcs_rate[0][index];
			} else {
				if (ht_info & 4)
					/* SGI, 20M */
					rate = mcs_rate[3][index];
				else
					/* LGI, 20M */
					rate = mcs_rate[2][index];
			}
		} else
			rate = mwifiex_data_rates[0];
	} else {
		if (index >= sizeof(mwifiex_data_rates))
			index = 0;
		rate = mwifiex_data_rates[index];
	}
	return rate;
}
