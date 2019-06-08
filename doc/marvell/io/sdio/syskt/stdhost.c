/**
 *
 * Name: stdhost.c
 * Project: Wireless LAN, Bus driver for SDIO interface
 * Version: $Revision: 1.1 $
 * Date: $Date: 2007/01/18 09:21:35 $
 * Purpose: This module handles the SDIO Standard Host Interface.
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
 * $Log: stdhost.c,v $
 * Revision 1.1  2007/01/18 09:21:35  pweber
 * Put under CVS control
 *
 * Revision 1.4  2005/12/08 12:00:27  ebauer
 * Driver ckecks AND update voltage registry value if not correct
 * 
 * Revision 1.3  2005/12/07 13:40:19  ebauer
 * Modify check of registry voltage value
 * 
 * Revision 1.2  2005/10/31 10:34:59  jschmalz
 * Bugfixes Chariot (Interface hang)
 * 
 * Revision 1.1  2005/10/07 08:43:47  jschmalz
 * Put SDIO with FPGA under CVS control
 * 
 * 
 ******************************************************************************/

#include "h/skdrv1st.h"
#include "h/skdrv2nd.h"

//define the SD Block Size
//for SD8381-B0 chip, it could be 32,64,128,256,512
//for all other chips including SD8381-A0, it must be 32
#define POLLING_TIMES 2000

#define USE_FPGA_POWER_RAMP
//  #define DMA_SUPPORT_RD
// #define DMA_SUPPORT_WR
// #define FIFO_CMD52_DEBUG

SK_U8 ena_func[8] = { 0, 0x02, 0x06, 0x0E, 0x1E, 0x3E, 0x7E, 0xFE };

SK_BOOL SDHost_DumpCIS(PSD_DEVICE_DATA pDev, SK_U8 function_number,
                       SK_U32 length);

extern int block_size, dma_support;

/******************************************************************************
 *
 * SDHost_InitializeCard  - Initialize the plugged in SDIO card
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 * TRUE on success
 */
SK_BOOL
SDHost_InitializeCard(PSD_DEVICE_DATA pDev)
{
    SK_U8 OCR[3];
    SK_U8 RCA[2];
    SK_U8 Cis[DEVICE_NAME_LENGTH];
    SK_U16 usVal, ucPowerCurr[FPGA_POWER_STEPS] = FPGA_POWER_VAL_ENUM;
    SK_U8 ucValue;
    SK_U8 ucConfigId;
    SK_U8 ucPowerValue = 0;
    SK_U8 R;
    SK_U32 ulArg;
    SK_U32 j, i, ulValue, ulResponse, cnt;
    SK_U32 cislength;
    SK_U32 RegVal;
    SK_U32 u32Val;

    SDHost_DisableInterrupt(pDev, STDHOST_NORMAL_IRQ_ALL_SIG_ENA);

    _SLEEP_MS(pDev, 100);

    DBGPRINT(DBG_W528D, (KERN_DEBUG "SDHost_InitializeCard()\n"));
               /*--- mmoser 3/7/2006 ---*/
    cnt = 0;

    pDev->crc_len = 0;

    ZERO_MEMORY(OCR, 3);
    ZERO_MEMORY(RCA, 2);
    ZERO_MEMORY(Cis, DEVICE_NAME_LENGTH);

    // Setup the SD clock
    if (pDev->baseClockFreq == 0) {
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG "Base clock frequency not available!\n"));
        return SK_FALSE;
    }
    if (pDev->ClockSpeed != 0) {
        usVal = MK_CLOCK((SK_U16) pDev->ClockSpeed) | STDHOST_INTERNAL_CLK_ENA;
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL, usVal);
    } else {
        SK_U32 ClockSpeed = 1;  // set the clock initially to 25MHz
        usVal = MK_CLOCK((SK_U16) ClockSpeed) | STDHOST_INTERNAL_CLK_ENA;
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL, usVal);
    }
    /* Set the Rx Clock to Rising Edge */

    MEM_READ_ULONG(pDev, SK_SLOT_0, FPGA_CORE_CTRL_REG, &u32Val);
    u32Val |= FPGA_RAISING_EDGE;
    MEM_WRITE_ULONG(pDev, SK_SLOT_0, FPGA_CORE_CTRL_REG, u32Val);

    for (j = 0; j < MAX_WAIT_CLOCK_STABLE; j++) {
        MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL, &usVal);
        if ((usVal & STDHOST_INTERNAL_CLK_STABLE) ==
            STDHOST_INTERNAL_CLK_STABLE) {
            break;
        }
    }
    if (j >= MAX_WAIT_CLOCK_STABLE) {
        DBGPRINT(DBG_ERROR, (KERN_DEBUG "SD clock remains unstable!\n"));
        return SK_FALSE;
    }
    // Enable clock to card
    usVal |= STDHOST_CLOCK_ENA;
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL, usVal);

    // enable DMA support depending on ChipRev
    MEM_READ_UCHAR(pDev, SK_SLOT_0, FPGA_CHIP_REVISION, &ucValue);
    DBGPRINT(DBG_ERROR, (KERN_DEBUG "ChipRevision: 0x%x\n", ucValue));
    {
        SK_U8 hi, lo;
        lo = ucValue & 0x0F;
        hi = (ucValue >> 4) & 0x0F;
        printk("\n****************************************************\n");
        printk("*********   FPGA-ChipRevision: %x.%x (0x%2.02X)  *********\n",
               hi, lo, ucValue);
        printk("****************************************************\n");
    }

    pDev->dma_support = 0;
    if ((ucValue >= FPGA_CHIP_REV_2_0) && dma_support) {
        if ((pDev->debug_flags & DEBUG_USE_RD_DMA) == DEBUG_USE_RD_DMA)
            pDev->dma_support |= DMA_RD_SUPPORT;

        if ((pDev->debug_flags & DEBUG_USE_WR_DMA) == DEBUG_USE_WR_DMA)
            pDev->dma_support |= DMA_WR_SUPPORT;

        // pDev->dma_support = DMA_WR_SUPPORT | DMA_RD_SUPPORT;
        // pDev->dma_support = DMA_RD_SUPPORT;
        // pDev->dma_support = 0;
    } else {
        pDev->dma_support = 0;
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG
                  "***************************************************\n"));
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG
                  "* CardBus-to-SDIO Adapter FPGA Rev: V0x%x         *\n",
                  ucValue));
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG
                  "* This FPGA revision does NOT support DMA mode,   *\n"));
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG
                  "* please update your adapter to Rev 20 or higher  *\n"));
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG
                  "***************************************************\n"));
    }
    DBGPRINT(DBG_ERROR, (KERN_DEBUG "DMA_SUPPORT: 0x%x\n", pDev->dma_support));
    printk
        ("**************++ DMA_SUPPORT: 0x%x debug_flags=0x%8.08X ++************************\n",
         pDev->dma_support, pDev->debug_flags);
    // pweber / tschrobenhauser 03.08.2005
    // check the power regulator output voltage is stable
    // WAIT UNTIL ADDR 0x20E, Bit 0 == 1

    MEM_READ_UCHAR(pDev, SK_SLOT_0, FPGA_CARD_REVISION, &ucValue);
    DBGPRINT(DBG_ERROR, (KERN_DEBUG "CardRevision: 0x%x\n", ucValue));
    if (ucValue == FPGA_CARD_REV_1_1) {
//  SK_U8 ucConfigId;
//  SK_U8 ucPowerValue=0;
        // LINUX: get voltage value from module parameter 'sdio_voltage'
        RegVal = pDev->sdio_voltage;

        MEM_READ_UCHAR(pDev, SK_SLOT_0, FPGA_CONFIG_ID, &ucConfigId);
        if (ucConfigId & FPGA_CONFIG_3_3_V) {
            DBGPRINT(DBG_ERROR, (KERN_DEBUG "Configuration ID : 3.3V\n"));
            // Check if registry value is ok
            if ((RegVal >= FPGA_POWER_REG_3_3_V_MIN) &&
                (RegVal <= FPGA_POWER_REG_3_3_V_MAX)) {
                ucPowerValue = (SK_U8) RegVal;
            } else {
                ucPowerValue = FPGA_POWER_REG_3_3_V;
            }
        } else if (ucConfigId & FPGA_CONFIG_2_5_V) {
            DBGPRINT(DBG_ERROR, (KERN_DEBUG "Configuration ID : 2.5V\n"));
            // Check if registry value is ok
            if ((RegVal >= FPGA_POWER_REG_2_5_V_MIN) &&
                (RegVal <= FPGA_POWER_REG_2_5_V_MAX)) {
                ucPowerValue = (SK_U8) RegVal;
            } else {
                ucPowerValue = FPGA_POWER_REG_2_5_V;
            }
        } else if (ucConfigId & FPGA_CONFIG_1_8_V) {
            DBGPRINT(DBG_ERROR, (KERN_DEBUG "Configuration ID : 1.8V\n"));
            // Check if registry value is ok
            if ((RegVal >= FPGA_POWER_REG_1_8_V_MIN) &&
                (RegVal <= FPGA_POWER_REG_1_8_V_MAX)) {
                ucPowerValue = (SK_U8) RegVal;
            } else {
                ucPowerValue = FPGA_POWER_REG_1_8_V;
            }
        }
    }
#ifndef USE_FPGA_POWER_RAMP     // [fhm] ramp-up SDIO port voltage

    MEM_WRITE_UCHAR(pDev, SK_SLOT_0, FPGA_POWER_REG_DATA, ucPowerValue);
    MEM_WRITE_UCHAR(pDev, SK_SLOT_0, FPGA_POWER_REG_CMD,
                    FPGA_POWER_REG_CMD_START);

    DBGPRINT(DBG_ERROR, (KERN_DEBUG "wait for stable voltage\n"));
    cnt = 0;
    do {
        MEM_READ_UCHAR(pDev, SK_SLOT_0, FPGA_POWER_REG_STATUS, &ucValue);
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG "PowerRegulatorControl: 0x%x\n", ucValue));
    } while (++cnt < 10000 && (ucValue & FPGA_POWER_REG_STABLE) == 0);
    DBGPRINT(DBG_ERROR, ("wait for stable voltage: cnt=%d\n", cnt));

    // Turn on highest supplied voltage 
    ucValue = MK_VOLTAGE(pDev->maxSupplyVoltage);
    MEM_WRITE_UCHAR(pDev, SK_SLOT_0, STDHOST_POWER_CTRL, ucValue);

    DBGPRINT(DBG_ERROR,
             (KERN_DEBUG "MaxSupplyVoltage: %d\n", pDev->maxSupplyVoltage));

    ucValue = MK_VOLTAGE(pDev->maxSupplyVoltage) | STDHOST_POWER_ON;
    MEM_WRITE_UCHAR(pDev, SK_SLOT_0, STDHOST_POWER_CTRL, ucValue);

#else
    _SLEEP_MS(pDev, 100);       // [mb] for de-bounce 
    for (i = 0; i < FPGA_POWER_STEPS; i++) {
        MEM_WRITE_UCHAR(pDev, SK_SLOT_0, FPGA_POWER_REG_DATA,
                        (SK_U8) ucPowerCurr[i]);
        MEM_WRITE_UCHAR(pDev, SK_SLOT_0, FPGA_POWER_REG_CMD,
                        FPGA_POWER_REG_CMD_START);

        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG "wait for stable voltage (0x%x)\n",
                  ucPowerCurr[i]));
        cnt = 0;
        do {
            MEM_READ_UCHAR(pDev, SK_SLOT_0, FPGA_POWER_REG_STATUS, &ucValue);
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG "PowerRegulatorControl: 0x%x\n", ucValue));
        } while (++cnt < 10000 && (ucValue & FPGA_POWER_REG_STABLE) == 0);
        DBGPRINT(DBG_ERROR, ("wait for stable voltage: cnt=%d\n", cnt));
        if (cnt == 10000)
            return SK_FALSE;

        // switch on SDIO port voltage if lowest voltage is stable
        if (ucPowerCurr[i] == FPGA_POWER_REG_0_7_V) {
            ucValue = MK_VOLTAGE(pDev->maxSupplyVoltage);
            MEM_WRITE_UCHAR(pDev, SK_SLOT_0, STDHOST_POWER_CTRL, ucValue);
            ucValue = MK_VOLTAGE(pDev->maxSupplyVoltage) | STDHOST_POWER_ON;
//[mb]
            _SLEEP_MS(pDev, 50);        // wait until voltage has reached the
                                        // set level 
            MEM_WRITE_UCHAR(pDev, SK_SLOT_0, STDHOST_POWER_CTRL, ucValue);
        }
        // leave for loop, if desired power level has been set
        if (ucPowerCurr[i] == (SK_U16) ucPowerValue)
            break;
//[mb]
        if ((ucPowerCurr[i] < FPGA_POWER_REG_1_2_V) &&
            (ucPowerCurr[i] > FPGA_POWER_REG_1_0_V))
            udelay(FPGA_POWER_RAMP_DELAY * 6);
        else
            udelay(FPGA_POWER_RAMP_DELAY);
        if (i < FPGA_POWER_STEPS - 1) {
            if (ucPowerCurr[i + 1] > (SK_U16) ucPowerValue)
                ucPowerCurr[i + 1] = (SK_U16) ucPowerValue;
        }
    }

#endif // [fhm]

    _SLEEP_MS(pDev, 100);       // pweber 03.08.2005

    if (pDev->bus_type == SDIO_SPI_TYPE) {
        DBGPRINT(DBG_W528D,
                 (KERN_DEBUG "-------------->   SPI-Mode : Send CMD0\n"));
        MEM_READ_ULONG(pDev, SK_SLOT_0, 0x200, &ulValue);
        ulValue |= 0x00000001;
        MEM_WRITE_ULONG(pDev, SK_SLOT_0, 0x200, ulValue);

        SDHost_SendCmd(pDev, 0, 0, 0, &ulResponse);     // CMD0 for transit to
                                                        // SPI mode
        SDHost_SendCmd(pDev, 59, 0, 0, &ulResponse);    // CMD59 turn off CRC
    }
// _SLEEP_MS( pDev, 100);  pweber 03.08.2005

    if (!SDHost_ReadOCR(pDev, OCR)) {
        DBGPRINT(DBG_W528D | DBG_ERROR, (KERN_DEBUG "ReadOCR failed!\n"));
        return SK_FALSE;
    }
    // check if our supply voltage is supported

    if ((OCR[0] & 0xE0) == 0)   // Support for 3.3 - 3.6 V
    {
        DBGPRINT(DBG_W528D | DBG_ERROR,
                 (KERN_DEBUG "Card does not support our supply voltage!\n"));
        return SK_FALSE;
    }

    if (!SDHost_WriteOCR(pDev, OCR)) {
        DBGPRINT(DBG_W528D | DBG_ERROR, (KERN_DEBUG "WriteOCR failed!\n"));
        return SK_FALSE;
    }

    if (pDev->bus_type == SDIO_BUS_TYPE) {
        // Send CMD3 ident -> standby
        if (!SDHost_ReadRCA(pDev, RCA)) {
            return SK_FALSE;
        }

        if (RCA[0] == 0 && RCA[1] == 0) {
            // Try to get new RCA
            if (!SDHost_ReadRCA(pDev, RCA)) {
                return SK_FALSE;
            }
            if (RCA[0] == 0 && RCA[1] == 0) {
                // Failed to get a valid RCA
                return SK_FALSE;
            }
        }
        ulArg = RCA[0] << 24 | RCA[1] << 16;
        // CMD7 to select card and go to transfer state.
        if (!SDHost_SendCmd(pDev, 7, ulArg, 0, &ulResponse)) {
            if (++pDev->bus_errors > MAX_BUS_ERRORS) {
                SDHost_SetCardOutEvent(pDev);
            }
            return SK_FALSE;
        }
    }

    j = 0;
    while (j++ < 5) {
        // IO Enable all supported functions
        DBGPRINT(DBG_W528D,
                 (KERN_DEBUG "IO Enable all supported functions: 0x%2.02X\n",
                  ena_func[pDev->number_of_functions]));
        SDHost_CMD52_Write(pDev, IO_ENABLE_REG, 0,
                           ena_func[pDev->number_of_functions]);
        if (SDHost_CMD52_Read(pDev, IO_ENABLE_REG, 0, &R)) {
            break;
        }
    }

    if (j >= 5) {
        DBGPRINT(DBG_W528D,
                 (KERN_DEBUG "FN0 : I/O Enable FAILED!!!!!!!!!!!!\n"));
        return SK_FALSE;
    }

                     /*--- mmoser 3/22/2006 ---*/
    j = 0;
    while (j++ < 5) {
        // INT Enable function 0
        if (SDHost_CMD52_Write
            (pDev, INT_ENABLE_REG, 0,
             ena_func[pDev->number_of_functions] | IENM)) {
            SDHost_CMD52_Read(pDev, INT_ENABLE_REG, 0, &R);
            DBGPRINT(DBG_W528D, (KERN_DEBUG "FN0 : INT Enable 0x%2.02X\n", R));
            break;
        }
    }

    if (j >= 5) {
        DBGPRINT(DBG_W528D,
                 (KERN_DEBUG "FN0 : INT Enable FAILED!!!!!!!!!!!!\n"));
        return SK_FALSE;
    }

    // IO Ready function 1
    if (SDHost_CMD52_Read(pDev, IO_READY_REG, 0, &R)) {
        DBGPRINT(DBG_W528D, (KERN_DEBUG "I/O Ready(R/O) 0x%2.02X\n", R));
    }
    // Read Card Capability
    if (SDHost_CMD52_Read(pDev, CARD_CAPABILITY_REG, 0, &R)) {
#ifdef DBG
        DBGPRINT(DBG_W528D, (KERN_DEBUG "Card Capability\n"));
        if (R & 0x01) {
            DBGPRINT(DBG_W528D, (KERN_DEBUG "       SDC:CMD52 supported\n"));
        } else {
            DBGPRINT(DBG_W528D,
                     (KERN_DEBUG "       SDC:CMD52 not supported\n"));
        }
        if (R & 0x02) {
            DBGPRINT(DBG_W528D, (KERN_DEBUG "       SMB:CMD53 supported\n"));
        } else {
            DBGPRINT(DBG_W528D,
                     (KERN_DEBUG "       SMB:CMD53 not supported\n"));
        }
        if (R & 0x04) {
            DBGPRINT(DBG_W528D,
                     (KERN_DEBUG "       SRW:Read Wait supported\n"));
        } else {
            DBGPRINT(DBG_W528D,
                     (KERN_DEBUG "       SRW:Read Wait not supported\n"));
        }
        if (R & 0x08) {
            DBGPRINT(DBG_W528D, (KERN_DEBUG "       SBS supported\n"));
        } else {
            DBGPRINT(DBG_W528D, (KERN_DEBUG "       SBD not supported\n"));
        }
        if (R & 0x10) {
            DBGPRINT(DBG_W528D, (KERN_DEBUG "       S4MI supported\n"));
        } else {
            DBGPRINT(DBG_W528D, (KERN_DEBUG "       S4MI not supported\n"));
        }
        if (R & 0x20) {
            DBGPRINT(DBG_W528D, (KERN_DEBUG "       E4MI supported(R/W)\n"));
        } else {
            DBGPRINT(DBG_W528D,
                     (KERN_DEBUG "       E4MI not supported(R/W)\n"));
        }
        if (R & 0x40) {
            DBGPRINT(DBG_W528D, (KERN_DEBUG "       LSC supported\n"));
        } else {
            DBGPRINT(DBG_W528D, (KERN_DEBUG "       LSC not supported\n"));
        }
        if (R & 0x80) {
            DBGPRINT(DBG_W528D, (KERN_DEBUG "       4BLS supported\n"));
        } else {
            DBGPRINT(DBG_W528D, (KERN_DEBUG "       4BLS not supported\n"));
        }
#endif
    }
    if (SDHost_CMD52_Read(pDev, FUNCTION_SELECT_REG, 0, &R)) {
        DBGPRINT(DBG_W528D,
                 (KERN_DEBUG
                  "Card Function Selected for Suspend/Resume(R/W) 0x%2.02X!\n",
                  R));
    }
    // Exec Flags
    if (SDHost_CMD52_Read(pDev, EXEC_FLAGS_REG, 0, &R)) {
        DBGPRINT(DBG_W528D, (KERN_DEBUG "Exec Flags(R/O)0x%2.02X\n", R));
    }
    // Ready Flags
    if (SDHost_CMD52_Read(pDev, READY_FLAGS_REG, 0, &R)) {
        DBGPRINT(DBG_W528D, (KERN_DEBUG "Ready Flags(R/O)0x%2.02X\n", R));
    }

    if (pDev->bus_width == SDIO_4_BIT) {
        SDHost_CMD52_Write(pDev, BUS_INTERFACE_CONTROL_REG, 0, 0x82);   // 0x80:CD 
                                                                        // disable,1 
                                                                        // bit;0x82:CD 
                                                                        // disable,4 
                                                                        // bit
    } else {
        SDHost_CMD52_Write(pDev, BUS_INTERFACE_CONTROL_REG, 0, 0x80);   // 0x80:CD 
                                                                        // disable,1 
                                                                        // bit;0x82:CD 
                                                                        // disable,4 
                                                                        // bit
    }

    SDHost_CMD52_Read(pDev, BUS_INTERFACE_CONTROL_REG, 0, &R);

    switch (R & 0x0F) {
    case 0x02:
        DBGPRINT(DBG_W528D, (KERN_DEBUG "4 bit mode,"));
        break;
    case 0x00:
        DBGPRINT(DBG_W528D, (KERN_DEBUG "1 bit mode,"));
        break;
    default:
        DBGPRINT(DBG_W528D, (KERN_DEBUG "1 or 4 bit mode error,"));
    }
    if ((R & 0xF0) == 0x80) {
        DBGPRINT(DBG_W528D, (KERN_DEBUG "CD Disable!\n"));
    } else {
        DBGPRINT(DBG_W528D, (KERN_DEBUG "CD Enable!\n"));
    }

    SDHost_Enable_HighSpeedMode(pDev);

    // set function 0-7 block size
    for (i = 0; i <= pDev->number_of_functions; i++) {
        SDHost_CMD52_Write(pDev, FN_BLOCK_SIZE_0_REG(i), 0,
                           (SK_U8) (block_size & 0x00ff));
        SDHost_CMD52_Write(pDev, FN_BLOCK_SIZE_1_REG(i), 0,
                           (SK_U8) (block_size >> 8));
        pDev->sd_ids[i].blocksize = block_size;
    }

/*
 for (i=0;i<8;i++)
 {
	 SDHost_DumpCIS(pDev, i, 512 );
 }
*/

    for (i = 0; i <= pDev->number_of_functions; i++) {
        if (!SDHost_CMD52_Read(pDev, FN_CIS_POINTER_0_REG(i), 0, &R)) {
            break;
        }
        pDev->cisptr[i] = R;
        if (!SDHost_CMD52_Read(pDev, FN_CIS_POINTER_1_REG(i), 0, &R)) {
            pDev->cisptr[i] = 0;
        }
        pDev->cisptr[i] |= (R << 8);
    }

    cislength = DEVICE_NAME_LENGTH;
    if (!SDHost_ReadCIS(pDev, 0, CISTPL_VERS_1, Cis, &cislength)) {
        return SK_FALSE;
    }
    if (cislength > 0) {
        memcpy(pDev->DeviceName, "SDIOBus\\", 8);
        for (i = 0, j = 8; i < cislength; i++) {
            pDev->DeviceName[j++] = (Cis[i] == ' ' ? '_' : Cis[i]);
        }
        pDev->DeviceName[j++] = 0;
        pDev->DeviceNameLength = j;
        DBGPRINT(DBG_W528D,
                 (KERN_DEBUG "       DeviceName (len=%d): %s\n",
                  pDev->DeviceNameLength, pDev->DeviceName));
    }

    cislength = 4;
    if (!SDHost_ReadCIS(pDev, 0, CISTPL_MANFID, Cis, &cislength)) {
        return SK_FALSE;
    }
    if (cislength > 0) {
        unsigned short vendor, device;
        vendor = *((short *) Cis);
        device = *((short *) &Cis[2]);
        DBGPRINT(DBG_LOAD,
                 (KERN_DEBUG "%s: vendor=0x%4.04X device=0x%4.04X\n",
                  __FUNCTION__, vendor, device));
        for (i = 0; i <= pDev->number_of_functions; i++) {
            pDev->sd_ids[i].vendor = vendor;
            pDev->sd_ids[i].device = device;
            pDev->sd_ids[i].fn = i;
            if (i > 0) {
                if (SDHost_ReadCIS(pDev, i, CISTPL_MANFID, Cis, &cislength)) {
                    if (cislength > 0) {
                        pDev->sd_ids[i].vendor = *((short *) Cis);
                        pDev->sd_ids[i].device = *((short *) &Cis[2]);
                    }
                }
                SDHost_CMD52_Read(pDev, FN_CSA_REG(i), 0, &R);
                pDev->sd_ids[i].class = R;
                DBGPRINT(DBG_LOAD,
                         (KERN_DEBUG
                          "fn%d: class=0x%4.04X vendor=0x%4.04X,device=0x%4.04X\n",
                          i, pDev->sd_ids[i].class, pDev->sd_ids[i].vendor,
                          pDev->sd_ids[i].device));
            } else {
                pDev->sd_ids[i].class = 0;
            }
        }
    }
    // Clear irq status bits
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS, 0xFFFF);

                     /*--- mmoser 08.09.2005 ---*/
    pDev->initialized = SK_TRUE;
    return SK_TRUE;
}

/******************************************************************************
 *
 *  SDHost_Init - Initialize the FPGA SDIO/CardBus interface
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *
 */
void
SDHost_Init(PSD_DEVICE_DATA pDev)
{
    SK_U32 ulVal;
    SK_U8 ucHostCtrl = 0;
    SK_U16 usVal;

    ENTER();

    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "SDHost_Init(pDev=0x%p, IOBase=0x%p)\n", pDev,
              pDev->IOBase[0]));

                      /*--- mmoser 4/4/2006 ---*/
    pDev->SurpriseRemoved = 0;

    // issue software reset
    MEM_WRITE_UCHAR(pDev, SK_SLOT_0, STDHOST_SW_RESET, STDHOST_SW_RESET_ALL);

    DBGPRINT(DBG_LOAD, (KERN_DEBUG "SDHost_Init() line=%d\n", __LINE__));

    // Read the Host Controller Version
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_HOST_CONTROLLER_VERSION, &usVal);
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG
              "SD Host Spec Version 0x%2.02X - Vendor Version 0x%2.02x\n",
              GET_SPEC_VERSION_NR(usVal), GET_VENDOR_VERSION_NR(usVal)));

    if (pDev->bus_type == SDIO_BUS_TYPE && pDev->bus_width == SDIO_4_BIT) {
        ucHostCtrl |= STDHOST_4_BIT_ENA;
    }
    // Read capabilities 
    MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_CAPABILITIES, &ulVal);

    if ((ulVal & STDHOST_CAP_VOLTAGE_1_8) == STDHOST_CAP_VOLTAGE_1_8) {
        DBGPRINT(DBG_LOAD, (KERN_DEBUG "1.8V supported\n"));
        pDev->maxSupplyVoltage = STDHOST_VOLTAGE_1_8_V;
    }
    if ((ulVal & STDHOST_CAP_VOLTAGE_3_0) == STDHOST_CAP_VOLTAGE_3_0) {
        DBGPRINT(DBG_LOAD, (KERN_DEBUG "3.0V supported\n"));
        pDev->maxSupplyVoltage = STDHOST_VOLTAGE_3_0_V;
    }
    if ((ulVal & STDHOST_CAP_VOLTAGE_3_3) == STDHOST_CAP_VOLTAGE_3_3) {
        DBGPRINT(DBG_LOAD, (KERN_DEBUG "3.3V supported\n"));
        pDev->maxSupplyVoltage = STDHOST_VOLTAGE_3_3_V;
    }
    if ((ulVal & STDHOST_CAP_SUSPENSE_RESUME) == STDHOST_CAP_SUSPENSE_RESUME) {
        DBGPRINT(DBG_LOAD, (KERN_DEBUG "Suspense/resume supported\n"));
    }
    if ((ulVal & STDHOST_CAP_DMA) == STDHOST_CAP_DMA) {
        DBGPRINT(DBG_LOAD, (KERN_DEBUG "DMA supported\n"));
    }
    if ((ulVal & STDHOST_CAP_HIGH_SPEED) == STDHOST_CAP_HIGH_SPEED) {
        DBGPRINT(DBG_LOAD, (KERN_DEBUG "High speed supported\n"));
        ucHostCtrl |= STDHOST_HIGH_SPEED_ENA;
    }
    pDev->max_block_size = SD_BLOCK_SIZE;
    if ((ulVal & STDHOST_CAP_MAX_BLOCK_512) == STDHOST_CAP_MAX_BLOCK_512) {
        DBGPRINT(DBG_LOAD, (KERN_DEBUG "Max Block Length : 512\n"));
        pDev->max_block_size = 512;
    } else if ((ulVal & STDHOST_CAP_MAX_BLOCK_1024) ==
               STDHOST_CAP_MAX_BLOCK_1024) {
        DBGPRINT(DBG_LOAD, (KERN_DEBUG "Max Block Length : 1024\n"));
        pDev->max_block_size = 1024;
    } else if ((ulVal & STDHOST_CAP_MAX_BLOCK_2048) ==
               STDHOST_CAP_MAX_BLOCK_2048) {
        DBGPRINT(DBG_LOAD, (KERN_DEBUG "Max Block Length : 2048\n"));
        pDev->max_block_size = 2048;
    }
    if (block_size > pDev->max_block_size)
        block_size = pDev->max_block_size;

    pDev->baseClockFreq = GET_CAP_BASE_CLOCK_FREQ(ulVal);

    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "Base clock : %d MHz\n", pDev->baseClockFreq));

    if ((ulVal & STDHOST_CAP_TIMEOUT_CLOCK_UNIT_MHZ) ==
        STDHOST_CAP_TIMEOUT_CLOCK_UNIT_MHZ) {
        DBGPRINT(DBG_LOAD,
                 (KERN_DEBUG "Timeout clock : %d MHz\n",
                  (ulVal & STDHOST_CAP_TIMEOUT_CLOCK_FREQ)));
    } else {
        DBGPRINT(DBG_LOAD, (KERN_DEBUG "Timeout clock : %d kHz\n",
                            (ulVal & STDHOST_CAP_TIMEOUT_CLOCK_FREQ)));
    }

    // Read the max current capabilities
    MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_MAX_CURRENT_CAP, &ulVal);
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "Max. current @1.8V=%dmA @3.0V=%dmA @3.3V=%dmA\n",
              CALC_MAX_CURRENT_IN_MA(GET_MAX_CURRENT_CAP_1_8_V(ulVal)),
              CALC_MAX_CURRENT_IN_MA(GET_MAX_CURRENT_CAP_3_0_V(ulVal)),
              CALC_MAX_CURRENT_IN_MA(GET_MAX_CURRENT_CAP_3_3_V(ulVal))));

    // Setup host control register
    MEM_WRITE_UCHAR(pDev, SK_SLOT_0, STDHOST_HOST_CTRL, ucHostCtrl);

                     /*--- mmoser 14.09.2005 ---*/
    // Reset all pending irq bits 
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS, 0xFFFF);
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_STATUS, 0xFFFF);

    // Enable error irq status
    usVal = 0xFFFF;
    // usVal = STDHOST_ERROR_IRQ_VENDOR_SIG_ENA;
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_STATUS_ENABLE, usVal);

                                                                                                                                                                                        /*--- mmoser 1/2/2007 ---*/
    // Enable GPI interrupts

    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                     STDHOST_ERROR_IRQ_VENDOR_SIG_ENA);

    // Enable card detect
    usVal = 0xFFFF;
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE, usVal);
    usVal = STDHOST_NORMAL_IRQ_CARD_ALL_ENA;
    usVal &= ~STDHOST_NORMAL_IRQ_CARD_SIG_ENA;  // Don't enable card interrupt
                                                // at this point!
    pDev->lastIRQSignalMask = 0;
    pDev->currentIRQSignalMask = usVal;
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_SIGNAL_ENABLE, usVal);

    MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_PRESENT_STATE, &ulVal);
    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "SDHost_Init() : present state=0x%8.08X\n", ulVal));

                     /*--- mmoser 12.09.2005 ---*/
    pDev->initialized = SK_TRUE;

    LEAVE();

}

/******************************************************************************
 *
 * SDHost_isCardPresent  - checks whether a SDIO card is in the SDIO socket
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *     SK_TRUE   card is in socket
 *     SK_FALSE  no card in socket
 *
 */
SK_BOOL
SDHost_isCardPresent(PSD_DEVICE_DATA pDev)
{
    SK_U32 ulVal;
    SK_U16 usVal;

// mmoser 2005-11-21 
                        /*--- mmoser 13.09.2005 ---*/
    if (pDev->SurpriseRemoved) {
        DBGPRINT(DBG_W528D,
                 (KERN_DEBUG
                  "SDHost_isCardPresent: Surprise Removal of Card!\n"));
        return SK_FALSE;
    }

    if (pDev->initialized == SK_FALSE) {
        DBGPRINT(DBG_W528D,
                 (KERN_DEBUG
                  "SDHost_isCardPresent(): SD Host Adapter is NOT initialized. (%d)\n",
                  pDev->initialized));
        return SK_FALSE;
    }
    // Read the Host Controller Version to check if SD Host adapter is
    // available
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_HOST_CONTROLLER_VERSION, &usVal);
    if (usVal == 0xFFFF) {
        // There is probably no SD Host Adapter available
        DBGPRINT(DBG_W528D,
                 (KERN_DEBUG
                  "SDHost_isCardPresent(): SD Host Adapter is NOT present. (%4.04X)\n",
                  usVal));
        return SK_FALSE;
    }
    // Read present state register to check whether a SD card is present
    MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_PRESENT_STATE, &ulVal);
    if ((ulVal & STDHOST_STATE_CARD_INSERTED) == 0) {
        // There is probably no card in the socket
        DBGPRINT(DBG_W528D,
                 (KERN_DEBUG
                  "SDHost_isCardPresent(): Card is NOT present. (%8.08X)\n",
                  ulVal));
        return SK_FALSE;
    }

    return SK_TRUE;
}

/******************************************************************************
 *
 *  SDHost_SendCmd - Send a SDIO command to the card
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *     SK_TRUE on success
 */
SK_BOOL
SDHost_SendCmd(PSD_DEVICE_DATA pDev, SK_U8 cmd, SK_U32 arg, SK_U16 transferMode,
               SK_U32 * pulResp)
{
    SK_U32 retry, max_retry;
    SK_U32 ulVal;
    SK_U16 usCmd;
    SK_U32 ulIrq;
    SK_U16 usSigEna;

    // max_retry depends on actual bus clock speed
    max_retry =
        MAX_WAIT_COMMAND_INHIBIT * (pDev->ClockSpeed ==
                                    0 ? 1 : pDev->ClockSpeed);

    // Read present state register to check whether Command Inhibit (CMD) = 0
    for (retry = 0; retry < max_retry; retry++) {
        MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_PRESENT_STATE, &ulVal);
        if (cmd == 53) {
            if ((ulVal & STDHOST_STATE_CMD_INHIB_DAT) == 0) {
                break;
            }
        } else {
            if ((ulVal & STDHOST_STATE_CMD_INHIB_CMD) == 0) {
                break;
            }
        }
  /*--- mmoser 3/7/2006 ---*/
        if (retry & 0xFFFFFFFE)
            UDELAY(1);
    }
    if (retry >= max_retry) {
        if (cmd == 53) {
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG
                      "SDHost_SendCmd() : Command Inhibit DAT remains busy! (max_retry=%d)\n",
                      max_retry));
            // reset the data line and FIFO
            MEM_WRITE_UCHAR(pDev, SK_SLOT_0,
                            STDHOST_SW_RESET, STDHOST_SW_RESET_DAT_LINE);
            UDELAY(10);         // wait until reset is complete

            MEM_WRITE_UCHAR(pDev, SK_SLOT_0, STDHOST_SW_RESET, 0);
        } else {
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG
                      "SDHost_SendCmd() : Command Inhibit CMD remains busy! (max_retry=%d)\n",
                      max_retry));
            // reset the cmd line
            MEM_WRITE_UCHAR(pDev, SK_SLOT_0,
                            STDHOST_SW_RESET, STDHOST_SW_RESET_CMD_LINE);
            UDELAY(10);         // wait until reset is complete

            MEM_WRITE_UCHAR(pDev, SK_SLOT_0, STDHOST_SW_RESET, 0);
        }
        return SK_FALSE;
    }
    // clear irq bit

    MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS, 0xFFFF0000 |    // ERROR_IRQ_STATUS
                    STDHOST_NORMAL_IRQ_ERROR |
                    STDHOST_NORMAL_IRQ_TRANS_COMPLETE |
                    STDHOST_NORMAL_IRQ_CMD_COMPLETE);

    usCmd = MK_CMD(cmd) | MK_CMD_TYPE(STDHOST_CMD_TYPE_NORMAL);

    switch (cmd) {
    case 0:
        {
            usCmd |= STDHOST_RESP_TYPE_NONE;
            break;
        }
    case 3:
        {
            usCmd |= STDHOST_CMD_INDEX_CHK_ENA |
                STDHOST_CMD_CRC_CHK_ENA | STDHOST_RESP_TYPE_R6;
            break;
        }
    case 5:
        {
            usCmd |= STDHOST_RESP_TYPE_R4;
            break;
        }
    case 7:
        {
            usCmd |= STDHOST_CMD_INDEX_CHK_ENA |
                STDHOST_CMD_CRC_CHK_ENA | STDHOST_RESP_TYPE_R1b;
            break;
        }
    case 52:
    case 53:
        {
            usCmd |= STDHOST_CMD_INDEX_CHK_ENA |
                STDHOST_CMD_CRC_CHK_ENA | STDHOST_RESP_TYPE_R5;
            break;
        }
    case 59:
        {
            usCmd |= STDHOST_RESP_TYPE_NONE;
            break;
        }
    default:
        {
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG
                      "SDHost_SendCmd() : Command 0x%2.02X not supported!\n",
                      cmd));
            return SK_FALSE;
        }
    }

    MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS, &ulVal);

    clear_bit(0, &pDev->cmd_complete_evt.event);

    // mmoser 2007-06-08 : Ensure that CMD_COMPLETE signal is enabled
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                    &usSigEna);
    usSigEna |= STDHOST_NORMAL_IRQ_CMD_COMPLETE_ENA;
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                     usSigEna);

    pDev->errorOccured = 0;

    // initiate the cmd
    MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_CMD_ARGUMENT, arg);
    ulVal = (usCmd << 16) | transferMode;
    MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_TRANSFER_MODE, ulVal);

#ifdef DBG
    if ((pDev->debug_flags & DEBUG_SHOW_REG_10_14) == DEBUG_SHOW_REG_10_14) {
        if (cmd == 53) {
            for (retry = 0; retry < 10; retry++) {
                SK_U32 ulR1, ulR2;
                MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_RESP_BITS_31_0, &ulR1);
                MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_RESP_BITS_63_32, &ulR2);
                DBGPRINT(DBG_W528D,
                         (KERN_DEBUG
                          "retry=%d 63-32 : 0x%8.08X 31-0 : 0x%8.08X\n", retry,
                          ulR2, ulR1));
            }
        }
    }
#endif

     /**/ if (pDev->currentIRQSignalMask & STDHOST_NORMAL_IRQ_CMD_COMPLETE) {
//printk("%s @ %d: Wait CMD complete\n",__FUNCTION__,__LINE__);
        if (SDHost_wait_event(pDev, &pDev->cmd_complete_evt, 100)) {
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG "%s @ %d: Wait CMD complete ---> FAILED !\n",
                      __FUNCTION__, __LINE__));
            return SK_FALSE;
        }
        clear_bit(0, &pDev->cmd_complete_evt.event);
    } else
        /**/ {
        max_retry =
            MAX_WAIT_COMMAND_COMPLETE * 10 * (pDev->ClockSpeed ==
                                              0 ? 1 : pDev->ClockSpeed);
        // Wait for end of command 
        for (retry = 0; retry < max_retry; retry++) {
            MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS, &ulIrq);
            if ((ulIrq & STDHOST_NORMAL_IRQ_CMD_COMPLETE) ==
                STDHOST_NORMAL_IRQ_CMD_COMPLETE) {
                MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS,
                                STDHOST_NORMAL_IRQ_CMD_COMPLETE);

                break;
            }
   /*--- mmoser 3/7/2006 ---*/
            if (retry & 0xFFFFFFFE)
                UDELAY(1);
        }
        if (retry >= max_retry) {

            MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                            &usCmd);
//              printk("status_enable=0x%4.04X\n",usCmd);
            if ((usCmd & STDHOST_NORMAL_IRQ_CMD_COMPLETE_ENA) == 0) {
                usCmd |= STDHOST_NORMAL_IRQ_CMD_COMPLETE_ENA;
                MEM_WRITE_USHORT(pDev, SK_SLOT_0,
                                 STDHOST_NORMAL_IRQ_STATUS_ENABLE, usCmd);
                MEM_READ_USHORT(pDev, SK_SLOT_0,
                                STDHOST_NORMAL_IRQ_STATUS_ENABLE, &usCmd);
//                      printk("RESTORED:status_enable=0x%4.04X\n",usCmd);
            }

            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG
                      "SDHost_SendCmd() : Command not completed! (max_retry=%d)\n",
                      max_retry));
//       printk("#");
            return SK_FALSE;
        }

        }
    MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_RESP_BITS_31_0, pulResp);

    return SK_TRUE;
}

/******************************************************************************
 *
 *  SDHost_SendAbort - Aborts an SDIO CMD53 block transfer
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *     SK_TRUE on success
 */
SK_BOOL
SDHost_SendAbort(PSD_DEVICE_DATA pDev)
{

//      return SDHost_CMD52_Write(pDev, IO_ABORT_REG, 0, 0x03); 

//#ifdef __XXX__
    SK_U32 retry, max_retry;
    SK_U32 ulVal;
    SK_U32 ulArg;
    SK_U16 usCmd;
// SK_U32 ulIrq;

    ulArg = MAKE_SDIO_OFFSET(IO_ABORT_REG) | MAKE_SDIO_FUNCTION(0x00) | 0x07;

    // max_retry depends on actual bus clock speed
    max_retry =
        MAX_WAIT_COMMAND_INHIBIT * (pDev->ClockSpeed ==
                                    0 ? 1 : pDev->ClockSpeed);

    // clear irq bit

    MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS,
                    STDHOST_NORMAL_IRQ_TRANS_COMPLETE |
                    STDHOST_NORMAL_IRQ_CMD_COMPLETE);

                     /*--- mmoser 3/7/2006 ---*/
    MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS, &ulVal);

    usCmd = MK_CMD(52) |
        MK_CMD_TYPE(STDHOST_CMD_TYPE_ABORT) |
        STDHOST_CMD_INDEX_CHK_ENA |
        STDHOST_CMD_CRC_CHK_ENA | STDHOST_RESP_TYPE_R5;

    // initiate the cmd
    MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_CMD_ARGUMENT, ulArg);

    ulVal = (usCmd << 16) | STDHOST_READ_DIR_SELECT;
    MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_TRANSFER_MODE, ulVal);

    // Read present state register to check whether Command Inhibit (CMD) = 0
    for (retry = 0; retry < max_retry; retry++) {
        MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_PRESENT_STATE, &ulVal);
        if (((ulVal & STDHOST_STATE_CMD_INHIB_DAT) == 0) &&
            ((ulVal & STDHOST_STATE_CMD_INHIB_CMD) == 0)) {
            break;
        }
        UDELAY(1);
    }
    if (retry >= max_retry) {
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG
                  "SDHost_SendAbort() : Command Inhibit (CMD/DAT) remains busy! (max_retry=%d)\n",
                  max_retry));
        return SK_FALSE;
    }

    return SK_TRUE;
//#endif
}

/******************************************************************************
 *
 *   isCmdFailed - checks whether the FPGA reports an error condition
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *     SK_TRUE on success
 *
 */
SK_BOOL
isCmdFailed(PSD_DEVICE_DATA pDev)
{
    SK_U16 errStatus;

    // Read error status register
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_STATUS, &errStatus);

    if ((errStatus & 0x00FF) != 0) {
        printk("%s: 0x%4.04X: ", __FUNCTION__, errStatus);
        if (errStatus & STDHOST_ERROR_IRQ_CURRENT_LIMIT) {
            printk("CURRENT_LIMIT ");
        }
        if (errStatus & STDHOST_ERROR_IRQ_DATA_END_BIT) {
            printk("DATA_END_BIT ");
        }
        if (errStatus & STDHOST_ERROR_IRQ_DATA_CRC) {
            printk("DATA_CRC ");
        }
        if (errStatus & STDHOST_ERROR_IRQ_DATA_TIMEOUT) {
            printk("DATA_TIMEOUT ");
        }
        if (errStatus & STDHOST_ERROR_IRQ_CMD_INDEX) {
            printk("CMD_INDEX ");
        }
        if (errStatus & STDHOST_ERROR_IRQ_CMD_END_BIT) {
            printk("CMD_END_BIT ");
        }
        if (errStatus & STDHOST_ERROR_IRQ_CMD_CRC) {
            printk("CMD_CRC ");
        } else if (errStatus & STDHOST_ERROR_IRQ_CMD_TIMEOUT) {
            printk("CMD_TIMEOUT ");
        }
        printk("\n");

        // Reset the error status irq register
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_STATUS, 0x00FF);
        return SK_TRUE;
    } else {
        return SK_FALSE;
    }
}

/******************************************************************************
 *
 *   SDHost_CMD52_Read - Read one byte from the SDIO card
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *     SK_TRUE on success
 *
 */
SK_BOOL
SDHost_CMD52_Read(PSD_DEVICE_DATA pDev, SK_U32 Offset, SK_U8 function_number,
                  SK_U8 * pReturnData)
{
    SK_U8 Resp;
    SK_U32 ulResponse;
    SK_U8 D8, D16;
    SK_U32 retry, Arg;

                                                                                                                                                                        /*--- mmoser 8/8/2007 ---*/
    SK_U16 lastIRQMask;
    SK_U16 lastErrorIRQMask;

                      /*--- mmoser 3/31/2006 ---*/
    if (pDev->SurpriseRemoved == SK_TRUE) {
        printk
            ("CMD52 READ FAILED : Surprise Removed !!!  fn=%2.02X reg=%8.08X (bus_errors=%d)\n",
             function_number, Offset, pDev->bus_errors);
        return SK_FALSE;
    }

                                                                                                                                                                        /*--- mmoser 8/8/2007 ---*/
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                    &lastIRQMask);
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                    &lastErrorIRQMask);

    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                     (lastIRQMask & (~STDHOST_NORMAL_IRQ_CARD)));
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                     (lastErrorIRQMask & (~STDHOST_ERROR_IRQ_VENDOR_ENA)));

#ifdef FIFO_CMD52_DEBUG
    MEM_READ_ULONG(pDev, SK_SLOT_0, 0x0204, &ulResponse);
    DBGPRINT(DBG_ERROR,
             (KERN_DEBUG "(3)RD Fifo Stat   : 0x%x  \n", ulResponse));

    if (ulResponse != 0x40010000) {
        SK_U32 tmp;
        SK_U16 i;
        printk("(3)RD Fifo Stat   : 0x%x  \n", ulResponse);
        for (i = 0; i < (ulResponse & 0x3ff); i++)
            MEM_READ_ULONG(pDev, SK_SLOT_0, 0x20, &tmp);
    }

    MEM_READ_ULONG(pDev, SK_SLOT_0, 0x0208, &ulResponse);
    DBGPRINT(DBG_ERROR,
             (KERN_DEBUG "(3)WR Fifo Stat   : 0x%x  \n", ulResponse));
    if (ulResponse != 0x40000000)
        printk("(3)WR Fifo Stat   : 0x%x  \n", ulResponse);
#endif

    Arg = MAKE_SDIO_OFFSET(Offset) | MAKE_SDIO_FUNCTION(function_number);

    retry = 0;
    while (1) {
        if (SDHost_SendCmd(pDev, 52, Arg, STDHOST_READ_DIR_SELECT, &ulResponse)) {
            break;
        }
        if (retry++ > 5) {
/*
		 if (++pDev->bus_errors > MAX_BUS_ERRORS )
		 {
			 SDHost_SetCardOutEvent(pDev);
		 }
*/
            if (!SDHost_isCardPresent(pDev)) {
                pDev->SurpriseRemoved = SK_TRUE;
                SDHost_SetCardOutEvent(pDev);
            }
                                                                                                                                                                        /*--- mmoser 8/8/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);

            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG
                      "CMD52 READ FAILED : fn=%2.02X reg=%8.08X (bus_errors=%d)\n",
                      function_number, Offset, pDev->bus_errors));
            return SK_FALSE;
        }
// mmoser 2006-02-22
        if (retry > 1) {
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG
                      "CMD52 READ FAILED : fn=%2.02X reg=%8.08X (bus_errors=%d) retry=%d\n",
                      function_number, Offset, pDev->bus_errors, retry));
        }

        UDELAY(200);
    }

    D16 = GET_BITS_23_16(ulResponse);

    Resp = D16;

    if (pDev->bus_type == SDIO_BUS_TYPE) {
        // SDIO bus

        // mask 11001011
        if (Resp & 0xCB) {
            DBGPRINT(DBG_W528D_CMD52 | DBG_ERROR,
                     (KERN_DEBUG
                      "CMD52 READ fn=%2.02X reg=%8.08X RESP Error:0x%2.02X (0x%8.08X)\n",
                      function_number, Offset, Resp, ulResponse));
            UDELAY(100);
                                                                                                                                                                        /*--- mmoser 8/8/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            return SK_FALSE;
        } else if ((Resp & 0x30) == 0) {
            DBGPRINT(DBG_W528D_CMD52 | DBG_ERROR,
                     (KERN_DEBUG
                      "CMD52 READ fn=%2.02X reg=%8.08X RESP Error,card not selected!:0x%2.02X (0x%8.08X)\n",
                      function_number, Offset, Resp, ulResponse));
            UDELAY(100);
                                                                                                                                                                        /*--- mmoser 8/8/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            return SK_FALSE;
        }
    } else {
        // SPI bus

        if (Resp & 0x5C) {
            DBGPRINT(DBG_W528D_CMD52 | DBG_ERROR,
                     (KERN_DEBUG "CMD52 READ RESP Error:0x%.2X\n", Resp));
                                                                                                                                                                        /*--- mmoser 8/8/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            return SK_FALSE;
        }
    }
    if (isCmdFailed(pDev)) {
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                         lastIRQMask);
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                         lastErrorIRQMask);
        return SK_FALSE;
    }

    D8 = GET_BITS_15_08(ulResponse);
    *pReturnData = D8;

    if (pDev->bus_errors > 0) {
        pDev->bus_errors--;
    }

    DBGPRINT(DBG_W528D_CMD52,
             (KERN_DEBUG
              "CMD52 READ  : fn=%2.02X reg=%8.08X data=%2.02X (%2.02X %2.02X/%8.08X)\n",
              function_number, Offset, *pReturnData, D16, D8, ulResponse));
                                                                                                                                                                        /*--- mmoser 8/8/2007 ---*/
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                     lastIRQMask);
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                     lastErrorIRQMask);
    return SK_TRUE;
}

/******************************************************************************
 *
 *   SDHost_CMD52_Write - Write one byte from the SDIO card
 *   -
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *     SK_TRUE on success
 *
 */
SK_BOOL
SDHost_CMD52_Write(PSD_DEVICE_DATA pDev, SK_U32 Offset, SK_U8 function_number,
                   SK_U8 Data)
{
    SK_U8 Resp;
    SK_U32 ulResponse;
    SK_U32 Arg, retry;
                                                                                                                                                                        /*--- mmoser 8/8/2007 ---*/
    SK_U16 lastIRQMask;
    SK_U16 lastErrorIRQMask;

                      /*--- mmoser 3/31/2006 ---*/
    if (pDev->SurpriseRemoved == SK_TRUE) {
        printk
            ("CMD52 WRITE : Surprise Removed !!!! fn=%2.02X reg=%8.08X data=%2.02X (bus_errors=%d)\n",
             function_number, Offset, Data, pDev->bus_errors);
        return SK_FALSE;
    }

                                                                                                                                                                        /*--- mmoser 8/8/2007 ---*/
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                    &lastIRQMask);
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                    &lastErrorIRQMask);

    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                     (lastIRQMask & (~STDHOST_NORMAL_IRQ_CARD)));
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                     (lastErrorIRQMask & (~STDHOST_ERROR_IRQ_VENDOR_ENA)));

    DBGPRINT(DBG_W528D_CMD52,
             (KERN_DEBUG
              "CMD52 WRITE : fn=%2.02X reg=%8.08X data=%2.02X (bus_errors=%d)\n",
              function_number, Offset, Data, pDev->bus_errors));

#ifdef FIFO_CMD52_DEBUG
    MEM_READ_ULONG(pDev, SK_SLOT_0, 0x0204, &ulResponse);
    DBGPRINT(DBG_ERROR,
             (KERN_DEBUG "(4)RD Fifo Stat   : 0x%x  \n", ulResponse));

    if (ulResponse != 0x40010000) {
        SK_U32 tmp;
        SK_U16 i;
        printk("(4)RD Fifo Stat   : 0x%x  \n", ulResponse);
        for (i = 0; i < (ulResponse & 0x3ff); i++)
            MEM_READ_ULONG(pDev, SK_SLOT_0, 0x20, &tmp);
    }

    MEM_READ_ULONG(pDev, SK_SLOT_0, 0x0208, &ulResponse);
    DBGPRINT(DBG_ERROR,
             (KERN_DEBUG "(4)WR Fifo Stat   : 0x%x  \n", ulResponse));

    if (ulResponse != 0x40000000)
        printk("(4)WR Fifo Stat   : 0x%x  \n", ulResponse);
#endif

    Arg = MAKE_SDIO_OFFSET(Offset) |
        MAKE_SDIO_FUNCTION(function_number) | MAKE_SDIO_DIR(1) | (SK_U8) Data;
    retry = 0;
    while (1) {

        if (SDHost_SendCmd(pDev, 52, Arg, STDHOST_READ_DIR_SELECT, &ulResponse)) {
            break;
        }
        if (retry++ > 5) {
            if (!SDHost_isCardPresent(pDev)) {
                pDev->SurpriseRemoved = SK_TRUE;
                SDHost_SetCardOutEvent(pDev);
            }

            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG
                      "CMD52 WRITE FAILED : fn=%2.02X reg=%8.08X data=%2.02X\n",
                      function_number, Offset, Data));
                                                                                                                                                                        /*--- mmoser 8/8/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            return SK_FALSE;
        }
        UDELAY(200);
    }

    Resp = GET_BITS_23_16(ulResponse);

    if (pDev->bus_type == SDIO_BUS_TYPE) {
        // SDIO bus

        // mask 11001011
        if (Resp & 0xCB) {
            DBGPRINT(DBG_W528D_CMD52 | DBG_ERROR,
                     (KERN_DEBUG
                      "CMD52 READ fn=%2.02X reg=%8.08X RESP Error:0x%2.02X (0x%8.08X)\n",
                      function_number, Offset, Resp, ulResponse));
                                                                                                                                                                        /*--- mmoser 8/8/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            return SK_FALSE;
        } else if ((Resp & 0x30) == 0) {
            DBGPRINT(DBG_W528D_CMD52 | DBG_ERROR,
                     (KERN_DEBUG
                      "CMD52 READ fn=%2.02X reg=%8.08X RESP Error,card not selected!:0x%2.02X (0x%8.08X)\n",
                      function_number, Offset, Resp, ulResponse));
                                                                                                                                                                        /*--- mmoser 8/8/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            return SK_FALSE;
        }
    } else {
        // SPI bus

        if (Resp & 0x5C) {
            DBGPRINT(DBG_W528D_CMD52 | DBG_ERROR,
                     (KERN_DEBUG "CMD52 READ RESP Error:0x%.2X\n", Resp));
                                                                                                                                                                        /*--- mmoser 8/8/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            return SK_FALSE;
        }
    }

    if (isCmdFailed(pDev)) {
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                         lastIRQMask);
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                         lastErrorIRQMask);
        return SK_FALSE;
    }
    if (pDev->bus_errors > 0) {
        pDev->bus_errors--;
    }

                                                                                                                                                                        /*--- mmoser 8/8/2007 ---*/
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                     lastIRQMask);
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                     lastErrorIRQMask);
    return SK_TRUE;
}

/******************************************************************************
 *
 *  SDHost_ReadRCA - Read the RCA from SDIO card
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *     SK_TRUE on success
 *
 */
SK_BOOL
SDHost_ReadRCA(PSD_DEVICE_DATA pDev, SK_U8 * pRca)
{
    SK_U8 ucStatus1, ucStatus2;
    SK_U32 ulResponse;

    // CMD3 to read card RCA.
    if (!SDHost_SendCmd(pDev, 3, 0, 0, &ulResponse)) {
        if (++pDev->bus_errors > MAX_BUS_ERRORS) {
            SDHost_SetCardOutEvent(pDev);
        }
        return SK_FALSE;
    }

    MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_RESP_BITS_31_0, &ulResponse);

    pRca[0] = GET_BITS_39_32(ulResponse);
    pRca[1] = GET_BITS_31_24(ulResponse);

    ucStatus1 = GET_BITS_23_16(ulResponse);
    ucStatus2 = GET_BITS_15_08(ulResponse);

    DBGPRINT(DBG_W528D,
             (KERN_DEBUG
              "SDHost_ReadRCA: %2.02X%2.02X - CardStatus=%2.02X%2.02X\n",
              pRca[0], pRca[1], ucStatus1 &= 0xE0, ucStatus2));
    return SK_TRUE;
}

SK_BOOL
is_ValidCISTuple(SK_U8 cistpl)
{
    switch (cistpl) {
    case CISTPL_VERS_1:
    case CISTPL_MANFID:
    case CISTPL_FUNCID:
    case CISTPL_FUNCE:
    case CISTPL_END:
        {
            return SK_TRUE;
        }
    default:
        {
            return SK_FALSE;
        }
    }
}

/******************************************************************************
 *
 * SDHost_ReadCIS  - Read element 'cistpl' from the card's CIS
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *     SK_TRUE on success
 *
 */
SK_BOOL
SDHost_ReadCIS(PSD_DEVICE_DATA pDev, SK_U8 function_number, SK_U8 cistpl,
               SK_U8 * pBuf, SK_U32 * length)
{
    SK_U8 R;
    SK_U8 tuple;
    SK_U8 tuple_len;
    SK_U8 tuple_end = 0;
    SK_U32 _cis = 0;

    if (pBuf == NULL || length == NULL || *length == 0) {
        DBGPRINT(DBG_W528D | DBG_ERROR,
                 (KERN_DEBUG
                  "SDHost_ReadCIS : Illegal Parameters pBuf=%p length=%d\n",
                  pBuf, *length));
        return SK_FALSE;
    }

    if (!SDHost_CMD52_Read(pDev, FN_CIS_POINTER_0_REG(function_number), 0, &R)) {
        return SK_FALSE;
    }
    _cis = R;
    if (!SDHost_CMD52_Read(pDev, FN_CIS_POINTER_1_REG(function_number), 0, &R)) {
        return SK_FALSE;
    }
    _cis |= (R << 8);
    if (!SDHost_CMD52_Read(pDev, FN_CIS_POINTER_2_REG(function_number), 0, &R)) {
        return SK_FALSE;
    }
    _cis |= (R << 16);

    // Search for CISTPL
    if (_cis != 0) {
        SK_U32 i = 0;
        SK_U32 j = 0;

        DBGPRINT(DBG_W528D, (KERN_DEBUG "SDHost_ReadCIS : CIS=%8.08X\n", _cis));

        R = 0;
        tuple_len = 0;
        while (R != CISTPL_END) {
            // Read Tuple
            SDHost_CMD52_Read(pDev, _cis + j, function_number, &tuple);
            j++;

            R = tuple;
            if (!is_ValidCISTuple(R)) {
                continue;
            }
            // Read Tuple Length
            SDHost_CMD52_Read(pDev, _cis + j, function_number, &tuple_len);
            j++;
            R = tuple;

//                      printk("cistpl=0x%2.02X tuple=0x%2.02X tuple_length=%d j=%d\n",cistpl,tuple,tuple_len,j);
            if (R != CISTPL_NULL && R != CISTPL_END) {
                tuple_end = j + tuple_len;
            }

            if (tuple == cistpl) {
                switch (cistpl) {
                case CISTPL_VERS_1:
                    {
                        j += 2;
                        i = 0;

                        while (j < tuple_end && i < *length - 1) {
                            SDHost_CMD52_Read(pDev, _cis + j, function_number,
                                              &R);
                            j++;
//                                                      printk("%2.02X ",R);
                            pBuf[i++] = (R == 0 ? ' ' : R);
                        }
                        i -= 3;
                        pBuf[i] = 0;
                        *length = i;

                        DBGPRINT(DBG_W528D,
                                 (KERN_DEBUG
                                  "SDHost_ReadCIS : fn=%d cistpl=%2.02X |%s| len=%d\n",
                                  function_number, cistpl, pBuf, *length));
//                                              printk("\nCISTPL_VERS_1 found!\n");
                        return SK_TRUE;
                        break;
                    }
                case CISTPL_MANFID:
                case CISTPL_FUNCID:
                case CISTPL_FUNCE:
                    {
                        i = 0;
                        DBGPRINT(DBG_W528D,
                                 ("SDHost_ReadCIS : fn=%d cistpl=%2.02X |",
                                  function_number, cistpl));
                        while (j <= tuple_end && i < *length) {
                            SDHost_CMD52_Read(pDev, _cis + j, function_number,
                                              &R);
                            j++;
                            pBuf[i++] = R;
//                                                      printk("%2.02X ",R);

                            DBGPRINT(DBG_W528D, ("%2.02x ", R));
                        }
                        *length = i;

                        DBGPRINT(DBG_W528D, ("| len=%d\n", *length));

//                                              printk("\ncistpl=0x%2.02X found!\n",cistpl);

                        return SK_TRUE;

                        break;
                    }
                }
            } else {
                j = tuple_end;
            }
        }
    }
    DBGPRINT(DBG_W528D,
             (KERN_DEBUG "SDHost_ReadCIS : fn=%d cistpl=%2.02X |%s| len=%d\n",
              function_number, cistpl, pBuf, *length));

    *length = 0;
    return SK_FALSE;
}

/******************************************************************************
 *
 *  SDHost_ReadOCR - Read the OCR (supported voltages) from the SDIO card
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *     SK_TRUE on success
 *
 */
SK_BOOL
SDHost_ReadOCR(PSD_DEVICE_DATA pDev, SK_U8 * pOcr)
{
    SK_U32 i, j;
    SK_U32 ulResponse;
    SK_U8 ucValue = 0;
// SK_U8  R1,R2,R3,R4,R5,R6;
    SK_U8 R2, R3, R4, R5;

    R2 = R3 = R4 = R5 = 0;
    j = 0;
    while (j++ < 5) {
        // Wait until power cycle is complete
        for (i = 0; i < 100; i++) {
            if (!SDHost_SendCmd(pDev, 5, 0, 0, &ulResponse)) {
                if (++pDev->bus_errors > MAX_BUS_ERRORS) {
                    SDHost_SetCardOutEvent(pDev);
                    return SK_FALSE;
                }
                DBGPRINT(DBG_W528D,
                         (KERN_DEBUG "SDHost_ReadOCR() retry=%d\n", j));

                continue;
            }

            R2 = GET_BITS_15_08(ulResponse);
            R3 = GET_BITS_23_16(ulResponse);
            R4 = GET_BITS_31_24(ulResponse);
            R5 = GET_BITS_39_32(ulResponse);

            DBGPRINT(DBG_W528D,
                     (KERN_DEBUG
                      "SDHost_ReadOCR(31-0) retry=%d  : 0x%2.02X %2.02X %2.02X %2.02X\n",
                      j, R5, R4, R3, R2));

            pDev->number_of_functions = (R5 >> 4) & 0x07;
            ucValue = GET_BITS_39_32(ulResponse);

            DBGPRINT(DBG_W528D,
                     (KERN_DEBUG "SDHost_ReadOCR(%d) : 0x%2.02X\n",
                      pDev->bus_type, ucValue));
            DBGPRINT(DBG_W528D,
                     (KERN_DEBUG "SDHost_ReadOCR() : 0x%8.08X\n", ulResponse));

            if (ucValue & 0x80) {
                // power cycle is complete
                DBGPRINT(DBG_W528D,
                         (KERN_DEBUG
                          "SDHost_ReadOCR() : power cycle is complete\n"));
                break;
            }
        }
        if (i == 100) {
            DBGPRINT(DBG_W528D | DBG_ERROR,
                     (KERN_DEBUG
                      "SDHost_ReadOCR() FAILED : Power cycle not complete after %d retries (0x%2.02X)!\n",
                      i, ucValue));
            return SK_FALSE;
        }

        pOcr[0] = R4;
        pOcr[1] = R3;
        pOcr[2] = R2;

        if (R4 != 0 || R3 != 0) {
            DBGPRINT(DBG_W528D,
                     (KERN_DEBUG
                      "SDHost_ReadOCR() : PwrCycle:%X,IO_n:%X,Mem_p:%X,OCR:[0x%2.02X%2.02X%2.02X]\n",
                      ucValue & 0x80, ucValue & 0x70, ucValue & 0x40, pOcr[0],
                      pOcr[1], pOcr[2]));

            return SK_TRUE;
        }
    }

    DBGPRINT(DBG_ERROR,
             (KERN_DEBUG "SDHost_ReadOCR() FAILED : Cannot read OCR!\n"));
    return SK_FALSE;
}

/******************************************************************************
 *
 *  SDHost_WriteOCR - Write the OCR (supported voltages) to the SDIO card
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *					SK_TRUE on success
 *
 */
SK_BOOL
SDHost_WriteOCR(PSD_DEVICE_DATA pDev, SK_U8 * pOcr)
{
    SK_U32 ulArg;
    SK_U32 ulResponse;
    SK_U8 ucValue;
    SK_U32 i;

    ulArg = pOcr[0] << 16 | pOcr[1] << 8 | pOcr[2];
    // Wait until power cycle is complete
    for (i = 0; i < 100; i++) {
        if (!SDHost_SendCmd(pDev, 5, ulArg, 0, &ulResponse)) {
            if (++pDev->bus_errors > MAX_BUS_ERRORS) {
                SDHost_SetCardOutEvent(pDev);
            }
            return SK_FALSE;
        }

        MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_RESP_BITS_31_0, &ulResponse);

        ucValue = GET_BITS_39_32(ulResponse);

        if (ucValue & 0x80) {
            // power cycle is complete
            DBGPRINT(DBG_W528D,
                     (KERN_DEBUG
                      "I/O Power up status is ready and set new voltage ok!\n"));
            break;
        }
    }
    if (i == 100) {
        DBGPRINT(DBG_W528D | DBG_ERROR,
                 (KERN_DEBUG
                  "SDHost_WriteOCR() FAILED : Power cycle not complete after %d retries!\n",
                  i));
        return SK_FALSE;
    }
    DBGPRINT(DBG_W528D,
             (KERN_DEBUG "SDHost_WriteOCR() : PwrCycle:%X,IO_n:%X,Mem_p:%X\n",
              ucValue & 0x80, ucValue & 0x70, ucValue & 0x40));

    return SK_TRUE;
}

/******************************************************************************
 *
 *   SDHost_CMD53_Read - Read a block of data from the SDIO card
 *   -
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *     SK_TRUE on success
 *
 */
SK_BOOL
SDHost_CMD53_Read(PSD_DEVICE_DATA pDev,
                  SK_U32 Offset,
                  SK_U8 function_number,
                  SK_U8 mode,
                  SK_U8 opcode, SK_U8 * pData, SK_U32 Count, SK_U32 blksz)
{
    SK_U16 uwTransferMode;
    SK_U32 size;
    SK_U32 i, blkCnt, Arg;
    SK_U32 ulResponse, retry, max_retry;
    SK_U32 *pSrc32;
    SK_U32 *pDest32;
    SK_U8 RESP;

    DBGPRINT(DBG_ERROR,
             (KERN_DEBUG "CMD53 READ fn=%2.02X reg=%8.08X data=0x%2.0X\n",
              function_number, Offset, *(pData)));
                      /*--- mmoser 3/31/2006 ---*/
    if (pDev->SurpriseRemoved == SK_TRUE) {
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG
                  "CMD53 READ FAILED : Surprise Removed !!!  fn=%2.02X reg=%8.08X (bus_errors=%d)\n",
                  function_number, Offset, pDev->bus_errors));
        return SK_FALSE;
    }

    if (Count > 0x1FF) {
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG
                  "CMD53 %s READ : fn=%2.02X reg=%8.08X len=%d > 512 ERROR!!!!!!\n",
                  (mode ? "BLOCK" : "BYTE"), function_number, Offset, Count));
        return SK_FALSE;
    }
    DBGPRINT(DBG_W528D_CMD53,
             (KERN_DEBUG "CMD53 %s READ : fn=%2.02X reg=%8.08X len=%d\n",
              (mode ? "BLOCK" : "BYTE"), function_number, Offset, Count));

    if (mode && (blksz != pDev->sd_ids[function_number].blocksize))     // block 
                                                                        // mode
    {
        SDHost_CMD52_Write(pDev, FN_BLOCK_SIZE_0_REG(function_number), 0,
                           (SK_U8) (blksz & 0x00ff));
        SDHost_CMD52_Write(pDev, FN_BLOCK_SIZE_1_REG(function_number), 0,
                           (SK_U8) (blksz >> 8));
        pDev->sd_ids[function_number].blocksize = blksz;
    }

    uwTransferMode = STDHOST_READ_DIR_SELECT;

    if (mode == BYTE_MODE) {
        size = Count;

        blkCnt = 1 << 16 | Count;
        MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_BLOCK_SIZE, blkCnt);
    } else {
        size = blksz * Count;

/* 20050630 mmoser HW-Workaround */
        if ((pDev->debug_flags & DEBUG_DISABLE_HW_WORKAROUND_RD) ==
            DEBUG_DISABLE_HW_WORKAROUND_RD) {
            uwTransferMode |= STDHOST_MULTI_BLOCK_SELECT;
        } else {
            if (Count > 1) {
                uwTransferMode |= STDHOST_MULTI_BLOCK_SELECT;
            }
        }
        uwTransferMode |= STDHOST_BLOCK_COUNT_ENA;

        blkCnt = Count << 16 | blksz | DMA_16K_BOUNDARY;

        MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_BLOCK_SIZE, blkCnt);
    }

    Arg = MAKE_SDIO_OFFSET(Offset) |
        MAKE_SDIO_OP_CODE(opcode) |
        MAKE_SDIO_BLOCK_MODE(mode) |
        MAKE_SDIO_FUNCTION(function_number) | (Count & 0x01FF);

    retry = 0;
    while (1) {
        if (SDHost_SendCmd(pDev, 53, Arg, uwTransferMode, &ulResponse)) {
            break;
        }
        if (retry++ > 5) {
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG "CMD53 READ FAILED : fn=%2.02X reg=%8.08X\n",
                      function_number, Offset));
            return SK_FALSE;
        }
        UDELAY(5);
    }

    RESP = GET_BITS_23_16(ulResponse);
    if (pDev->bus_type == SDIO_BUS_TYPE) {
        if (RESP & 0xCB) {
            DBGPRINT(DBG_W528D_CMD53,
                     (KERN_DEBUG "CMD53 Read RESP Error:0x%.2X\n", RESP));
            return SK_FALSE;
        } else if ((RESP & 0x30) == 0) {
            DBGPRINT(DBG_W528D_CMD53 | DBG_ERROR,
                     (KERN_DEBUG
                      "CMD53 Read RESP Error,card not selected!:0x%.2X\n",
                      RESP));
            return SK_FALSE;
        }
    } else {
        if (RESP & 0x5C) {
            DBGPRINT(DBG_W528D_CMD52 | DBG_ERROR,
                     (KERN_DEBUG "CMD53 READ RESP Error:0x%.2X\n", RESP));
            return SK_FALSE;
        }
    }

    pSrc32 =
        (SK_U32 *) (((SK_U32) (SK_BAR((pDev), (SK_SLOT_0)))) +
                    (STDHOST_DATA_PORT));
    pDest32 = (SK_U32 *) pData;

    retry = 0;
    max_retry =
        MAX_WAIT_COMMAND_COMPLETE * (pDev->ClockSpeed ==
                                     0 ? 1 : pDev->ClockSpeed);

    i = 0;
    while (i < size) {
        if (i % blksz == 0) {
            retry = 0;

            do {
                MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_PRESENT_STATE,
                               &ulResponse);
                if ((ulResponse & STDHOST_STATE_BUFFER_RD_ENABLE) ==
                    STDHOST_STATE_BUFFER_RD_ENABLE) {
                    break;
                }

    /*--- mmoser 3/13/2006 ---*/
                UDELAY(1);
            } while (++retry < max_retry);
   /*--- mmoser 3/3/2006 ---*/
            if (retry >= max_retry) {
                DBGPRINT(DBG_ERROR,
                         (KERN_DEBUG
                          "CMD53 READ FAILED : STDHOST_STATE_BUFFER_RD_ENABLE remains 0\nfn=%2.02X reg=%8.08X\n",
                          function_number, Offset));
                if (mode) {
                    SDHost_SendAbort(pDev);
                }
                return SK_FALSE;
            }
        }
        *pDest32 = *pSrc32;
        pDest32++;
        i += 4;
    }

/*
 {
  SK_U16 i;
  
  printk( KERN_DEBUG "\n%s\n", __FUNCTION__ );
  for( i=0; i<size; i++ ) {
   if( i % 16 ==0 )
    printk( "\n");

   printk( "%2.02X ", pData[i] );
   
  }
 }
*/

    if (mode) {
        SDHost_SendAbort(pDev);
    }
 /*--- mmoser 1/5/2007 ---*/
// STDHOST_NORMAL_IRQ_ERROR is used for GPI interrupts !!!!
//
/*
 MEM_READ_ULONG (pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS, &ulVal);
 if (ulVal&STDHOST_NORMAL_IRQ_ERROR)
 {
  SDHost_ErrorRecovery(pDev);
  DBGPRINT(DBG_ERROR,( KERN_DEBUG "SDHost_CMD53_Read() : Command Error 0x%8.08X\n",ulVal));
  return SK_FALSE; 
 }
*/
    return SK_TRUE;
}

/******************************************************************************
 *
 *   SDHost_CMD53_Read_DMA - Read a block of data from the SDIO card
 *   -
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *     SK_TRUE on success
 *
 */
SK_BOOL
SDHost_CMD53_Read_DMA(PSD_DEVICE_DATA pDev,
                      SK_U32 Offset,
                      SK_U8 function_number,
                      SK_U8 mode,
                      SK_U8 opcode, SK_U8 * pData, SK_U32 Count, SK_U32 blksz)
{
    SK_U16 uwTransferMode;
    SK_U32 size;
    SK_U32 blkCnt, Arg;
    SK_U32 ulResponse, retry, max_retry;
    SK_U8 RESP;
    SK_U64 PhysAddr;
    SK_U32 ulResp;

                                                                                                                                                                         /*--- mmoser 1/10/2007 ---*/
    SK_U16 lastIRQMask;
    SK_U16 lastErrorIRQMask;

 /*--- mmoser 3/31/2006 ---*/
    if (pDev->SurpriseRemoved == SK_TRUE) {
        printk
            ("CMD53 READ FAILED : Surprise Removed !!!  fn=%2.02X reg=%8.08X (bus_errors=%d)\n",
             function_number, Offset, pDev->bus_errors);
        return SK_FALSE;
    }

    if (Count > 0x1FF) {
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG
                  "CMD53 %s READ : fn=%2.02X reg=%8.08X len=%d > 512 ERROR!!!!!!\n",
                  (mode ? "BLOCK" : "BYTE"), function_number, Offset, Count));
        return SK_FALSE;
    }
    clear_bit(0, &pDev->trans_complete_evt.event);
                                                                                                                                                                         /*--- mmoser 1/10/2007 ---*/
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                    &lastIRQMask);
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                    &lastErrorIRQMask);

    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                     (lastIRQMask & (~STDHOST_NORMAL_IRQ_CARD)));
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                     (lastErrorIRQMask & (~STDHOST_ERROR_IRQ_VENDOR_ENA)));

    DBGPRINT(DBG_W528D_CMD53,
             (KERN_DEBUG "CMD53 %s READ : fn=%2.02X reg=%8.08X len=%d\n",
              (mode ? "BLOCK" : "BYTE"), function_number, Offset, Count));

    if (mode && (blksz != pDev->sd_ids[function_number].blocksize))     // block 
                                                                        // mode
    {
        SDHost_CMD52_Write(pDev, FN_BLOCK_SIZE_0_REG(function_number), 0,
                           (SK_U8) (blksz & 0x00ff));
        SDHost_CMD52_Write(pDev, FN_BLOCK_SIZE_1_REG(function_number), 0,
                           (SK_U8) (blksz >> 8));
        pDev->sd_ids[function_number].blocksize = blksz;
    }

    uwTransferMode = STDHOST_READ_DIR_SELECT;

    if (mode == BYTE_MODE) {
        size = Count;
        blkCnt = 1 << 16 | Count;
        uwTransferMode |= STDHOST_DMA_ENA;
        MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_BLOCK_SIZE, blkCnt);
    } else {
        size = blksz * Count;

        /* 20050630 mmoser HW-Workaround */
        if ((pDev->debug_flags & DEBUG_DISABLE_HW_WORKAROUND_RD) ==
            DEBUG_DISABLE_HW_WORKAROUND_RD) {
            uwTransferMode |= STDHOST_MULTI_BLOCK_SELECT;
        } else {
            if (Count > 1) {
                uwTransferMode |= STDHOST_MULTI_BLOCK_SELECT;
            }
        }

        uwTransferMode |= STDHOST_BLOCK_COUNT_ENA | STDHOST_DMA_ENA;

        blkCnt = Count << 16 | blksz | DMA_16K_BOUNDARY;

        DBGPRINT(DBG_ERROR, (KERN_DEBUG "blkCnt:   0x%x  \n", blkCnt));

        MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_BLOCK_SIZE, blkCnt);
    }

    Arg = MAKE_SDIO_OFFSET(Offset) |
        MAKE_SDIO_OP_CODE(opcode) |
        MAKE_SDIO_BLOCK_MODE(mode) |
        MAKE_SDIO_FUNCTION(function_number) | (Count & 0x01FF);

#ifdef SYSKT_DMA_MALIGN_TEST
    // set the DMA address
    PhysAddr = (SK_U64) pci_map_page(pDev->dev,
                                     virt_to_page(pDev->dma_rbuff),
                                     ((unsigned long) (pDev->
                                                       dma_rbuff) & ~PAGE_MASK),
                                     8192, PCI_DMA_FROMDEVICE);
    MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_SYSTEM_ADDRESS,
                    (SK_U32) (PhysAddr + pDev->dma_start_malign +
                              pDev->dma_rx_malign));
// MEM_READ_ULONG (pDev, SK_SLOT_0, STDHOST_SYSTEM_ADDRESS,&ulResp); 
// printk("dma_abuf:0x%p phys:0x%x rx_malign:%d (read-back=0x%x)\n", pDev->dma_rbuff, (SK_U32)PhysAddr, pDev->dma_rx_malign, ulResp );
#else // SYSKT_DMA_MALIGN_TEST

    // set the DMA address
    PhysAddr = (SK_U64) pci_map_page(pDev->dev, virt_to_page(pData),
                                     ((unsigned long) pData & ~PAGE_MASK),
                                     size, PCI_DMA_FROMDEVICE);

    MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_SYSTEM_ADDRESS, (SK_U32) PhysAddr);
    // printk("\n\nCMD53 READ PhysAddr: 0x%x (virt:0x%x)\n",(SK_U32) PhysAddr,
    // (SK_U32) pData );
#endif // SYSKT_DMA_MALIGN_TEST

/*
  DBGPRINT(DBG_ERROR,( KERN_DEBUG "\n\nCMD53 READ PhysAddr: 0x%x (virt:0x%x)\n",(SK_U32) PhysAddr, (SK_U32) pData ) ); 
  MEM_READ_ULONG (pDev, SK_SLOT_0, 0x0204,  &ulResp);
  DBGPRINT(DBG_ERROR, (KERN_DEBUG "(1)RD Fifo Stat   : 0x%x  \n", ulResp ));
  MEM_READ_ULONG (pDev, SK_SLOT_0, 0x0208,  &ulResp);
  DBGPRINT(DBG_ERROR, (KERN_DEBUG "(1)WR Fifo Stat   : 0x%x  \n", ulResp ));
*/
    pDev->CmdInProgress = 1;
    clear_bit(0, &pDev->trans_complete_evt.event);

    retry = 0;
    while (1) {

        if (SDHost_SendCmd(pDev, 53, Arg, uwTransferMode, &ulResponse)) {
            break;
        }
        if (retry++ > 5) {
            if (!SDHost_isCardPresent(pDev)) {
                pDev->SurpriseRemoved = SK_TRUE;
                SDHost_SetCardOutEvent(pDev);
            }
        /*--- mmoser 1/10/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            pDev->CmdInProgress = 0;
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG "CMD53 READ FAILED : fn=%2.02X reg=%8.08X\n",
                      function_number, Offset));
#ifdef SYSKT_DMA_MALIGN_TEST
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, 8192,
                           PCI_DMA_FROMDEVICE);
#else
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, size,
                           PCI_DMA_FROMDEVICE);
#endif
            return SK_FALSE;
        }
        UDELAY(5);
    }

    RESP = GET_BITS_23_16(ulResponse);
    if (pDev->bus_type == SDIO_BUS_TYPE) {
        if (RESP & 0xCB) {
            DBGPRINT(DBG_W528D_CMD53,
                     (KERN_DEBUG "CMD53 Read RESP Error:0x%.2X\n", RESP));
   /*--- mmoser 1/10/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            pDev->CmdInProgress = 0;
#ifdef SYSKT_DMA_MALIGN_TEST
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, 8192,
                           PCI_DMA_FROMDEVICE);
#else
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, size,
                           PCI_DMA_FROMDEVICE);
#endif
            return SK_FALSE;
        } else if ((RESP & 0x30) == 0) {
        /*--- mmoser 1/10/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            pDev->CmdInProgress = 0;

            DBGPRINT(DBG_W528D_CMD53 | DBG_ERROR,
                     (KERN_DEBUG
                      "CMD53 Read RESP Error,card not selected!:0x%.2X\n",
                      RESP));
#ifdef SYSKT_DMA_MALIGN_TEST
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, 8192,
                           PCI_DMA_FROMDEVICE);
#else
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, size,
                           PCI_DMA_FROMDEVICE);
#endif
            return SK_FALSE;
        }
    } else {
        if (RESP & 0x5C) {
        /*--- mmoser 1/10/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            pDev->CmdInProgress = 0;
            DBGPRINT(DBG_W528D_CMD52 | DBG_ERROR,
                     (KERN_DEBUG "CMD53 READ RESP Error:0x%.2X\n", RESP));
#ifdef SYSKT_DMA_MALIGN_TEST
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, 8192,
                           PCI_DMA_FROMDEVICE);
#else
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, size,
                           PCI_DMA_FROMDEVICE);
#endif
            return SK_FALSE;
        }
    }
    if (isCmdFailed(pDev)) {
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                         lastIRQMask);
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                         lastErrorIRQMask);
        pDev->CmdInProgress = 0;
#ifdef SYSKT_DMA_MALIGN_TEST
        pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, 8192,
                       PCI_DMA_FROMDEVICE);
#else
        pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, size,
                       PCI_DMA_FROMDEVICE);
#endif
        return SK_FALSE;
    }

    retry = 0;
    max_retry =
        MAX_WAIT_COMMAND_COMPLETE * (pDev->ClockSpeed ==
                                     0 ? 1 : pDev->ClockSpeed);

    // wait for DMA to complete
    if (pDev->currentIRQSignalMask & STDHOST_NORMAL_IRQ_TRANS_COMPLETE) {
        if (SDHost_wait_event(pDev, &pDev->trans_complete_evt, 1000)) {
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG
                      "%s -> SDHost_wait_event trans_complete failed\n",
                      __FUNCTION__));
          /*--- mmoser 1/10/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            pDev->CmdInProgress = 0;
#ifdef SYSKT_DMA_MALIGN_TEST
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, 8192,
                           PCI_DMA_FROMDEVICE);
#else
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, size,
                           PCI_DMA_FROMDEVICE);
#endif
            return SK_FALSE;
        }
        clear_bit(0, &pDev->trans_complete_evt.event);
    } else {
        retry = 0;
        if (test_bit(0, &pDev->trans_complete_evt.event) == 0) {
            // if ( SDHost_wait_event(pDev, &pDev->trans_complete_evt, 1 ) ) {
            do {
                MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS,
                               &ulResp);
                if ((ulResp & STDHOST_NORMAL_IRQ_TRANS_COMPLETE) ==
                    STDHOST_NORMAL_IRQ_TRANS_COMPLETE) {
                    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS,
                                     STDHOST_NORMAL_IRQ_TRANS_COMPLETE);
                    break;
                }
                  /*--- mmoser 3/14/2006 ---*/
                UDELAY(1);
            } while (++retry < max_retry);
        } else {
            clear_bit(0, &pDev->trans_complete_evt.event);
        }
        if (retry >= max_retry) {
            DBGPRINT(DBG_WARNING | DBG_ERROR,
                     (KERN_DEBUG
                      "CMD53 Read : STDHOST_NORMAL_IRQ_TRANS_COMPLETE not set!(count=%d max_retry=%d)\n",
                      Count, max_retry));
            // reset the data line and FIFO
        }
    }
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_TRANSFER_MODE, 0);

#ifdef SYSKT_DMA_MALIGN_TEST
    {
        memcpy(pData,
               (void *) (pDev->dma_rbuff + pDev->dma_start_malign +
                         pDev->dma_rx_malign), size);
        // pDev->dma_rx_malign++;
        pDev->dma_rx_malign %= 64;
    }
    pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, 8192, PCI_DMA_FROMDEVICE);
#else
    pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, size, PCI_DMA_FROMDEVICE);
#endif

#ifdef WAR_SEND_ABORT
// Needed for FPGA workaround
    if (mode) {
        SDHost_SendAbort(pDev);
    }
#endif // WAR_SEND_ABORT

    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                     lastIRQMask);
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                     lastErrorIRQMask);
    pDev->CmdInProgress = 0;
/*
 {
	 int j;
	 for (j=0; j < 64 && j < size; j++)
	 {
		 printk("%2.02X ",pData[j]);
		 if ((j+1)%16 == 0)
		 {
			 printk("\n");
		 }
	 }
	 printk("\n");
 }
*/
    return SK_TRUE;
}

/******************************************************************************
 *
 *   SDHost_CMD53_Write - Write a block of data to the SDIO card
 *   -
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *     SK_TRUE on success
 *
 */
SK_BOOL
SDHost_CMD53_Write(PSD_DEVICE_DATA pDev,
                   SK_U32 Offset,
                   SK_U8 function_number,
                   SK_U8 mode,
                   SK_U8 opcode, SK_U8 * pData, SK_U32 Count, SK_U32 blksz)
{
    SK_U16 uwTransferMode;
    SK_U32 Arg;
    SK_U32 ulResponse;
    SK_U32 blk_cnt, cnt;
    SK_U32 *pSrc32, *pEnd32;
    SK_U32 *pDest32;
    SK_U8 RESP;
    SK_U32 retry, max_retry;
    SK_U32 i;

    DBGPRINT(DBG_ERROR,
             (KERN_DEBUG "CMD53 WRITE fn=%2.02X reg=%8.08X data=0x%2.0X\n",
              function_number, Offset, *(pData)));

 /*--- mmoser 3/31/2006 ---*/
    if (pDev->SurpriseRemoved == SK_TRUE) {
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG
                  "CMD53 WRITE FAILED : Surprise Removed !!!  fn=%2.02X reg=%8.08X (bus_errors=%d)\n",
                  function_number, Offset, pDev->bus_errors));
        return SK_FALSE;
    }
    clear_bit(0, &pDev->trans_complete_evt.event);

    if (Count >= 512) {
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG
                  "CMD53 WRITE : fn=%2.02X reg=%8.08X len=%d > 512 ERROR!!!!!!\n",
                  function_number, Offset, Count));
        return SK_FALSE;
    }

    DBGPRINT(DBG_W528D_CMD53,
             (KERN_DEBUG
              "CMD53 WRITE %s MODE : fn=%2.02X reg=%8.08X %s=%d block_size=%d\n",
              (mode ? "BLOCK" : "BYTE"), function_number, Offset,
              (mode ? "blocks" : "length"), Count, blksz));

    if ((*(pData + 4) == 0x03) && (Count == 2)) {
        SK_U32 x;
        DBGPRINT(DBG_ERROR, (KERN_DEBUG "\n**** TRIGGER *****\n"));
        MEM_READ_ULONG(pDev, SK_SLOT_0, 0x200, &x);
        x |= ((1 << 24) | (1 << 16));
        MEM_WRITE_ULONG(pDev, SK_SLOT_0, 0x200, x);
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG "**** pData 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x \n",
                  *(pData + 0), *(pData + 1), *(pData + 2), *(pData + 3),
                  *(pData + 4), *(pData + 5)));
    }

    if (mode && (blksz != pDev->sd_ids[function_number].blocksize))     // block 
                                                                        // mode
    {
        SDHost_CMD52_Write(pDev, FN_BLOCK_SIZE_0_REG(function_number), 0,
                           (SK_U8) (blksz & 0x00ff));
        SDHost_CMD52_Write(pDev, FN_BLOCK_SIZE_1_REG(function_number), 0,
                           (SK_U8) (blksz >> 8));
        pDev->sd_ids[function_number].blocksize = blksz;
    }
    uwTransferMode = 0;

    if (mode == BYTE_MODE) {
        cnt = Count;

        blk_cnt = 1 << 16 | Count;
        MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_BLOCK_SIZE, blk_cnt);
    } else {
        cnt = Count * blksz;

/* 20050630 mmoser HW-Workaround */
        if ((pDev->debug_flags & DEBUG_DISABLE_HW_WORKAROUND_WR) ==
            DEBUG_DISABLE_HW_WORKAROUND_WR) {
            uwTransferMode |= STDHOST_MULTI_BLOCK_SELECT;
        } else {
            if (Count > 1) {
                uwTransferMode |= STDHOST_MULTI_BLOCK_SELECT;
            }
        }

        uwTransferMode |= STDHOST_BLOCK_COUNT_ENA;

        blk_cnt = Count << 16 | blksz | DMA_16K_BOUNDARY;
        MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_BLOCK_SIZE, blk_cnt);
    }

    pSrc32 = (SK_U32 *) pData;
    pDest32 =
        (SK_U32 *) (((SK_U32) (SK_BAR((pDev), (SK_SLOT_0)))) +
                    (STDHOST_DATA_PORT));

    Arg = MAKE_SDIO_OFFSET(Offset) |
        MAKE_SDIO_OP_CODE(opcode) |
        MAKE_SDIO_BLOCK_MODE(mode) |
        MAKE_SDIO_FUNCTION(function_number) |
        MAKE_SDIO_DIR(1) | (Count & 0x01FF);

    max_retry =
        MAX_WAIT_COMMAND_COMPLETE * (pDev->ClockSpeed ==
                                     0 ? 1 : pDev->ClockSpeed);

    pEnd32 = (SK_U32 *) ((SK_U32) pSrc32 + cnt);

// #define PREFILL_FIFO

/*** mmoser 2006-03-03 **/
#ifdef PREFILL_FIFO
    // --> The whole message is always written to the FIFO! each FIFO has 2K
    // Fill the FIFO before issuing the command
    i = 0;
    while (i < 2000 && pSrc32 < pEnd32) {
        if (i % blksz == 0) {
            retry = 0;

            do {
                MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_PRESENT_STATE,
                               &ulResponse);
                if ((ulResponse & STDHOST_STATE_BUFFER_WR_ENABLE) ==
                    STDHOST_STATE_BUFFER_WR_ENABLE) {
                    break;
                }

                     /*--- mmoser 3/13/2006 ---*/
                UDELAY(1);
            } while (++retry < max_retry);

                     /*--- mmoser 3/3/2006 ---*/
            if (retry >= max_retry) {
                DBGPRINT(DBG_ERROR,
                         (KERN_DEBUG
                          "CMD53 WRITE FAILED : STDHOST_STATE_BUFFER_WR_ENABLE remains 0\nfn=%2.02X reg=%8.08X\n",
                          function_number, Offset));
                return SK_FALSE;
            }
        }

        *pDest32 = *pSrc32;
        pSrc32++;
        i += 4;
    }
#endif // PREFILL_FIFO

                     /*--- mmoser 8/29/2006 ---*/
    clear_bit(0, &pDev->trans_complete_evt.event);

    retry = 0;
    while (1) {

        if (SDHost_SendCmd(pDev, 53, Arg, uwTransferMode, &ulResponse)) {
            break;
        }
        if (retry++ > 5) {
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG "CMD53 WRITE FAILED : fn=%2.02X reg=%8.08X\n",
                      function_number, Offset));
            return SK_FALSE;
        }
        UDELAY(5);
    }

    RESP = GET_BITS_23_16(ulResponse);

    if (pDev->bus_type == SDIO_BUS_TYPE) {
        if (RESP & 0xCB) {
            DBGPRINT(DBG_W528D_CMD53 | DBG_ERROR,
                     (KERN_DEBUG "CMD53 WRITE RESP Error:0x%.2X\n", RESP));
            // reset the data line and FIFO
            MEM_WRITE_UCHAR(pDev, SK_SLOT_0,
                            STDHOST_SW_RESET, STDHOST_SW_RESET_DAT_LINE);
            return SK_FALSE;
        } else if ((RESP & 0x30) == 0) {
            DBGPRINT(DBG_W528D | DBG_ERROR,
                     (KERN_DEBUG
                      "CMD53 WRITE RESP Error,card not selected!:0x%.2X\n",
                      RESP));
            // reset the data line and FIFO
            MEM_WRITE_UCHAR(pDev, SK_SLOT_0,
                            STDHOST_SW_RESET, STDHOST_SW_RESET_DAT_LINE);
            return SK_FALSE;
        }

    } else {
        if (RESP & 0x5C) {
            DBGPRINT(DBG_W528D_CMD52 | DBG_ERROR,
                     (KERN_DEBUG "CMD53 WRITE RESP Error:0x%.2X\n", RESP));
            // reset the data line and FIFO
            MEM_WRITE_UCHAR(pDev, SK_SLOT_0,
                            STDHOST_SW_RESET, STDHOST_SW_RESET_DAT_LINE);
            return SK_FALSE;
        }
    }

#ifndef PREFILL_FIFO
    i = 0;
#endif

    // Fill FIFO with remaining data, if any ...
    while (pSrc32 < pEnd32) {
        if (i % blksz == 0) {
            retry = 0;

            do {
                MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_PRESENT_STATE,
                               &ulResponse);
                if ((ulResponse & STDHOST_STATE_BUFFER_WR_ENABLE) ==
                    STDHOST_STATE_BUFFER_WR_ENABLE) {
                    break;
                }

                     /*--- mmoser 3/13/2006 ---*/
                UDELAY(1);
            } while (++retry < max_retry);
                     /*--- mmoser 3/3/2006 ---*/
            if (retry >= max_retry) {
                DBGPRINT(DBG_ERROR,
                         (KERN_DEBUG
                          "CMD53 WRITE FAILED : STDHOST_STATE_BUFFER_WR_ENABLE remains 0\nfn=%2.02X reg=%8.08X\n",
                          function_number, Offset));
                if (mode) {
                    SDHost_SendAbort(pDev);
                }
                return SK_FALSE;
            }
        }

        *pDest32 = *pSrc32;
        pSrc32++;
        i += 4;
    }

    if (pDev->currentIRQSignalMask & STDHOST_NORMAL_IRQ_TRANS_COMPLETE) {
        if (SDHost_wait_event(pDev, &pDev->trans_complete_evt, 1000)) {
            return SK_FALSE;
        }
        clear_bit(0, &pDev->trans_complete_evt.event);
    } else {
        retry = 0;
        do {
            SK_U32 ulResp;
            MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS, &ulResp);
            if ((ulResp & STDHOST_NORMAL_IRQ_TRANS_COMPLETE) ==
                STDHOST_NORMAL_IRQ_TRANS_COMPLETE) {
                MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS,
                                STDHOST_NORMAL_IRQ_TRANS_COMPLETE);
                break;
            }

                        /*--- mmoser 3/14/2006 ---*/
            UDELAY(1);
        } while (++retry < max_retry);
        if (retry >= max_retry) {
            DBGPRINT(DBG_WARNING,
                     (KERN_DEBUG
                      "CMD53 Write : STDHOST_NORMAL_IRQ_TRANS_COMPLETE not set!(count=%d max_retry=%d)\n",
                      Count, max_retry));
            // reset the data line and FIFO
        }
    }

    if (mode) {
        SDHost_SendAbort(pDev);
    }

 /*--- mmoser 1/5/2007 ---*/
// STDHOST_NORMAL_IRQ_ERROR is used for GPI interrupts !!!!
/*
 MEM_READ_ULONG (pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS, &ulVal);
 if (ulVal&STDHOST_NORMAL_IRQ_ERROR)
 {
  SDHost_ErrorRecovery(pDev);
  DBGPRINT(DBG_ERROR,( KERN_DEBUG "SDHost_CMD53_Write() : Command Error 0x%8.08X\n",ulVal));
  return SK_FALSE; 
 }
*/
    return SK_TRUE;
}

/******************************************************************************
 *
 *   SDHost_CMD53_Write_DMA - Write a block of data to the SDIO card
 *   -
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *     SK_TRUE on success
 *
 */
SK_BOOL
SDHost_CMD53_Write_DMA(PSD_DEVICE_DATA pDev,
                       SK_U32 Offset,
                       SK_U8 function_number,
                       SK_U8 mode,
                       SK_U8 opcode, SK_U8 * pData, SK_U32 Count, SK_U32 blksz)
{
    SK_U16 uwTransferMode;
    SK_U32 Arg;
    SK_U32 ulResponse;
    SK_U32 blk_cnt, cnt;
    SK_U32 *pSrc32;
    SK_U32 *pDest32;
    SK_U8 RESP;
    SK_U32 retry, max_retry;
    SK_U32 ulResp;
    SK_U64 PhysAddr;
 /*--- mmoser 1/10/2007 ---*/
    SK_U16 lastIRQMask;
    SK_U16 lastErrorIRQMask;

 /*--- mmoser 3/31/2006 ---*/
    if (pDev->SurpriseRemoved == SK_TRUE) {
 /*--- mmoser 1/10/2007 ---*/
        printk
            ("CMD53 WRITE FAILED : Surprise Removed !!!  fn=%2.02X reg=%8.08X (bus_errors=%d)\n",
             function_number, Offset, pDev->bus_errors);
        return SK_FALSE;
    }
    clear_bit(0, &pDev->trans_complete_evt.event);

    if (Count >= 512) {
 /*--- mmoser 1/10/2007 ---*/
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG
                  "CMD53 WRITE : fn=%2.02X reg=%8.08X len=%d > 512 ERROR!!!!!!\n",
                  function_number, Offset, Count));
        return SK_FALSE;
    }

/*
 if(( *(pData) == 0x3C) && (Count == 2) ) {
   SK_U32 x;
   DBGPRINT(DBG_ERROR,( KERN_DEBUG "\n**** TRIGGER *****\n"));
   MEM_READ_ULONG (pDev, SK_SLOT_0, 0x200, &x);
   x |= ((1 << 24) | (1 << 16)); 
   MEM_WRITE_ULONG (pDev, SK_SLOT_0, 0x200, x);
   DBGPRINT(DBG_ERROR,( KERN_DEBUG "**** pData 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x \n",
          *(pData+0),*(pData+1),*(pData+2),*(pData+3), *(pData+4), *(pData+5)));
 }
*/
                                                                                                                                                                         /*--- mmoser 1/10/2007 ---*/
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                    &lastIRQMask);
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                    &lastErrorIRQMask);

    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                     (lastIRQMask & (~STDHOST_NORMAL_IRQ_CARD)));
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                     (lastErrorIRQMask & (~STDHOST_ERROR_IRQ_VENDOR_ENA)));

    DBGPRINT(DBG_W528D_CMD53,
             (KERN_DEBUG
              "CMD53 WRITE %s MODE : fn=%2.02X reg=%8.08X %s=%d block_size=%d\n",
              (mode ? "BLOCK" : "BYTE"), function_number, Offset,
              (mode ? "blocks" : "length"), Count, blksz));

    if (mode && (blksz != pDev->sd_ids[function_number].blocksize))     // block 
                                                                        // mode
    {
        SDHost_CMD52_Write(pDev, FN_BLOCK_SIZE_0_REG(function_number), 0,
                           (SK_U8) (blksz & 0x00ff));
        SDHost_CMD52_Write(pDev, FN_BLOCK_SIZE_1_REG(function_number), 0,
                           (SK_U8) (blksz >> 8));
        pDev->sd_ids[function_number].blocksize = blksz;
    }
    uwTransferMode = 0;

    if (mode == BYTE_MODE) {
        cnt = Count;

        blk_cnt = 1 << 16 | Count;
        uwTransferMode |= STDHOST_DMA_ENA;
        MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_BLOCK_SIZE, blk_cnt);
    } else {
        cnt = Count * blksz;

/* 20050630 mmoser HW-Workaround */
        if ((pDev->debug_flags & DEBUG_DISABLE_HW_WORKAROUND_WR) ==
            DEBUG_DISABLE_HW_WORKAROUND_WR) {
            uwTransferMode |= STDHOST_MULTI_BLOCK_SELECT;
        } else {
            if (Count > 1) {
                uwTransferMode |= STDHOST_MULTI_BLOCK_SELECT;
            }
        }

/*
	{
		int j;
		for (j=0; j < 64 && j < cnt; j++)
		{
			printk("%2.02X ",pData[j]);
			if ((j+1)%16 == 0)
			{
				printk("\n");
			}
		}
		printk("\n");
	}
*/
        // enable DMA transfer
        uwTransferMode |= (STDHOST_BLOCK_COUNT_ENA | STDHOST_DMA_ENA);

        blk_cnt = Count << 16 | blksz | DMA_16K_BOUNDARY;
        MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_BLOCK_SIZE, blk_cnt);
    }

    pSrc32 = (SK_U32 *) pData;
    pDest32 =
        (SK_U32 *) (((SK_U32) (SK_BAR((pDev), (SK_SLOT_0)))) +
                    (STDHOST_DATA_PORT));

    Arg = MAKE_SDIO_OFFSET(Offset) |
        MAKE_SDIO_OP_CODE(opcode) |
        MAKE_SDIO_BLOCK_MODE(mode) |
        MAKE_SDIO_FUNCTION(function_number) |
        MAKE_SDIO_DIR(1) | (Count & 0x01FF);

    max_retry =
        MAX_WAIT_COMMAND_COMPLETE * (pDev->ClockSpeed ==
                                     0 ? 1 : pDev->ClockSpeed);

    // max_retry = MAX_WAIT_COMMAND_COMPLETE * 100 * (pDev->ClockSpeed == 0 ? 1 
    // : pDev->ClockSpeed);
                      /*--- mmoser 8/29/2006 ---*/

#ifdef SYSKT_DMA_MALIGN_TEST
    // set the DMA address
    PhysAddr = (SK_U64) pci_map_page(pDev->dev,
                                     virt_to_page(pDev->dma_tbuff),
                                     ((unsigned long) (pDev->
                                                       dma_tbuff) & ~PAGE_MASK),
                                     8192, PCI_DMA_FROMDEVICE);
    MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_SYSTEM_ADDRESS,
                    (SK_U32) (PhysAddr + pDev->dma_start_malign +
                              pDev->dma_tx_malign));

//  MEM_READ_ULONG (pDev, SK_SLOT_0, STDHOST_SYSTEM_ADDRESS,&ulResp); 
// printk("dma_abuf:0x%p phys:0x%x tx_malign:%d (read-back=0x%x)\n", pDev->dma_tbuff, (SK_U32)PhysAddr, pDev->dma_tx_malign, ulResp );
    memcpy((void *) (pDev->dma_tbuff + pDev->dma_start_malign +
                     pDev->dma_tx_malign), pData, cnt);
//printk("TX org: %2.02X %2.02X %2.02X %2.02X ...\n",pData[0],pData[1],pData[2],pData[3]);
//pTmp = (SK_U8 *)(pDev->dma_tbuff+pDev->dma_start_malign+pDev->dma_tx_malign);
//printk("TX cp : %2.02X %2.02X %2.02X %2.02X ...\n",pTmp[0],pTmp[1],pTmp[2],pTmp[3]);
    pDev->dma_tx_malign++;
    pDev->dma_tx_malign %= 64;

#else // SYSKT_DMA_MALIGN_TEST
    // set the DMA address
    PhysAddr = (SK_U64) pci_map_page(pDev->dev, virt_to_page(pData),
                                     ((unsigned long) pData & ~PAGE_MASK),
                                     cnt, PCI_DMA_TODEVICE);

    MEM_WRITE_ULONG(pDev, SK_SLOT_0, STDHOST_SYSTEM_ADDRESS, (SK_U32) PhysAddr);
#endif

    pDev->CmdInProgress = 1;
    clear_bit(0, &pDev->trans_complete_evt.event);

    retry = 0;
    while (1) {

        if (SDHost_SendCmd(pDev, 53, Arg, uwTransferMode, &ulResponse)) {
            break;
        }
        if (retry++ > 5) {
            if (!SDHost_isCardPresent(pDev)) {
                pDev->SurpriseRemoved = SK_TRUE;
                SDHost_SetCardOutEvent(pDev);
            }
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG "CMD53 WRITE FAILED : fn=%2.02X reg=%8.08X\n",
                      function_number, Offset));
                                                                                                                                                                         /*--- mmoser 1/10/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            pDev->CmdInProgress = 0;
#ifdef SYSKT_DMA_MALIGN_TEST
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, 8192,
                           PCI_DMA_FROMDEVICE);
#else
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, cnt,
                           PCI_DMA_FROMDEVICE);
#endif
            return SK_FALSE;
        }
        UDELAY(5);
    }
    RESP = GET_BITS_23_16(ulResponse);

    if (pDev->bus_type == SDIO_BUS_TYPE) {
        if (RESP & 0xCB) {
            DBGPRINT(DBG_W528D_CMD53 | DBG_ERROR,
                     (KERN_DEBUG "CMD53 WRITE RESP Error:0x%.2X\n", RESP));
            // reset the data line and FIFO
            MEM_WRITE_UCHAR(pDev, SK_SLOT_0,
                            STDHOST_SW_RESET, STDHOST_SW_RESET_DAT_LINE);
   /*--- mmoser 1/10/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            pDev->CmdInProgress = 0;
#ifdef SYSKT_DMA_MALIGN_TEST
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, 8192,
                           PCI_DMA_FROMDEVICE);
#else
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, cnt,
                           PCI_DMA_FROMDEVICE);
#endif
            return SK_FALSE;
        } else if ((RESP & 0x30) == 0) {
            DBGPRINT(DBG_W528D | DBG_ERROR,
                     (KERN_DEBUG
                      "CMD53 WRITE RESP Error,card not selected!:0x%.2X\n",
                      RESP));
            // reset the data line and FIFO
            MEM_WRITE_UCHAR(pDev, SK_SLOT_0,
                            STDHOST_SW_RESET, STDHOST_SW_RESET_DAT_LINE);
   /*--- mmoser 1/10/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            pDev->CmdInProgress = 0;
#ifdef SYSKT_DMA_MALIGN_TEST
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, 8192,
                           PCI_DMA_FROMDEVICE);
#else
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, cnt,
                           PCI_DMA_FROMDEVICE);
#endif
            return SK_FALSE;
        }

    } else {
        if (RESP & 0x5C) {
            DBGPRINT(DBG_W528D_CMD52 | DBG_ERROR,
                     (KERN_DEBUG "CMD53 WRITE RESP Error:0x%.2X\n", RESP));
            // reset the data line and FIFO
            MEM_WRITE_UCHAR(pDev, SK_SLOT_0,
                            STDHOST_SW_RESET, STDHOST_SW_RESET_DAT_LINE);
   /*--- mmoser 1/10/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            pDev->CmdInProgress = 0;
#ifdef SYSKT_DMA_MALIGN_TEST
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, 8192,
                           PCI_DMA_FROMDEVICE);
#else
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, cnt,
                           PCI_DMA_FROMDEVICE);
#endif
            return SK_FALSE;
        }
    }
    if (isCmdFailed(pDev)) {
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                         lastIRQMask);
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                         lastErrorIRQMask);
        pDev->CmdInProgress = 0;
#ifdef SYSKT_DMA_MALIGN_TEST
        pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, 8192,
                       PCI_DMA_FROMDEVICE);
#else
        pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, cnt,
                       PCI_DMA_FROMDEVICE);
#endif
        return SK_FALSE;
    }
    // wait for DMA to complete
    if (pDev->currentIRQSignalMask & STDHOST_NORMAL_IRQ_TRANS_COMPLETE) {
        if (SDHost_wait_event(pDev, &pDev->trans_complete_evt, 1000)) {
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG
                      "%s -> SDHost_wait_event trans_complete failed\n",
                      __FUNCTION__));
         /*--- mmoser 1/10/2007 ---*/
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                             lastIRQMask);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                             lastErrorIRQMask);
            pDev->CmdInProgress = 0;
#ifdef SYSKT_DMA_MALIGN_TEST
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, 8192,
                           PCI_DMA_FROMDEVICE);
#else
            pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, cnt,
                           PCI_DMA_FROMDEVICE);
#endif
            return SK_FALSE;
        }
        clear_bit(0, &pDev->trans_complete_evt.event);
    } else {
        retry = 0;
        ulResp = 0;
        if (test_bit(0, &pDev->trans_complete_evt.event) == 0) {
            do {

                MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS,
                               &ulResp);
                if ((ulResp & STDHOST_NORMAL_IRQ_TRANS_COMPLETE) ==
                    STDHOST_NORMAL_IRQ_TRANS_COMPLETE) {
                    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS,
                                     STDHOST_NORMAL_IRQ_TRANS_COMPLETE);
                    break;
                }
                 /*--- mmoser 3/14/2006 ---*/
                UDELAY(1);
            } while (++retry < max_retry);

        }                       // check event
        else {
            clear_bit(0, &pDev->trans_complete_evt.event);
            printk("%s xxx event instead of polling\n", __FUNCTION__);
        }
        if (retry >= max_retry) {
            DBGPRINT(DBG_WARNING,
                     (KERN_DEBUG
                      "CMD53 Write : STDHOST_NORMAL_IRQ_TRANS_COMPLETE not set!(count=%d max_retry=%d)0x%x\n",
                      Count, max_retry, ulResp));
            // reset the data line and FIFO
        }
    }
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_TRANSFER_MODE, 0);

#ifdef SYSKT_DMA_MALIGN_TEST
    pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, 8192, PCI_DMA_FROMDEVICE);
#else
    pci_unmap_page(pDev->dev, (dma_addr_t) PhysAddr, cnt, PCI_DMA_FROMDEVICE);
#endif

#ifdef WAR_SEND_ABORT
// Needed for FPGA workaround
    if (mode) {
        SDHost_SendAbort(pDev);
    }
#endif // WAR_SEND_ABORT

        /*--- mmoser 1/5/2007 ---*/
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE,
                     lastIRQMask);
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                     lastErrorIRQMask);
    pDev->CmdInProgress = 0;

    return SK_TRUE;
}

/******************************************************************************
 *
 *   SDHost_CMD53_Read - Read a block of data from the SDIO card
 *   -
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *					SK_TRUE on success
 *
 */
SK_BOOL
SDHost_CMD53_ReadEx(PSD_DEVICE_DATA pDev,
                    SK_U32 Offset,
                    SK_U8 function_number,
                    SK_U8 mode,
                    SK_U8 opcode, SK_U8 * pData, SK_U32 Count, SK_U32 blksz)
{
    SK_BOOL rc;

    if (pDev->dma_support & DMA_RD_SUPPORT) {
        // printk("SDHost_CMD53_ReadEx (DMA): Count=%d
        // blksz=%d\n",Count,blksz);
        rc = SDHost_CMD53_Read_DMA(pDev, Offset, function_number, mode, opcode,
                                   pData, Count, blksz);
    } else {
        // printk("SDHost_CMD53_ReadEx: Count=%d blksz=%d\n",Count,blksz);
        rc = SDHost_CMD53_Read(pDev, Offset, function_number, mode, opcode,
                               pData, Count, blksz);
    }

    return rc;
}

/******************************************************************************
 *
 *   SDHost_CMD53_Write - Write a block of data to the SDIO card
 *   -
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *     SK_TRUE on success
 *
 */
SK_BOOL
SDHost_CMD53_WriteEx(PSD_DEVICE_DATA pDev,
                     SK_U32 Offset,
                     SK_U8 function_number,
                     SK_U8 mode,
                     SK_U8 opcode, SK_U8 * pData, SK_U32 Count, SK_U32 blksz)
{

    if (pDev->dma_support & DMA_WR_SUPPORT) {
        // printk("SDHost_CMD53_WriteEx (DMA): Count=%d
        // blksz=%d\n",Count,blksz);
        return SDHost_CMD53_Write_DMA(pDev, Offset, function_number, mode,
                                      opcode, pData, Count, blksz);
    } else {
        // printk("SDHost_CMD53_WriteEx: Count=%d blksz=%d\n",Count,blksz);
        return SDHost_CMD53_Write(pDev, Offset, function_number, mode, opcode,
                                  pData, Count, blksz);
    }
}

/******************************************************************************
 *
 * SDHost_EnableInterrupt  - Enable the interrupts on the FPGA
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *
 */
VOID
SDHost_EnableInterrupt(PSD_DEVICE_DATA pDev, SK_U16 mask)
{
//      SK_U16 usSigEna;
    pDev->currentIRQSignalMask |= mask;

/*
	MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE, &usSigEna);			
	if ( (usSigEna & STDHOST_NORMAL_IRQ_CMD_COMPLETE_ENA) == 0 )
	{
		printk("@%d ####################################################################################################\n",__LINE__);
	}
*/

    DBGPRINT(DBG_IRQ, (KERN_DEBUG "Enable PCI Interrupt (0x%4.04X)\n",
                       pDev->currentIRQSignalMask));

        /*--- mmoser 1/2/2007 ---*/
    if (pDev->CmdInProgress == 0) {
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE,
                         STDHOST_ERROR_IRQ_VENDOR_SIG_ENA);
/*	
		MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE, &usSigEna);			
		if ( (usSigEna & STDHOST_NORMAL_IRQ_CMD_COMPLETE_ENA) == 0 )
		{
			printk("@%d ####################################################################################################\n",__LINE__);
		}
*/
    }
    // save last irq mask
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_SIGNAL_ENABLE,
                    &pDev->lastIRQSignalMask);

    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_SIGNAL_ENABLE,
                     pDev->currentIRQSignalMask);
/*
	MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE, &usSigEna);			

	if ( (usSigEna & STDHOST_NORMAL_IRQ_CMD_COMPLETE_ENA) == 0 )
	{
		printk("@%d #################################################################################################### (mask=0x%4.04X)\n",__LINE__,pDev->currentIRQSignalMask);
	}
*/
}

/******************************************************************************
 *
 * SDHost_DisableInterrupt  - Disable the interrupts on the FPGA
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *
 */
VOID
SDHost_DisableInterrupt(PSD_DEVICE_DATA pDev, SK_U16 mask)
{

/*
{
	SK_U16 usSigEna;

	MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE, &usSigEna);			
	if ( (usSigEna & STDHOST_NORMAL_IRQ_CMD_COMPLETE_ENA) == 0 )
	{
		printk("@%d ####################################################################################################\n",__LINE__);
	}

}
*/

    // save current irq mask
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_SIGNAL_ENABLE,
                    &pDev->currentIRQSignalMask);

    pDev->lastIRQSignalMask = pDev->currentIRQSignalMask;

    DBGPRINT(DBG_IRQ, (KERN_DEBUG "Disable PCI Interrupt (0x%4.04X)\n",
                       pDev->currentIRQSignalMask & ~mask));

        /*--- mmoser 1/2/2007 ---*/
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_SIGNAL_ENABLE, 0);
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_SIGNAL_ENABLE,
                     pDev->currentIRQSignalMask & ~mask);
/*
{
	SK_U16 usSigEna;

	MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS_ENABLE, &usSigEna);			
	if ( (usSigEna & STDHOST_NORMAL_IRQ_CMD_COMPLETE_ENA) == 0 )
	{
		printk("@%d #################################################################################################### (mask=0x%4.04X)\n",__LINE__,pDev->currentIRQSignalMask & ~mask);
	}
}
*/
    pDev->currentIRQSignalMask &= ~mask;

}

/******************************************************************************
 *
 * SDHost_if_EnableInterrupt  - Exported interface function 
 *
 * Description:
 *							Exported interface function : Enable only card related interrupts
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *
 */
VOID
SDHost_if_EnableInterrupt(PSD_DEVICE_DATA pDev)
{
    SDHost_EnableInterrupt(pDev, STDHOST_NORMAL_IRQ_CARD_ALL_ENA);
}

/******************************************************************************
 *
 * SDHost_if_DisableInterrupt  - Exported interface function 
 *
 * Description:
 *							Exported interface function : Disable only card related interrupts
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *
 */
VOID
SDHost_if_DisableInterrupt(PSD_DEVICE_DATA pDev)
{
    SDHost_DisableInterrupt(pDev, STDHOST_NORMAL_IRQ_CARD_ALL_ENA);
}

/******************************************************************************
 *
 *  SDHost_SetCardOutEvent - Set the card out event
 *
 * Description:
 *
 * Notes:	WINDOWS only 
 *
 * Context:
 *
 * Returns:
 *
 */
VOID
SDHost_SetCardOutEvent(PSD_DEVICE_DATA pDev)
{
    DBGPRINT(DBG_W528D,
             (KERN_DEBUG "SDHost_SetCardOutEvent() ---> CARD REMOVED\n"));
    if (!pDev->isRemoving) {
        queue_work(pDev->workqueue, &pDev->card_out_work);
    }
}

/******************************************************************************
 *
 * SDHost_lockInterface  - Lock the SDIO interface
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *
 */
SK_BOOL
SDHost_lockInterface(PSD_DEVICE_DATA pDev)
{
#ifdef SK_SDIO_LOCK_INTERFACE
    if (INTERLOCKED_INC(&pDev->if_lock) > 1) {
        INTERLOCKED_DEC(&pDev->if_lock);
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG "SDHost_lockInterface() FAILED !!! if_lock=%d\n",
                  pDev->if_lock));
        return SK_FALSE;
    }
#endif

    return SK_TRUE;
}

/******************************************************************************
 *
 * SDHost_unlockInterface  - unlock the SDIO interface
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *
 */
VOID
SDHost_unlockInterface(PSD_DEVICE_DATA pDev)
{
#ifdef SK_SDIO_LOCK_INTERFACE
    if (INTERLOCKED_DEC(&pDev->if_lock) < 0) {
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG
                  "SDHost_unlockInterface() was already zero !!!! --> ignore.\n",
                  pDev->if_lock));
        INTERLOCKED_INC(&pDev->if_lock);
    }
#endif
}

/******************************************************************************
 *
 *  SDHost_SetLed - Set the LED on the FPGA interface
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *
 */
VOID
SDHost_SetLed(PSD_DEVICE_DATA pDev, SK_U8 led, SK_U8 on)
{
    if (on) {
        LED_ON(led);
    } else {
        LED_OFF(led);
    }
}

/******************************************************************************
 *
 *  SDHost_SetClockSpeed - Set the SDIO bus clock speed
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *
 */
VOID
SDHost_SetClockSpeed(PSD_DEVICE_DATA pDev, SK_U32 bus_freq)
{
    SK_U32 freq = 0;

    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "%s: set clock to %d\n", __FUNCTION__,
              bus_freq == 0 ? 25 : bus_freq));

    if (bus_freq == 50) {
        freq = 0;               /* 50 MHz */
    } else if (bus_freq == 0 || bus_freq == 25) {
        freq = 1;               /* default: 25 MHz */
    } else if (bus_freq == 12) {
        freq = 2;               /* 12 MHz */
    } else if (bus_freq == 6) {
        freq = 4;               /* 6 MHz */
    } else if (bus_freq == 3) {
        freq = 8;               /* 3 MHz */
    } else if (bus_freq == 2) {
        freq = 16;              /* 1,5 MHz */
    } else if (bus_freq == 800) {
        freq = 32;              /* 800 kHz */
    } else if (bus_freq == 300) {
        freq = 64;              /* 300 kHz */
    } else if (bus_freq == 200) {
        freq = 128;             /* 200 kHz */
    } else {
        DBGPRINT(DBG_LOAD,
                 (KERN_DEBUG "%s: illegal clock %d\n", __FUNCTION__, bus_freq));
        return;
    }
    pDev->ClockSpeed = freq;

    if (!pDev->ClockSpeed) {
        SDHost_Enable_HighSpeedMode(pDev);
    } else {

        SDHost_Disable_HighSpeedMode(pDev);
    }
}

/******************************************************************************
 *
 *  SDHost_SetClock - Set the SDIO bus clock on/off
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *
 */
VOID
SDHost_SetClock(PSD_DEVICE_DATA pDev, SK_U8 on)
{
    SK_U16 usVal;
    int j;

    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL, &usVal);

    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "%s: turn clock %s\n", __FUNCTION__,
              (on == 1 ? "ON" : "OFF")));

    if (on) {
        if (usVal & STDHOST_INTERNAL_CLK_ENA) {
            if (usVal & STDHOST_INTERNAL_CLK_STABLE) {
                if (usVal & STDHOST_CLOCK_ENA) {
                    // clock is already stable and running
                    return;
                } else {
                    // Enable clock to card
                    usVal |= STDHOST_CLOCK_ENA;
                    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL,
                                     usVal);
                    return;
                }
            }
            // there is something fishy with the clock. 
            // -> disable internal clock first
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG "%s: SD internal clock is unstable!\n",
                      __FUNCTION__));
            usVal &= ~STDHOST_INTERNAL_CLK_ENA;
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL, usVal);
        }

        usVal |= STDHOST_INTERNAL_CLK_ENA;
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL, usVal);
        for (j = 0; j < MAX_WAIT_CLOCK_STABLE; j++) {
            MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL, &usVal);
            if ((usVal & STDHOST_INTERNAL_CLK_STABLE) ==
                STDHOST_INTERNAL_CLK_STABLE) {
                break;
            }
        }
        if (j >= MAX_WAIT_CLOCK_STABLE) {
            DBGPRINT(DBG_ERROR,
                     (KERN_DEBUG "%s: SD clock remains unstable!\n",
                      __FUNCTION__));
            return;
        }
        // Enable clock to card
        usVal |= STDHOST_CLOCK_ENA;
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL, usVal);
    } else {
        // Disable clock to card
        usVal &= ~(STDHOST_CLOCK_ENA | STDHOST_INTERNAL_CLK_ENA);
        MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL, usVal);
    }
}

/******************************************************************************
 *
 *  SDHost_SetBusWidth - Set the SDIO bus width
 *
 * Description:
 *
 * Notes: see Part_A2_Host_Controller.pdf  p.61
 *
 * Context:
 *
 * Returns:
 *
 */
SK_BOOL
SDHost_SetBusWidth(PSD_DEVICE_DATA pDev, SK_U8 width)
{
    SK_U16 usValue, usOrgValue;
    SK_U8 ucHostCtrl = 0;
    SK_U8 orgIntEnable;

    if (pDev->bus_type != SDIO_BUS_TYPE) {
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG "%s: not possible in current bus mode (%d)\n",
                  __FUNCTION__, pDev->bus_type));
        return SK_FALSE;
    }

    DBGPRINT(DBG_LOAD,
             (KERN_DEBUG "%s: bus width = %d\n", __FUNCTION__, width));
    // save current irq mask
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_SIGNAL_ENABLE,
                    &usOrgValue);

    // disable card interrupt
    usValue = usOrgValue;
    usValue &= ~STDHOST_NORMAL_IRQ_CARD_SIG_ENA;

    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_SIGNAL_ENABLE,
                     usValue);

    // Disable Host Interrupt on card
    if (!SDHost_CMD52_Read(pDev, INT_ENABLE_REG, 0, &orgIntEnable)) {
        return SK_FALSE;
    }
    if (!SDHost_CMD52_Write(pDev, INT_ENABLE_REG, 0, 0)) {
        return SK_FALSE;
    }
    // Read current host control register
    MEM_READ_UCHAR(pDev, SK_SLOT_0, STDHOST_HOST_CTRL, &ucHostCtrl);

    if (width == SDIO_4_BIT) {
        pDev->bus_width = SDIO_4_BIT;
        if (!SDHost_CMD52_Write(pDev, BUS_INTERFACE_CONTROL_REG, 0, 0x82))      // 0x80:CD 
                                                                                // disable,1 
                                                                                // bit;0x82:CD 
                                                                                // disable,4 
                                                                                // bit
        {
            return SK_FALSE;
        }
        ucHostCtrl |= STDHOST_4_BIT_ENA;
    } else {
        pDev->bus_width = SDIO_1_BIT;
        if (!SDHost_CMD52_Write(pDev, BUS_INTERFACE_CONTROL_REG, 0, 0x80))      // 0x80:CD 
                                                                                // disable,1 
                                                                                // bit;0x80:CD 
                                                                                // disable,1 
                                                                                // bit
        {
            return SK_FALSE;
        }
        ucHostCtrl &= ~STDHOST_4_BIT_ENA;
    }

    // Setup host control register
    MEM_WRITE_UCHAR(pDev, SK_SLOT_0, STDHOST_HOST_CTRL, ucHostCtrl);

    // Re-enable Host Interrupt on card
    if (!SDHost_CMD52_Write(pDev, INT_ENABLE_REG, 0, orgIntEnable)) {
        return SK_FALSE;
    }
    // Re-enable card interrupt
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_SIGNAL_ENABLE,
                     usOrgValue);
    return SK_TRUE;
}

/******************************************************************************
 *
 *  SDHost_SetGPO - Set the GPO on/off
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *
 */
SK_BOOL
SDHost_SetGPO(PSD_DEVICE_DATA pDev, SK_U8 on)
{
    if (!pDev->CardIn) {
        return SK_FALSE;
    }

    if (on) {

        GPO_ON();
    } else {

        GPO_OFF();
    }
    return SK_TRUE;
}

/******************************************************************************
 *
 *  SDHost_PsState - Set the power state of the upper layer driver
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *
 */
VOID
SDHost_PsState(PSD_DEVICE_DATA pDev, SK_U8 State)
{
    pDev->PsState = State;
}

/******************************************************************************
 *
 *  SDHost_ErrorRecovery - SDIO Standard Host Spec conform error recovery
 *
 * Description:
 *
 * Notes:
 *
 * Context:
 *
 * Returns:
 *
 */
VOID
SDHost_ErrorRecovery(PSD_DEVICE_DATA pDev)
{
    SK_U8 ucValue;
    SK_U16 usValue;
    SK_U32 j, ulVal, ulDataLineMask;

    // We have to do spec conform error recovery
    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_STATUS, &usValue);
    DBGPRINT(DBG_ERROR, (KERN_DEBUG "~#~#~#~#\n"));
    DBGPRINT(DBG_W528D,
             (KERN_DEBUG "SDHost_ErrorRecovery : ERROR_IRQ_STATUS=0x%4.04X\n",
              usValue));
    // 1.) check whether any of bits3-0 are set.
    if (usValue & 0x000F) {
        // reset the command line
        MEM_WRITE_UCHAR(pDev, SK_SLOT_0,
                        STDHOST_SW_RESET, STDHOST_SW_RESET_CMD_LINE);

        // wait until CMD line reset is 0
        j = 0;
        do {
            MEM_READ_UCHAR(pDev, SK_SLOT_0, STDHOST_SW_RESET, &ucValue);
            DBGPRINT(DBG_W528D,
                     (KERN_DEBUG
                      "SDHost_ErrorRecovery : CMD-LINE SW_RESET=0x%2.02X\n",
                      ucValue));
            UDELAY(2);
        } while ((ucValue & STDHOST_SW_RESET_CMD_LINE) && j++ < 10000);
    }
    // 2.) check whether any of bits6-4 are set.
    if (usValue & 0x0070) {

        // reset the data line 
        MEM_WRITE_UCHAR(pDev, SK_SLOT_0,
                        STDHOST_SW_RESET, STDHOST_SW_RESET_DAT_LINE);

        // wait until data line reset is 0
        j = 0;
        do {

            MEM_READ_UCHAR(pDev, SK_SLOT_0, STDHOST_SW_RESET, &ucValue);
            DBGPRINT(DBG_W528D,
                     (KERN_DEBUG
                      "SDHost_ErrorRecovery : DAT-LINE SW_RESET=0x%2.02X\n",
                      ucValue));
            UDELAY(2);
        } while ((ucValue & STDHOST_SW_RESET_DAT_LINE) && j++ < 10000);

    }
    // clear error irq bits
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_STATUS, 0xFFFF);

    // wait for more than 40 us
    UDELAY(100);

    if (pDev->bus_type == SDIO_SPI_TYPE) {
        ulDataLineMask = STDHOST_STATE_DAT0_LINE_LVL;
    } else {
        if (pDev->bus_width == SDIO_1_BIT) {
            ulDataLineMask = STDHOST_STATE_DAT0_LINE_LVL;
        } else {
            ulDataLineMask =
                (STDHOST_STATE_DAT0_LINE_LVL | STDHOST_STATE_DAT1_LINE_LVL |
                 STDHOST_STATE_DAT2_LINE_LVL | STDHOST_STATE_DAT3_LINE_LVL);
        }
    }
    for (j = 0; j < 50; j++) {
        MEM_READ_ULONG(pDev, SK_SLOT_0, STDHOST_PRESENT_STATE, &ulVal);
        UDELAY(5);
        if ((ulVal & ulDataLineMask) == ulDataLineMask) {
            break;
        }
    }
    DBGPRINT(DBG_W528D,
             (KERN_DEBUG
              "SDHost_ErrorRecovery : DAT-LINE=%d %d %d %d CMD-Line=%d\n",
              ulVal & STDHOST_STATE_DAT3_LINE_LVL ? 1 : 0,
              ulVal & STDHOST_STATE_DAT2_LINE_LVL ? 1 : 0,
              ulVal & STDHOST_STATE_DAT1_LINE_LVL ? 1 : 0,
              ulVal & STDHOST_STATE_DAT0_LINE_LVL ? 1 : 0,
              ulVal & STDHOST_STATE_CMD_LINE_LVL ? 1 : 0));
    if (j >= 50) {
        DBGPRINT(DBG_ERROR,
                 (KERN_DEBUG
                  "SDHost_ErrorRecovery : Present state=8x%8.08X : Non-recoverable error (bad datalines 0x%8.08X)!!!!!!!!!!!!!!!!!!!!\n",
                  ulVal, ulVal & ulDataLineMask));
    }
    // clear error irq bits
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_ERROR_IRQ_STATUS, 0xFFFF);
    // clear normal irq bits
    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_NORMAL_IRQ_STATUS,
                     ~(pDev->currentIRQSignalMask));

    UDELAY(100);
}

SK_U32
SDHost_wait_event(PSD_DEVICE_DATA pDev, SDHOST_EVENT * pEvt, SK_U32 to)
{
    SK_U32 timeOut = 0;
    SK_U32 rc = 0;

    DECLARE_WAITQUEUE(sdwait, current);

    if (to == 0) {
        timeOut = LONG_MAX;
    } else {
        timeOut = to;
    }

//  DBGPRINT(DBG_ALL, (">>> %s: timeout=%d event=%ld\n",__FUNCTION__,timeOut,pEvt->event));

    add_wait_queue(&pEvt->wq, &sdwait);
    set_current_state(TASK_INTERRUPTIBLE);

    while ((test_bit(0, &pEvt->event) == 0) && timeOut) {
        timeOut = schedule_timeout(timeOut);
        if (signal_pending(current)) {
            rc = 1;
            DBGPRINT(DBG_ALL, ("%s: Signal pending!\n", __FUNCTION__));
            break;
        }
    }

    if (timeOut <= 0 || (test_bit(0, &pEvt->event) == 0)) {
        rc = 2;
        DBGPRINT(DBG_ALL,
                 ("%s: timeout=%d event=%ld\n", __FUNCTION__, timeOut,
                  pEvt->event));
    }
    set_current_state(TASK_RUNNING);
    remove_wait_queue(&pEvt->wq, &sdwait);

    // DBGPRINT(DBG_ALL, ("<<< %s: timeout=%d
    // event=%ld\n",__FUNCTION__,timeOut,pEvt->event));

    return (rc);
}

SK_BOOL
SDHost_DumpCIS(PSD_DEVICE_DATA pDev, SK_U8 function_number, SK_U32 length)
{
    SK_U8 R;
    SK_U32 _cis = 0;
    SK_U32 j;
    SK_U32 i = 0;
    SK_U32 len = 0;
    SK_U32 state = 0;

    if (!SDHost_CMD52_Read(pDev, FN_CIS_POINTER_0_REG(function_number), 0, &R)) {
        return SK_FALSE;
    }
    _cis = R;
    if (!SDHost_CMD52_Read(pDev, FN_CIS_POINTER_1_REG(function_number), 0, &R)) {
        return SK_FALSE;
    }
    _cis |= (R << 8);
    if (!SDHost_CMD52_Read(pDev, FN_CIS_POINTER_2_REG(function_number), 0, &R)) {
        return SK_FALSE;
    }
    _cis |= (R << 16);

    // Search for CISTPL
    printk("SDHost_DumpCIS : FN=%d CIS=%8.08X\n", function_number, _cis);
    R = 0;

    if (_cis == 0) {
        return SK_FALSE;
    }

    j = 0;
    while (j < length) {
        SDHost_CMD52_Read(pDev, _cis + j, function_number, &R);
        j++;
        switch (state) {
        case 0:
            {
                printk("%2.02X:", R);
                state++;
                if (R == 0xFF) {
                    printk("END\n");
                    return SK_TRUE;
                }
                break;
            }
        case 1:
            {
                printk("len=%2.02X:", R);
                len = R;
                i = 0;
                state++;
                break;
            }
        case 2:
            {
                printk("%2.02X ", R);
                i++;
                if (i >= len) {
                    printk("\n");
                    state = 0;
                }
                break;
            }
        }
    }
    printk("\n");

    return SK_TRUE;
}

SK_BOOL
SDHost_Disable_HighSpeedMode(PSD_DEVICE_DATA pDev)
{
    SK_U8 R;
    SK_U16 usVal;
    SK_U8 ucHostCtrl = 0;

    // Turn Off Bus Clock
    SDHost_SetClock(pDev, 0);

    MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL, &usVal);

    usVal &= 0x00FF;
    usVal |= MK_CLOCK((SK_U16) pDev->ClockSpeed);

    MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL, usVal);
    // Turn On Bus Clock
    SDHost_SetClock(pDev, 1);

    if (SDHost_CMD52_Read(pDev, HIGH_SPEED_REG, 0, &R) && pDev->ClockSpeed != 0) {
        if (R & EHS) {
            // Disable high speed mode if requested bus clock is not 50MHz 
            R &= ~EHS;
            SDHost_CMD52_Write(pDev, HIGH_SPEED_REG, 0, R);
        }
    }
    MEM_READ_UCHAR(pDev, SK_SLOT_0, STDHOST_HOST_CTRL, &ucHostCtrl);

    if (ucHostCtrl & STDHOST_HIGH_SPEED_ENA) {
        ucHostCtrl &= ~STDHOST_HIGH_SPEED_ENA;
        MEM_WRITE_UCHAR(pDev, SK_SLOT_0, STDHOST_HOST_CTRL, ucHostCtrl);
    }
    return SK_TRUE;
}

SK_BOOL
SDHost_Enable_HighSpeedMode(PSD_DEVICE_DATA pDev)
{
    SK_U8 R;
    SK_U16 usVal;
    SK_U8 ucHostCtrl = 0;

    // Read High Speed register of CCCR
    if (SDHost_CMD52_Read(pDev, HIGH_SPEED_REG, 0, &R) &&
        (pDev->ClockSpeed == 0)) {
        if (R & SHS) {
            /* Make the clock edge to rising */
            MEM_READ_UCHAR(pDev, SK_SLOT_0, STDHOST_HOST_CTRL, &ucHostCtrl);
            ucHostCtrl |= STDHOST_HIGH_SPEED_ENA;
            MEM_WRITE_UCHAR(pDev, SK_SLOT_0, STDHOST_HOST_CTRL, ucHostCtrl);

            DBGPRINT(DBG_W528D, (KERN_DEBUG "High speed mode is supported.\n"));

            // Enable high speed mode if requested bus clock is 50MHz 
            // and card supports high speed mode
            R |= EHS;
            SDHost_CMD52_Write(pDev, HIGH_SPEED_REG, 0, R);

            /* Change the Host controller clock to 50MHZ */
            // Turn Off Bus Clock
            SDHost_SetClock(pDev, 0);
            MEM_READ_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL, &usVal);
            usVal &= 0x00FF;
            usVal |= MK_CLOCK((SK_U16) pDev->ClockSpeed);
            MEM_WRITE_USHORT(pDev, SK_SLOT_0, STDHOST_CLOCK_CTRL, usVal);
            // Turn On Bus Clock
            SDHost_SetClock(pDev, 1);
        } else {
            DBGPRINT(DBG_W528D,
                     (KERN_DEBUG "High speed mode is not supported.\n"));
        }
    }
    return SK_TRUE;
}
