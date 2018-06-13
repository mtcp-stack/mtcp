#! /bin/bash

MTCP_ROOT=`echo $PWD`

MTCP_DPDK_PATH=${MTCP_ROOT}/dpdk-17.08/

if [ -z "$RTE_TARGET" ]
then
    export RTE_TARGET=x86_64-native-linuxapp-gcc
fi

if [ -z "$RTE_SDK" ]
then
    export RTE_SDK=${MTCP_DPDK_PATH}
fi



