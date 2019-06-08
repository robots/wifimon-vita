/**
 *
 * Name:	skdrv1st.h
 * Project:	Wireless LAN, SDIO bus driver
 * Version:	$Revision: 1.1 $
 * Date:	$Date: 2007/01/18 09:26:19 $
 * Purpose:	Contains all defines system specific definitions
 *
 ******************************************************************************/

/******************************************************************************
 *
 *	Copyright (C) 2003-2009, Marvell International Ltd.
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF MARVELL
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	This Module contains Proprietary Information of Marvell
 *	and should be treated as Confidential.
 *
 *	The information in this file is provided for the exclusive use of
 *	the licensees of Marvell.
 *	Such users have the right to use, modify, and incorporate this code
 *	into products for purposes authorized by the license agreement
 *	provided they include this notice and the associated copyright notice
 *	with any such product.
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * History:
 *	$Log: skdrv1st.h,v $
 *	Revision 1.1  2007/01/18 09:26:19  pweber
 *	Put under CVS control
 *	
 *
 ******************************************************************************/

#ifndef __INC_SKDRV1ST_H
#define __INC_SKDRV1ST_H

#define SDIO_BUS_DRIVER

// Linux specific stuff

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#include <linux/config.h>
#endif
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif
#include <linux/sched.h>        // kernel_thread
#include <linux/mm.h>           // memory manager
#include <linux/stat.h>
#include <linux/wait.h>

// kernel specific inludes ++++++++++++++++++++++

#include <linux/workqueue.h>    /* for struct work_queue */
#include <linux/timer.h>

// defines ++++++++++++++++++++

// #define UDELAY udelay
#define UDELAY(x)

#define INTERLOCKED_INC(x) (atomic_inc((x)),atomic_read((x)))
#define INTERLOCKED_DEC(x) (atomic_dec((x)),atomic_read((x)))
#define ZERO_MEMORY( pBuf,len) memset((pBuf),0,(len));

#include "sktypes.h"
// #include "../h/sdio.h"

#define MRVL_DEVICE_NAME "MrvlSdioBus"

/* defines ********************************************************************/
/* typedefs *******************************************************************/
/* function prototypes ********************************************************/

#endif /* __INC_SKDRV1ST_H */
