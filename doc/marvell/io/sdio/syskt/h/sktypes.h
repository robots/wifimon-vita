/**
 *
 * Name:	sktypes.h
 * Project:	Wireless LAN, SDIO Bus driver
 * Version:	$Revision: 1.1 $
 * Date:	$Date: 2007/01/18 09:26:19 $
 * Purpose:	Define data types for SDIO bus driver
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
 *	$Log: sktypes.h,v $
 *	Revision 1.1  2007/01/18 09:26:19  pweber
 *	Put under CVS control
 *	
 *	
 *
 ******************************************************************************/

#ifndef __INC_SKTYPES_H
#define __INC_SKTYPES_H

/* defines ********************************************************************/

/*
 * Data types with a specific size. 'I' = signed, 'U' = unsigned.
 */
#define SK_I8	s8
#define SK_U8	u8
#define SK_I16	s16
#define SK_U16	u16
#define SK_I32	s32
#define SK_U32	u32
#define SK_U64	u64

#define SK_UPTR	SK_U32          /* casting pointer <-> integral */

#define	SK_CONSTI64(x)	(x##L)
#define	SK_CONSTU64(x)	(x##UL)
/*
 * Boolean type.
 */
#define SK_BOOL		SK_U8
#define SK_FALSE	(SK_BOOL)0
#define SK_TRUE		(SK_BOOL)(!SK_FALSE)

#define VOID 		void
#define PVOID 	VOID*
#define ULONG		SK_U32
#define USHORT	SK_U16
#define UCHAR		SK_U8
#define PUCHAR	SK_U8*
#define BOOLEAN	SK_BOOL
#define IN
#define OUT

/* typedefs *******************************************************************/

/* function prototypes ********************************************************/

#endif /* __INC_SKTYPES_H */
