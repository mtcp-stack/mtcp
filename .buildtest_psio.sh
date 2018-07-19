#!/usr/bin/env bash
export PSIO_TARGET=`echo $PWD`/io_engine
export MTCP_TARGET=`echo $PWD`/mtcp
autoreconf -ivf
./configure --with-psio-lib=`echo $PWD`/io_engine
make
cd apps/lighttpd-1.4.32/
autoreconf -ivf
./configure --without-bzip2 CFLAGS="-g -O3" --with-libmtcp=$MTCP_TARGET --with-libpsio=$PSIO_TARGET
make
cd ../apache_benchmark
./configure --with-libmtcp=$MTCP_TARGET --with-libpsio=$PSIO_TARGET
make
