#!/usr/bin/env bash
cd dpdk-17.08/
make install T=x86_64-native-linuxapp-gcc
cd ..
cd dpdk
rm -rf *
ln -s ../dpdk-17.08/x86_64-native-linuxapp-gcc/lib/ lib
ln -s ../dpdk-17.08/x86_64-native-linuxapp-gcc/include include
cd ..
autoreconf -ivf
./configure --with-dpdk-lib=`echo $PWD`/dpdk
make

