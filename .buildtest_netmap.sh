#!/usr/bin/env bash
autoreconf -ivf
./configure --enable-netmap
make

