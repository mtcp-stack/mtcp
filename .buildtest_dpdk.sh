#!/usr/bin/env bash
cd dpdk/
make install T=x86_64-native-linuxapp-gcc
cd ..
export RTE_SDK=`echo $PWD`/dpdk
export RTE_TARGET=x86_64-native-linuxapp-gcc
export MTCP_TARGET=`echo $PWD`/mtcp
#cd dpdk
#rm -rf *
#ln -s ../dpdk-17.08/x86_64-native-linuxapp-gcc/lib/ lib
#ln -s ../dpdk-17.08/x86_64-native-linuxapp-gcc/include include
#cd ..
autoreconf -ivf
./configure --with-dpdk-lib=$RTE_SDK/$RTE_TARGET
make
cd apps/lighttpd-1.4.32/
autoreconf -ivf
./configure --without-bzip2 CFLAGS="-g -O3" --with-libmtcp=$MTCP_TARGET --with-libdpdk=$RTE_SDK/$RTE_TARGET
make
cd ../apache_benchmark
./configure --with-libmtcp=$MTCP_TARGET --with-libdpdk=$RTE_SDK/$RTE_TARGET
make

