#make files in mtcp/src and apps/example
#mtcp/src
make clean
make "CFLAGS=-DDISABLE_DPDK -DDISABLE_HWCSUM -O0"
#files in util
cd ../../util
make clean
make
#files in apps/example
cd ../apps/example
make clean
make