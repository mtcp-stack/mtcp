#!/bin/sh
#
# Copyright (c) 2015, Linaro Limited
# All rights reserved.
#
# SPDX-License-Identifier:	BSD-3-Clause
#
# Script that passes command line arguments to odp_scheduling test when
# launched by 'make check'

TEST_DIR="${TEST_DIR:-$(dirname $0)}"
ret=0

run()
{
	echo odp_scheduling_run starts with $1 worker threads
	echo ===============================================

	$TEST_DIR/odp_scheduling${EXEEXT} -c $1 || ret=1
}

run 1
run 8

exit $ret
