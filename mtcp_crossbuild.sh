#!/bin/bash

args="$*"

usage ()
{
  echo "args=$args"
  echo
  echo "`basename $0` -h -c <x86/aarch64> -k <kernel_dir> -i <ext_lib> -d <dpdk_dir> -e"
  echo
  echo "Helper script, used to build dpdk."
  echo
  echo " -h               Help Usage"
  echo " -c <x86/aarch64> specify build platform"
  echo " -k <kernel_dir>  Directory that kernel builds if enable LKM build option"
  echo " -i <ext_lib>     If needed introduce external lib dependencies"
  echo " -d <dpdk_dir>    Directory that dpdk builds"
  echo " -e               Enable SPDK Support"
  echo
}

while getopts "hc:k:i:d:e" opt; do
  case $opt in
    h)  show_usage=1
        ;;
    c)  build_arch="$OPTARG"
        ;;
    k)  kernel_build="$OPTARG"
        ;;
    i)  ext_lib="$OPTARG"
        ;;
    d)  dpdk_build="$OPTARG"
        ;;
    e)  spdk_enable=1
	;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      show_usage=1
      ;;
  esac
done

if [ "$show_usage" == "1" ]; then
  usage
  exit 1
fi

CUR_PATH=`pwd`

if [ "$build_arch" == "aarch64" ]; then
  CROSS=aarch64-linux-gnu-
  RTE_TARGET=arm64-stingray-linuxapp-gcc
else
  RTE_TARGET=x86_64-native-linuxapp-gcc
fi

if [ -z "$kernel_build" ]; then
  RTE_KERNELDIR=/lib/modules/`uname -r`/build
else
  RTE_KERNELDIR=$kernel_build
fi

if [ -z "$ext_lib" ]; then
  EXT_LIB_DIR=$CUR_PATH/../ext_lib
else
  EXT_LIB_DIR=$ext_lib
fi

if [ "$spdk_enable" == "1" ]; then
  SPDK_OPTION="--enable-spdk"
fi

if [ -z $dpdk_build ]; then
  RTE_SDK=$CUR_PATH/../dpdk
else
  RTE_SDK=$dpdk_build
fi
DPDK_BUILD=$RTE_SDK/$RTE_TARGET
export RTE_SDK RTE_TARGET DPDK_BUILD RTE_KERNELDIR

# check ldflags.txt
if grep "ldflags.txt" $RTE_SDK/mk/rte.app.mk > /dev/null
then
    :
else
    sed -i -e 's/O_TO_EXE_STR =/\$(shell if [ \! -d \${RTE_SDK}\/\${RTE_TARGET}\/lib ]\; then mkdir \${RTE_SDK}\/\${RTE_TARGET}\/lib\; fi)\nLINKER_FLAGS = \$(call linkerprefix,\$(LDLIBS))\n\$(shell echo \${LINKER_FLAGS} \> \${RTE_SDK}\/\${RTE_TARGET}\/lib\/ldflags\.txt)\nO_TO_EXE_STR =/g' $RTE_SDK/mk/rte.app.mk
    echo "Need to rebuild dpdk."
    exit 1
fi

echo
echo "======================================"
echo "Build_arch  : $build_arch"
echo "RTE_TARGET  : $RTE_TARGET"
echo "DPDK Build  : $RTE_SDK"
echo "SPDK Support: $spdk_enable"
echo "======================================"
echo

CUR_PATH=`pwd`

# build kernel module
cd $CUR_PATH/dpdk-iface-kmod && make CROSS=$CROSS RTE_KERNELDIR=$RTE_KERNELDIR V=1

# build application
cd $CUR_PATH && autoreconf -f -i && \
    ./configure --host=aarch64 CC=${CROSS}gcc LD=${CROSS}ld --with-dpdk=$RTE_SDK/$RTE_TARGET --with-dpdk-lib=$RTE_SDK/$RTE_TARGET/lib $SPDK_OPTION CFLAGS="-I$EXT_LIB_DIR/include" LDFLAGS="-L$EXT_LIB_DIR/lib64 -L$EXT_LIB_DIR/lib"
make -j `grep -c ^processor /proc/cpuinfo` ARCH=arm64 CC=${CROSS}gcc LD=${CROSS}ld V=0

# end of file
