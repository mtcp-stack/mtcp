#!/bin/bash

# Check if you are root
user=`whoami`
if [ "root" != "$user" ]
then
    echo "You are not root!"
    exit 1
fi

sudo ./epserver -p /home/stack/asim/www/ -f epserver-master.conf -c 0
