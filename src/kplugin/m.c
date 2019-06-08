
#include "m.h"

struct ieee80211_channel {
	uint32_t center_freq;
	uint32_t hw_value;
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

uint8_t m_rate_to_radiotap(uint8_t rate)
{
	switch (rate) {
	case 0:		/*   1 Mbps */
		return 2;
	case 1:		/*   2 Mbps */
		return 4;
	case 2:		/* 5.5 Mbps */
		return 11;
	case 3:		/*  11 Mbps */
		return 22;
	/* case 4: reserved */
	case 5:		/*   6 Mbps */
		return 12;
	case 6:		/*   9 Mbps */
		return 18;
	case 7:		/*  12 Mbps */
		return 24;
	case 8:		/*  18 Mbps */
		return 36;
	case 9:		/*  24 Mbps */
		return 48;
	case 10:		/*  36 Mbps */
		return 72;
	case 11:		/*  48 Mbps */
		return 96;
	case 12:		/*  54 Mbps */
		return 108;
	}
	return 0;
}
