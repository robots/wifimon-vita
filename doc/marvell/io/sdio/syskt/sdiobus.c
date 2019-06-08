/**
 *
 * Name: sdiobus.c
 * Project: Wireless LAN, Bus driver for SDIO interface
 * Version: $Revision: 1.1 $
 * Date: $Date: 2007/01/18 09:21:35 $
 * Purpose: Bus driver for SDIO interface
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
 *	$Log: sdiobus.c,v $
 *	Revision 1.1  2007/01/18 09:21:35  pweber
 *	Put under CVS control
 *	
 *	Revision 1.2  2005/12/08 12:01:00  ebauer
 *	Driver ckecks AND update voltage registry value if not correct
 *	
 *	Revision 1.1  2005/10/07 08:43:47  jschmalz
 *	Put SDIO with FPGA under CVS control
 *	
 *	
 ******************************************************************************/
#ifndef _lint
static const char SysKonnectFileId[] = "@(#)" __FILE__ " (C) Marvell ";
#endif /* !_lint */

#include "diagvers.h"           /* diag version control */

#include "h/skdrv1st.h"
#include "h/skdrv2nd.h"

#include <linux/proc_fs.h>      // proc filesystem

#ifdef DBG
SK_U32 stdDebugLevel = DBG_DEFAULT;
#endif // #ifdef DBG

#define USE_DEBOUNCE_CARD_IN
                                                                                                                                                                        /*--- mmoser 12/21/2006 ---*/
#define MAX_DEBOUNCE_TIME 100

DECLARE_MUTEX(sd_if_sema);
DECLARE_MUTEX(sd_client_sema);

static int sdio_major = 0;

int mrvlsdio_open(struct inode *inode, struct file *filp);
int mrvlsdio_release(struct inode *inode, struct file *filp);
int SDIOBus_IoCtl(struct inode *inode, struct file *filp,
                  unsigned int cmd, unsigned long arg);

static void call_client_probe(PSD_DEVICE_DATA pDev, int fn);
//
// Module parameters
//

static int bus_type = SDIO_BUS_TYPE;
static int bus_width = SDIO_1_BIT;
int block_size = SD_BLOCK_SIZE;
int dma_support = 1;
static int clock_speed = 0;
static int gpi_0_mode = 1;      // edge triggered
static int gpi_0_level = 1;     // high->low 
static int gpi_1_mode = 1;      // edge triggered
static int gpi_1_level = 1;     // high-->low 
#define INTMODE_SDIO	0
#define INTMODE_GPIO	1
int intmode = 0;                // GPIO intmode enabled
int gpiopin = 0;                // GPIO pin number for SDIO alternative IRQ
EXPORT_SYMBOL(intmode);
EXPORT_SYMBOL(gpiopin);

//static int debug_flags  = 0;
static int debug_flags = 0x0c;

static int sdio_voltage = FPGA_POWER_REG_3_3_V;

DECLARE_WAIT_QUEUE_HEAD(add_remove_queue);

// kernel 2.6.x
module_param(bus_type, int, S_IRUGO);
module_param(bus_width, int, S_IRUGO);
module_param(block_size, int, S_IRUGO);
module_param(clock_speed, int, S_IRUGO);
module_param(dma_support, int, S_IRUGO);
module_param(debug_flags, int, S_IRUGO);
module_param(sdio_voltage, int, S_IRUGO);
module_param(gpi_0_mode, int, S_IRUGO); // 0 = level triggered 1 = edge
                                        // triggered
module_param(gpi_0_level, int, S_IRUGO);        // 0 = low->high or low 1 =
                                                // high->low or high
module_param(gpi_1_mode, int, S_IRUGO); // 0 = level triggered 1 = edge
                                        // triggered 
module_param(gpi_1_level, int, S_IRUGO);        // 0 = low->high or low 1 =
                                                // high->low or high 
module_param(intmode, int, S_IRUGO);
module_param(gpiopin, int, S_IRUGO);

// prototypes
// for filling the "/proc/mrvsdio" entry
int mrvlsdio_read_procmem(char *buf, char **start, off_t offset, int count,
                          int *eof, void *data);
int sd_match_device(sd_driver * drv, PSD_DEVICE_DATA dev);

// static PSD_DEVICE_DATA gSD_dev_data = NULL;

struct list_head sd_dev_list;
spinlock_t sd_dev_list_lock;
unsigned long sd_dev_list_lock_flags;

struct list_head sd_client_list;
spinlock_t sd_client_list_lock;
unsigned long sd_client_list_lock_flags;

static int SDIOThread(void *data);

static int probe(struct pci_dev *dev, const struct pci_device_id *id);
static void remove(struct pci_dev *dev);

static DECLARE_COMPLETION(on_exit);

// kernel 2.6

static struct pci_device_id ids[] = {
    {PCI_DEVICE(PCI_VENDOR_ID_SYSKONNECT, 0x8000),},
    {0,}
};

MODULE_DEVICE_TABLE(pci, ids);

/*
 * The fops
 */

struct file_operations mrvlsdio_fops = {
  llseek:NULL,
  read:NULL,
  write:NULL,
  ioctl:SDIOBus_IoCtl,
  mmap:NULL,
  open:mrvlsdio_open,
  release:mrvlsdio_release,
};

/*
 * Open and close
 */

int
mrvlsdio_open(struct inode *inode, struct file *filp)
{
    ENTER();

    if (list_empty(&sd_dev_list)) {
        DBGPRINT(DBG_LOAD,
                 (KERN_DEBUG
                  "mrvlsdio_open: sd_dev_list is empty -> Adapter not in CardBus slot!\n"));
        return -ENODEV;
    }

    filp->f_op = &mrvlsdio_fops;

    // Pointer to first device in list
    // This works ONLY with one CardBus slot and one SDIO2PCI Adapter !!!
    // With more SDIO2PCI Adapter we need to implement virtual devices
    filp->private_data = sd_dev_list.next;

    LEAVE();
    return 0;                   /* success */
}

int
mrvlsdio_release(struct inode *inode, struct file *filp)
{
    ENTER();
    LEAVE();
    return 0;
}

/*
 * The ioctl() implementation
 */

int
SDIOBus_IoCtl(struct inode *inode, struct file *filp,
              unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    UCHAR ucValue;
    ULONG ulValue;
    ULONG cnt;
    PSD_DEVICE_DATA sd_dev;

    ENTER();

    sd_dev = (SD_DEVICE_DATA *) filp->private_data;
    if (sd_dev == NULL) {
        DBGPRINT(DBG_LOAD,
                 (KERN_DEBUG "SDIOBus_IoCtl : sd_dev= 0x%p !!!!!!!!!!!!!!!!\n",
                  sd_dev));
        return -ENOTTY;
    }

    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG
              "SDIOBus_IoCtl : sd_dev= 0x%p sd_dev->IOBase[0]= 0x%p\n", sd_dev,
              sd_dev->IOBase[0]));

    switch (cmd) {
    case IOCTL_SDIOBUSENUM_SET_VOLTAGE:
        {
            ret = get_user(ucValue, (int *) arg);
            DBGPRINT(DBG_LOAD,
                     (KERN_DEBUG
                      "SDIOBus_IoCtl : IOCTL_SDIOBUSENUM_SET_VOLTAGE: 0x%2.02X\n",
                      ucValue));
            MEM_WRITE_UCHAR(sd_dev, SK_SLOT_0, FPGA_POWER_REG_DATA, ucValue);
            MEM_WRITE_UCHAR(sd_dev, SK_SLOT_0, FPGA_POWER_REG_CMD,
                            FPGA_POWER_REG_CMD_START);
            DBGPRINT(DBG_LOAD, (KERN_DEBUG "wait for stable voltage\n"));
            cnt = 0;
            do {
                MEM_READ_UCHAR(sd_dev, SK_SLOT_0, FPGA_POWER_REG_STATUS,
                               &ucValue);
                DBGPRINT(DBG_LOAD,
                         (KERN_DEBUG "PowerRegulatorControl: 0x%x\n", ucValue));
            } while (++cnt < 10000 && (ucValue & FPGA_POWER_REG_STABLE) == 0);
            DBGPRINT(DBG_LOAD,
                     ("IOCTL_SDIOBUSENUM_SET_VOLTAGE: cnt=%d\n", cnt));
            break;
        }
    case IOCTL_SDIOBUSENUM_GET_BOARDREV:
        {
            MEM_READ_ULONG(sd_dev, SK_SLOT_0, FPGA_CARD_REVISION, &ulValue);

            ret = put_user(ulValue, (int *) arg);
            DBGPRINT(DBG_LOAD,
                     (KERN_DEBUG
                      "SDIOBus_IoCtl : IOCTL_SDIOBUSENUM_GET_BOARDREV 0x%8.08X\n",
                      ulValue));
            break;
        }
    case IOCTL_SDIOBUSENUM_GET_JUMPER:
        {
            DBGPRINT(DBG_LOAD, ("IOCTL_SDIOBUSENUM_GET_JUMPER\n"));
            MEM_READ_ULONG(sd_dev, SK_SLOT_0, FPGA_CARD_REVISION, &ulValue);
            ret = put_user(ulValue, (int *) arg);
            DBGPRINT(DBG_LOAD,
                     (KERN_DEBUG
                      "SDIOBus_IoCtl : IOCTL_SDIOBUSENUM_GET_JUMPER 0x%8.08X\n",
                      ulValue));
            break;
        }

    case IOCTL_SDIOBUSENUM_SET_BUSTYPE:
        {
            DBGPRINT(DBG_LOAD,
                     (KERN_DEBUG
                      "SDIOBus_IoCtl : IOCTL_SDIOBUSENUM_SET_BUSTYPE --> IGNORED!\n"));
            break;
        }
    case IOCTL_SDIOBUSENUM_SET_BUSWIDTH:
        {
            DBGPRINT(DBG_LOAD,
                     (KERN_DEBUG
                      "SDIOBus_IoCtl : IOCTL_SDIOBUSENUM_SET_BUSWIDTH --> IGNORED!\n"));
            break;
        }
    case IOCTL_SDIOBUSENUM_SET_CLKSPEED:
        {
            DBGPRINT(DBG_LOAD,
                     (KERN_DEBUG
                      "SDIOBus_IoCtl : IOCTL_SDIOBUSENUM_SET_CLKSPEED --> IGNORED!\n"));
            break;
        }
    default:
        {
            DBGPRINT(DBG_LOAD,
                     (KERN_DEBUG "SDIOBus_IoCtl : invalid cmd=%d (0x%8.08X)!\n",
                      cmd, cmd));
            return -ENOTTY;
        }
    }

    LEAVE();
    return ret;

}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void
card_out_work(PSD_DEVICE_DATA pDev)
#else
static void
card_out_work(struct work_struct *work)
#endif
{
    int j;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19)
    PSD_DEVICE_DATA pDev = container_of(work, SD_DEVICE_DATA, card_out_work);
#endif

    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "%s: CARD_REMOVE_EVENT received.\n", __FUNCTION__));
    for (j = 1; j <= pDev->number_of_functions; j++) {
        if (down_interruptible(&sd_client_sema))
            return;
        if (pDev->sd_dev[j].drv != NULL && pDev->sd_dev[j].drv->remove != NULL) {
            if (pDev->sd_dev[j].remove_called == 0) {
                DBGPRINT(DBG_LOAD | DBG_ERROR,
                         (KERN_DEBUG "%s: call remove() handler on fn=%d\n",
                          __FUNCTION__, j));

                pDev->sd_dev[j].probe_called = 0;
                pDev->sd_dev[j].remove_called = 1;
                pDev->sd_dev[j].dev = NULL;
                pDev->sd_dev[j].drv->remove(&pDev->sd_dev[j]);
            }
        } else {
            // If we have no remove() handler at this point, we will never get
            // one. 
            // So discard the CARD_REMOVE_EVENT to avoid an endless loop.
            // This situation is considered a bug in the client driver !!!!
            DBGPRINT(DBG_LOAD,
                     (KERN_DEBUG
                      "%s: no remove() handler installed for fn=%d! Revise client driver!!!\n",
                      __FUNCTION__, j));
        }
        up(&sd_client_sema);
    }

}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void
card_irq_work(PSD_DEVICE_DATA pDev)
#else
static void
card_irq_work(struct work_struct *work)
#endif
{
    SK_U32 irq_handler_called;
    SK_U8 int_pending;
    SK_U32 ix;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19)
    PSD_DEVICE_DATA pDev = container_of(work, SD_DEVICE_DATA, irq_work);
#endif

    irq_handler_called = 0;

    // Read function specific interrupt information
    GET_IF_SEMA_NO_RC();
    if (SDHost_CMD52_Read(pDev, INT_PENDING_REG, 0, &int_pending)) {
        REL_IF_SEMA();
        DBGPRINT(DBG_IRQ,
                 (KERN_DEBUG "INT Pending = 0x%2.02X\n ", int_pending));

        for (ix = 1; ix <= 7; ix++) {
            if (int_pending & INT(ix)) {
                // Function ix interrupt is pending and enabled
                if (pDev->irq_device_cache[ix] != NULL) {
                    DBGPRINT(DBG_IRQ,
                             (KERN_DEBUG
                              "Call registered interrupt handler for function %d.\n",
                              ix));
                    irq_handler_called++;
                    if (pDev->irq_device_cache[ix]->functions[ix].int_handler !=
                        NULL) {
                        pDev->irq_device_cache[ix]->functions[ix].
                            int_handler(pDev->irq_device_cache[ix],
                                        pDev->irq_device_cache[ix]->
                                        pCurrent_Ids,
                                        pDev->irq_device_cache[ix]->
                                        functions[ix].context);
                    }
                }
            }
        }
    } else
        REL_IF_SEMA();

    if (irq_handler_called == 0) {
        DBGPRINT(DBG_IRQ,
                 (KERN_DEBUG "No interrupt handler registered. (sd_dev=%p)\n",
                  pDev));
        SDHost_EnableInterrupt(pDev, pDev->lastIRQSignalMask);
    }

}

static struct pci_driver mrvlsdio = {
    .name = "mrvlsdio",
    .id_table = ids,
    .probe = probe,
    .remove = remove,
};

static int
probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    int i, ret;
    SK_U8 ucValue;
    SK_U32 mem;
    SK_U32 ulValue;
    PSD_DEVICE_DATA pTmpDev;

    DBGPRINT(DBG_LOAD, (KERN_DEBUG "mrvlsdio : probe()\n"));

    if (pci_enable_device(dev)) {
        DBGPRINT(DBG_LOAD,
                 (KERN_DEBUG "mrvlsdio: pci_enable_device() failed\n"));
        return -EIO;
    }
    if (pci_set_dma_mask(dev, 0xffffffff)) {
        DBGPRINT(DBG_LOAD,
                 (KERN_DEBUG "mrvlsdio: 32-bit PCI DMA not supported"));
        pci_disable_device(dev);
        return -EIO;
    }
    pci_set_master(dev);

                                                                                                                                                                                /*--- mmoser 6/21/2007 ---*/
    pTmpDev = (PSD_DEVICE_DATA) kmalloc(sizeof(SD_DEVICE_DATA), GFP_KERNEL);

    if (pTmpDev == NULL) {
        return -ENOMEM;
    }
#ifdef SDIO_MEM_TRACE
    printk(">>> kmalloc(SD_DEVICE_DATA)\n");
#endif

    memset(pTmpDev, 0, sizeof(SD_DEVICE_DATA));

    pTmpDev->IOBaseIx = 0;
                                                                                                                                                                                /*--- mmoser 10/11/2006 ---*/
    pTmpDev->dev = dev;
    pTmpDev->workqueue = create_workqueue("MRVL-SDIO");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
    INIT_WORK(&pTmpDev->irq_work, (void (*)(void *)) card_irq_work, pTmpDev);
    INIT_WORK(&pTmpDev->card_out_work, (void (*)(void *)) card_out_work,
              pTmpDev);
#else
    INIT_WORK(&pTmpDev->irq_work, card_irq_work);
    INIT_WORK(&pTmpDev->card_out_work, card_out_work);
#endif

    if (pci_request_regions(dev, "MRVL-SDIO")) {
        DBGPRINT(DBG_LOAD,
                 (KERN_DEBUG "mrvlsdio: pci_request_regions() failed.\n"));
        pci_disable_device(dev);
        return -EIO;
    }

    for (i = 0; i < 5; i++) {
        mem = pci_resource_start(dev, i);

        if (mem != 0) {
            if (IORESOURCE_MEM & pci_resource_flags(dev, i)) {
                pTmpDev->IOBaseLength[pTmpDev->IOBaseIx] =
                    pci_resource_end(dev, i) - mem + 1;
                pTmpDev->IOBase[pTmpDev->IOBaseIx] =
                    ioremap_nocache(mem,
                                    pTmpDev->IOBaseLength[pTmpDev->IOBaseIx]);
                DBGPRINT(DBG_LOAD,
                         (KERN_DEBUG
                          "mrvlsdio: IOBase[%d] = 0x%8.08X IOBaseLength = %u\n",
                          i, (SK_U32) pTmpDev->IOBase[pTmpDev->IOBaseIx],
                          pTmpDev->IOBaseLength[pTmpDev->IOBaseIx]));
                pTmpDev->IOBaseIx++;
                if (pTmpDev->IOBaseIx >= 2) {
                    DBGPRINT(DBG_LOAD,
                             (KERN_DEBUG
                              "mrvlsdio : Too many memory address ranges!\n"));
                    pTmpDev->IOBaseIx = 0;
                }
            }
        }
    }

    // Dummy read. Function return SUCCESS but value is messy !!!
    pci_read_config_byte(dev, PCI_INTERRUPT_LINE,
                         (SK_U8 *) & pTmpDev->Interrupt);

    for (i = 0; i < 3; i++) {
        ret =
            pci_read_config_byte(dev, PCI_INTERRUPT_LINE,
                                 (SK_U8 *) & pTmpDev->Interrupt);
        if (ret) {
            DBGPRINT(DBG_LOAD,
                     (KERN_DEBUG
                      "mrvlsdio: Failed to retrieve Interrupt Line (%d) %d ret=%d\n",
                      i, pTmpDev->Interrupt, ret));
            UDELAY(100);
            continue;
        } else {
            break;
        }
    }
    if (i >= 3) {
        DBGPRINT(DBG_LOAD,
                 (KERN_DEBUG
                  "mrvlsdio: Failed to retrieve Interrupt Line (%d) ret=%d\n",
                  i, ret));
        return -ENODEV;
    }
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "mrvlsdio: Interrupt Line = %d  (%d)\n",
              pTmpDev->Interrupt, i));

    MEM_READ_UCHAR(pTmpDev, SK_SLOT_0, FPGA_CARD_REVISION, &ucValue);
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "mrvlsdio: CardRevision: 0x%X (sd_dev_data=0x%p)\n",
              ucValue, &pTmpDev));

    pTmpDev->bus_type = bus_type;
    pTmpDev->bus_width = bus_width;
    pTmpDev->ClockSpeed = clock_speed;
    pTmpDev->debug_flags = debug_flags;
    pTmpDev->sdio_voltage = sdio_voltage;

    pci_read_config_byte(dev, 0x0c, &ucValue);

    if (ucValue == 0)
        pci_write_config_byte(dev, 0x0c, 2);

    pci_read_config_byte(dev, 0x0c, &ucValue);
// printk("config[0x0c]=0x%2.02X\n",ucValue);
    pci_read_config_byte(dev, 0x0d, &ucValue);
    // printk("config[0x0d]=0x%2.02X\n",ucValue);
    printk("debug_flags=0x%8.08X\n", debug_flags);

    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "mrvlsdio: bus_type     : SDIO%s\n",
              bus_type == SDIO_BUS_TYPE ? "" : "/SPI"));
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "mrvlsdio: bus_width    : %s\n",
              bus_width == SDIO_4_BIT ? "4-bit" : "1-bit"));
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "mrvlsdio: block_size   : %d\n", block_size));
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "mrvlsdio: clock_speed  : %d\n", clock_speed));
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "mrvlsdio: debug_flags  : 0x%8.08X\n", debug_flags));
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "mrvlsdio: sdio_voltage : %d\n", sdio_voltage));

                                                                                                                                                                        /*--- mmoser 3/12/2007 ---*/
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "mrvlsdio: gpi_0_mode   : %s triggered\n",
              gpi_0_mode == 0 ? "level" : "edge"));
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "mrvlsdio: gpi_0_level  : %s\n",
              gpi_0_level == 0 ? (gpi_0_mode ==
                                  0 ? "low" : "low->high") : (gpi_0_mode ==
                                                              0 ? "high" :
                                                              "high->low")));
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "mrvlsdio: gpi_1_mode   : %s triggered\n",
              gpi_1_mode == 0 ? "level" : "edge"));
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "mrvlsdio: gpi_1_level  : %s\n",
              gpi_1_level == 0 ? (gpi_1_mode ==
                                  0 ? "low" : "low->high") : (gpi_1_mode ==
                                                              0 ? "high" :
                                                              "high->low")));

    // mmoser 2005-11-29
    atomic_set(&pTmpDev->card_add_event, 0);
    atomic_set(&pTmpDev->card_remove_event, 0);

                                                                                                                                                                        /*--- mmoser 8/29/2006 ---*/
    init_waitqueue_head(&pTmpDev->trans_complete_evt.wq);
    init_waitqueue_head(&pTmpDev->cmd_complete_evt.wq);
    init_waitqueue_head(&pTmpDev->thread_started_evt.wq);

                                                                                                                                                                        /*--- mmoser 3/12/2007 ---*/
    MEM_READ_ULONG(pTmpDev, SK_SLOT_0, 0x200, &ulValue);
    ulValue &= 0x0FFFFFFF;
    ulValue |=
        ((gpi_1_mode << 3 | gpi_1_level << 2 | gpi_0_mode << 1 | gpi_0_level) <<
         28);
//      printk("mrvlsdio: ulValue = 0x%8.08X\n",ulValue);
    MEM_WRITE_ULONG(pTmpDev, SK_SLOT_0, 0x200, ulValue);

//      printk("mrvlsdio: reg[0x200] = 0x%8.08X\n",ulValue);

    spin_lock_init(&pTmpDev->sd_dev_lock);

    spin_lock_irqsave(&sd_dev_list_lock, sd_dev_list_lock_flags);
    list_add_tail((struct list_head *) pTmpDev, &sd_dev_list);

    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG
              "%s@%d: sd_dev_list=%p sd_dev_list.prev=%p sd_dev_list.next=%p pTmpDev=%p\n",
              __FUNCTION__, __LINE__, &sd_dev_list, sd_dev_list.prev,
              sd_dev_list.next, pTmpDev));

    spin_unlock_irqrestore(&sd_dev_list_lock, sd_dev_list_lock_flags);

    SDHost_Init(pTmpDev);

    // mmoser 2005-11-29
    if (0 != request_irq(pTmpDev->Interrupt,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
                         SDIOBus_Isr, SA_SHIRQ,
#else
                         (irq_handler_t) SDIOBus_Isr, IRQF_SHARED,
#endif
                         MRVL_DEVICE_NAME, (PVOID) pTmpDev)
        ) {
        DBGPRINT(DBG_LOAD,
                 (KERN_DEBUG
                  "mrvlsdio: Failed to register interrupt handler\n"));
        return -ENODEV;
    }

    if (SDHost_isCardPresent(pTmpDev) == SK_TRUE) {
        if (SDIOBus_CardPluggedIn(pTmpDev) == SK_TRUE) {
            pTmpDev->CardIn = SK_TRUE;
        }
        SDHost_EnableInterrupt(pTmpDev, (STDHOST_NORMAL_IRQ_CARD_OUT_SIG_ENA |
                                         STDHOST_NORMAL_IRQ_CARD_IN_SIG_ENA));
    }
#ifdef SYSKT_DMA_MALIGN_TEST
    pTmpDev->dma_tx_malign = 0;
    pTmpDev->dma_rx_malign = 0;
    // pTmpDev->dma_start_malign = 4096 - 32;
    pTmpDev->dma_start_malign = 0;
#endif

// mmoser 2005-11-22 start the thread as the last action

// mmoser 2005-11-21
    pTmpDev->stop_thread = SK_FALSE;

    pTmpDev->thread_id = kernel_thread(SDIOThread,
                                       pTmpDev,
                                       (CLONE_FS | CLONE_FILES |
                                        CLONE_SIGHAND));

    if (pTmpDev->thread_id == 0) {
        DBGPRINT(DBG_LOAD,
                 (KERN_DEBUG "mrvlsdio: Failed to start kernel thread!\n"));
        return -1;
    } else {

        if (SDHost_wait_event(pTmpDev, &pTmpDev->thread_started_evt, 1000)) {
            pTmpDev->stop_thread = SK_TRUE;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
            kill_pid(find_get_pid(pTmpDev->thread_id), SIGTERM, 1);
#else
            kill_proc(pTmpDev->thread_id, SIGTERM, 1);
#endif
            pTmpDev->thread_id = 0;
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG
                      "%s @ %d: Wait SDIOThread started ---> FAILED !\n",
                      __FUNCTION__, __LINE__));
            return 0;
        }

    }

    return 0;
}

static void
remove(struct pci_dev *dev)
{
    struct list_head *pTmp;
    struct list_head *pNext;
    PSD_DEVICE_DATA pTmpDev;
    int j;
    SK_U8 ucValue;
    SK_U32 cnt;

    DBGPRINT(DBG_LOAD, (KERN_DEBUG "remove mrvlsdio ... \n"));

    /* clean up any allocated resources and stuff here. like call
       release_region(); */

    /* 
     *      iterate through the device list
     *      call remove() for each client driver
     *  remove device from list
     *  free allocated memory
     */
    if (!list_empty(&sd_dev_list)) {
        pTmp = sd_dev_list.next;

        while (pTmp != &sd_dev_list) {
            pNext = pTmp->next;
            pTmpDev = (PSD_DEVICE_DATA) pTmp;
            if (pTmpDev->dev == dev) {
                pTmpDev->isRemoving = SK_TRUE;
                for (j = 1; j <= pTmpDev->number_of_functions; j++) {
                    if (down_interruptible(&sd_client_sema))
                        return;
                    if (pTmpDev->sd_dev[j].drv != NULL &&
                        pTmpDev->sd_dev[j].drv->remove != NULL) {
                        if (pTmpDev->sd_dev[j].remove_called == 0) {
                            pTmpDev->sd_dev[j].probe_called = 0;
                            pTmpDev->sd_dev[j].remove_called = 1;
                            pTmpDev->sd_dev[j].dev = NULL;
                            pTmpDev->sd_dev[j].drv->remove(&pTmpDev->sd_dev[j]);
                        }
                    }
                    up(&sd_client_sema);
                }

                if (pTmpDev->thread_id) {
                    pTmpDev->stop_thread = SK_TRUE;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                    kill_pid(find_get_pid(pTmpDev->thread_id), SIGTERM, 1);
#else
                    kill_proc(pTmpDev->thread_id, SIGTERM, 1);
#endif
                    wait_for_completion(&on_exit);
                    DBGPRINT(DBG_LOAD,
                             (KERN_DEBUG "%s: SDIOThread killed ... \n",
                              __FUNCTION__));
                }
                // Send I/O Card Reset
                SDHost_CMD52_Write((PSD_DEVICE_DATA) pTmp, IO_ABORT_REG, FN0,
                                   RES);
                SDHost_DisableInterrupt((PSD_DEVICE_DATA) pTmp,
                                        STDHOST_NORMAL_IRQ_ALL_SIG_ENA);
                SDHost_SetClock((PSD_DEVICE_DATA) pTmp, 0);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
                cancel_delayed_work(&pTmpDev->card_out_work);
                cancel_delayed_work(&pTmpDev->irq_work);
#endif
                flush_scheduled_work();
                destroy_workqueue(pTmpDev->workqueue);
                pTmpDev->workqueue = NULL;

                // set host power voltage as low as possible
                // (still need despite "turn off host power" below, b/c
                // hotplug can reactivate host power independently in HW)
                MEM_WRITE_UCHAR((PSD_DEVICE_DATA) pTmp, SK_SLOT_0,
                                FPGA_POWER_REG_DATA, FPGA_POWER_REG_0_7_V);
                MEM_WRITE_UCHAR((PSD_DEVICE_DATA) pTmp, SK_SLOT_0,
                                FPGA_POWER_REG_CMD, FPGA_POWER_REG_CMD_START);
                DBGPRINT(DBG_LOAD, (KERN_DEBUG "wait for stable voltage\n"));
                cnt = 0;
                do {
                    MEM_READ_UCHAR((PSD_DEVICE_DATA) pTmp, SK_SLOT_0,
                                   FPGA_POWER_REG_STATUS, &ucValue);
                    DBGPRINT(DBG_LOAD,
                             (KERN_DEBUG "PowerRegulatorControl: 0x%x\n",
                              ucValue));
                } while (++cnt < 10000 &&
                         (ucValue & FPGA_POWER_REG_STABLE) == 0);
                DBGPRINT(DBG_LOAD, ("REMOVE_SET_VOLTAGE: cnt=%d\n", cnt));

                // turn off host power
                MEM_READ_UCHAR((PSD_DEVICE_DATA) pTmp, SK_SLOT_0,
                               STDHOST_POWER_CTRL, &ucValue);
                MEM_WRITE_UCHAR((PSD_DEVICE_DATA) pTmp, SK_SLOT_0,
                                STDHOST_POWER_CTRL,
                                ucValue & ~STDHOST_POWER_ON);

                // give some time for host power off to settle
                mdelay(200);

                free_irq(pTmpDev->Interrupt, pTmpDev);
                for (j = 0; j < pTmpDev->IOBaseIx; j++)
                    iounmap(pTmpDev->IOBase[j]);
                                /*--- mmoser 10/11/2006 ---*/
                pci_release_regions(pTmpDev->dev);

                list_del((struct list_head *) pTmpDev);
                kfree(pTmpDev);

#ifdef SDIO_MEM_TRACE
                printk("<<< kfree(SD_DEVICE_DATA)\n");
#endif

                return;
            }
            pTmp = pNext;
        }
    }

}

BOOLEAN
SDIOBus_CardRemoved(PSD_DEVICE_DATA SdData)
{
    SK_U32 ulVal;

    ENTER();

#ifdef USE_DEBOUNCE_CARD_IN

                                                                                                                                                                        /*--- mmoser 12/21/2006 ---*/
    mdelay(MAX_DEBOUNCE_TIME);

    // Read present state register to check whether a SD card is present
    MEM_READ_ULONG(SdData, SK_SLOT_0, STDHOST_PRESENT_STATE, &ulVal);
    if ((ulVal & STDHOST_STATE_CARD_INSERTED) == 1) {
        // There is probably still a card in the socket
        DBGPRINT(DBG_W528D,
                 (KERN_DEBUG
                  "SDHost_isCardPresent(): Card is present. (%8.08X)\n",
                  ulVal));
        return SK_FALSE;
    }
#endif // USE_DEBOUNCE_CARD_IN

    SdData->CardIn = SK_FALSE;

    // mmoser 2005-11-29
    queue_work(SdData->workqueue, &SdData->card_out_work);

    LEAVE();
    return SK_TRUE;
}

BOOLEAN
SDIOBus_CardPluggedIn(PSD_DEVICE_DATA SdData)
{
    SK_U32 ulVal;

    ENTER();

#ifdef USE_DEBOUNCE_CARD_IN

                                                                                                                                                                        /*--- mmoser 12/21/2006 ---*/
    mdelay(MAX_DEBOUNCE_TIME);

    // Read present state register to check whether a SD card is present
    MEM_READ_ULONG(SdData, SK_SLOT_0, STDHOST_PRESENT_STATE, &ulVal);
    if ((ulVal & STDHOST_STATE_CARD_INSERTED) == 0) {
        // There is probably no card in the socket
        DBGPRINT(DBG_W528D,
                 (KERN_DEBUG
                  "SDHost_isCardPresent(): Card is NOT present. (%8.08X)\n",
                  ulVal));
        return SK_FALSE;
    }
#endif // USE_DEBOUNCE_CARD_IN

    SDHost_Init(SdData);

    if (!SDHost_InitializeCard(SdData)) {
        SDHost_EnableInterrupt(SdData, SdData->lastIRQSignalMask);
        return SK_FALSE;
    }
    SdData->CardIn = SK_TRUE;
    DBGPRINT(DBG_LOAD, (KERN_DEBUG "Device Name : %s\n", SdData->DeviceName));

    // mmoser 2005-11-29
    atomic_inc(&SdData->card_add_event);
    LEAVE();
    return SK_TRUE;
}

static int
SDIOThread(void *data)
{
    SK_U16 usVal;
    SK_U32 card_in = 0;
    SK_U32 card_out = 0;
    PSD_DEVICE_DATA pDev;
    int clients_found;
    int fn = 0;
    int j;
    struct list_head *pTmp;
    PSD_CLIENT pClientTmp;
    int fn_mask;
    u8 attached_fn[MAX_SDIO_FUNCTIONS];

    daemonize("SDIOThread");
    allow_signal(SIGTERM);

    DBGPRINT(DBG_LOAD, (KERN_DEBUG "SDIOThread() started ... \n"));

    if (data == NULL) {
        DBGPRINT(DBG_LOAD, (KERN_DEBUG "SDIOThread() data == NULL !\n"));
        complete_and_exit(&on_exit, 0);
        return 1;
    }
    pDev = (PSD_DEVICE_DATA) data;

    LED_OFF(4);

    if (!test_and_set_bit(0, &pDev->thread_started_evt.event)) {
        wake_up(&pDev->thread_started_evt.wq);
    }

    while (pDev != NULL && !pDev->stop_thread) {
        LED_ON(4);

// mmoser 2005-11-21
        // Read the Host Controller Version to check if SD Host adapter is
        // available
        MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_HOST_CONTROLLER_VERSION,
                        &usVal);
        if (usVal == 0xFFFF) {
            // There is probably no SD Host Adapter available
            printk("%s: SD Host Adapter is NOT present.\n", __FUNCTION__);
            pDev->SurpriseRemoved = SK_TRUE;
            pDev->CardIn = SK_FALSE;
            pDev->thread_id = 0;
            DBGPRINT(DBG_LOAD, (KERN_DEBUG "SDIOThread terminated ... \n"));
            complete_and_exit(&on_exit, 0);
            return -ENODEV;
        }
// mmoser 2005-11-29
        card_in = atomic_read(&pDev->card_add_event);
        card_out = atomic_read(&pDev->card_remove_event);

        if (card_in == 0 && card_out == 0) {
            LED_OFF(4);
                                                                                                                                                                        /*--- mmoser 6/21/2006 ---*/
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(10);
            continue;
        }
        DBGPRINT(DBG_LOAD,
                 (KERN_DEBUG "%s: card_in=%d card_out=%d\n", __FUNCTION__,
                  card_in, card_out));

        // mmoser 2005-11-29
        if (card_in > 0) {

            DBGPRINT(DBG_LOAD,
                     (KERN_DEBUG "%s: CARD_ADD_EVENT received.\n",
                      __FUNCTION__));

            // Check whether we have a client driver in the global list for
            // this device
            if (!list_empty(&sd_client_list)) {
                spin_lock_irqsave(&sd_client_list_lock,
                                  sd_client_list_lock_flags);

                fn = pDev->number_of_functions + 1;

                clients_found = 0;

                // walk through the list of client drivers
                list_for_each(pTmp, &sd_client_list) {
                    pClientTmp = (PSD_CLIENT) pTmp;
                    if ((fn_mask = sd_match_device(pClientTmp->drv, pDev)) > 0) {
                        // Matching client driver found
                        // Assign the client driver to the lowest supported
                        // function
                        for (fn = 1; fn <= pDev->number_of_functions; fn++) {
                            if ((fn_mask & (1 << fn)) != 0) {
                                pDev->sd_dev[fn].drv = pClientTmp->drv;
                                pDev->sd_dev[fn].supported_functions = fn_mask;
                                pDev->sd_dev[fn].cisptr = &pDev->cisptr[0];
                                pDev->sd_dev[fn].sd_bus = pDev; // Backpointer
                                                                // to bus
                                                                // driver's
                                                                // device
                                                                // structure
                                pDev->sd_dev[fn].dev = &pDev->dev->dev; // Generic 
                                                                        // device 
                                                                        // interface 
                                                                        // for
                                                                        // hotplug
                                attached_fn[clients_found] = fn;
                                clients_found++;
                                break;
                            }
                        }
                    }
                }

                spin_unlock_irqrestore(&sd_client_list_lock,
                                       sd_client_list_lock_flags);

                if (clients_found) {
                    // Call probe() for each previously attached client driver
                    for (j = 0; j < clients_found; j++) {
                        fn = attached_fn[j];
                        if (pDev->sd_dev[fn].drv->probe != NULL) {
                            if (pDev->sd_dev[fn].probe_called == 0) {
                                DBGPRINT(DBG_LOAD | DBG_ERROR,
                                         (KERN_DEBUG
                                          "%s: call probe() handler on fn=%d\n",
                                          __FUNCTION__, fn));

                                pDev->SurpriseRemoved = 0;
                                pDev->sd_dev[fn].probe_called = 1;
                                pDev->sd_dev[fn].remove_called = 0;
                                call_client_probe(pDev, fn);
                            }
                        } else {
                            // If we have no probe() handler at this point, we
                            // will never get one. 
                            // So discard the CARD_ADD_EVENT to avoid an
                            // endless loop.
                            // This situation is considered a bug in the client 
                            // driver !!!!
                            DBGPRINT(DBG_LOAD,
                                     (KERN_DEBUG
                                      "%s@%d: no probe() handler installed for fn=%d! Revise client driver!!!\n",
                                      __FUNCTION__, __LINE__, fn));
                        }
                    }
                } else {
                    // There is at least one client driver registered with our
                    // bus driver
                    // but the sd_device_ids of this driver does not match the
                    // ids of the inserted card.
                    // We have to wait until a matching driver will be
                    // installed.

                    DBGPRINT(DBG_LOAD,
                             (KERN_DEBUG
                              "%s@%d: no matching client driver installed.\n",
                              __FUNCTION__, __LINE__));
                }
            } else {
                // Until there is no client driver registered with our bus
                // driver.

                DBGPRINT(DBG_LOAD,
                         (KERN_DEBUG "%s@%d: no client driver installed.\n",
                          __FUNCTION__, __LINE__));
            }

            // We can reset the card event here since the association with a
            // matching client driver
            // is already done or will be done in sd_register_driver()
            atomic_set(&pDev->card_add_event, 0);

        }
        // mmoser 2005-11-29
        else if (card_out > 0) {
            DBGPRINT(DBG_LOAD,
                     (KERN_DEBUG "%s: CARD_REMOVE_EVENT received.\n",
                      __FUNCTION__));
            for (j = 1; j <= pDev->number_of_functions; j++) {
                if (down_interruptible(&sd_client_sema))
                    return 0;
                if (pDev->sd_dev[j].drv != NULL &&
                    pDev->sd_dev[j].drv->remove != NULL) {
                    if (pDev->sd_dev[j].remove_called == 0) {
                        DBGPRINT(DBG_LOAD | DBG_ERROR,
                                 (KERN_DEBUG
                                  "%s: call remove() handler on fn=%d\n",
                                  __FUNCTION__, j));

                        pDev->sd_dev[j].probe_called = 0;
                        pDev->sd_dev[j].remove_called = 1;
                        pDev->sd_dev[j].dev = NULL;
                        pDev->sd_dev[j].drv->remove(&pDev->sd_dev[j]);
                    }
                } else {
                    // If we have no remove() handler at this point, we will
                    // never get one. 
                    // So discard the CARD_REMOVE_EVENT to avoid an endless
                    // loop.
                    // This situation is considered a bug in the client driver
                    // !!!!
                    DBGPRINT(DBG_LOAD,
                             (KERN_DEBUG
                              "%s: no remove() handler installed for fn=%d! Revise client driver!!!\n",
                              __FUNCTION__, j));
                }
                up(&sd_client_sema);
            }
            atomic_set(&pDev->card_remove_event, 0);
        } else {
            DBGPRINT(DBG_LOAD, (KERN_DEBUG "SDIOThread terminated ... \n"));
            pDev->thread_id = 0;
            complete_and_exit(&on_exit, 0);
            return 1;
        }
    }

    DBGPRINT(DBG_LOAD, (KERN_DEBUG "SDIOThread terminated ... \n"));

    if (pDev != NULL) {
        pDev->thread_id = 0;
    }

    complete_and_exit(&on_exit, 0);

    return 0;
}

/*************************  New SDIO Bus Driver API   ************************/

/** 
 *  @brief This function matches a client driver with a SDIO device
 *  @param drv		pointer to client drivers device IDs
 *  @param dev    pointer to SDIO device
 *  @return				mask with supported functions  -> 0 means no match found
 * 
 * 								76543210
 * 								||||||+-- fn 1 supported
 * 								|||||+--- fn 2 supported
 * 								||||+---- fn 3 supported
 * 								|||+----- fn 4 supported
 * 								||+------ fn 5 supported
 * 								|+------- fn 6 supported
 * 								+-------- fn 7 supported
 * 
 * 								0x02  -> fn 1 supported
 * 								0x06  -> fn 1 and 2 supported
 * 								0x12  -> fn 1 and 4 supported
*/
int
sd_match_device(sd_driver * drv, PSD_DEVICE_DATA dev)
{
    psd_device_id pIds;
    int i;
    int matched_fn = 0;
    int fn_mask = 0;

    DBGPRINT(DBG_API,
             (KERN_DEBUG
              "%s: drv-name: %s device: vendor=0x%4.04X device=0x%4.04X\n",
              __FUNCTION__, drv->name, dev->sd_ids[0].vendor,
              dev->sd_ids[0].device));

    for (pIds = drv->ids; pIds->device != 0 ||
         pIds->vendor != 0 || pIds->class != 0 || pIds->fn != 0; pIds++) {
        DBGPRINT(DBG_API,
                 (KERN_DEBUG
                  "vendor=0x%4.04X device=0x%4.04X class=0x%4.04X fn=0x%4.04X\n",
                  pIds->vendor, pIds->device, pIds->class, pIds->fn));
        if ((pIds->vendor != SD_VENDOR_ANY) &&
            (pIds->vendor != dev->sd_ids[0].vendor)) {
            continue;
        }
        if (pIds->device != SD_DEVICE_ANY) {
            matched_fn = 0;
            for (i = 0; i <= dev->number_of_functions; i++) {
                if (pIds->device == dev->sd_ids[i].device) {
                    matched_fn = i;
                    fn_mask |= (1 << matched_fn);
                    DBGPRINT(DBG_API,
                             (KERN_DEBUG "driver matches with function %d\n",
                              i));
                    break;
                }
            }
            if (matched_fn == 0) {
                continue;
            }
        }
        if (pIds->class != SD_CLASS_ANY) {
            matched_fn = 0;
            for (i = 1; i <= dev->number_of_functions; i++) {
//                              printk("fn[%d] class-id=0x%4.04X\n",i,dev->sd_ids[i].class);
                if (pIds->class == dev->sd_ids[i].class) {
                    matched_fn = i;
                    fn_mask |= (1 << matched_fn);
                    DBGPRINT(DBG_API,
                             (KERN_DEBUG "driver matches with function %d\n",
                              i));
                    break;
                }
            }
            if (matched_fn == 0) {
                continue;
            }
        } else {
            // At this point vendor==ANY && device==ANY && class==ANY
            // Now we have to check whether the card supports the desired
            // function number
            if (pIds->fn > 0 && pIds->fn <= dev->number_of_functions) {
                matched_fn = pIds->fn;
                fn_mask |= (1 << matched_fn);
            }
        }

        if ((pIds->fn != SD_FUNCTION_ANY) && (pIds->fn != matched_fn)) {
            continue;
        }
    }

    printk("%s: supported function mask = 0x%2.02X\n", __FUNCTION__, fn_mask);
    return (fn_mask);
}

/** 
 *  @brief This function registers a client driver with the bus driver using device IDs
 *  @param drv		pointer to client drivers device IDs and call backs
 *  @return				0 : Success < 0: Failed  
*/
int
sd_driver_register(sd_driver * drv)
{
    struct list_head *pTmp;
    PSD_CLIENT pClientTmp;
    PSD_DEVICE_DATA pDevTmp;
    int fn_mask;
    int fn;

    GET_IF_SEMA();

    ENTER();

    pClientTmp = (PSD_CLIENT) kmalloc(sizeof(SD_CLIENT), GFP_KERNEL);

    if (pClientTmp == NULL) {
        REL_IF_SEMA();
        LEAVE();
        return -ENOMEM;
    }
#ifdef SDIO_MEM_TRACE
    printk(">>> kmalloc(SD_CLIENT)\n");
#endif

    memset(pClientTmp, 0, sizeof(SD_CLIENT));

    pClientTmp->drv = drv;

    // Check whether there is a SDIO device available
    if (!list_empty(&sd_dev_list)) {
        list_for_each(pTmp, &sd_dev_list) {
            pDevTmp = (PSD_DEVICE_DATA) pTmp;

            spin_lock_irqsave(&pDevTmp->sd_dev_lock,
                              pDevTmp->sd_dev_lock_flags);
            if ((fn_mask = sd_match_device(drv, pDevTmp)) > 0) {
                // Assign the client driver to the lowest supported function
                for (fn = 1; fn <= pDevTmp->number_of_functions; fn++) {
                    if ((fn_mask & (1 << fn)) != 0) {
                        break;
                    }
                }
                if (fn <=
                    MIN(pDevTmp->number_of_functions, MAX_SDIO_FUNCTIONS - 1)) {
                    if (pDevTmp->sd_dev[fn].drv == NULL) {
                        pDevTmp->sd_dev[fn].drv = drv;
                        pDevTmp->sd_dev[fn].supported_functions = fn_mask;
                        pDevTmp->sd_dev[fn].cisptr = &pDevTmp->cisptr[0];
                        pDevTmp->sd_dev[fn].sd_bus = pDevTmp;   // Backpointer
                                                                // to bus
                                                                // driver's
                                                                // device
                                                                // structure
                        pDevTmp->sd_dev[fn].dev = &pDevTmp->dev->dev;   // Generic 
                                                                        // device 
                                                                        // interface 
                                                                        // for
                                                                        // hotplug

                        spin_unlock_irqrestore(&pDevTmp->sd_dev_lock,
                                               pDevTmp->sd_dev_lock_flags);
                        DBGPRINT(DBG_API,
                                 (KERN_DEBUG
                                  "%s: Driver registered: %s for functions 0x%2.02X\n",
                                  __FUNCTION__, pDevTmp->sd_dev[fn].drv->name,
                                  fn_mask));

                        if (pDevTmp->sd_dev[fn].drv->probe != NULL) {
                            pDevTmp->sd_dev[fn].probe_called = 1;
                            pDevTmp->sd_dev[fn].remove_called = 0;

                            DBGPRINT(DBG_LOAD | DBG_ERROR,
                                     (KERN_DEBUG
                                      "%s: call probe() handler on fn=%d\n",
                                      __FUNCTION__, fn));
                            REL_IF_SEMA();
                            call_client_probe(pDevTmp, fn);
                            GET_IF_SEMA();
                        } else {
                            // If we have no probe() handler at this point, we
                            // will never get one. 
                            // This situation is considered a bug in the client 
                            // driver !!!!
                            DBGPRINT(DBG_LOAD,
                                     (KERN_DEBUG
                                      "%s@%d: no probe() handler installed for fn=%d! Revise client driver!!!\n",
                                      __FUNCTION__, __LINE__, fn));
                        }
                        break;
                    } else {
                        spin_unlock_irqrestore(&pDevTmp->sd_dev_lock,
                                               pDevTmp->sd_dev_lock_flags);
                        continue;
                    }
                }
            } else {
                spin_unlock_irqrestore(&pDevTmp->sd_dev_lock,
                                       pDevTmp->sd_dev_lock_flags);
                continue;
            }

        }
        DBGPRINT(DBG_API,
                 (KERN_ERR
                  "%s: There are already drivers registered for all devices!\n",
                  __FUNCTION__));
    }
    // Add driver to global client list
    spin_lock_irqsave(&sd_client_list_lock, sd_client_list_lock_flags);
    list_add_tail((struct list_head *) pClientTmp, &sd_client_list);
    spin_unlock_irqrestore(&sd_client_list_lock, sd_client_list_lock_flags);

    REL_IF_SEMA();
    LEAVE();
    return 0;
}

/** 
 *  @brief This function unregisters a client driver from the bus driver
 *  @param drv		pointer to client drivers device IDs and call backs
 *  @return				0 : Success -1: Failed  
*/
int
sd_driver_unregister(sd_driver * drv)
{
    struct list_head *pTmp;
    PSD_CLIENT pClientTmp;
    PSD_DEVICE_DATA pDevTmp;
    int j;

    ENTER();

    if (drv == NULL) {
        printk("%s INVALID ARGS: drv==NULL!\n", __FUNCTION__);
        LEAVE();
        return -1;
    }

    if (down_interruptible(&sd_client_sema))
        return 0;
    // Delete any association with this client driver
    spin_lock_irqsave(&sd_dev_list_lock, sd_dev_list_lock_flags);
    if (!list_empty(&sd_dev_list)) {
        list_for_each(pTmp, &sd_dev_list) {
            pDevTmp = (PSD_DEVICE_DATA) pTmp;
            spin_unlock_irqrestore(&sd_dev_list_lock, sd_dev_list_lock_flags);
            for (j = 0; j < MAX_SDIO_FUNCTIONS; j++) {
                if (pDevTmp->sd_dev[j].drv == drv) {

                    DBGPRINT(DBG_API,
                             (KERN_DEBUG
                              "%s: Driver unregistered: %s on fn = %d\n",
                              __FUNCTION__, pDevTmp->sd_dev[j].drv->name, j));

                    if (drv != NULL && drv->remove != NULL) {
                        if (pDevTmp->sd_dev[j].remove_called == 0) {
                            pDevTmp->sd_dev[j].probe_called = 0;
                            pDevTmp->sd_dev[j].remove_called = 1;
                            if (pDevTmp->SurpriseRemoved == SK_TRUE)
                                pDevTmp->sd_dev[j].dev = NULL;
                            pDevTmp->sd_dev[j].drv->remove(&pDevTmp->sd_dev[j]);
                        }
                    }

                    spin_lock_irqsave(&pDevTmp->sd_dev_lock,
                                      pDevTmp->sd_dev_lock_flags);
                    pDevTmp->sd_dev[j].drv = NULL;
                    spin_unlock_irqrestore(&pDevTmp->sd_dev_lock,
                                           pDevTmp->sd_dev_lock_flags);
                }
            }
        }
        DBGPRINT(DBG_API,
                 (KERN_ERR "%s: Driver %s is not registered!\n", __FUNCTION__,
                  drv->name));
    } else {
        DBGPRINT(DBG_API,
                 (KERN_ERR "%s: SDIO Device List is empty!\n", __FUNCTION__));
        spin_unlock_irqrestore(&sd_dev_list_lock, sd_dev_list_lock_flags);
    }
    up(&sd_client_sema);

    GET_IF_SEMA();
    // Remove the client driver from global client list
    if (!list_empty(&sd_client_list)) {
        spin_lock_irqsave(&sd_client_list_lock, sd_client_list_lock_flags);

        list_for_each(pTmp, &sd_client_list) {
            pClientTmp = (PSD_CLIENT) pTmp;
            if (pClientTmp->drv != drv) {
                continue;
            }

            list_del(pTmp);
            kfree(pClientTmp);

#ifdef SDIO_MEM_TRACE
            printk("<<< kfree(SD_CLIENT)\n");
#endif

            break;
        }

        spin_unlock_irqrestore(&sd_client_list_lock, sd_client_list_lock_flags);
    } else {
        DBGPRINT(DBG_API,
                 (KERN_ERR "%s: SDIO Client Driver List is empty!\n",
                  __FUNCTION__));
        REL_IF_SEMA();
        LEAVE();
        return -1;
    }

    REL_IF_SEMA();
    LEAVE();
    return 0;
}

/** 
 *  @brief Register an interrupt handler with a card function
 *  @param dev				bus driver's device structure
 *  @param id					sd_device_ids (contains function number)
 *  @param function		interrupt handler
 *  @return				0 : Success -1: Failed  
*/
int
sd_request_int(sd_device * dev, sd_device_id * id, sd_function * function)
{
    PSD_DEVICE_DATA pDev;

    if (dev == NULL || id == NULL || function == NULL) {
        DBGPRINT(DBG_API,
                 (KERN_ERR "%s: missing parameters: dev=%p id=%p function=%p\n",
                  __FUNCTION__, dev, id, function));
        return -1;
    }

    if (id->fn < 1 || id->fn > 7) {
        DBGPRINT(DBG_API,
                 (KERN_ERR "%s: illegal function number: %d\n", __FUNCTION__,
                  id->fn));
        return -1;
    }
    if (dev->functions[id->fn].int_handler != NULL) {
        DBGPRINT(DBG_API,
                 (KERN_ERR
                  "%s: There is already an interrupt handler registered with function %d\n",
                  __FUNCTION__, id->fn));
        return -1;
    }

    GET_IF_SEMA();

    DBGPRINT(DBG_API,
             (KERN_DEBUG "%s: FN%d handler=%p context=%p\n", __FUNCTION__,
              id->fn, function->int_handler, function->context));

    pDev = (PSD_DEVICE_DATA) dev->sd_bus;

    SDHost_DisableInterrupt((PSD_DEVICE_DATA) dev->sd_bus,
                            STDHOST_NORMAL_IRQ_CARD_ALL_ENA);
    dev->functions[id->fn].int_handler = function->int_handler;
    dev->functions[id->fn].context = function->context;

    // cache the device to speed up irq response
    pDev->irq_device_cache[id->fn] = dev;
    SDHost_EnableInterrupt((PSD_DEVICE_DATA) dev->sd_bus,
                           pDev->lastIRQSignalMask);

    REL_IF_SEMA();
    return 0;
}

/** 
 *  @brief Remove an interrupt handler from a card function
 *  @param dev				bus driver's device structure
 *  @param id					sd_device_ids (contains function number)
 *  @return				0 : Success -1: Failed  
*/
int
sd_release_int(sd_device * dev, sd_device_id * id)
{
    PSD_DEVICE_DATA pDev;

    if (dev == NULL || id == NULL) {
        DBGPRINT(DBG_API,
                 (KERN_ERR "%s: missing parameters: dev=%p id=%p\n",
                  __FUNCTION__, dev, id));
        return -1;
    }
    if (id->fn < 1 || id->fn > 7) {
        DBGPRINT(DBG_API,
                 (KERN_ERR "%s: illegal function number: %d\n", __FUNCTION__,
                  id->fn));
        return -1;
    }

    GET_IF_SEMA();

    pDev = (PSD_DEVICE_DATA) dev->sd_bus;

    DBGPRINT(DBG_API, (KERN_DEBUG "%s: FN%d\n", __FUNCTION__, id->fn));

    SDHost_DisableInterrupt((PSD_DEVICE_DATA) dev->sd_bus,
                            STDHOST_NORMAL_IRQ_CARD_ALL_ENA);
    dev->functions[id->fn].int_handler = NULL;
    dev->functions[id->fn].context = NULL;

    pDev->irq_device_cache[id->fn] = NULL;
    SDHost_EnableInterrupt((PSD_DEVICE_DATA) dev->sd_bus,
                           pDev->lastIRQSignalMask);

    REL_IF_SEMA();
    return 0;
}

/** 
 *  @brief Enable interrupt of function id->fn
 *  @param dev				bus driver's device structure
 *  @param id					sd_device_ids (contains function number)
 *  @return				0 : Success -1: Failed  
*/
int
sd_unmask(sd_device * dev, sd_device_id * id)
{
    PSD_DEVICE_DATA pDev;
    if (dev == NULL || id == NULL) {
        DBGPRINT(DBG_API,
                 (KERN_ERR "%s: missing parameters: dev=%p id=%p\n",
                  __FUNCTION__, dev, id));
        return 1;
    }
    if (id->fn < 1 || id->fn > 7) {
        DBGPRINT(DBG_API,
                 (KERN_ERR "%s: illegal function number: %d\n", __FUNCTION__,
                  id->fn));
        return -1;
    }

    GET_IF_SEMA();

    pDev = dev->sd_bus;
    SDHost_EnableInterrupt(pDev, STDHOST_NORMAL_IRQ_CARD_ALL_ENA);
    REL_IF_SEMA();
    return 0;
}

/** 
 *  @brief Enable interrupt of function id->fn
 *  @param dev				bus driver's device structure
 *  @param id					sd_device_ids (contains function number)
 *  @return				0 : Success -1: Failed  
*/
int
sd_enable_int(sd_device * dev, sd_device_id * id)
{
    PSD_DEVICE_DATA pDev;
    SK_U8 R;

    if (dev == NULL || id == NULL) {
        DBGPRINT(DBG_API,
                 (KERN_ERR "%s: missing parameters: dev=%p id=%p\n",
                  __FUNCTION__, dev, id));
        return 1;
    }
    if (id->fn < 1 || id->fn > 7) {
        DBGPRINT(DBG_API,
                 (KERN_ERR "%s: illegal function number: %d\n", __FUNCTION__,
                  id->fn));
        return -1;
    }

    GET_IF_SEMA();

    pDev = dev->sd_bus;

    // Set global interrupt enable
    if (!SDHost_CMD52_Read(pDev, INT_ENABLE_REG, 0, &R)) {
        REL_IF_SEMA();
        return -1;
    }

    R |= (1 << id->fn);
    if (SDHost_CMD52_Write(pDev, INT_ENABLE_REG, 0, R)) {
        if (!SDHost_CMD52_Read(pDev, INT_ENABLE_REG, 0, &R)) {
            REL_IF_SEMA();
            return -1;
        }

        DBGPRINT(DBG_API,
                 (KERN_DEBUG "%s:FN0 : INT Enable Register: 0x%2.02X\n",
                  __FUNCTION__, R));
    } else {
        REL_IF_SEMA();
        return -1;
    }

    SDHost_EnableInterrupt(pDev, STDHOST_NORMAL_IRQ_CARD_ALL_ENA);
    REL_IF_SEMA();
    return 0;
}

/** 
 *  @brief Disable interrupt of function id->fn
 *  @param dev				bus driver's device structure
 *  @param id					sd_device_ids (contains function number)
 *  @return				0 : Success -1: Failed  
*/
int
sd_disable_int(sd_device * dev, sd_device_id * id)
{
    PSD_DEVICE_DATA pDev;
    SK_U8 R;

    if (dev == NULL || id == NULL) {
        DBGPRINT(DBG_API,
                 (KERN_ERR "%s: missing parameters: dev=%p id=%p\n",
                  __FUNCTION__, dev, id));
        return -1;
    }
    if (id->fn < 1 || id->fn > 7) {
        DBGPRINT(DBG_API,
                 (KERN_ERR "%s: illegal function number: %d\n", __FUNCTION__,
                  id->fn));
        return -1;
    }

    GET_IF_SEMA();

    pDev = dev->sd_bus;

    // Set global interrupt enable
    if (!SDHost_CMD52_Read(pDev, INT_ENABLE_REG, 0, &R)) {
        REL_IF_SEMA();
        return -1;
    }

    R &= ~(1 << id->fn);
    if (SDHost_CMD52_Write(pDev, INT_ENABLE_REG, 0, R)) {
        if (!SDHost_CMD52_Read(pDev, INT_ENABLE_REG, 0, &R)) {
            REL_IF_SEMA();
            return -1;
        }

        DBGPRINT(DBG_API,
                 (KERN_DEBUG "%s:FN0 : INT Disable Register: 0x%2.02X\n",
                  __FUNCTION__, R));
    } else {
        REL_IF_SEMA();
        return -1;
    }

    REL_IF_SEMA();
    return 0;
}

/** 
 *  @brief Set SDIO bus width
 *  @param dev				bus driver's device structure
 *  @param with				SDIO bus with 1 or 4
 *  @return				0 : Success -1: Failed  
*/
int
sd_set_buswidth(sd_device * dev, int width)
{
    SK_BOOL rc;
    GET_IF_SEMA();

    if (width == 1 || width == 4) {
        DBGPRINT(DBG_API, (KERN_DEBUG "%s: width = %d\n", __FUNCTION__, width));
        rc = SDHost_SetBusWidth((PSD_DEVICE_DATA) dev->sd_bus, width);
        REL_IF_SEMA();
        return (rc ? 0 : -1);
    } else {
        REL_IF_SEMA();
        return -1;
    }
}

/** 
 *  @brief Set GPO on or off
 *  @param dev			bus driver's device structure
 *  @param on				1: turn GPO on 0: turn GPO off
 *  @return				0 : Success -1: Failed  
*/
int
sd_set_gpo(sd_device * dev, int on)
{
    SK_BOOL rc;

    GET_IF_SEMA();
    DBGPRINT(DBG_API, (KERN_DEBUG "%s: on = %d\n", __FUNCTION__, on));
    rc = SDHost_SetGPO((PSD_DEVICE_DATA) dev->sd_bus, on);

    REL_IF_SEMA();
    return (rc ? 0 : -1);
}

/** 
 *  @brief Set SDIO bus clock on
 *  @param dev			bus driver's device structure
 *  @return				0 : Success -1: Failed  
*/
int
sd_start_clock(sd_device * dev)
{
    GET_IF_SEMA();
    DBGPRINT(DBG_API, (KERN_DEBUG "%s\n", __FUNCTION__));
    SDHost_SetClock((PSD_DEVICE_DATA) dev->sd_bus, 1);
    REL_IF_SEMA();
    return 0;
}

/** 
 *  @brief Set SDIO bus clock on
 *  @param dev			bus driver's device structure
 *  @return				0 : Success -1: Failed  
*/
int
sd_stop_clock(sd_device * dev)
{
    GET_IF_SEMA();
    DBGPRINT(DBG_API, (KERN_DEBUG "%s\n", __FUNCTION__));
    SDHost_SetClock((PSD_DEVICE_DATA) dev->sd_bus, 0);
    REL_IF_SEMA();
    return 0;
}

/** 
 *  @brief Set SDIO bus clock
 *  @param dev			bus driver's device structure
 *  @param clock		SDIO bus clock frequency
 *  @return				0 : Success -1: Failed  
*/
int
sd_set_busclock(sd_device * dev, int clock)
{
    GET_IF_SEMA();
    DBGPRINT(DBG_API, (KERN_DEBUG "%s: clock = %d\n", __FUNCTION__, clock));
    SDHost_SetClockSpeed((PSD_DEVICE_DATA) dev->sd_bus, clock);
    REL_IF_SEMA();
    return 0;
}

/** 
 *  @brief Read a byte from SDIO device
 *  @param dev			bus driver's device structure
 *  @param fn				function number 0..7
 *  @param reg 			register offset
 *  @param dat  		pointer to return value
 *  @return				0 : Success -1: Failed  
*/
int
sdio_read_ioreg(sd_device * dev, u8 fn, u32 reg, u8 * dat)
{
    GET_IF_SEMA();
    if (SDHost_CMD52_Read((PSD_DEVICE_DATA) dev->sd_bus, reg, fn, dat)) {
//              printk("%s: dev=%p fn=%d reg=0x%8.08X dat=0x%2.02x\n",__FUNCTION__,dev,fn,reg,*dat);
        REL_IF_SEMA();
        return 0;
    } else {
        printk("%s: dev=%p fn=%d reg=0x%8.08X FAILED!\n", __FUNCTION__, dev, fn,
               reg);
        REL_IF_SEMA();
        return -1;
    }

}

/** 
 *  @brief Write a byte to SDIO device
 *  @param dev			bus driver's device structure
 *  @param fn				function number 0..7
 *  @param reg 			register offset
 *  @param dat  		value
 *  @return				0 : Success -1: Failed  
*/
int
sdio_write_ioreg(sd_device * dev, u8 fn, u32 reg, u8 dat)
{
    GET_IF_SEMA();
    if (SDHost_CMD52_Write((PSD_DEVICE_DATA) dev->sd_bus, reg, fn, dat)) {
//              printk("%s: dev=%p fn=%d reg=0x%8.08X dat=0x%2.02x\n",__FUNCTION__,dev,fn,reg,dat);
        REL_IF_SEMA();
        return 0;
    } else {
        printk("%s: dev=%p fn=%d reg=0x%8.08X FAILED!\n", __FUNCTION__, dev, fn,
               reg);
        REL_IF_SEMA();
        return -1;
    }
}

/** 
 *  @brief Read multiple bytes from SDIO device
 *  @param dev			bus driver's device structure
 *  @param fn				function number 0..7
 *  @param address	offset
 *  @param blkmode	block or byte mode
 *  @param opcode		fixed or auto-inc address
 *  @param blkcnt 	number of bytes or blocks (depends on blkmode)
 *  @param blksz		size of block
 *  @param buffer		pointer to data buffer
 *  @return				0 : Success -1: Failed  
*/
int
sdio_read_iomem(sd_device * dev, u8 fn, u32 address, u8 blkmode, u8 opcode,
                u32 blkcnt, u32 blksz, u8 * buffer)
{
//      printk("%s: dev=%p fn=%d address=0x%8.08X blockcnt=%d blocksize=%d\n",__FUNCTION__,dev,fn,address,blkcnt,blksz);

    GET_IF_SEMA();
    if (SDHost_CMD53_ReadEx
        ((PSD_DEVICE_DATA) dev->sd_bus, address, fn, blkmode, opcode, buffer,
         blkcnt, blksz)) {
        REL_IF_SEMA();
        return 0;
    } else {
        printk("%s: dev=%p fn=%d FAILED!\n", __FUNCTION__, dev, fn);
        REL_IF_SEMA();
        return -1;
    }
}

/** 
 *  @brief Write multiple bytes to SDIO device
 *  @param dev			bus driver's device structure
 *  @param fn				function number 0..7
 *  @param address	offset
 *  @param blkmode	block or byte mode
 *  @param opcode		fixed or auto-inc address
 *  @param blkcnt 	number of bytes or blocks (depends on blkmode)
 *  @param blksz		size of block
 *  @param buffer		pointer to data buffer
 *  @return				0 : Success -1: Failed  
*/
int
sdio_write_iomem(sd_device * dev, u8 fn, u32 address, u8 blkmode, u8 opcode,
                 u32 blkcnt, u32 blksz, u8 * buffer)
{
//      printk("%s: dev=%p fn=%d address=0x%8.08X blockcnt=%d blocksize=%d\n",__FUNCTION__,dev,fn,address,blkcnt,blksz);
    GET_IF_SEMA();
    if (SDHost_CMD53_WriteEx
        ((PSD_DEVICE_DATA) dev->sd_bus, address, fn, blkmode, opcode, buffer,
         blkcnt, blksz)) {
        REL_IF_SEMA();
        return 0;
    } else {
        printk("%s: dev=%p fn=%d FAILED!\n", __FUNCTION__, dev, fn);
        REL_IF_SEMA();
        return -1;
    }
}

// for filling the "/proc/mrvsdio" entry
int
mrvlsdio_read_procmem(char *buf,
                      char **start,
                      off_t offset, int count, int *eof, void *data)
{
    PSD_DEVICE_DATA pDevTmp;
    int len = 0;

    if (!list_empty(&sd_dev_list)) {
        pDevTmp = (PSD_DEVICE_DATA) sd_dev_list.next;

        // after probe-event, we have "good" data in /proc/mrvsdio
        len =
            sprintf(buf,
                    "mrvlsdio is running with:\nbus_type     = 0x%x\nbus_with     = 0x%x\nclock_speed  = 0x%x\ndebug_flags  = 0x%x\nsdio_voltage = 0x%x\n",
                    pDevTmp->bus_type, pDevTmp->bus_width, pDevTmp->ClockSpeed,
                    pDevTmp->debug_flags, pDevTmp->sdio_voltage);
    } else {
        // not probed yet
        len = sprintf(buf, "mrvlsdio is up and waiting for card\n");
    }

    *eof = 1;
    return (len);
}

static void
call_client_probe(PSD_DEVICE_DATA pDev, int fn)
{

    if (down_interruptible(&sd_client_sema))
        return;
    if (pDev->sd_dev[fn].drv->probe != NULL) {
        pDev->sd_dev[fn].drv->probe(&pDev->sd_dev[fn], &pDev->sd_ids[fn]);
    }
    up(&sd_client_sema);
}

static int
mrvlsdio_init_module(void)
{
    int ret;

    ENTER();

    // generate /proc/ entry
    create_proc_read_entry("mrvlsdio",  // entry in "/proc/mrvlsdio"
                           0,   // file attributes
                           NULL,        // parent dir
                           mrvlsdio_read_procmem,       // name of function
                           NULL);       // client data

    DBGPRINT(DBG_ALL, ("*** mrvlsdio ***\n"));
    DBGPRINT(DBG_ALL, ("*** multifunction API ***\n"));
    DBGPRINT(DBG_ALL, ("*** Built on %s %s ***\n", __DATE__, __TIME__));

                                                                                                                                                                                /*--- mmoser 6/21/2007 ---*/
    INIT_LIST_HEAD(&sd_dev_list);
    spin_lock_init(&sd_dev_list_lock);

    INIT_LIST_HEAD(&sd_client_list);
    spin_lock_init(&sd_client_list_lock);

    ret = register_chrdev(sdio_major, "mrvlsdio", &mrvlsdio_fops);
    if (ret < 0) {
        DBGPRINT(DBG_LOAD,
                 (KERN_WARNING "mrvlsdio: can't get major %d  ret=%d\n",
                  sdio_major, ret));
    } else {
        DBGPRINT(DBG_LOAD, (KERN_DEBUG "mrvlsdio: major %d\n", ret));
    }
    if (sdio_major == 0)
        sdio_major = ret;       /* dynamic */

        /** from 2.4 kernel source:
	 * pci_register_driver - register a new pci driver
	 *
	 * Adds the driver structure to the list of registered drivers
	 * Returns the number of pci devices which were claimed by the driver
	 * during registration.  The driver remains registered even if the
	 * return value is zero.
	 */

    ret = pci_register_driver(&mrvlsdio);
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "%s: pci_register_driver with %d\n", __FILE__, ret));

    LEAVE();
//      return (ret);
    return 0;
}

static void
mrvlsdio_exit_module(void)
{
    PSD_DEVICE_DATA pTmpDev;
    PSD_CLIENT pTmpClient;

    // remove proc entry
    remove_proc_entry("mrvlsdio", NULL);
    DBGPRINT(DBG_LOAD, (KERN_DEBUG "Module mrvlsdio exit\n"));

    unregister_chrdev(sdio_major, "mrvlsdio");
    pci_unregister_driver(&mrvlsdio);

    // Clean up device list

    while (!list_empty(&sd_dev_list)) {
        pTmpDev = (PSD_DEVICE_DATA) sd_dev_list.next;
        DBGPRINT(DBG_LOAD,
                 (KERN_DEBUG "%s@%d: pTmpDev=%p\n", __FUNCTION__, __LINE__,
                  pTmpDev));
        list_del((struct list_head *) pTmpDev);
        kfree(pTmpDev);

#ifdef SDIO_MEM_TRACE
        printk("<<< kfree(SD_DEVICE_DATA)\n");
#endif

    }

    // Clean up client list
    while (!list_empty(&sd_client_list)) {
        pTmpClient = (PSD_CLIENT) sd_client_list.next;
        DBGPRINT(DBG_LOAD,
                 (KERN_DEBUG "%s@%d: pTmpClient=%p\n", __FUNCTION__, __LINE__,
                  pTmpClient));
        list_del((struct list_head *) pTmpClient);
        kfree(pTmpClient);
#ifdef SDIO_MEM_TRACE
        printk("<<< kfree(SD_CLIENT)\n");
#endif

    }
}

module_init(mrvlsdio_init_module);
module_exit(mrvlsdio_exit_module);

EXPORT_SYMBOL(sd_driver_register);
EXPORT_SYMBOL(sd_driver_unregister);
EXPORT_SYMBOL(sd_request_int);
EXPORT_SYMBOL(sd_release_int);
EXPORT_SYMBOL(sd_unmask);
EXPORT_SYMBOL(sd_enable_int);
EXPORT_SYMBOL(sd_disable_int);
EXPORT_SYMBOL(sd_set_buswidth);
EXPORT_SYMBOL(sd_set_busclock);
EXPORT_SYMBOL(sd_start_clock);
EXPORT_SYMBOL(sd_stop_clock);
EXPORT_SYMBOL(sd_set_gpo);
EXPORT_SYMBOL(sdio_read_ioreg);
EXPORT_SYMBOL(sdio_write_ioreg);
EXPORT_SYMBOL(sdio_read_iomem);
EXPORT_SYMBOL(sdio_write_iomem);

MODULE_DESCRIPTION("Marvell SDIO Bus Driver");
MODULE_AUTHOR("Marvell International Ltd.");
MODULE_LICENSE("GPL");
