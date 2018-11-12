#!/usr/bin/env bash

GREEN='\033[0;32m'
NC='\033[0m'

# Get to script directory
cd $(dirname ${BASH_SOURCE[0]})/

# First download dpdk
if [ -z "$(ls -A $PWD/dpdk)" ]; then
    printf "${GREEN}Cloning dpdk...\n $NC"
    git submodule init
    git submodule update
fi

# Setup dpdk source for compilation
if [ "$#" -ne 1 ];
then
    export RTE_SDK=$PWD/dpdk
else
    export RTE_SDK=$1
fi
printf "${GREEN}Running dpdk_setup.sh...\n $NC"
if grep "ldflags.txt" $RTE_SDK/mk/rte.app.mk > /dev/null
then
    :
else
    sed -i -e 's/O_TO_EXE_STR =/\$(shell if [ \! -d \${RTE_SDK}\/\${RTE_TARGET}\/lib ]\; then mkdir \${RTE_SDK}\/\${RTE_TARGET}\/lib\; fi)\nLINKER_FLAGS = \$(call linkerprefix,\$(LDLIBS))\n\$(shell echo \${LINKER_FLAGS} \> \${RTE_SDK}\/\${RTE_TARGET}\/lib\/ldflags\.txt)\nO_TO_EXE_STR =/g' $RTE_SDK/mk/rte.app.mk
fi

# Compile dpdk and configure system
if [ -f $RTE_SDK/usertools/dpdk-setup.sh ]; then
    bash $RTE_SDK/usertools/dpdk-setup.sh
else
    bash $RTE_SDK/tools/setup.sh
fi

# Print the user message
cd $RTE_SDK
CONFIG_NUM=1
for cfg in config/defconfig_* ; do
    cfg=${cfg/config\/defconfig_/}
    if [ -d "$cfg" ]; then
	printf "Setting RTE_TARGET as $cfg\n"
	export RTE_TARGET=$cfg
    fi
    let "CONFIG_NUM+=1"
done
cd ..
printf "Set ${GREEN}RTE_SDK$NC env variable as $RTE_SDK\n"
printf "Set ${GREEN}RTE_TARGET$NC env variable as $RTE_TARGET\n"

# Create interfaces
printf "Creating ${GREEN}dpdk$NC interface entries\n"
cd dpdk-iface-kmod
make
if lsmod | grep dpdk_iface &> /dev/null ; then
    :
else    
    sudo insmod ./dpdk_iface.ko
fi
sudo -E make run
cd ..
