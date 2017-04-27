#!/bin/bash
#
# Test input AH
#  - 2 loop interfaces
#  - 10 packets
#  - Specify API mode on command line
odp_ipsec -i loop1,loop2 \
-r 192.168.111.2/32:loop1:08.00.27.76.B5.E0 \
-p 192.168.222.0/24:192.168.111.0/24:in:ah \
-a 192.168.222.2:192.168.111.2:md5:300:27f6d123d7077b361662fc6e451f65d8 \
-s 192.168.222.2:192.168.111.2:loop2:loop1:10:100 \
-c 2 -m $1
