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

#define MIN(x, y) ((x)<(y)?(x):(y))
#define MAX_FILELEN 200

#define HOOKS_NUMBER 5
static int uids[HOOKS_NUMBER];
static int hooks_uid[HOOKS_NUMBER];
static tai_hook_ref_t ref_hooks[HOOKS_NUMBER];

uint32_t kwifimon_channel_freq;
uint32_t kwifimon_channel_band;
// TODO: mutex?
volatile int kwifimon_state = 0;

// missing taihen prototype
int module_get_offset(SceUID pid, SceUID modid, int segidx, size_t offset, uintptr_t *addr);
int module_get_export_func(SceUID pid, const char *modname, uint32_t libnid, uint32_t funcnid, uintptr_t *func);

int (*wlan_cmd_send1)(struct wlan_dev_t *dev, struct wlan_cmd_t *cmd, uint16_t cmdid, int cmd_len);
int (*wlan_cmd_send2)(struct wlan_dev_t *dev, struct wlan_cmd_t *cmd, uint16_t cmdid, int cmd_len);
int (*wlan_cmd_wait)(struct wlan_dev_t *dev, struct wlan_cmd_t *cmd);
struct wlan_cmd_t *(*wlan_cmd_alloc)(struct wlan_dev_t *dev, int cmd_len);
int (*wlan_cmd_free)(struct wlan_dev_t *dev, struct wlan_cmd_t *cmd);

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

	ksceKernelStrncpyUserToKernel(filename, (uintptr_t)file, MAX_FILELEN);
	ret = pcap_open(file);
	if (ret == 0) {
		kwifimon_state |= STATE_REC_FILE;
	}

	EXIT_SYSCALL(state);

	return ret;
}

int kwifimon_cap_stop(void)
{
	int state, ret;

	ENTER_SYSCALL(state);

	pcap_close();
	kwifimon_state &= ~STATE_REC_FILE;
	ret = 0;

	EXIT_SYSCALL(state);

	return ret;
}

int kwifimon_net_start(void)
{
	int state, ret;

	ENTER_SYSCALL(state);

	ret = knet_start(KWIFIMON_NET_PORT);
	if (ret == 0) {
		kwifimon_state |= STATE_REC_NET;
	}

	EXIT_SYSCALL(state);

	return ret;
}

int kwifimon_net_stop(void)
{
	int state, ret;

	ENTER_SYSCALL(state);

	knet_stop();
	kwifimon_state &= ~STATE_REC_NET;
	ret = 0;

	EXIT_SYSCALL(state);

	return ret;
}

int kwifimon_mac_control(struct wlan_dev_t *dev, uint16_t mode)
{
//	ksceKernelLockFastMutex(dev->tcmmutex);

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

//	ksceKernelUnlockFastMutex(dev->tcmmutex);

	return ret;
}


int kwifimon_monitor_mode(struct wlan_dev_t *dev, int mode)
{
//	ksceKernelLockFastMutex(dev->tcmmutex);

	struct wlan_cmd_t *cmd = wlan_cmd_alloc(dev, sizeof(struct wlan_mon_t));

	if (!cmd) {
		return 0x80418005;
	}

	cmd->result_cb = 0;
	struct wlan_mon_t *wm = (struct wlan_mon_t *)cmd->out_data;

	wm->action = 1;
	wm->mode = !!mode;

	int ret = wlan_cmd_send2(dev, cmd, WLAN_CMD_802_11_MONITOR_MODE, sizeof(struct wlan_mon_t));
	if (ret >= 0) {
		ret = wlan_cmd_wait(dev, cmd);
	}

	wlan_cmd_free(dev, cmd);

//	ksceKernelUnlockFastMutex(dev->tcmmutex);

	return ret;
}

int kwifimon_channel(struct wlan_dev_t *dev, int band, int chan)
{
//	ksceKernelLockFastMutex(dev->tcmmutex);

	struct wlan_cmd_t *cmd = wlan_cmd_alloc(dev, sizeof(struct wlan_rf_channel_t));

	if (!cmd) {
		return 0x80418005;
	}

	cmd->result_cb = 0;
	struct wlan_rf_channel_t *rf = (struct wlan_rf_channel_t *)cmd->out_data;

	rf->action = 1;
	rf->current_channel = chan;
	rf->rf_type = band ? WLAN_RADIO_TYPE_A : WLAN_RADIO_TYPE_BG;

	int ret = wlan_cmd_send2(dev, cmd, WLAN_CMD_802_11_RF_CHANNEL, sizeof(struct wlan_rf_channel_t));
	if (ret >= 0) {
		ret = wlan_cmd_wait(dev, cmd);
	}

	wlan_cmd_free(dev, cmd);

//	ksceKernelUnlockFastMutex(dev->tcmmutex);

	return ret;
}

// hooked wifi command response handler
int kwifimon_process_respose(struct wlan_dev_t *dev, uint8_t *pkt, int pkt_len, uint32_t *somenumber)
{
	struct sdio_rx_t *rxt = (struct sdio_rx_t *)pkt;
/*
	// we dont care about events or cmds
	if (rxt->pkt_type == 1 || rxt->pkt_type == 3) {
		return TAI_CONTINUE(int, ref_hooks[0], dev, pkt, pkt_len, somenumber);
	}
*/
	// pkt or amsdu
	if (rxt->pkt_type == 0 || rxt->pkt_type == 10) {
		if (kwifimon_state & (STATE_REC_FILE | STATE_REC_NET)) {
			struct rxpd *rx_pd = (void *)pkt + sizeof(struct sdio_rx_t);
			uint8_t *pkt = (void *)rx_pd + rx_pd->rx_pkt_offset;
			uint32_t pkt_len = rx_pd->rx_pkt_length;
		
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
				radiotap.mcs_flags |= (rx_pd->ht_info & 3) ? IEEE80211_RADIOTAP_MCS_SGI : 0;
			} else {
				radiotap.rate = m_rate_to_radiotap(rx_pd->rx_rate);
			}
			radiotap.antsignal = MIN(127, rx_pd->snr + rx_pd->nf);
			radiotap.antnoise = rx_pd->nf;

			// write to file
			if (kwifimon_state & STATE_REC_FILE) {
				pcap_write_rt(&radiotap.hdr, pkt, pkt_len);
			}

			if (kwifimon_state & STATE_REC_NET) {
				knet_write_rt(&radiotap.hdr, pkt, pkt_len);
			}
		}
	}
	
/*	
	if (ref_hooks[1]) {
		return TAI_CONTINUE(int, ref_hooks[1], dev, pkt, pkt_len, somenumber);
	} else {
		return 0;
	}
*/
	return TAI_CONTINUE(int, ref_hooks[1], dev, pkt, pkt_len, somenumber);
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
			}
		} else if (req == WLAN_IOCTL_SET_RF_CHANNEL) {
			if (buf_len >= 4) {
				uint16_t band;
				uint16_t chan;

				memcpy(&band, &buf[0], 2);
				memcpy(&chan, &buf[2], 2);
				if (m_chan_valid(chan, band)) {
					ret = kwifimon_channel(dev, band, chan); 
					if (ret >= 0) {
						kwifimon_channel_freq = m_hwvalue_to_freq(chan, band);
						kwifimon_channel_band = band;
					}
				}
			}
		} else if (req == WLAN_IOCTL_SET_MONITOR_MODE) {
			if (buf_len >= 2) {
				uint16_t mode;
				memcpy(&mode, buf, 2);
				ret = kwifimon_monitor_mode(dev, mode); 

				if (ret == 0) {
					kwifimon_state |= STATE_MONITOR;
				}
			}
		}

		wlan_unlock(&dev->wlan_lock);
	}

	return ret;
}

/*
int unload_allowed_patched(void)
{
	int ret;
	ret = TAI_CONTINUE(int, ref_hooks[0]);
	return 1; // always allowed
}
*/

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args)
{
//	hooks_uid[0] = taiHookFunctionImportForKernel(KERNEL_PID, &ref_hooks[0], "SceKernelModulemgr", 0x11F9B314, 0xBBA13D9C, unload_allowed_patched);
	STATIC_ASSERT((sizeof(struct netdev_t) == 0xb0), "Bad size of struct netdev_t!")
//	STATIC_ASSERT((sizeof(struct netdev_2_t) == 0x360), "Bad size of struct netdev_2_t!")
//	STATIC_ASSERT((sizeof(struct netdev_3_t) == 0x1f8), "Bad size of struct netdev_3_t!")
	STATIC_ASSERT((sizeof(struct wlan_dev_t) == 0x1e30), "Bad size of struct wlan_dev_t!")
	STATIC_ASSERT((offsetof(struct wlan_dev_t, wlan_lock) == 0x718), "Bad wlan_lock offset!")

	tai_module_info_t tai_info;
	
	// first: fetch SceWlan stuff
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

	hooks_uid[1] = taiHookFunctionOffsetForKernel(KERNEL_PID, &ref_hooks[1], tai_info.modid, 0, 0x1cd4, 1, kwifimon_process_respose);
	hooks_uid[2] = taiHookFunctionOffsetForKernel(KERNEL_PID, &ref_hooks[2], tai_info.modid, 0, 0x73f0, 1, kwifimon_ioctl);


/* not needed
	// second: patch SceNetPS 
	memset(&tai_info,0,sizeof(tai_module_info_t));
	tai_info.size = sizeof(tai_module_info_t);
	if (taiGetModuleInfoForKernel(KERNEL_PID, "SceNetPs", &tai_info) < 0) {
		kwifimon_state = STATE_ERROR_1;
		return SCE_KERNEL_START_SUCCESS;
	}

	// stolen from Adrenaline
	uint32_t movs_a1_0_nop_opcode = 0xBF002000;
	// patch access to sceNetSyscallControl (replace bl xxx with mov R0, 0; nop)
	uids[0] = taiInjectDataForKernel(KERNEL_PID, tai_info.modid, 0, 0x2710, &movs_a1_0_nop_opcode, sizeof(movs_a1_0_nop_opcode));
*/

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
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
	return SCE_KERNEL_STOP_SUCCESS;
}
