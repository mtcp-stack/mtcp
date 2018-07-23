#!/usr/bin/env bash
export MTCP_TARGET=`echo $PWD`/mtcp
autoreconf -ivf
./configure --enable-netmap
make
cd apps/lighttpd-1.4.32/
autoreconf -ivf
./configure --without-bzip2 CFLAGS="-g -O3" --with-libmtcp=$MTCP_TARGET --enable-netmap
make
cd ../apache_benchmark
./configure --with-libmtcp=$MTCP_TARGET --enable-netmap
make
