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

# How many processors are there?
cpus=`cat /proc/cpuinfo | grep processor | wc -l`

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

# Configure each device (multi-process version)
i=0
while [ $i -lt $counter ]
do
    # first set dpdk? interface
    echo "/sbin/ifconfig dpdk$i 10.0.$i.$1 netmask 255.255.255.0 up"
    /sbin/ifconfig dpdk$i 10.0.$i.$1 netmask 255.255.255.0 up

    # next set all aliased interfaces
    for (( j=0; j < $(($cpus)); j++ ))
    do
	echo "/sbin/ifconfig dpdk$i:$j 10.0.$i.$(($1 + 10 + $j)) netmask 255.255.255.0 up"
	/sbin/ifconfig dpdk$i:$j 10.0.$i.$(($1 + 10 + $j)) netmask 255.255.255.0 up
    done
    let "i=$i + 1"
done
