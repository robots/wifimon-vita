/**
 *
 * Name:	sdiobus.h
 * Project:	Wireless LAN, Bus driver for SDIO interface
 * Version:	$Revision: 1.1 $
 * Date:	$Date: 2007/01/18 09:26:19 $
 * Purpose:	Bus driver for SDIO interface definitions
 *
 *
 * Copyright (C) 2003-2009, Marvell International Ltd.
 *
 * This software file (the "File") is distributed by Marvell International 
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991 
 * (the "License").  You may use, redistribute and/or modify this File in 
 * accordance with the terms and conditions of the License, a copy of which 
 * is available by writing to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
 * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
 * this warranty disclaimer.
 *
 */

/******************************************************************************
 *
 * History:
 *
 *	$Log: sdiobus.h,v $
 *	Revision 1.1  2007/01/18 09:26:19  pweber
 *	Put under CVS control
 *	
 ******************************************************************************/
#ifndef SDIOBUS_H
#define SDIOBUS_H

#include "sdio.h"

#define USE_SDIO_IF_SEMA

#ifdef USE_SDIO_IF_SEMA
#ifdef DEBUG_SDIO_IF_SEMA
#define GET_IF_SEMA() \
		printk(">>>> %s@%d: SEMA DOWN\n",__FUNCTION__,__LINE__);\
		if ( down_interruptible(&sd_if_sema)) return -ERESTARTSYS;
#define GET_IF_SEMA_NO_RC() \
		printk(">>>> %s@%d: SEMA DOWN\n",__FUNCTION__,__LINE__);\
		if ( down_interruptible(&sd_if_sema)) return;
#else
#define GET_IF_SEMA() \
		if ( down_interruptible(&sd_if_sema)) return -ERESTARTSYS;
#define GET_IF_SEMA_NO_RC() \
		if ( down_interruptible(&sd_if_sema)) return;
#endif

#ifdef DEBUG_SDIO_IF_SEMA
#define REL_IF_SEMA() \
		printk("<<<< %s@%d: SEMA UP\n",__FUNCTION__,__LINE__);\
		up(&sd_if_sema)
#else
#define REL_IF_SEMA() \
		up(&sd_if_sema)
#endif
#else
#define GET_IF_SEMA()
#define GET_IF_SEMA_NO_RC()
#define REL_IF_SEMA()
#endif

#define MIN(a,b)		((a) < (b) ? (a) : (b))

#define FILE_DEVICE_SDIOBUSENUM 0x2A    // same as for Windows
#define SDIOBUSENUM_IOCTL(_index_) \
		_IOWR(FILE_DEVICE_SDIOBUSENUM, _index_, unsigned int )

#define IOCTL_SDIOBUSENUM_PLUGIN_HARDWARE               SDIOBUSENUM_IOCTL (0x0)
#define IOCTL_SDIOBUSENUM_UNPLUG_HARDWARE               SDIOBUSENUM_IOCTL (0x1)
#define IOCTL_SDIOBUSENUM_EJECT_HARDWARE                SDIOBUSENUM_IOCTL (0x2)
#define IOCTL_SDIOBUS_DONT_DISPLAY_IN_UI_DEVICE         SDIOBUSENUM_IOCTL (0x3)
// IOCTL for FPGA Configuration GUI
#define IOCTL_SDIOBUSENUM_SET_VOLTAGE                   SDIOBUSENUM_IOCTL (0x4)
#define IOCTL_SDIOBUSENUM_SET_BUSTYPE                   SDIOBUSENUM_IOCTL (0x5)
#define IOCTL_SDIOBUSENUM_SET_BUSWIDTH                  SDIOBUSENUM_IOCTL (0x6)
#define IOCTL_SDIOBUSENUM_SET_CLKSPEED                  SDIOBUSENUM_IOCTL (0x7)
#define IOCTL_SDIOBUSENUM_GET_BOARDREV                  SDIOBUSENUM_IOCTL (0x8)
#define IOCTL_SDIOBUSENUM_GET_JUMPER                    SDIOBUSENUM_IOCTL (0x9)

#define _SLEEP_MS( pDev, millisecs ) mdelay(millisecs)

#define MMC_TYPE_HOST		1
#define MMC_TYPE_CARD		2
#define MMC_REG_TYPE_USER	3

#define SD_BLOCK_SIZE	32

#define BIT_0         0x01
#define BIT_1         0x02
#define BIT_2         0x04
#define BIT_3         0x08
#define BIT_4         0x10
#define BIT_5         0x20
#define BIT_6         0x40
#define BIT_7         0x80

/* FBR (Function Basic Registers) */
#define FN_CSA_REG(x)             (0x100 * (x) + 0x00)
#define FN_CIS_POINTER_0_REG(x)   (0x100 * (x) + 0x09)
#define FN_CIS_POINTER_1_REG(x)   (0x100 * (x) + 0x0a)
#define FN_CIS_POINTER_2_REG(x)   (0x100 * (x) + 0x0b)
#define FN_CSA_DAT_REG(x)         (0x100 * (x) + 0x0f)
#define FN_BLOCK_SIZE_0_REG(x)    (0x100 * (x) + 0x10)
#define FN_BLOCK_SIZE_1_REG(x)    (0x100 * (x) + 0x11)

/* The following defines function 1 registers of SD25 */
/* Ref.: SD-25 SDIO Interface Specification Rev. 2.0.  */
/*       Marvell Semiconductor, Inc.                  */

/* SDIO Interface Registers */
#define IO_ENABLE_REG								0x02
#define IO_READY_REG								0x03
#define INT_ENABLE_REG							0x04
#define INT_PENDING_REG							0x05
#define IO_ABORT_REG								0x06
#define BUS_INTERFACE_CONTROL_REG 	0x07
#define CARD_CAPABILITY_REG					0x08
#define FUNCTION_SELECT_REG					0x0d
#define EXEC_FLAGS_REG							0x0e
#define READY_FLAGS_REG							0x0f
#define HIGH_SPEED_REG              	0x13
#define SHS                         	BIT_0
#define EHS                         	BIT_1

#define CARD_ADD_EVENT 			1
#define CARD_REMOVED_EVENT 	2

#define SDIOBUS_COMPATIBLE_IDS L"SDIOBUS\\MrvlCompatibleSDIOBus\0"
#define SDIOBUS_COMPATIBLE_IDS_LENGTH sizeof(SDIOBUS_COMPATIBLE_IDS)

#define SDIOBUS_POOL_TAG (ULONG) 'lvrM'

#define WAIT_SDIO_CMD_RESPONSE() UDELAY(2)

#define MILLI_SECOND 10000

#define SDIO_BUS_TYPE			1
#define SDIO_SPI_TYPE			2

#define SDIO_1_BIT				1
#define SDIO_4_BIT				4

#define DMA_WR_SUPPORT			0x01
#define DMA_RD_SUPPORT			0x02
                                                                                                                                                                        /*--- mmoser 8/29/2006 ---*/
typedef struct _SDHOST_EVENT
{
    long event;
    wait_queue_head_t wq;
} SDHOST_EVENT;

//
// A common header for the device extensions of the PDOs and FDO
//

#define DEVICE_NAME_LENGTH 256
typedef struct _SD_CLIENT
{
    struct list_head sd_client_list_entry;
    struct _sd_driver *drv;     /* pointer to the SD client driver */
} SD_CLIENT, *PSD_CLIENT;

typedef struct _SD_DEVICE_DATA
{
    struct list_head sd_dev_list_entry;

#ifdef SYSKT_DMA_MALIGN_TEST
    SK_U32 dma_rx_malign;
    SK_U32 dma_tx_malign;
    SK_U32 dma_start_malign;
    SK_U8 dma_rbuff[1024 * 8];  // __attribute__((aligned(4)));
    SK_U8 dma_tbuff[1024 * 8];  // __attribute__((aligned(4)));
    // struct sk_buff *dma_skb;
#endif
    sd_device sd_dev[MAX_SDIO_FUNCTIONS];       /* API to client driver */
    sd_device_id sd_ids[MAX_SDIO_FUNCTIONS];    /* sd ids for this device */
    sd_device *irq_device_cache[MAX_SDIO_FUNCTIONS];    /* cache for speed up
                                                           irq response */
    u32 cisptr[MAX_SDIO_FUNCTIONS];

    u8 number_of_functions;

    spinlock_t sd_dev_lock;
    unsigned long sd_dev_lock_flags;

    int thread_id;
    int stop_thread;

    // mmoser 2005-11-28
    atomic_t card_add_event;
    atomic_t card_remove_event;

                                                                                                                                                                        /*--- mmoser 8/29/2006 ---*/
    SDHOST_EVENT trans_complete_evt;
    SDHOST_EVENT cmd_complete_evt;
    SDHOST_EVENT thread_started_evt;
    struct workqueue_struct *workqueue;

    struct work_struct irq_work;
    struct work_struct card_out_work;
    SK_BOOL isRemoving;

                                                                                                                                                                        /*--- mmoser 10/11/2006 ---*/
    struct pci_dev *dev;

    SK_U32 ioport;

    // IOBase Addresses
    SK_U32 *IOBase[2];
    SK_U32 IOBaseLength[2];
    SK_U32 IOBaseIx;
    SK_U32 Interrupt;
    SK_U32 Affinity;
    SK_U32 InterruptMode;
    SK_U32 lastIntStatus;
    SK_U8 lastCardIntStatus;
    SK_U8 MACEvent;
    SK_U8 DeviceName[2 * (DEVICE_NAME_LENGTH + 1)];
    SK_U32 DeviceNameLength;

    SK_BOOL dump;
    SK_U32 ClockSpeed;
    SK_BOOL initialized;
                                                                                                                                                                                        /*--- mmoser 13.09.2005 ---*/
    SK_BOOL SurpriseRemoved;

    atomic_t if_lock;

    SK_U8 dnlType;
    SK_U8 CmdInProgress;

    SK_U16 lastIRQSignalMask;
    SK_U16 currentIRQSignalMask;
    SK_U16 lastErrorIRQSignalMask;
    SK_U8 baseClockFreq;
    SK_U8 maxSupplyVoltage;
    SK_U16 max_block_size;
    SK_U32 debug_flags;
    SK_U32 sdio_voltage;

    SK_U32 errorOccured;

    SK_U32 bus_errors;
    SK_U32 bus_width;
    SK_U32 bus_type;
    SK_U32 crc_len;

    SK_U8 dma_support;

    SK_U8 PsState;              // pweber 17.08.2005

    SK_BOOL CardIn;
} SD_DEVICE_DATA, *PSD_DEVICE_DATA;

BOOLEAN SDIOBus_CardPluggedIn(PSD_DEVICE_DATA SdData);
BOOLEAN SDIOBus_CardRemoved(PSD_DEVICE_DATA SdData);

#endif
