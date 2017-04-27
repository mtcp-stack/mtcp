#!/bin/sh
export BUILD_PATH=$(cd "$(dirname "$0")"; pwd)/..
export ROOT_DIR=$BUILD_PATH/../../..
export BUILD_DIR=$BUILD_PATH

export pv660_hns_32=no
export pv660_hns_64=yes
export pv660_sec_32=no
export pv660_sec_64=no
export ixgbe_32=no
export ixgbe_64=yes

rm  -f $BUILD_DIR/objs/drv/*

if [ "$pv660_hns_32" == "yes" ] ; then
	export BUILD_TYPE=build_32
	export FILE_NAME=pv660_net
        export MAKE_PATH=$ROOT_DIR/platform/linux-hisilicon/drivers/net/pv660/user

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile
fi

if [ "$pv660_hns_64" == "yes" ] ; then
	export BUILD_TYPE=build_64
	export FILE_NAME=pv660_net
        export MAKE_PATH=$ROOT_DIR/platform/linux-hisilicon/drivers/net/pv660/user

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile
fi

if [ "$pv660_sec_32" == "yes" ] ; then
	export BUILD_TYPE=build_32
	export FILE_NAME=pv660_sec
        export MAKE_PATH=$ROOT_DIR/platform/linux-hisilicon/drivers/accel/pv660/user

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile
fi

if [ "$pv660_sec_64" == "yes" ] ; then
	export BUILD_TYPE=build_64
	export FILE_NAME=pv660_sec
        export MAKE_PATH=$ROOT_DIR/platform/linux-hisilicon/drivers/accel/pv660/user

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile
fi

if [ "$ixgbe_32" == "yes" ] ; then
	export BUILD_TYPE=build_32
	export FILE_NAME=ixgbe
        export MAKE_PATH=$ROOT_DIR/platform/linux-hisilicon/drivers/net/ixgbe/user

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile
fi

if [ "$ixgbe_64" == "yes" ] ; then
	export BUILD_TYPE=build_64
	export FILE_NAME=ixgbe
        export MAKE_PATH=$ROOT_DIR/platform/linux-hisilicon/drivers/net/ixgbe/user

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile
fi