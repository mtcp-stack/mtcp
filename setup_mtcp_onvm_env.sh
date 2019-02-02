#!/usr/bin/env bash

GREEN='\033[0;32m'
NC='\033[0m'

if [ -z "$RTE_SDK" ]; then
    echo "Please follow onvm install instructions to export \$RTE_SDK"
    exit 1
fi

if [ -z "$RTE_TARGET" ]; then
    echo "Please follow onvm install instructions to export \$RTE_TARGET"
    exit 1
fi

# Get to script directory
cd $(dirname ${BASH_SOURCE[0]})/

printf "${GREEN}Checking ldflags.txt...\n$NC"
if [ ! -f $RTE_SDK/$RTE_TARGET/lib/ldflags.txt ]; then
   echo "File $RTE_SDK/$RTE_TARGET/lib/ldflags.txt does not exist, please reinstall dpdk."
   sed -i -e 's/O_TO_EXE_STR =/\$(shell if [ \! -d \${RTE_SDK}\/\${RTE_TARGET}\/lib ]\; then mkdir \${RTE_SDK}\/\${RTE_TARGET}\/lib\; fi)\nLINKER_FLAGS = \$(call linkerprefix,\$(LDLIBS))\n\$(shell echo \${LINKER_FLAGS} \> \${RTE_SDK}\/\${RTE_TARGET}\/lib\/ldflags\.txt)\nO_TO_EXE_STR =/g' $RTE_SDK/mk/rte.app.mk
   exit 1
fi

printf "${GREEN}RTE_SDK$NC env variable is set to $RTE_SDK\n"
printf "${GREEN}RTE_TARGET$NC env variable is set to $RTE_TARGET\n"

# Check if you are using an Intel NIC
while true; do
    read -p "Are you using an Intel NIC (y/n)? " response
    case $response in
	[Yy]* ) break;;
	[Nn]* ) exit;;
    esac
done

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
