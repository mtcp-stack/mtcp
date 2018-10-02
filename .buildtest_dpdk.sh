#!/usr/bin/env bash
git submodule init
git submodule update
export RTE_SDK=`echo $PWD`/dpdk
export RTE_TARGET=x86_64-native-linuxapp-gcc
export MTCP_TARGET=`echo $PWD`/mtcp
sed -i -e 's/O_TO_EXE_STR =/\$(shell if [ \! -d \${RTE_SDK}\/\${RTE_TARGET}\/lib ]\; then mkdir \${RTE_SDK}\/\${RTE_TARGET}\/lib\; fi)\nLINKER_FLAGS = \$(call linkerprefix,\$(LDLIBS))\n\$(shell echo \${LINKER_FLAGS} \> \${RTE_SDK}\/\${RTE_TARGET}\/lib\/ldflags\.txt)\nO_TO_EXE_STR =/g' $RTE_SDK/mk/rte.app.mk
cd dpdk/
make install T=x86_64-native-linuxapp-gcc
cd ..
cd dpdk-iface-kmod
make
cd ..
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

