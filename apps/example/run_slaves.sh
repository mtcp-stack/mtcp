#!/bin/bash

# Check if you are root
user=`whoami`
if [ "root" != "$user" ]
then
    echo "You are not root!"
    exit 1
fi

for i in {1..15}; do ./epserver -p /home/stack/asim/www/ -f epserver-slave.conf -c $i & done
