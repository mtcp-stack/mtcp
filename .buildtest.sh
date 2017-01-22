#!/usr/bin/env bash
cd dpdk-16.11/
make install T=x86_64-native-linuxapp-gcc
cd ..
cd dpdk
rm -rf *
ln -s ../dpdk-16.11/x86_64-native-linuxapp-gcc/lib/ lib
ln -s ../dpdk-16.11/x86_64-native-linuxapp-gcc/include include
cd ..
./configure --with-dpdk-lib=`echo $PWD`/dpdk
make

