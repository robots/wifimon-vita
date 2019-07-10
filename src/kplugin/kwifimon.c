#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/syslimits.h>
#include <sys/time.h>

#include <vitasdkkern.h>
#include <taihen.h>

#include "kwifimon_export.h"
#include "wlan_kernel.h"
#include "assert.h"
#include "radiotap.h"

#include "kwifimon.h"
#include "pcap.h"
#include "knet.h"
#include "m.h"

#define MAX(x, y) ((x)>(y)?(x):(y))
#define MIN(x, y) ((x)<(y)?(x):(y))
#define MAX_FILELEN 200

#define HOOKS_NUMBER 5
static int uids[HOOKS_NUMBER];
static int hooks_uid[HOOKS_NUMBER];
static tai_hook_ref_t ref_hooks[HOOKS_NUMBER];

SceUID kwifimon_mutex;
uint32_t kwifimon_channel_freq;
uint32_t kwifimon_channel_band;
volatile int kwifimon_state = 0;
struct wifimon_stats_t kwifimon_stats;

// missing taihen prototype
int module_get_offset(SceUID pid, SceUID modid, int segidx, size_t offset, uintptr_t *addr);
int module_get_export_func(SceUID pid, const char *modname, uint32_t libnid, uint32_t funcnid, uintptr_t *func);

int (*wlan_cmd_send1)(struct wlan_dev_t *dev, struct wlan_cmd_t *cmd, uint16_t cmdid, int cmd_len);
int (*wlan_cmd_send2)(struct wlan_dev_t *dev, struct wlan_cmd_t *cmd, uint16_t cmdid, int cmd_len);
int (*wlan_cmd_wait)(struct wlan_dev_t *dev, struct wlan_cmd_t *cmd);
struct wlan_cmd_t *(*wlan_cmd_alloc)(struct wlan_dev_t *dev, int cmd_len);
int (*wlan_cmd_free)(struct wlan_dev_t *dev, struct wlan_cmd_t *cmd);

int (*wlan_mem_read)(struct wlan_dev_t *dev, uint32_t addr, uint32_t *value);
int (*wlan_mem_write)(struct wlan_dev_t *dev, uint32_t addr, uint32_t value);

int (*wlan_do_init)(struct wlan_dev_t *dev);

int (*wlan_lock)(struct wlan_lock_t *ptr);
void (*wlan_unlock)(struct wlan_lock_t *ptr);

int kwifimon_mod_state(void)
{
	int state, ret;

	ENTER_SYSCALL(state);
	ret = kwifimon_state;
	EXIT_SYSCALL(state);

	return ret;
}

int kwifimon_cap_start(char *file)
{
	int state, ret;
	char filename[MAX_FILELEN];

	ENTER_SYSCALL(state);

	ret = ksceKernelLockMutex(kwifimon_mutex, 1, NULL);
	if (ret >= 0) {
//	ksceKernelStrncpyUserToKernel(filename, (uintptr_t)file, MAX_FILELEN);
		ret = pcap_open("ux0:/data/test.cap");
		if (ret == 0) {
			kwifimon_state |= STATE_REC_FILE;
		}
		ret = ksceKernelUnlockMutex(kwifimon_mutex, 1);
	}

	EXIT_SYSCALL(state);


	return ret;
}

int kwifimon_cap_stop(void)
{
	int state, ret;

	ENTER_SYSCALL(state);

	ret = ksceKernelLockMutex(kwifimon_mutex, 1, NULL);
	if (ret >= 0) {
		pcap_close();
		kwifimon_state &= ~STATE_REC_FILE;
		ret = ksceKernelUnlockMutex(kwifimon_mutex, 1);
	}

	EXIT_SYSCALL(state);

	return ret;
}

int kwifimon_mod_stats(struct wifimon_stats_t *s, int reset)
{
	int state, ret;

	ENTER_SYSCALL(state);

	ret = ksceKernelLockMutex(kwifimon_mutex, 1, NULL);
	if (ret >= 0) {
		ksceKernelMemcpyKernelToUser((uintptr_t)s, &kwifimon_stats, sizeof(struct wifimon_stats_t));

		if (reset) {
			memset(&kwifimon_stats, 0, sizeof(struct wifimon_stats_t));
		}

		ret = ksceKernelUnlockMutex(kwifimon_mutex, 1);
	}

	EXIT_SYSCALL(state);

	return ret;
}

int kwifimon_net_start(void)
{
	int state, ret;

	ENTER_SYSCALL(state);

	ret = ksceKernelLockMutex(kwifimon_mutex, 1, NULL);
	if (ret >= 0) {
		ret = knet_start(KWIFIMON_NET_PORT);
		if (ret == 0) {
			kwifimon_state |= STATE_REC_NET;
		}
		ret = ksceKernelUnlockMutex(kwifimon_mutex, 1);
	}

	EXIT_SYSCALL(state);

	return ret;
}

int kwifimon_net_stop(void)
{
	int state, ret;

	ENTER_SYSCALL(state);

	ret = ksceKernelLockMutex(kwifimon_mutex, 1, NULL);
	if (ret >= 0) {
		knet_stop();
		kwifimon_state &= ~STATE_REC_NET;
		ret = ksceKernelUnlockMutex(kwifimon_mutex, 1);
	}

	EXIT_SYSCALL(state);

	return ret;
}

int kwifimon_mac_control(struct wlan_dev_t *dev, uint16_t mode)
{
	struct wlan_cmd_t *cmd = wlan_cmd_alloc(dev, sizeof(struct wlan_mac_control_t));

	if (!cmd) {
		return 0x80418005;
	}

	cmd->result_cb = 0;
	struct wlan_mac_control_t *mc = (struct wlan_mac_control_t *)cmd->out_data;

	mc->action = mode;

	int ret = wlan_cmd_send2(dev, cmd, WLAN_CMD_MAC_CONTROL, sizeof(struct wlan_mac_control_t));

	if (ret >= 0) {
		ret = wlan_cmd_wait(dev, cmd);
	}

	wlan_cmd_free(dev, cmd);

	return ret;
}

int kwifimon_wlan_anycmd(struct wlan_dev_t *dev, int wlancmd, uint8_t *buf, uint8_t out_len, uint16_t in_len)
{
	struct wlan_cmd_t *cmd = wlan_cmd_alloc(dev, out_len);

	if (!cmd) {
		return 0x80418005;
	}

	cmd->result_cb = 0;
	memcpy(cmd->out_data, buf, out_len);

	int ret = wlan_cmd_send2(dev, cmd, wlancmd, out_len);
	if (ret >= 0) {
		ret = wlan_cmd_wait(dev, cmd);

		if (in_len) {
			memcpy(buf, cmd->in_data, in_len);
		}
	}

	wlan_cmd_free(dev, cmd);

	return ret;
}

// hooked wifi command response handler
int kwifimon_process_respose(struct wlan_dev_t *dev, uint8_t *in_pkt, int in_pkt_len, uint32_t *somenumber)
{
	struct sdio_rx_t *rxt = (struct sdio_rx_t *)in_pkt;
/*
	// we dont care about cmds
	if (rxt->pkt_type == 1) {
		return TAI_CONTINUE(int, ref_hooks[0], dev, pkt, pkt_len, somenumber);
	}
*/

	// event
	if (rxt->pkt_type == 3) {
		uint16_t evt;

		memcpy(&evt, in_pkt, 2);

		evt = evt & 0xfff;

		if (evt == 0x123) {
			int ret = ksceKernelLockMutex(kwifimon_mutex, 1, NULL);
			if (ret >= 0) {
				kwifimon_stats.evt_cnt++;
				ksceKernelUnlockMutex(kwifimon_mutex, 1);
			}
			return 0;
		}
	}


	// data or amsdu
	if (rxt->pkt_type == 0 || rxt->pkt_type == 10) {
		struct rxpd *rx_pd = (void *)in_pkt + sizeof(struct sdio_rx_t);

		uint8_t *pkt = (void *)rx_pd + rx_pd->rx_pkt_offset;
		uint32_t pkt_len = rx_pd->rx_pkt_length;

		int ret = ksceKernelLockMutex(kwifimon_mutex, 1, NULL);
		if (ret >= 0) {
/*	
			if (kwifimon_state & (STATE_REC_FILE | STATE_REC_NET)) {
				struct rx_radiotap_hdr radiotap;

				// fill radiotap header with known information
				memset(&radiotap, 0, sizeof(radiotap));

				radiotap.hdr.it_len = sizeof(struct rx_radiotap_hdr);
				radiotap.hdr.it_present = RX_RADIOTAP_PRESENT;

				radiotap.ch_freq = kwifimon_channel_freq;
				radiotap.ch_flags = (kwifimon_channel_band == WLAN_RADIO_TYPE_A)? IEEE80211_CHAN_5GHZ : IEEE80211_CHAN_2GHZ;
				if (rx_pd->ht_info & 1) {
					radiotap.mcs = rx_pd->rx_rate;
					radiotap.mcs_known =  IEEE80211_RADIOTAP_MCS_HAVE_BW | IEEE80211_RADIOTAP_MCS_HAVE_MCS | IEEE80211_RADIOTAP_MCS_HAVE_GI;
					radiotap.mcs_flags |= (rx_pd->ht_info & 2) ? IEEE80211_RADIOTAP_MCS_BW_40 : IEEE80211_RADIOTAP_MCS_BW_20;
					radiotap.mcs_flags |= (rx_pd->ht_info & 4) ? IEEE80211_RADIOTAP_MCS_SGI : 0;
				}

				radiotap.rate = MIN(255, mwifiex_index_to_data_rate(rx_pd->rx_rate, rx_pd->ht_info));

				radiotap.antsignal = MIN(127, rx_pd->snr + rx_pd->nf);
				radiotap.antnoise = rx_pd->nf;

				// write to file
				if (kwifimon_state & STATE_REC_FILE) {
	//				pcap_write_rt(&radiotap.hdr, pkt, pkt_len);
				}

				if (kwifimon_state & STATE_REC_NET) {
					knet_write_rt(&radiotap.hdr, pkt, pkt_len);
				}
			}
*/
/*			if (kwifimon_state & STATE_REC_FILE) {
				pcap_write_raw(in_pkt, in_pkt_len);
			}*/

			if (rx_pd->rx_pkt_type == PKT_TYPE_MGMT) {
				kwifimon_stats.mgmt_cnt++;
			} else if (rx_pd->rx_pkt_type == PKT_TYPE_AMSDU) {
				kwifimon_stats.amsdu_cnt++;
			} else if (rx_pd->rx_pkt_type == PKT_TYPE_BAR) {
				kwifimon_stats.bar_cnt++;
			} else {
				kwifimon_stats.pkt_cnt++;
			}

			ksceKernelUnlockMutex(kwifimon_mutex, 1);
		}

		if (rx_pd->rx_pkt_type == PKT_TYPE_MGMT) {
			// dont pass mgmt frame, driver will ignore it, but there is allocation overhead
			return 0;
		}
	}

	return TAI_CONTINUE(int, ref_hooks[1], dev, in_pkt, in_pkt_len, somenumber);
}

// hooked ioctl function
int kwifimon_ioctl(struct netdev_t *netdev, unsigned int req, uint8_t *buf, int buf_len)
{
	int ret = -1;
/*	if (ref_hooks[2]) {
		ret = TAI_CONTINUE(int, ref_hooks[2], netdev, req, buf, buf_len);
	}*/

	ret = TAI_CONTINUE(int, ref_hooks[2], netdev, req, buf, buf_len);
	
	struct wlan_dev_t *dev = netdev->priv;

	int lockret = wlan_lock(&dev->wlan_lock);
	if (lockret >= 0) {
		if (req == WLAN_IOCTL_GET_MAC_CONTROL) {
			if (buf_len >= 2) {
				uint16_t mode = dev->current_mac_control;
				memcpy(buf, &mode, 2);
				ret = 0;
			}
		} else if (req == WLAN_IOCTL_SET_MAC_CONTROL) {
			if (buf_len >= 2) {
				uint16_t mode;
				memcpy(&mode, buf, 2);
				ret = kwifimon_mac_control(dev, mode); 
				if (ret >= 0) {
					dev->current_mac_control = mode;
				}
			}
		} else if (req == WLAN_IOCTL_ANYCMD) {
			uint16_t out_len, in_len, cmd;
			if (buf_len >= 6) {
				memcpy(&cmd, &buf[0], 2);
				memcpy(&out_len, &buf[2], 2);
				memcpy(&in_len, &buf[4], 2);
				if (buf_len >= MAX(out_len, in_len) + 6) {
					ret = kwifimon_wlan_anycmd(dev, cmd, &buf[6], out_len, in_len); 
				}
			}
		} else if (req == WLAN_IOCTL_MEM) {
			uint32_t addr, value;
			if (buf_len >= 9) {
				memcpy(&addr, &buf[1], 4);
				memcpy(&value, &buf[5], 4);
				if (buf[0] == 0) {
					ret = wlan_mem_read(dev, addr, &value);
				} else if (buf[0] == 1) {
					ret = wlan_mem_write(dev, addr, value);
				}
				memcpy(&buf[5], &value, 4);
			}
		}/* else if (req == WLAN_IOCTL_INIT) {
			ret = wlan_do_init(dev);
		}*/

		wlan_unlock(&dev->wlan_lock);
	}

	return ret;
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args)
{
	kwifimon_mutex = ksceKernelCreateMutex("kwifimon_mutex", 0, 0, NULL);

	STATIC_ASSERT((sizeof(struct netdev_t) == 0xb0), "Bad size of struct netdev_t!")
//	STATIC_ASSERT((sizeof(struct netdev_2_t) == 0x360), "Bad size of struct netdev_2_t!")
//	STATIC_ASSERT((sizeof(struct netdev_3_t) == 0x1f8), "Bad size of struct netdev_3_t!")
	STATIC_ASSERT((sizeof(struct wlan_dev_t) == 0x1e30), "Bad size of struct wlan_dev_t!")
	STATIC_ASSERT((offsetof(struct wlan_dev_t, wlan_lock) == 0x718), "Bad wlan_lock offset!")

	tai_module_info_t tai_info;
	
	// fetch SceWlan stuff
	memset(&tai_info,0,sizeof(tai_module_info_t));
	tai_info.size = sizeof(tai_module_info_t);
	if (taiGetModuleInfoForKernel(KERNEL_PID, "SceWlanBt", &tai_info) < 0) {
		kwifimon_state = STATE_ERROR;
		return SCE_KERNEL_START_SUCCESS;
	}

	module_get_offset(KERNEL_PID, tai_info.modid, 0, 0x2954 | 1, (uintptr_t *)&wlan_cmd_alloc);
	module_get_offset(KERNEL_PID, tai_info.modid, 0, 0x2A18 | 1, (uintptr_t *)&wlan_cmd_free);
	module_get_offset(KERNEL_PID, tai_info.modid, 0, 0x2F44 | 1, (uintptr_t *)&wlan_cmd_send1);
	module_get_offset(KERNEL_PID, tai_info.modid, 0, 0x2DB4 | 1, (uintptr_t *)&wlan_cmd_send2);
	module_get_offset(KERNEL_PID, tai_info.modid, 0, 0x2E7C | 1, (uintptr_t *)&wlan_cmd_wait);
	module_get_offset(KERNEL_PID, tai_info.modid, 0, 0x0E50 | 1, (uintptr_t *)&wlan_lock);
	module_get_offset(KERNEL_PID, tai_info.modid, 0, 0x0E70 | 1, (uintptr_t *)&wlan_unlock);
/*
	module_get_offset(KERNEL_PID, tai_info.modid, 0, 0xADD8 | 1, (uintptr_t *)&wlan_do_init);
*/
	module_get_offset(KERNEL_PID, tai_info.modid, 0, 0x4568 | 1, (uintptr_t *)&wlan_mem_read);
	module_get_offset(KERNEL_PID, tai_info.modid, 0, 0x45E8 | 1, (uintptr_t *)&wlan_mem_write);

	hooks_uid[1] = taiHookFunctionOffsetForKernel(KERNEL_PID, &ref_hooks[1], tai_info.modid, 0, 0x1cd4, 1, kwifimon_process_respose);
	hooks_uid[2] = taiHookFunctionOffsetForKernel(KERNEL_PID, &ref_hooks[2], tai_info.modid, 0, 0x73f0, 1, kwifimon_ioctl);

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	int i;

	pcap_close();
	knet_stop();
	kwifimon_state = 0;

	i = HOOKS_NUMBER;
	while (i--) {
		if (uids[i]) taiInjectReleaseForKernel(uids[i]);
	}

	i = HOOKS_NUMBER;
	while (i--) {
		if (hooks_uid[i]) taiHookReleaseForKernel(hooks_uid[i], ref_hooks[i]);
	}

	ksceKernelDeleteMutex(kwifimon_mutex);

	return SCE_KERNEL_STOP_SUCCESS;
}
