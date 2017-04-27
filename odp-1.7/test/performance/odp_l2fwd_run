#!/bin/sh
#
# Copyright (c) 2015, Linaro Limited
# All rights reserved.
#
# SPDX-License-Identifier:	BSD-3-Clause
#

# TEST_DIR is set by Makefile, when we add a rule to Makefile for odp_l2fwd_run
# we can use TEST_DIR the same way odp_pktio_run uses it now.
# If TEST_DIR is not set it means we are not running with make, and in this case
# there are two situations:
# 1. user build ODP in the same dir as the source (most likely)
#    here the user can simply call odp_l2fwd_run
# 2. user may have built ODP in a separate build dir (like bitbake usually does)
#    here the user has to do something like $ODP/test/performance/odp_l2fwd_run
#
# In both situations the script assumes that the user is in the directory where
# odp_l2fwd exists. If that's not true, then the user has to specify the path
# to it and run:
# TEST_DIR=$builddir $ODP/test/performance/odp_l2fwd_run

# directory where test binaries have been built
TEST_DIR="${TEST_DIR:-$PWD}"
# directory where test sources are, including scripts
TEST_SRC_DIR=$(dirname $0)

PATH=$TEST_DIR:$TEST_DIR/../../example/generator:$PATH

# exit codes expected by automake for skipped tests
TEST_SKIPPED=77

# Use installed pktio env or for make check take it from platform directory
if [ -f "./pktio_env" ]; then
	. ./pktio_env
elif  [ "$ODP_PLATFORM" = "" ]; then
	echo "$0: error: ODP_PLATFORM must be defined"
	# not skipped as this should never happen via "make check"
	exit 1
elif [ -f ${TEST_SRC_DIR}/../../platform/$ODP_PLATFORM/test/pktio/pktio_env ]; then
	. ${TEST_SRC_DIR}/../../platform/$ODP_PLATFORM/test/pktio/pktio_env
else
	echo "BUG: unable to find pktio_env!"
	echo "pktio_env has to be in current directory or in platform/\$ODP_PLATFORM/test."
	echo "ODP_PLATFORM=\"$ODP_PLATFORM\""
	exit 1
fi

run_l2fwd()
{
	setup_pktio_env clean # install trap to call cleanup_pktio_env

	if [ $? -ne 0 ]; then
		echo "setup_pktio_env error $?"
		exit $TEST_SKIPPED
	fi

	type odp_generator > /dev/null
	if [ $? -ne 0 ]; then
		echo "odp_generator not installed. Aborting."
		cleanup_pktio_env
		exit 1
	fi

	#@todo: limit odp_generator to cores
	#https://bugs.linaro.org/show_bug.cgi?id=1398
	(odp_generator${EXEEXT} -I $IF0 \
			--srcip 192.168.0.1 --dstip 192.168.0.2 -m u 2>&1 > /dev/null) \
			2>&1 > /dev/null &
	GEN_PID=$!

	# this just turns off output buffering so that you still get periodic
	# output while piping to tee, as long as stdbuf is available.
	if [ "$(which stdbuf)" != "" ]; then
		STDBUF="stdbuf -o 0"
	else
		STDBUF=
	fi
	LOG=odp_l2fwd_tmp.log
	$STDBUF odp_l2fwd${EXEEXT} -i $IF1,$IF2 -m 0 -t 30 -c 2 | tee $LOG
	ret=$?

	kill ${GEN_PID}

	if [ ! -f $LOG ]; then
		echo "FAIL: $LOG not found"
		ret=1
	elif [ $ret -eq 0 ]; then
		PASS_PPS=5000
		MAX_PPS=$(awk '/TEST RESULT/ {print $3}' $LOG)
		if [ "$MAX_PPS" -lt "$PASS_PPS" ]; then
			echo "FAIL: pps below threshold $MAX_PPS < $PASS_PPS"
			ret=1
		fi
	fi

	rm -f $LOG
	cleanup_pktio_env

	exit $ret
}

case "$1" in
	setup)   setup_pktio_env   ;;
	cleanup) cleanup_pktio_env ;;
	*)       run_l2fwd ;;
esac
