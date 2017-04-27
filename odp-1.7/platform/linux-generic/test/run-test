#!/bin/bash
#
# Run the ODP test applications and report status in a format that
# matches the automake "make check" output.
#
# The list of tests to be run is obtained by sourcing a file that
# contains an environment variable in the form;
#
# TEST="test_app1 test_app2"
#
# The default behaviour is to run all the tests defined in files
# named tests-*.env in the same directory as this script, but a single
# test definition file can be specified using the TEST_DEF environment
# variable.
#
# Test definition files may optionally also specify a LOG_COMPILER
# which will be invoked as a wrapper to each of the test application
# (as per automake).
#
TDIR=$(dirname $(readlink -f $0))
PASS=0
FAIL=0
SKIP=0
res=0

if [ "$V" != "0" ]; then
	verbose=1
else
	verbose=0
	mkdir -p logs
fi

do_run_tests() {
	source $1

	for tc in $TESTS; do
		tc=$(basename $tc)
		if [ "$verbose" = "0" ]; then
			logfile=logs/${tc}.log
			touch $logfile || logfile=/dev/null
			$LOG_COMPILER $TDIR/$tc > $logfile 2>&1
		else
			$LOG_COMPILER $TDIR/$tc
		fi

		tres=$?
		case $tres in
		0)  echo "PASS: $tc"; let PASS=$PASS+1 ;;
		77) echo "SKIP: $tc"; let SKIP=$SKIP+1 ;;
		*)  echo "FAIL: $tc"; let FAIL=$FAIL+1; res=1 ;;
		esac
	done
}

if [ "$TEST_DEFS" != "" -a -f "$TEST_DEFS" ]; then
	do_run_tests $TEST_DEFS
elif [ "$1" != "" ]; then
	do_run_tests $TDIR/tests-${1}.env
else
	for tenv in $TDIR/tests-*.env; do
		do_run_tests $tenv
	done
fi

echo "TEST RESULT: $PASS tests passed, $SKIP skipped, $FAIL failed"

exit $res
