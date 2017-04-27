#!/bin/sh
export BUILD_PATH=$(cd "$(dirname "$0")"; pwd)/..
export ROOT_DIR=$BUILD_PATH/../../..
export BUILD_DIR=$BUILD_PATH

export MAKE_SRC_DIR=$BUILD_DIR/test/validation

rm  -f $BUILD_DIR/objs/test/validation/*


str="shmem,errno,time,scheduler,crypto,cpumask,buffer,hash,config,pool,queue,barrier,timer,pktio,classification,random,std_clib,system,packet,atomic,lock, thread"

arr=(${str//,/ })  


for file_name in ${arr[@]}  
do  
	export MAKE_DST_PATH=$ROOT_DIR/test/validation/$file_name
	cp $MAKE_SRC_DIR/Makefile_$file_name $MAKE_DST_PATH/Makefile
	make -s -f $MAKE_DST_PATH/Makefile clean
	make -s -f $MAKE_DST_PATH/Makefile -j4
	rm -f $MAKE_DST_PATH/Makefile
done 

export MAKE_DST_PATH=$ROOT_DIR/test/validation/init

export FILE_NAME=init_abort
cp $MAKE_SRC_DIR/Makefile_init_abort  $MAKE_DST_PATH/Makefile
make -s -f $MAKE_DST_PATH/Makefile clean
make -s -f $MAKE_DST_PATH/Makefile -j4

export FILE_NAME=init_log
cp $MAKE_SRC_DIR/Makefile_init_log  $MAKE_DST_PATH/Makefile
make -s -f $MAKE_DST_PATH/Makefile clean
make -s -f $MAKE_DST_PATH/Makefile -j4

export FILE_NAME=init_ok
cp $MAKE_SRC_DIR/Makefile_init_ok  $MAKE_DST_PATH/Makefile
make -s -f $MAKE_DST_PATH/Makefile clean
make -s -f $MAKE_DST_PATH/Makefile -j4

rm -f $MAKE_DST_PATH/Makefile
