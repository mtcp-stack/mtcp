#!/bin/bash

# check if there is only one additional command-line argument
if [ $# -ne 1 ]
then
    echo "Usage:"
    echo "$0 <4th-ip-octet-value>"
    exit 1
fi

# Check if you are root
user=`whoami`
if [ "root" != "$user" ]
then
    echo "You are not root!"
    exit 1
fi

# Create & configure /dev/dpdk-iface
rm -rf /dev/dpdk-iface
mknod /dev/dpdk-iface c 1110 0
chmod 666 /dev/dpdk-iface

# First check whether igb_uio module is already loaded
MODULE="igb_uio"

if lsmod | grep "$MODULE" &> /dev/null ; then
  echo "$MODULE is loaded!"
else
  echo "$MODULE is not loaded!"
  exit 1
fi

# Next check how many devices are there in the system
counter=0
cd /sys/module/igb_uio/drivers/pci:igb_uio/
for i in *
do
    if [[ $i == *":"* ]]
    then
	let "counter=$counter + 1"
    fi
done
cd -

# Configure each device (single-process version)
while [ $counter -gt 0 ]
do
    echo "/sbin/ifconfig dpdk$(($counter - 1)) 10.0.$(( $counter - 1 )).$1 netmask 255.255.255.0 up"
    /sbin/ifconfig dpdk$(($counter - 1)) 10.0.$(($counter - 1)).$1 netmask 255.255.255.0 up
    let "counter=$counter - 1"
done
