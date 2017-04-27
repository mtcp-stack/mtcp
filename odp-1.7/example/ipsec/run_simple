#!/bin/bash
#
# Simple router test
#  - 2 loop interfaces
#  - 10 packets
#  - Specify API mode on command line
odp_ipsec -i loop1,loop2 \
-r 192.168.222.2/32:loop2:08.00.27.F5.8B.DB \
-s 192.168.111.2:192.168.222.2:loop1:loop2:10:100 \
-c 2 -m $1
