/**
 *
 * Name:	sdio.h
 * Project:	Wireless LAN, Bus driver for SDIO interface
 * Version:	$Revision: 1.1 $
 * Date:	$Date: 2007/01/18 09:26:19 $
 * Purpose:	Bus driver for SDIO interface definitions
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
#ifndef	_SDIO_H_
#define	_SDIO_H_

#include <linux/types.h>
#include "sdio_spec.h"

/* CMD53 parameter*/
//mode
#define BLOCK_MODE	0x1
#define BYTE_MODE    0x0
//opcode
#define FIXED_ADDRESS 	0x0
#define INCREMENTAL_ADDRESS 0x1

/*****************************************************************/
/*            New Multifunction SDIO Bus Driver API              */
/*****************************************************************/

#define SD_CLASS_ANY            (0xFFFF)
#define SD_FUNCTION_ANY         (0xFFFF)
#define SD_VENDOR_ANY           (0xFFFF)
#define SD_DEVICE_ANY           (0xFFFF)

#define SD_VENDOR_MARVELL 			0x02df

#define SD_CLASS_BT_A						0x02
#define SD_CLASS_BT_B						0x03
#define SD_CLASS_WLAN						0x07

typedef struct _sd_device_id
{
    u16 vendor;                 /* vendor id */
    u16 device;                 /* device id */
    u16 class;                  /* class id */
    u16 fn;                     /* function number: 1-7 */
    u16 blocksize;              /* block size */
} sd_device_id, *psd_device_id;

#define COMPARE_DEVICE_ID(dev_id_1,dev_id_2) \
(((sd_device_id *)(dev_id_1)->vendor == (sd_device_id *)(dev_id_2)->vendor) && \
 ((sd_device_id *)(dev_id_1)->device == (sd_device_id *)(dev_id_2)->device) && \
 ((sd_device_id *)(dev_id_1)->class  == (sd_device_id *)(dev_id_2)->class) &&  \
 ((sd_device_id *)(dev_id_1)->fn     == (sd_device_id *)(dev_id_2)->fn))

#define MAX_SDIO_FUNCTIONS	8
typedef void (*sd_int_handler) (void *dev, sd_device_id * id, void *context);

typedef struct _sd_function
{
    u16 class;                  /* class id */
    sd_int_handler int_handler; /* interrupt handler per function */
    void *context;              /* private context per function, will be
                                   passed back to the int_handler */
} sd_function, *psd_function;

struct _sd_device
{
    struct _sd_driver *drv;     /* pointer to the SD client driver */
    sd_function functions[MAX_SDIO_FUNCTIONS];  /* All supported functions */
    void *sd_bus;               /* Pointer to bus driver's private data */
    psd_device_id pCurrent_Ids; /* pointer to current ids */
    u8 chiprev;
    u32 *cisptr;
    u8 supported_functions;
    struct device *dev;         /* generic device interface for hotplug */
    u8 probe_called;
    u8 remove_called;
    void *priv;
};

struct _sd_driver
{
    const char *name;           /* name of client */
    sd_device_id *ids;          /* ids for this client driver */
    int (*probe) (struct _sd_device * dev, sd_device_id * id);  /* client
                                                                   driver probe 
                                                                   handler */
    int (*remove) (struct _sd_device * dev);    /* client driver remove handler 
                                                 */
    int (*suspend) (struct _sd_device * dev);   /* client driver suspend
                                                   handler */
    int (*resume) (struct _sd_device * dev);    /* client driver resume handler 
                                                 */
};

typedef struct _sd_driver sd_driver, *psd_driver;
typedef struct _sd_device sd_device, *psd_device;

/* Functions exported out for WLAN Layer */
int sd_driver_register(sd_driver * drv);
int sd_driver_unregister(sd_driver * drv);
int sd_request_int(sd_device * dev, sd_device_id * id, sd_function * function);
int sd_release_int(sd_device * dev, sd_device_id * id);
int sd_unmask(sd_device * dev, sd_device_id * id);
int sd_enable_int(sd_device * dev, sd_device_id * id);
int sd_disable_int(sd_device * dev, sd_device_id * id);
int sd_set_buswidth(sd_device * dev, int width);
int sd_set_busclock(sd_device * dev, int clock);
int sd_set_clock_on(sd_device * dev, int on);
int sd_start_clock(sd_device * dev);
int sd_stop_clock(sd_device * dev);
int sd_set_gpo(sd_device * dev, int on);
int sdio_read_ioreg(sd_device * dev, u8 fn, u32 reg, u8 * dat);
int sdio_write_ioreg(sd_device * dev, u8 fn, u32 reg, u8 dat);
int sdio_read_iomem(sd_device * dev, u8 fn, u32 address, u8 blkmode, u8 opcode,
                    u32 blkcnt, u32 blksz, u8 * buffer);
int sdio_write_iomem(sd_device * dev, u8 fn, u32 address, u8 blkmode, u8 opcode,
                     u32 blkcnt, u32 blksz, u8 * buffer);
#endif /* sdio.h */
