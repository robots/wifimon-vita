/**
 *
 * Name: skdebug.h
 * Project: Wireless LAN SDIO bus driver
 * Version: $Revision: 1.1 $
 * Date:    $Date: 2007/01/18 09:26:19 $
 * Purpose: 
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

/*****************************************************************************
*
* History:
* $Log: skdebug.h,v $
* Revision 1.1  2007/01/18 09:26:19  pweber
* Put under CVS control
*
*
*****************************************************************************/

#ifndef _SKDEBUG_H_
#define _SKDEBUG_H_

// #define DEBUG_VERBOSE                1

#define DBG_NONE        0x00000000      // No debug - all off.
#define DBG_ERROR       0x00000001      // Debug for ERRORS
#define DBG_WARNING     0x00000002      // Debug for Warnings.
#define DBG_LOAD        0x00000004      // Debug for driver load
#define DBG_PNP		      0x00000008
#define DBG_PWR		      0x00000010
#define DBG_IOCTL				0x00000020
#define DBG_PDO					0x00000040
#define DBG_IRQ					0x00000080
#define DBG_W528D				0x00000100
#define DBG_W528D_CMD52	0x00000200
#define DBG_W528D_CMD53	0x00000400
#define DBG_API					0x00000800

#define DBG_T1          0x01000000      // Our various time stamps.. up to 6.
#define DBG_T2          0x02000000      // ||
#define DBG_T3          0x04000000      // ||
#define DBG_T4          0x08000000      // ||
#define DBG_T5          0x10000000      // ||
#define DBG_T6          0x20000000      // \/
#define DBG_ALL         0xffffffff      // All debug enabled.

#define DBG_TIMESTAMPS ( DBG_T1 |  DBG_T2  | DBG_T3 | DBG_T4 | DBG_T5 | DBG_T6 )

#undef KERN_DEBUG
#define KERN_DEBUG

#define DBG_DEFAULT (DBG_NONE \
| DBG_ERROR      \
| DBG_WARNING    \
| DBG_W528D			 \
| DBG_LOAD      \
)
/***
| DBG_W528D_CMD52 \
| DBG_W528D_CMD53 \
|	DBG_TIMESTAMPS \
| DBG_PNP        \
| DBG_IRQ        \
| DBG_PDO        \
| DBG_PWR        \
| DBG_W528D			 \
| DBG_LOAD      \
| DBG_IOCTL      \
| DBG_ERROR      \
| DBG_WARNING    \

****/

#ifdef DBG
extern unsigned char lptRegImage1;
extern unsigned char lptRegImage2;
extern ULONG stdDebugLevel;
#endif // #ifdef DBG

#ifdef DBG

#define DBG_CRLF        0x00000000      // add some cr lfs before this message.
#define DBG_RAW         0x00000000      // raw debug output
#define DBG_DONTCARE (DBG_RAW | DBG_CRLF)       // either of these by
                                                // themselves should not cause
                                                // debug to print.

/**
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
**/

#define BUGPRINT(Str) DbgPrint Str

#define	ENTER()			DBGPRINT(DBG_LOAD,( KERN_DEBUG "Enter: %s, %s:%i\n", __FUNCTION__, \
							__FILE__, __LINE__))
#define	LEAVE()			DBGPRINT(DBG_LOAD,( KERN_DEBUG "Exit: %s, %s:%i\n", __FUNCTION__, \
							__FILE__, __LINE__))

#define PrintMacro printk

#define INITDEBUG()

#define DBGPRINT(lvl, Str)                              \
    {                                                   \
    ULONG __lvl = lvl;                                  \
    if (stdDebugLevel & (__lvl & ~DBG_DONTCARE))            \
    {                                                   \
        PrintMacro Str ;                                   \
    }}

#else
#define DBGPRINT(lvl,Str)
#define INITDEBUG()
#define ENTER()
#define LEAVE()
#endif
#endif
