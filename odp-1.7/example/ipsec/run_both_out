#!/bin/bash
#
# Test AH and ESP output
#  - 2 loop interfaces
#  - 10 packets
#  - Specify API mode on command line
odp_ipsec -i loop1,loop2 \
-r 192.168.222.2/32:loop2:08.00.27.F5.8B.DB \
-p 192.168.111.0/24:192.168.222.0/24:out:both \
-e 192.168.111.2:192.168.222.2:\
3des:201:656c8523255ccc23a66c1917aa0cf30991fce83532a4b224 \
-a 192.168.111.2:192.168.222.2:md5:200:a731649644c5dee92cbd9c2e7e188ee6 \
-s 192.168.111.2:192.168.222.2:loop1:loop2:10:100 \
-c 2 -m $1
