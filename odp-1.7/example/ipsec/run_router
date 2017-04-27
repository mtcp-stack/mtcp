#!/bin/bash
#
# Live router test
#  - 2 interfaces interfaces
#  - Specify API mode on command line
sudo odp_ipsec -i p7p1,p8p1 \
-r 192.168.111.2/32:p7p1:08.00.27.76.B5.E0 \
-r 192.168.222.2/32:p8p1:08.00.27.F5.8B.DB \
-c 1 -m $1
