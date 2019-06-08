#!/bin/bash

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

export KERNELDIR=$PWD/../../build/kernels/kernel-i386-intel-menlow/linux-2.6.31.4/
export CROSS_COMPILE=
export ARCH=i386

# The number of jobs to pass to tools that can run in parallel (such as make
# and dpkg-buildpackage
NUM_JOBS=`cat /proc/cpuinfo | grep processor | awk '{a++} END {print a}'`

BUILDDIR=$PWD/build
rm -rf $BUILDDIR
mkdir -p $BUILDDIR

pushd wlan_src
make clean || true
make -j$NUM_JOBS KERNELDIR=$KERNELDIR CROSS_COMPILE=$CROSS_COMPILE ARCH=$ARCH
make KERNELDIR=$KERNELDIR ARCH=$ARCH INSTALLDIR=$BUILDDIR install
popd
