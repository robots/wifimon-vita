/**
 *
 * Name:	skisr.c
 * Project:	Wireless LAN, Bus driver for SDIO interface
 * Version:	$Revision: 1.1 $
 * Date:	$Date: 2007/01/18 09:21:35 $
 * Purpose:	This module handles the interrupts.
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
 *	$Log: skisr.c,v $
 *	Revision 1.1  2007/01/18 09:21:35  pweber
 *	Put under CVS control
 *	
 *
 ******************************************************************************/

#include "h/skdrv1st.h"
#include "h/skdrv2nd.h"

static void (*gpio_int_callback) (void *) = NULL;
static void *gpio_int_callback_arg = NULL;

int
request_gpio_irq_callback(void (*callback) (void *), void *arg)
{
    if (!gpio_int_callback && !gpio_int_callback_arg) {
        gpio_int_callback = callback;
        gpio_int_callback_arg = arg;
        return 0;
    }
    return -1;
}

EXPORT_SYMBOL(request_gpio_irq_callback);

int
release_gpio_irq_callback(void (*callback) (void *), void *arg)
{
    if ((callback == gpio_int_callback)
        && (arg == gpio_int_callback_arg)) {
        gpio_int_callback = NULL;
        gpio_int_callback_arg = NULL;
        return 0;
    }
    return -1;
}

EXPORT_SYMBOL(release_gpio_irq_callback);

DECLARE_TASKLET(SDIOBus_tasklet, SDIOBus_Dpc, 0);

void
SDIOBus_Dpc(unsigned long arg)
{
    SK_U32 irq_status, status;
    SK_U16 errorIRQ_status = 0;
    PSD_DEVICE_DATA pDev;

    DBGPRINT(DBG_IRQ, (KERN_DEBUG "SDIOBUS_Dpc startet ....\n"));

    if (arg == 0) {
        DBGPRINT(DBG_ERROR, (KERN_DEBUG "SDIOBUS_Dpc : arg == NULL !\n"));
        return;
    }

    pDev = (PSD_DEVICE_DATA) arg;

    if (pDev == NULL) {
        DBGPRINT(DBG_ERROR, (KERN_DEBUG "SDIOBUS_Dpc : pDev == NULL !\n"));
        return;
    }

    irq_status = pDev->lastIntStatus;

    if ((irq_status & STDHOST_NORMAL_IRQ_ERROR) != 0) {
        MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_STATUS,
                        &errorIRQ_status);
        if ((errorIRQ_status & STDHOST_ERROR_IRQ_GPI_0) != 0) {
            if ((pDev->lastErrorIRQSignalMask & STDHOST_ERROR_IRQ_GPI_0) != 0) {
                // clear irq bit
                MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_STATUS,
                                 STDHOST_ERROR_IRQ_GPI_0);
                // Fake a SDIO card interrupt
                irq_status |= STDHOST_NORMAL_IRQ_CARD;
            }
        }
        if ((errorIRQ_status & STDHOST_ERROR_IRQ_GPI_1) != 0) {
            if ((pDev->lastErrorIRQSignalMask & STDHOST_ERROR_IRQ_GPI_1) != 0) {
                printk("GPI-1 interrupt fired (0x%4.04X)\n", errorIRQ_status);
                                /*--- mmoser 3/12/2007 ---*/
                // clear irq bit
                MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_STATUS,
                                 STDHOST_ERROR_IRQ_GPI_1);

                SDHost_SetClock(pDev, 1);
                if (gpio_int_callback && gpio_int_callback_arg)
                    gpio_int_callback(gpio_int_callback_arg);
            }
        }
    }

    if ((irq_status & STDHOST_NORMAL_IRQ_CARD_OUT) ==
        STDHOST_NORMAL_IRQ_CARD_OUT) {
        // clear irq bit
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS,
                         STDHOST_NORMAL_IRQ_CARD_OUT);

                /*--- mmoser 14.09.2005 ---*/
        if (pDev->initialized) {
            DBGPRINT(DBG_IRQ | DBG_W528D,
                     (KERN_DEBUG "SDIOBUS_Dpc : NORMAL_IRQ_CARD_OUT\n"));

            /*--- mmoser 10.08.2005 ---*/
            DBGPRINT(DBG_IRQ, (KERN_DEBUG "CARD REMOVED\n"));
            SDIOBus_CardRemoved(pDev);

                        /*--- mmoser 3/31/2006 ---*/
            pDev->SurpriseRemoved = SK_TRUE;
        }
        SDHost_EnableInterrupt(pDev, pDev->lastIRQSignalMask);
        DBGPRINT(DBG_IRQ,
                 (KERN_DEBUG "SDIOBUS_Dpc finished.(line=%d)\n", __LINE__));
        return;
    }
    if ((irq_status & STDHOST_NORMAL_IRQ_CARD_IN) == STDHOST_NORMAL_IRQ_CARD_IN) {
        // clear irq bit
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS,
                         STDHOST_NORMAL_IRQ_CARD_IN);

        DBGPRINT(DBG_IRQ, (KERN_DEBUG "SDIOBUS_Dpc : NORMAL_IRQ_CARD_IN\n"));
        DBGPRINT(DBG_IRQ, (KERN_DEBUG "CARD PLUGGED IN\n"));
        SDIOBus_CardPluggedIn(pDev);
        SDHost_EnableInterrupt(pDev, pDev->lastIRQSignalMask);
        DBGPRINT(DBG_IRQ,
                 (KERN_DEBUG "SDIOBUS_Dpc finished.(line=%d)\n", __LINE__));
        return;
    }

    if ((irq_status & STDHOST_NORMAL_IRQ_CARD) == STDHOST_NORMAL_IRQ_CARD) {
        status = (irq_status << 16);
        if (pDev->number_of_functions > 1) {
            queue_work(pDev->workqueue, &pDev->irq_work);
        } else {
            if (pDev->irq_device_cache[1]->functions[1].int_handler != NULL) {
                pDev->irq_device_cache[1]->functions[1].int_handler(pDev->
                                                                    irq_device_cache
                                                                    [1],
                                                                    pDev->
                                                                    irq_device_cache
                                                                    [1]->
                                                                    pCurrent_Ids,
                                                                    pDev->
                                                                    irq_device_cache
                                                                    [1]->
                                                                    functions
                                                                    [1].
                                                                    context);
            } else {
                DBGPRINT(DBG_IRQ,
                         (KERN_DEBUG "No interrupt handler registered.\n"));
                SDHost_EnableInterrupt(pDev, STDHOST_NORMAL_IRQ_CARD_ALL_ENA);
                DBGPRINT(DBG_IRQ, (KERN_DEBUG "SDIOBUS_Dpc finished.\n"));
                return;
            }
        }
        DBGPRINT(DBG_IRQ,
                 (KERN_DEBUG "SDIOBUS_Dpc finished.(line=%d)\n", __LINE__));
        return;
    }

    if (irq_status & (STDHOST_NORMAL_IRQ_ERROR |
                      STDHOST_NORMAL_IRQ_TRANS_COMPLETE |
                      STDHOST_NORMAL_IRQ_CMD_COMPLETE)) {
        if (((irq_status & STDHOST_NORMAL_IRQ_TRANS_COMPLETE) &&
             (pDev->lastIRQSignalMask & STDHOST_NORMAL_IRQ_TRANS_COMPLETE)) ||
            ((irq_status & STDHOST_NORMAL_IRQ_CMD_COMPLETE) &&
             (pDev->lastIRQSignalMask & STDHOST_NORMAL_IRQ_CMD_COMPLETE)) ||
            ((errorIRQ_status & STDHOST_ERROR_IRQ_GPI_1) &&
             (pDev->lastErrorIRQSignalMask & STDHOST_ERROR_IRQ_GPI_1))

            )
        {
            SDHost_EnableInterrupt(pDev, pDev->lastIRQSignalMask);
        }
        DBGPRINT(DBG_IRQ,
                 (KERN_DEBUG "SDIOBUS_Dpc finished.(line=%d)\n", __LINE__));
        return;
    } else {
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS, 0xFFFF);
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_STATUS, 0xFFFF);
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG "SDIOBus_Dpc(): Unhandled IRQ : 0x%8.08X.\n",
                  irq_status));
    }

    SDHost_EnableInterrupt(pDev, pDev->lastIRQSignalMask);
    DBGPRINT(DBG_IRQ,
             (KERN_DEBUG "SDIOBUS_Dpc finished.(line=%d)\n", __LINE__));
}

irqreturn_t
SDIOBus_Isr(int irq, void *dev_id, struct pt_regs *regs)
{
    SK_U32 tmp;
    SK_U16 tmpsig;
    PSD_DEVICE_DATA pDev;

    if (dev_id == NULL) {
        DBGPRINT(DBG_IRQ, (KERN_DEBUG "SDIOBUS_Isr : dev_id == NULL !\n"));
        return IRQ_HANDLED;
    }

    pDev = (PSD_DEVICE_DATA) dev_id;

    if (pDev == NULL) {
        DBGPRINT(DBG_IRQ, (KERN_DEBUG "SDIOBUS_Isr : pDev == NULL !\n"));
        return IRQ_HANDLED;
    }

    MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS, &tmp);
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE, &tmpsig);

    // Check if interrupt came from our device
    if (((((tmp & STDHOST_NORMAL_IRQ_ERROR) != 0) &&
          ((tmpsig & (STDHOST_ERROR_IRQ_GPI_1 | STDHOST_ERROR_IRQ_GPI_0)) != 0))
         || ((tmp & pDev->currentIRQSignalMask) != 0)) && tmp != 0xFFFFFFFF)
    {
                                                                                                                                                                /*--- mmoser 8/29/2006 ---*/
        if (tmp & STDHOST_NORMAL_IRQ_TRANS_COMPLETE) {
            if (pDev->currentIRQSignalMask & STDHOST_NORMAL_IRQ_TRANS_COMPLETE) {
                // clear irq bit
                MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS,
                                 STDHOST_NORMAL_IRQ_TRANS_COMPLETE);

                if (!test_and_set_bit(0, &pDev->trans_complete_evt.event)) {
                    wake_up(&pDev->trans_complete_evt.wq);
                }
            }
        }

        if (tmp & STDHOST_NORMAL_IRQ_CMD_COMPLETE) {
            if (pDev->currentIRQSignalMask & STDHOST_NORMAL_IRQ_CMD_COMPLETE) {
                // clear irq bit
                MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS,
                                 STDHOST_NORMAL_IRQ_CMD_COMPLETE);

                if (!test_and_set_bit(0, &pDev->cmd_complete_evt.event)) {
                    wake_up(&pDev->cmd_complete_evt.wq);
                }
            }
        }

        MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                        &pDev->lastErrorIRQSignalMask);
        pDev->lastIntStatus = tmp;
        DBGPRINT(DBG_IRQ,
                 (KERN_DEBUG "SDIOBUS_Isr (0x%4.04X)\n", pDev->lastIntStatus));
        SDHost_DisableInterrupt(pDev, STDHOST_NORMAL_IRQ_ALL_SIG_ENA);

        SDIOBus_tasklet.data = (unsigned long) pDev;
        tasklet_schedule(&SDIOBus_tasklet);

    } else {
        if (tmp == 0xFFFFFF)
            pDev->SurpriseRemoved = SK_TRUE;
        return IRQ_NONE;
    }
    return IRQ_HANDLED;

}
