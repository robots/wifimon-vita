/**
 *
 * Name: stdhost.h
 * Project: Wireless LAN, Bus driver for SDIO interface
 * Version: $Revision: 1.1 $
 * Date: $Date: 2007/01/18 09:26:19 $
 * Purpose: This module handles the SDIO Standard Host Interface.
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
 * $Log: stdhost.h,v $
 * Revision 1.1  2007/01/18 09:26:19  pweber
 * Put under CVS control
 *
 * 
 ******************************************************************************/

//  pweber 17.08.2005 taken from eagledev.h
typedef enum
{
    // card is in full power
    PS_STATE_FULL_POWER,
    // PS mode, but card is ready for TX
    PS_STATE_WAKEUP,
    // PS Mode, card is not ready for TX
    PS_STATE_SLEEP,
    // SLEEP event is already received, but have not sent confirm yet
    PS_STATE_SLEEP_PENDING,
} PS_STATE;

#define MAX_BUS_ERRORS 10

// #define SD_BLOCK_SIZE 32

#define DEBUG_BREAK_CMD53_RD      0x00000001
#define DEBUG_BREAK_CMD53_WR      0x00000002
#define DEBUG_DISABLE_HW_WORKAROUND_RD 0x00000004
#define DEBUG_DISABLE_HW_WORKAROUND_WR 0x00000008
#define DEBUG_SHOW_REG_10_14      0x00000010
#define DEBUG_USE_RD_DMA      0x00000100
#define DEBUG_USE_WR_DMA            0x00000200
#define DEBUG_BREAK_START_FDO      0x80000000

// SDIO CMD Macros
#define MAKE_SDIO_OFFSET(x)   ((SK_U32)((SK_U32)(x)<<9))
#define MAKE_SDIO_OP_CODE(x)   ((SK_U32)((SK_U32)(x)<<26))
#define MAKE_SDIO_BLOCK_MODE(x) ((SK_U32)((SK_U32)(x)<<27))
#define MAKE_SDIO_FUNCTION(x) 	((SK_U32)((SK_U32)(x)<<28))
#define MAKE_SDIO_DIR(x) 				((SK_U32)((SK_U32)(x)<<31))

// SD Host Standard Register                                                                                                                                            
#define DMA_16K_BOUNDARY (0x2 << 12)
#define STDHOST_SYSTEM_ADDRESS			 			0x00    // System 
                                                                                // Address 
                                                                                // 4 
                                                                                // bytes
#define STDHOST_BLOCK_SIZE					 			0x04    // 2 
                                                                                                // bytes
#define STDHOST_BLOCK_COUNT					 			0x06    // 2 
                                                                                                // bytes
#define STDHOST_CMD_ARGUMENT				 			0x08    // 4 
                                                                                        // bytes 
                                                                                        // -> 
                                                                                        // bit 
                                                                                        // 39 
                                                                                        // - 
                                                                                        // 8 
                                                                                        // of 
                                                                                        // SDIO 
                                                                                        // CMD
#define STDHOST_TRANSFER_MODE				 			0x0C    // 2 
                                                                                        // bytes
#define STDHOST_MULTI_BLOCK_SELECT 				(1<<5)
#define STDHOST_READ_DIR_SELECT						(1<<4)
#define STDHOST_AUTO_CMD12_ENA						(1<<2)
#define STDHOST_BLOCK_COUNT_ENA 					(1<<1)
#define STDHOST_DMA_ENA										(1<<0)

#define STDHOST_COMMAND							 			0x0E    // 2 
                                                                                                        // bytes
#define STDHOST_CMD_TYPE_NORMAL						0x00
#define STDHOST_CMD_TYPE_SUSPEND					0x01
#define STDHOST_CMD_TYPE_RESUME						0x02
#define STDHOST_CMD_TYPE_ABORT						0x03
#define STDHOST_CMD_TYPE_MASK							0x03
#define MK_CMD_TYPE(x) 										((x)<<6)
#define MK_CMD(x) 												((x)<<8)
#define MAX_WAIT_COMMAND_COMPLETE					1000

#define STDHOST_CMD_DATA_PRESENT					(1<<5)
#define STDHOST_CMD_INDEX_CHK_ENA					(1<<4)
#define STDHOST_CMD_CRC_CHK_ENA						(1<<3)
#define STDHOST_RESP_TYPE_NONE						0x00
#define STDHOST_RESP_TYPE_R2							0x01
#define STDHOST_RESP_TYPE_R1							0x02
#define STDHOST_RESP_TYPE_R3							0x02
#define STDHOST_RESP_TYPE_R4							0x02
#define STDHOST_RESP_TYPE_R5							0x02
#define STDHOST_RESP_TYPE_R6							0x02
#define STDHOST_RESP_TYPE_R1b							0x03
#define STDHOST_RESP_TYPE_R5b							0x03

#define STDHOST_RESP_BITS_31_0  		 			0x10    // 4 
                                                                                // bytes
#define STDHOST_RESP_BITS_63_32			 			0x14    // 4 
                                                                                // bytes
#define STDHOST_RESP_BITS_95_64			 			0x18    // 4 
                                                                                // bytes
#define STDHOST_RESP_BITS_127_96		 			0x1C    // 4 
                                                                                // bytes

#define	GET_BITS_135_128(x)	((SK_U8)((SK_U32)(x)>>24))
#define	GET_BITS_127_120(x)	((SK_U8)((SK_U32)(x)>>16))
#define	GET_BITS_119_112(x)	((SK_U8)((SK_U32)(x)>>8))
#define	GET_BITS_111_104(x)	((SK_U8)(x))
#define	GET_BITS_103_96(x) 	((SK_U8)((SK_U32)(x)>>24))
#define	GET_BITS_95_88(x)		((SK_U8)((SK_U32)(x)>>16))
#define	GET_BITS_87_80(x)		((SK_U8)((SK_U32)(x)>>8))
#define	GET_BITS_79_72(x)		((SK_U8)(x))
#define	GET_BITS_71_64(x)		((SK_U8)((SK_U32)(x)>>24))
#define	GET_BITS_63_56(x)		((SK_U8)((SK_U32)(x)>>16))
#define	GET_BITS_55_48(x)		((SK_U8)((SK_U32)(x)>>8))
#define	GET_BITS_47_40(x)		((SK_U8)(x))
#define	GET_BITS_39_32(x)		((SK_U8)((SK_U32)(x)>>24))
#define	GET_BITS_31_24(x)		((SK_U8)((SK_U32)(x)>>16))
#define	GET_BITS_23_16(x) 	((SK_U8)((SK_U32)(x)>>8))
#define	GET_BITS_15_08(x) 	((SK_U8)(x))

/*
#define	GET_BITS_135_128(x)	((SK_U8)(x))
#define	GET_BITS_127_120(x)	((SK_U8)((SK_U32)(x)>>8)) 
#define	GET_BITS_119_112(x)	((SK_U8)((SK_U32)(x)>>16))  
#define	GET_BITS_111_104(x)	((SK_U8)((SK_U32)(x)>>24))      
#define	GET_BITS_103_96(x) 	((SK_U8)(x))               
#define	GET_BITS_95_88(x)		((SK_U8)((SK_U32)(x)>>8))  
#define	GET_BITS_87_80(x)		((SK_U8)((SK_U32)(x)>>16)) 
#define	GET_BITS_79_72(x)		((SK_U8)((SK_U32)(x)>>24))
#define	GET_BITS_71_64(x)		((SK_U8)(x))               
#define	GET_BITS_63_56(x)		((SK_U8)((SK_U32)(x)>>8))  
#define	GET_BITS_55_48(x)		((SK_U8)((SK_U32)(x)>>16)) 
#define	GET_BITS_47_40(x)		((SK_U8)((SK_U32)(x)>>24))
#define	GET_BITS_39_32(x)		((SK_U8)(x))              
#define	GET_BITS_31_24(x)		((SK_U8)((SK_U32)(x)>>8)) 
#define	GET_BITS_23_16(x) 	((SK_U8)((SK_U32)(x)>>16))	
#define	GET_BITS_15_08(x) 	((SK_U8)((SK_U32)(x)>>24))
*/

/* CIS Tuples*/
#define CISTPL_VERS_1   0x15
#define CISTPL_MANFID   0x20
#define CISTPL_FUNCID   0x21
#define CISTPL_FUNCE    0x22
#define CISTPL_NULL			0x00
#define CISTPL_END      0xFF

#define STDHOST_DATA_PORT						 			0x20    // 4 
                                                                                                        // bytes

#define STDHOST_PRESENT_STATE				 			0x24    // 4 
                                                                                        // bytes
#define STDHOST_STATE_CMD_LINE_LVL				(1<<24)
#define STDHOST_STATE_DAT3_LINE_LVL				(1<<23)
#define STDHOST_STATE_DAT2_LINE_LVL				(1<<22)
#define STDHOST_STATE_DAT1_LINE_LVL				(1<<21)
#define STDHOST_STATE_DAT0_LINE_LVL				(1<<20)
#define STDHOST_STATE_WRITE_PROTECT_LVL		(1<<19)
#define STDHOST_STATE_CARD_DETECT_LVL			(1<<18)
#define STDHOST_STATE_CARD_STATE_STABLE 	(1<<17)
#define STDHOST_STATE_CARD_INSERTED				(1<<16)
#define STDHOST_STATE_BUFFER_RD_ENABLE		(1<<11)
#define STDHOST_STATE_BUFFER_WR_ENABLE		(1<<10)
#define STDHOST_STATE_RD_TRANSFER_ACTIVE	(1<<9)
#define STDHOST_STATE_WR_TRANSFER_ACTIVE	(1<<8)
#define STDHOST_STATE_DAT_LINE_ACTIVE			(1<<2)
#define STDHOST_STATE_CMD_INHIB_DAT				(1<<1)
#define STDHOST_STATE_CMD_INHIB_CMD				(1<<0)
#define MAX_WAIT_COMMAND_INHIBIT					1000

#define STDHOST_HOST_CTRL						 			0x28    // 1 
                                                                                                        // byte
#define STDHOST_HIGH_SPEED_ENA						(1<<2)
#define STDHOST_4_BIT_ENA									(1<<1)
#define STDHOST_LED_ON										(1<<0)

#define STDHOST_POWER_CTRL					 			0x29    // 1 
                                                                                                // byte
#define STDHOST_VOLTAGE_3_3_V							0x07
#define STDHOST_VOLTAGE_3_0_V							0x06
#define STDHOST_VOLTAGE_1_8_V							0x05
#define MK_VOLTAGE(x)											((x)<<1)
#define STDHOST_POWER_ON									(1<<0)

#define STDHOST_BLOCK_GAP_CTRL			 			0x2A    // 1 
                                                                                // byte
#define STDHOST_WAKEUP_CTRL					 			0x2B    // 1 
                                                                                                // byte
#define STDHOST_WAKEUP_ON_CARD_REMOVE			0x04
#define STDHOST_WAKEUP_ON_CARD_INSERT			0x02
#define STDHOST_WAKEUP_ON_CARD_IRQ				0x01

#define STDHOST_CLOCK_CTRL					 			0x2C    // 2 
                                                                                                // byte
#define STDHOST_CLOCK_DIV_BASE_CLK  			0x00
#define STDHOST_CLOCK_DIV_2								0x01
#define STDHOST_CLOCK_DIV_4								0x02
#define STDHOST_CLOCK_DIV_8								0x04
#define STDHOST_CLOCK_DIV_16							0x08
#define STDHOST_CLOCK_DIV_32							0x10
#define STDHOST_CLOCK_DIV_64							0x20
#define STDHOST_CLOCK_DIV_128							0x40
#define STDHOST_CLOCK_DIV_256							0x80
#define MK_CLOCK(x)												((x)<<8)
#define STDHOST_CLOCK_ENA								  (1<<2)
#define STDHOST_INTERNAL_CLK_STABLE				(1<<1)
#define STDHOST_INTERNAL_CLK_ENA					(1<<0)
#define	MAX_WAIT_CLOCK_STABLE							1000

#define STDHOST_TIMEOUT_CTRL				 			0x2E    // 1 
                                                                                        // byte
#define STDHOST_SW_RESET						 			0x2F    // 1 
                                                                                                        // byte
#define STDHOST_SW_RESET_DAT_LINE					(1<<2)
#define STDHOST_SW_RESET_CMD_LINE					(1<<1)
#define STDHOST_SW_RESET_ALL							(1<<0)

#define STDHOST_NORMAL_IRQ_STATUS		 			0x30    // 2 
                                                                                // byte
#define STDHOST_NORMAL_IRQ_ERROR					(1<<15)
#define STDHOST_NORMAL_IRQ_CARD 					(1<<8)
#define STDHOST_NORMAL_IRQ_CARD_OUT				(1<<7)
#define STDHOST_NORMAL_IRQ_CARD_IN				(1<<6)
#define STDHOST_NORMAL_IRQ_BUF_RD_RDY			(1<<5)
#define STDHOST_NORMAL_IRQ_BUF_WR_RDY			(1<<4)
#define STDHOST_NORMAL_IRQ_DMA						(1<<3)
#define STDHOST_NORMAL_IRQ_BLOCK_GAP			(1<<2)
#define STDHOST_NORMAL_IRQ_TRANS_COMPLETE	(1<<1)
#define STDHOST_NORMAL_IRQ_CMD_COMPLETE		(1<<0)

#define STDHOST_ERROR_IRQ_STATUS		 			0x32    // 2 
                                                                                // byte
#define STDHOST_ERROR_IRQ_VENDOR					(0x0F<<12)
#define STDHOST_ERROR_IRQ_GPI_1						(1<<15)
#define STDHOST_ERROR_IRQ_GPI_0						(1<<14)
#define STDHOST_ERROR_IRQ_AUTO_CMD12			(1<<8)
#define STDHOST_ERROR_IRQ_CURRENT_LIMIT		(1<<7)
#define STDHOST_ERROR_IRQ_DATA_END_BIT		(1<<6)
#define STDHOST_ERROR_IRQ_DATA_CRC				(1<<5)
#define STDHOST_ERROR_IRQ_DATA_TIMEOUT		(1<<4)
#define STDHOST_ERROR_IRQ_CMD_INDEX				(1<<3)
#define STDHOST_ERROR_IRQ_CMD_END_BIT			(1<<2)
#define STDHOST_ERROR_IRQ_CMD_CRC					(1<<1)
#define STDHOST_ERROR_IRQ_CMD_TIMEOUT			(1<<0)

#define STDHOST_NORMAL_IRQ_STATUS_ENABLE	0x34    // 2 byte
#define STDHOST_NORMAL_IRQ_CARD_ENA				(1<<8)
#define STDHOST_NORMAL_IRQ_CARD_OUT_ENA		(1<<7)
#define STDHOST_NORMAL_IRQ_CARD_IN_ENA		(1<<6)
#define STDHOST_NORMAL_IRQ_BUF_RD_RDY_ENA	(1<<5)
#define STDHOST_NORMAL_IRQ_BUF_WR_RDY_ENA	(1<<4)
#define STDHOST_NORMAL_IRQ_DMA_ENA				(1<<3)
#define STDHOST_NORMAL_IRQ_BLOCK_GAP_ENA	(1<<2)
#define STDHOST_NORMAL_IRQ_TRANS_COMPLETE_ENA	(1<<1)
#define STDHOST_NORMAL_IRQ_CMD_COMPLETE_ENA	(1<<0)

#define STDHOST_ERROR_IRQ_STATUS_ENABLE		0x36    // 2 byte
#define STDHOST_ERROR_IRQ_VENDOR_ENA				(0x0F<<12)
#define STDHOST_ERROR_IRQ_AUTO_CMD12_ENA		(1<<8)
#define STDHOST_ERROR_IRQ_CURRENT_LIMIT_ENA	(1<<7)
#define STDHOST_ERROR_IRQ_DATA_END_BIT_ENA	(1<<6)
#define STDHOST_ERROR_IRQ_DATA_CRC_ENA			(1<<5)
#define STDHOST_ERROR_IRQ_DATA_TIMEOUT_ENA	(1<<4)
#define STDHOST_ERROR_IRQ_CMD_INDEX_ENA			(1<<3)
#define STDHOST_ERROR_IRQ_CMD_END_BIT_ENA		(1<<2)
#define STDHOST_ERROR_IRQ_CMD_CRC_ENA				(1<<1)
#define STDHOST_ERROR_IRQ_CMD_TIMEOUT_ENA		(1<<0)

#define STDHOST_NORMAL_IRQ_SIGNAL_ENABLE	0x38    // 2 byte
#define STDHOST_NORMAL_IRQ_ALL_SIG_ENA						0xFFFF
#define STDHOST_NORMAL_IRQ_CARD_SIG_ENA						(1<<8)
#define STDHOST_NORMAL_IRQ_CARD_OUT_SIG_ENA				(1<<7)
#define STDHOST_NORMAL_IRQ_CARD_IN_SIG_ENA				(1<<6)
#define STDHOST_NORMAL_IRQ_BUF_RD_RDY_SIG_ENA			(1<<5)
#define STDHOST_NORMAL_IRQ_BUF_WR_RDY_SIG_ENA			(1<<4)
#define STDHOST_NORMAL_IRQ_DMA_SIG_ENA						(1<<3)
#define STDHOST_NORMAL_IRQ_BLOCK_GAP_SIG_ENA			(1<<2)
#define STDHOST_NORMAL_IRQ_TRANS_COMPLETE_SIG_ENA	(1<<1)
#define STDHOST_NORMAL_IRQ_CMD_COMPLETE_SIG_ENA		(1<<0)

/*
#define STDHOST_NORMAL_IRQ_CARD_ALL_ENA	( STDHOST_NORMAL_IRQ_CARD_OUT_SIG_ENA | \
																					STDHOST_NORMAL_IRQ_CARD_IN_SIG_ENA  | \
			      STDHOST_NORMAL_IRQ_CARD_SIG_ENA)
*/

/*
#define STDHOST_NORMAL_IRQ_CARD_ALL_ENA	( STDHOST_NORMAL_IRQ_CARD_OUT_SIG_ENA | \
																					STDHOST_NORMAL_IRQ_CARD_IN_SIG_ENA  | \
																					STDHOST_NORMAL_IRQ_TRANS_COMPLETE_SIG_ENA | \
																					STDHOST_NORMAL_IRQ_CARD_SIG_ENA)
*/
 /**/ extern int intmode;
#define STDHOST_GPIO_IRQ_CARD_ALL_ENA	(  \
		STDHOST_NORMAL_IRQ_CARD_OUT_SIG_ENA | \
		STDHOST_NORMAL_IRQ_CARD_IN_SIG_ENA  | \
		STDHOST_NORMAL_IRQ_TRANS_COMPLETE_SIG_ENA)

#define STDHOST_NORMAL_IRQ_CARD_ALL_ENA         \
		((intmode == 0) ? (STDHOST_GPIO_IRQ_CARD_ALL_ENA | STDHOST_NORMAL_IRQ_CARD_SIG_ENA) : \
		(STDHOST_GPIO_IRQ_CARD_ALL_ENA))
 /**/
#define STDHOST_ERROR_IRQ_SIGNAL_ENABLE		0x3A    // 2 byte
#define STDHOST_ERROR_IRQ_VENDOR_SIG_ENA				(0x0F<<12)
#define STDHOST_ERROR_IRQ_AUTO_CMD12_SIG_ENA		(1<<8)
#define STDHOST_ERROR_IRQ_CURRENT_LIMIT_SIG_ENA	(1<<7)
#define STDHOST_ERROR_IRQ_DATA_END_BIT_SIG_ENA	(1<<6)
#define STDHOST_ERROR_IRQ_DATA_CRC_SIG_ENA			(1<<5)
#define STDHOST_ERROR_IRQ_DATA_TIMEOUT_SIG_ENA	(1<<4)
#define STDHOST_ERROR_IRQ_CMD_INDEX_SIG_ENA			(1<<3)
#define STDHOST_ERROR_IRQ_CMD_END_BIT_SIG_ENA		(1<<2)
#define STDHOST_ERROR_IRQ_CMD_CRC_SIG_ENA				(1<<1)
#define STDHOST_ERROR_IRQ_CMD_TIMEOUT_SIG_ENA		(1<<0)
#define STDHOST_AUTO_CMD12_ERROR_STATUS		0x3C    // 2 byte
#define STDHOST_CAPABILITIES							0x40    // 4 
                                                                                        // byte
#define STDHOST_CAP_VOLTAGE_1_8						(1<<26)
#define STDHOST_CAP_VOLTAGE_3_0						(1<<25)
#define STDHOST_CAP_VOLTAGE_3_3						(1<<24)
#define STDHOST_CAP_SUSPENSE_RESUME				(1<<23)
#define STDHOST_CAP_DMA										(1<<22)
#define STDHOST_CAP_HIGH_SPEED						(1<<21)
#define STDHOST_CAP_MAX_BLOCK_LEN					(0x03<<16)
#define STDHOST_CAP_MAX_BLOCK_512					(0x00<<16)
#define STDHOST_CAP_MAX_BLOCK_1024				(0x01<<16)
#define STDHOST_CAP_MAX_BLOCK_2048				(0x02<<16)
#define STDHOST_CAP_BASE_CLOCK_FREQ				(0x3F<<8)
#define GET_CAP_BASE_CLOCK_FREQ(x)				(SK_U8)(((x)>>8)&0x000000FF)
#define STDHOST_CAP_TIMEOUT_CLOCK_UNIT		(1<<7)
#define STDHOST_CAP_TIMEOUT_CLOCK_UNIT_MHZ	(1<<7)
#define STDHOST_CAP_TIMEOUT_CLOCK_FREQ		0x3F
#define STDHOST_CAPABILITIES_RESERVED			0x44    // 4 byte
#define STDHOST_MAX_CURRENT_CAP						0x48    // 4 
                                                                                // byte
#define STDHOST_MAX_CURRENT_CAP_1_8_V			(0xFF<<16)
#define GET_MAX_CURRENT_CAP_1_8_V(x)			(((x)>>16)&0x000000FF)
#define STDHOST_MAX_CURRENT_CAP_3_0_V			(0xFF<<8)
#define GET_MAX_CURRENT_CAP_3_0_V(x)			(((x)>>8)&0x000000FF)
#define STDHOST_MAX_CURRENT_CAP_3_3_V		 	0xFF
#define GET_MAX_CURRENT_CAP_3_3_V(x)			((x)&0x000000FF)
#define CALC_MAX_CURRENT_IN_MA(x)					((x)*4)
#define STDHOST_MAX_CURRENT_CAP_RESERVED	0x4C    // 4 byte
#define STDHOST_SLOT_IRQ_STATUS						0xFC    // 2 
                                                                                // byte
#define STDHOST_SLOT_IRQ_SLOT_0						0x01
#define STDHOST_SLOT_IRQ_SLOT_1						0x02
#define STDHOST_SLOT_IRQ_SLOT_2						0x04
#define STDHOST_SLOT_IRQ_SLOT_3						0x08
#define STDHOST_SLOT_IRQ_SLOT_4						0x10
#define STDHOST_SLOT_IRQ_SLOT_5						0x20
#define STDHOST_SLOT_IRQ_SLOT_6						0x40
#define STDHOST_SLOT_IRQ_SLOT_7						0x80
#define STDHOST_HOST_CONTROLLER_VERSION		0xFE    // 2 byte
#define GET_VENDOR_VERSION_NR(x)					((x)>>8)
#define GET_SPEC_VERSION_NR(x)						((x)&0x00FF)
// FPGA specific registers
#define FPGA_CORE_CTRL_REG									0x200
#define FPGA_RAISING_EDGE								(1 << 8)
#define FPGA_CARD_REVISION								0x210
#define FPGA_CARD_REV_1_0									0x10
#define FPGA_CARD_REV_1_1									0x11
#define FPGA_CHIP_REVISION								0x211
#define FPGA_CHIP_REV_1_0									0x10    // SDIO/SPI
#define FPGA_CHIP_REV_1_1									0x11    // SDIO/SPI/G-SPI
#define FPGA_CHIP_REV_2_0									0x20    // SDIO/SPI/G-SPI/DMA
#define FPGA_CONFIG_ID										0x212
#define FPGA_CONFIG_3_3_V									(1<<0)
#define FPGA_CONFIG_2_5_V									(1<<1)
#define FPGA_CONFIG_1_8_V									(1<<2)
#define FPGA_POWER_REG_DATA								0x20C
#define FPGA_POWER_REG_3_3_V							0xEF
#define FPGA_POWER_REG_2_5_V              0xDC
#define FPGA_POWER_REG_1_8_V              0xCC
#define FPGA_POWER_REG_CMD								0x20D
#define FPGA_POWER_REG_CMD_START					0x80
#define FPGA_POWER_REG_STATUS							0x20E
#define FPGA_POWER_REG_STABLE							(1<<0)
#define FPGA_POWER_REG_1_8_V_MIN            0xBC        // 1.54 V
#define FPGA_POWER_REG_1_8_V_MAX            0xD9        // 2.31 V
#define FPGA_POWER_REG_2_5_V_MIN            0xCF        // 1.98 V
#define FPGA_POWER_REG_2_5_V_MAX            0xEC        // 3.08 V
#define FPGA_POWER_REG_3_3_V_MIN            0xE8        // 2.70 V
#define FPGA_POWER_REG_3_3_V_MAX            0xEF        // 3.30 V
#define FPGA_POWER_REG_0_7_V		    0x88        // 0.72 V
#define FPGA_POWER_REG_0_8_V		    0x8B        // 0.78 V
#define FPGA_POWER_REG_0_9_V		    0x98        // 0.90 V
#define FPGA_POWER_REG_1_0_V		    0x9B        // 0.97 V
#define FPGA_POWER_REG_1_1_V		    0xA8        // 1.08 V
#define FPGA_POWER_REG_1_2_V		    0xAB        // 1.17 V
#define FPGA_POWER_REG_1_2_5_V		    0xAC        // 1.23 V
#define FPGA_POWER_REG_1_3_V		    0xAF        // 1.32 V
#define FPGA_POWER_REG_1_4_V		    0xBA        // 1.42 V
#define FPGA_POWER_REG_1_5_V		    0xBC        // 1.53 V
#define FPGA_POWER_REG_1_6_V		    0xC8        // 1.62 V
#define FPGA_POWER_REG_1_8_V		    0xCC        // 1.84 V
#define FPGA_POWER_REG_2_0_V		    0xCF        // 1.98 V
#define FPGA_POWER_REG_2_2_V		    0xD8        // 2.25 V
#define FPGA_POWER_REG_2_4_V		    0xDB        // 2.43 V
#define FPGA_POWER_REG_2_6_V		    0xDD        // 2.62 V
#define FPGA_POWER_REG_2_7_V		    0xDF        // 2.75 V
#define FPGA_POWER_REG_2_9_V		    0xEB        // 2.92 V
#define FPGA_POWER_REG_3_1_V		    0xED        // 3.15 V
#define FPGA_POWER_REG_3_3_V		    0xEF        // 3.30 V
#define FPGA_POWER_STEPS		    20
//[mb] 300 is a good value for 3.3V
#define FPGA_POWER_RAMP_DELAY		300
#define FPGA_POWER_VAL_ENUM	{ \
				FPGA_POWER_REG_0_7_V, \
				FPGA_POWER_REG_0_8_V, \
				FPGA_POWER_REG_0_9_V, \
				FPGA_POWER_REG_1_0_V, \
				FPGA_POWER_REG_1_1_V, \
				FPGA_POWER_REG_1_2_V, \
				FPGA_POWER_REG_1_2_5_V, \
				FPGA_POWER_REG_1_3_V, \
				FPGA_POWER_REG_1_4_V, \
				FPGA_POWER_REG_1_5_V, \
				FPGA_POWER_REG_1_6_V, \
				FPGA_POWER_REG_1_8_V, \
				FPGA_POWER_REG_2_0_V, \
				FPGA_POWER_REG_2_2_V, \
				FPGA_POWER_REG_2_4_V, \
				FPGA_POWER_REG_2_6_V, \
				FPGA_POWER_REG_2_7_V, \
				FPGA_POWER_REG_2_9_V, \
				FPGA_POWER_REG_3_1_V, \
				FPGA_POWER_REG_3_3_V}
#define SK_SLOT_0	0
#define SK_SLOT_1	1
#define SK_BAR( _pObj, _nr ) ((PSD_DEVICE_DATA)(_pObj)->IOBase[_nr])
//
//          PCI memory mapped I/O access routines
//
#define MEM_WRITE_UCHAR(_pObj,_base, _offset, _value)  *(SK_U8 *)(((SK_U32)(SK_BAR((_pObj),(_base))))+(_offset)) = (SK_U8)(_value)
#define MEM_WRITE_USHORT(_pObj,_base, _offset, _value) *(SK_U16 *)(((SK_U32)(SK_BAR((_pObj),(_base))))+(_offset)) = (SK_U16)(_value)
#define MEM_WRITE_ULONG(_pObj,_base, _offset, _value)  *(SK_U32 *)(((SK_U32)(SK_BAR((_pObj),(_base))))+(_offset)) = (SK_U32)(_value)
/*
#define MEM_WRITE_UCHAR(_pObj,_base, _offset, _value)\
	printk("wr-uc:%p=0x%2.02X\n",(SK_U8 *)(((SK_U32)(SK_BAR((_pObj),(_base))))+(_offset)),(SK_U8)(_value));\
  *(SK_U8 *)(((SK_U32)(SK_BAR((_pObj),(_base))))+(_offset)) = (SK_U8)(_value)

#define MEM_WRITE_USHORT(_pObj,_base, _offset, _value)\
	printk("wr-us:%p=0x%4.04X\n",(SK_U16 *)(((SK_U32)(SK_BAR((_pObj),(_base))))+(_offset)),(SK_U16)(_value));\
 *(SK_U16 *)(((SK_U32)(SK_BAR((_pObj),(_base))))+(_offset)) = (SK_U16)(_value)

#define MEM_WRITE_ULONG(_pObj,_base, _offset, _value)\
	printk("wr-ul:%p=0x%8.08X\n",(SK_U32 *)(((SK_U32)(SK_BAR((_pObj),(_base))))+(_offset)),(SK_U32)(_value));\
  *(SK_U32 *)(((SK_U32)(SK_BAR((_pObj),(_base))))+(_offset)) = (SK_U32)(_value)
*/
#define MEM_READ_UCHAR(_pObj,_base, _offset, _pBuf)  *(SK_U8 *)(_pBuf) = *(volatile SK_U8 *)(((SK_U32)(SK_BAR((_pObj),(_base))))+(_offset))
#define MEM_READ_USHORT(_pObj,_base, _offset, _pBuf) *(SK_U16 *)(_pBuf) = *(volatile SK_U16 *)(((SK_U32)(SK_BAR((_pObj),(_base))))+(_offset))
#define MEM_READ_ULONG(_pObj,_base, _offset, _pBuf)  *(SK_U32 *)(_pBuf) = *(volatile SK_U32 *)(((SK_U32)(SK_BAR((_pObj),(_base))))+(_offset))
void SDHost_Init(PSD_DEVICE_DATA pDev);
SK_BOOL SDHost_isCardPresent(PSD_DEVICE_DATA pDev);
SK_BOOL isCmdFailed(PSD_DEVICE_DATA pDev);
SK_BOOL SDHost_InitializeCard(PSD_DEVICE_DATA pDev);
VOID SDHost_EnableInterrupt(PSD_DEVICE_DATA pDev, SK_U16 mask);
VOID SDHost_DisableInterrupt(PSD_DEVICE_DATA pDev, SK_U16 mask);
VOID SDHost_if_EnableInterrupt(PSD_DEVICE_DATA pDev);
VOID SDHost_if_DisableInterrupt(PSD_DEVICE_DATA pDev);
SK_BOOL SDHost_ReadOCR(PSD_DEVICE_DATA pDev, SK_U8 * pOcr);
SK_BOOL SDHost_SendCmd(PSD_DEVICE_DATA pDev, SK_U8 cmd, SK_U32 arg,
                       SK_U16 transferMode, SK_U32 * pulResp);
VOID SDHost_SetCardOutEvent(PSD_DEVICE_DATA pDev);
SK_BOOL SDHost_WriteOCR(PSD_DEVICE_DATA pDev, SK_U8 * pOcr);
SK_BOOL SDHost_ReadRCA(PSD_DEVICE_DATA pDev, SK_U8 * pRca);
SK_BOOL SDHost_SendAbort(PSD_DEVICE_DATA pDev);
SK_BOOL SDHost_CMD52_Write(PSD_DEVICE_DATA pDev, SK_U32 Offset,
                           SK_U8 function_number, SK_U8 Data);
SK_BOOL SDHost_CMD52_Read(PSD_DEVICE_DATA pDev, SK_U32 Offset,
                          SK_U8 function_number, SK_U8 * pReturnData);
SK_BOOL SDHost_ReadCIS(PSD_DEVICE_DATA pDev, SK_U8 function_number,
                       SK_U8 cistpl, SK_U8 * pBuf, SK_U32 * length);
SK_BOOL SDHost_CMD53_Write(PSD_DEVICE_DATA pDev, SK_U32 Offset,
                           SK_U8 function_number, SK_U8 mode, SK_U8 opcode,
                           SK_U8 * pData, SK_U32 Count, SK_U32 blksz);
SK_BOOL SDHost_CMD53_Write_DMA(PSD_DEVICE_DATA pDev, SK_U32 Offset,
                               SK_U8 function_number, SK_U8 mode, SK_U8 opcode,
                               SK_U8 * pData, SK_U32 Count, SK_U32 blksz);
SK_BOOL SDHost_CMD53_Read(PSD_DEVICE_DATA pDev, SK_U32 Offset,
                          SK_U8 function_number, SK_U8 mode, SK_U8 opcode,
                          SK_U8 * pData, SK_U32 Count, SK_U32 blksz);
SK_BOOL SDHost_CMD53_Read_DMA(PSD_DEVICE_DATA pDev, SK_U32 Offset,
                              SK_U8 function_number, SK_U8 mode, SK_U8 opcode,
                              SK_U8 * pData, SK_U32 Count, SK_U32 blksz);

SK_BOOL SDHost_CMD53_WriteEx(PSD_DEVICE_DATA pDev,
                             SK_U32 Offset,
                             SK_U8 function_number,
                             SK_U8 mode,
                             SK_U8 opcode,
                             SK_U8 * pData, SK_U32 Count, SK_U32 blksz);

SK_BOOL SDHost_CMD53_ReadEx(PSD_DEVICE_DATA pDev,
                            SK_U32 Offset,
                            SK_U8 function_number,
                            SK_U8 mode,
                            SK_U8 opcode,
                            SK_U8 * pData, SK_U32 Count, SK_U32 blksz);

SK_BOOL SDHost_disable_host_int(PSD_DEVICE_DATA pDev, SK_U8 function_number,
                                SK_U8 mask);
SK_BOOL SDHost_enable_host_int(PSD_DEVICE_DATA pDev, SK_U8 function_number,
                               SK_U8 mask);
SK_U32 SDHost_send_cmd(PSD_DEVICE_DATA pDev, SK_U8 * payload, SK_U32 length,
                       SK_BOOL wait_status);
VOID SDHost_ErrorRecovery(PSD_DEVICE_DATA pDev);
SK_BOOL SDHost_lockInterface(PSD_DEVICE_DATA pDev);
VOID SDHost_unlockInterface(PSD_DEVICE_DATA pDev);
VOID SDHost_SetLed(PSD_DEVICE_DATA pDev, SK_U8 led, SK_U8 on);
VOID SDHost_PsState(PSD_DEVICE_DATA pDev, SK_U8 State); // pweber 17.08.2005
SK_BOOL SDHost_SetGPO(PSD_DEVICE_DATA pDev, SK_U8 on);  // mmoser 2007-02-20
SK_U32 SDHost_wait_event(PSD_DEVICE_DATA pDev, SDHOST_EVENT * pEvt, SK_U32 to);
VOID SDHost_SetClock(PSD_DEVICE_DATA pDev, SK_U8 on);
SK_BOOL SDHost_SetBusWidth(PSD_DEVICE_DATA pDev, SK_U8 width);
VOID SDHost_SetClockSpeed(PSD_DEVICE_DATA pDev, SK_U32 bus_freq);
SK_BOOL SDHost_Enable_HighSpeedMode(PSD_DEVICE_DATA pDev);
SK_BOOL SDHost_Disable_HighSpeedMode(PSD_DEVICE_DATA pDev);

#define LED_ON(x){\
SK_U32 ___ulVal,___ulMask;\
	MEM_READ_ULONG (pDev, SK_SLOT_0, 0x200,  &___ulVal);\
	___ulMask = (1<<(16+(x)));\
	___ulVal |= ___ulMask;\
	MEM_WRITE_ULONG (pDev, SK_SLOT_0, 0x200,  ___ulVal);\
}
#define LED_OFF(x){\
SK_U32 ___ulVal,___ulMask;\
	MEM_READ_ULONG (pDev, SK_SLOT_0, 0x200,  &___ulVal);\
	___ulMask = (1<<(16+(x)));\
	___ulVal &= ~___ulMask;\
	MEM_WRITE_ULONG (pDev, SK_SLOT_0, 0x200,  ___ulVal);\
}

#define GPO_ON(){\
SK_U32 ___ulVal,___ulMask;\
	MEM_READ_ULONG (pDev, SK_SLOT_0, 0x200,  &___ulVal);\
	___ulMask = (1<<24);\
	___ulVal |= ___ulMask;\
	MEM_WRITE_ULONG (pDev, SK_SLOT_0, 0x200,  ___ulVal);\
}
#define GPO_OFF(){\
SK_U32 ___ulVal,___ulMask;\
	MEM_READ_ULONG (pDev, SK_SLOT_0, 0x200,  &___ulVal);\
	___ulMask = (1<<24);\
	___ulVal &= ~___ulMask;\
	MEM_WRITE_ULONG (pDev, SK_SLOT_0, 0x200,  ___ulVal);\
}
