#!/bin/bash

if [ -z ${1} ]; then
	echo "should be called with a path"
	exit
fi
ROOTDIR=${1}

CUSTOM_STR=${CUSTOM_STR:-https://git.linaro.org/lng/odp.git}

echo -n "'${CUSTOM_STR}' ($(cat ${ROOTDIR}/.scmversion))"
