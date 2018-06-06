#!/usr/bin/env bash
autoreconf -ivf
./configure --with-psio-lib=`echo $PWD`/io_engine
make

