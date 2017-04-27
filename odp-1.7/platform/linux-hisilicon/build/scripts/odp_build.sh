#!/bin/sh

export ABS_GCC_DIR="/opt/install"

export BUILD_PATH=$(cd "$(dirname "$0")"; pwd)/..
export ROOT_DIR=$BUILD_PATH/../../..

export LINUX_ENDNESS=LITTLE

export B=P660
export ENV=CHIP
export HRD_OS=LINUX
export HRD_ENDNESS=$LINUX_ENDNESS
export HRD_ARCH=HRD_ARM64

export OBJ_SO=libodp.so
export OBJ_A=libodp.a
export ODP_OBJ_FILENAME=odp.o
export HISI_OBJ_FILENAME=hisi.o


cp Makefile_odp $ROOT_DIR/platform/linux-generic
cd $ROOT_DIR/platform/linux-generic

make -s -f Makefile_odp clean
make -s -f Makefile_odp  -j16
cp $ODP_OBJ_FILENAME $BUILD_PATH/
make -s -f Makefile_odp clean
cd $BUILD_PATH

cp Makefile_hisilicon $ROOT_DIR/platform/linux-hisilicon
cd $ROOT_DIR/platform/linux-hisilicon

make -s -f Makefile_hisilicon clean
make -s -f Makefile_hisilicon  -j16
cp $HISI_OBJ_FILENAME $BUILD_PATH/
make -s -f Makefile_hisilicon clean
cd $BUILD_PATH

make -s -f Makefile_main clean
make -s -f Makefile_main $OBJ_A  -j16
make -s -f Makefile_main $OBJ_SO -j16

cp $OBJ_A  $BUILD_PATH/objs/lib/
cp $OBJ_SO $BUILD_PATH/objs/lib/

rm *.o
rm *.a
rm *.so

