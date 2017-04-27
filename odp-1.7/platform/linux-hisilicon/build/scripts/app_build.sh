#!/bin/sh
export BUILD_PATH=$(cd "$(dirname "$0")"; pwd)/..
export ROOT_DIR=$BUILD_PATH/../../..
export BUILD_DIR=$BUILD_PATH

export l2fwd_hns_32=no
export l2fwd_hns_64=no
export l2fwd_32=no
export l2fwd_64=yes
export l2fwd_rss_32=no
export l2fwd_rss_64=yes
export classifier_64=yes
export classifier_direct_64=yes
export generator_64=yes
export ipsec_64=yes
export packet_64=yes
export time_64=yes
export timer_64=yes


rm  -f $BUILD_DIR/objs/examples/*


if [ "$l2fwd_hns_32" == "yes" ] ; then
	export BUILD_TYPE=build_32
	export FILE_NAME=l2fwd_hns_32
        export MAKE_PATH=$ROOT_DIR/platform/linux-hisilicon/example/hns_l2fwd

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile -j4
fi

if [ "$l2fwd_hns_64" == "yes" ] ; then
	export BUILD_TYPE=build_64
	export FILE_NAME=l2fwd_hns
        export MAKE_PATH=$ROOT_DIR/platform/linux-hisilicon/example/hns_l2fwd

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile -j4
fi


if [ "$l2fwd_32" == "yes" ] ; then
	export BUILD_TYPE=build_32
	export FILE_NAME=l2fwd
        export MAKE_PATH=$ROOT_DIR/example/l2fwd
	cp $BUILD_DIR/Makefile_app  $MAKE_PATH/Makefile

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile -j4
	rm -f $MAKE_PATH/Makefile
fi


if [ "$l2fwd_64" == "yes" ] ; then
	export BUILD_TYPE=build_64
	export FILE_NAME=l2fwd
        export MAKE_PATH=$ROOT_DIR/example/l2fwd
	cp $BUILD_DIR/Makefile_app  $MAKE_PATH/Makefile

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile -j4
	rm -f $MAKE_PATH/Makefile
fi

if [ "$l2fwd_rss_32" == "yes" ] ; then
	export BUILD_TYPE=build_32
	export FILE_NAME=l2fwd_rss
        export MAKE_PATH=$ROOT_DIR/example/l2fwd_rss
	cp $BUILD_DIR/Makefile_app  $MAKE_PATH/Makefile

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile -j4
	rm -f $MAKE_PATH/Makefile
fi


if [ "$l2fwd_rss_64" == "yes" ] ; then
	export BUILD_TYPE=build_64
	export FILE_NAME=l2fwd_rss
        export MAKE_PATH=$ROOT_DIR/example/l2fwd_rss
	cp $BUILD_DIR/Makefile_app  $MAKE_PATH/Makefile

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile -j4
	rm -f $MAKE_PATH/Makefile
fi

if [ "$classifier_64" == "yes" ] ; then
	export BUILD_TYPE=build_64
	export FILE_NAME=classifier
        export MAKE_PATH=$ROOT_DIR/example/classifier
	cp $BUILD_DIR/Makefile_app  $MAKE_PATH/Makefile

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile -j4
	rm -f $MAKE_PATH/Makefile
fi

if [ "$classifier_direct_64" == "yes" ] ; then
        export BUILD_TYPE=build_64
        export FILE_NAME=classifier_direct
        export MAKE_PATH=$ROOT_DIR/example/classifier_direct
        cp $BUILD_DIR/Makefile_app  $MAKE_PATH/Makefile

        make -s -f $MAKE_PATH/Makefile clean
        make -s -f $MAKE_PATH/Makefile -j4
        rm -f $MAKE_PATH/Makefile
fi


if [ "$generator_64" == "yes" ] ; then
	export BUILD_TYPE=build_64
	export FILE_NAME=generator
        export MAKE_PATH=$ROOT_DIR/example/generator
	cp $BUILD_DIR/Makefile_app  $MAKE_PATH/Makefile

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile -j4
	rm -f $MAKE_PATH/Makefile
fi

if [ "$ipsec_64" == "yes" ] ; then
	export BUILD_TYPE=build_64
	export FILE_NAME=ipsec
        export MAKE_PATH=$ROOT_DIR/example/ipsec
	cp $BUILD_DIR/Makefile_app  $MAKE_PATH/Makefile

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile -j4
	rm -f $MAKE_PATH/Makefile
fi


if [ "$packet_64" == "yes" ] ; then
	export BUILD_TYPE=build_64
	export FILE_NAME=packet
        export MAKE_PATH=$ROOT_DIR/example/packet
	cp $BUILD_DIR/Makefile_app  $MAKE_PATH/Makefile

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile -j4
	rm -f $MAKE_PATH/Makefile
fi



if [ "$time_64" == "yes" ] ; then
	export BUILD_TYPE=build_64
	export FILE_NAME=time
        export MAKE_PATH=$ROOT_DIR/example/time
	cp $BUILD_DIR/Makefile_app  $MAKE_PATH/Makefile

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile -j4
	rm -f $MAKE_PATH/Makefile
fi


if [ "$timer_64" == "yes" ] ; then
	export BUILD_TYPE=build_64
	export FILE_NAME=timer
        export MAKE_PATH=$ROOT_DIR/example/timer
	cp $BUILD_DIR/Makefile_app  $MAKE_PATH/Makefile

	make -s -f $MAKE_PATH/Makefile clean
	make -s -f $MAKE_PATH/Makefile -j4
	rm -f $MAKE_PATH/Makefile
fi




