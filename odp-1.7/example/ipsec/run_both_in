#!/bin/bash
#
# Test AH and ESP input
#  - 2 loop interfaces
#  - 10 packets
#  - Specify API mode on command line
odp_ipsec -i loop1,loop2 \
-r 192.168.111.2/32:loop1:08.00.27.76.B5.E0 \
-p 192.168.222.0/24:192.168.111.0/24:in:both \
-a 192.168.222.2:192.168.111.2:md5:300:27f6d123d7077b361662fc6e451f65d8 \
-e 192.168.222.2:192.168.111.2:\
3des:301:c966199f24d095f3990a320d749056401e82b26570320292 \
-s 192.168.222.2:192.168.111.2:loop2:loop1:10:100 \
-c 2 -m $1
