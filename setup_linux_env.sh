#!/bin/bash

GREEN='\033[0;32m'
NC='\033[0m'

# Get to script directory
cd $(dirname ${BASH_SOURCE[0]})/

# Remove dpdk_iface.ko module
if [$# -ne 1];
then
    export RTE_SDK=$1
else
    export RTE_SDK=$PWD/dpdk
fi
printf "${GREEN}Removing dpdk_iface module...\n $NC"
if lsmod | grep dpdk_iface &> /dev/null ; then
    sudo rmmod dpdk_iface.ko
else    
    :
fi

# Compile dpdk and configure system
if [ -f $RTE_SDK/usertools/dpdk-setup.sh ]; then
    bash $RTE_SDK/usertools/dpdk-setup.sh
else
    bash $RTE_SDK/tools/setup.sh
fi

printf "${GREEN}Goodbye!$NC\n"

