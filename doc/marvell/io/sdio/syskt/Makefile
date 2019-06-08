# File: Makefile
#
# Copyright (C) 2003-2009, Marvell International Ltd. 
#
# This software file (the "File") is distributed by Marvell International 
# Ltd. under the terms of the GNU General Public License Version 2, June 1991 
# (the "License").  You may use, redistribute and/or modify this File in 
# accordance with the terms and conditions of the License, a copy of which 
# is available by writing to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
# worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
#
# THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
# IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
# ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
# this warranty disclaimer.
 
# Things that need to export symbols
# export-objs := sdiobus.o


# Objects to compile in this module
# 
STDHOSTOBJS = 	sdiobus.o \
		stdhost.o \
		skisr.o 


obj-m += mrvlsdio.o
mrvlsdio-objs := $(STDHOSTOBJS)


# EXTRA_CFLAGS += -DDBG
# EXTRA_CFLAGS += -DSDIO_API_INTERNAL
# EXTRA_CFLAGS += -DSDIO_BUS_DRIVER
EXTRA_CFLAGS += -DSDIO_ALTERNATIVE_IRQ
EXTRA_CFLAGS += -I$(PWD)

#
# Make target rules
# 
all: mrvlsdio.o
ifeq ($(KVER),2.6)
	@echo "Finished Making Marvell SDIO Stdhost Linux Driver for Kernel 2.6"
else
	@echo "Finished Making Marvell SDIO Stdhost Linux Driver for Kernel 2.4"
endif

mrvlsdio.o: $(STDHOSTOBJS)
	   $(LD) -r $^ -o $@	

clean:
	find . -name "*.o" -exec rm {} \;
	find . -name "*.*~" -exec rm {} \;
	find . -name "*.d" -exec rm {} \;
	find . -name "*.mod.c" -exec rm {} \;
	find . -name "*.ko" -exec rm {} \;
	find . -name ".*.cmd" -exec rm {} \;
