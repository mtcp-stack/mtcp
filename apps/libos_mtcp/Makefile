# TODO: Make this Makefile.in pretty

TARGETS = mtcp_queue
CC=gcc -g -O3 -Wall -Werror -fgnu89-inline
DPDK=1
PS=0
NETMAP=0
ONVM=0
CFLAGS=-g -O2

# DPDK LIBRARY and HEADER
DPDK_INC=/users/jingliu/datacenter-OS/libos/libmtcp/mtcp/dpdk/include
DPDK_LIB=/users/jingliu/datacenter-OS/libos/libmtcp/mtcp/dpdk/lib/

# mtcp library and header 
MTCP_FLD    =../../mtcp/
MTCP_INC    =-I${MTCP_FLD}/include
MTCP_LIB    =-L${MTCP_FLD}/lib
MTCP_TARGET = ${MTCP_LIB}/libmtcp.a

UTIL_FLD = ../../util
UTIL_INC = -I${UTIL_FLD}/include
UTIL_OBJ = ${UTIL_FLD}/http_parsing.o ${UTIL_FLD}/tdate_parse.o ${UTIL_FLD}/netlib.o


PS_DIR = ../../io_engine/
PS_INC = ${PS_DIR}/include
INC = -I./include/ ${UTIL_INC} ${MTCP_INC} -I${UTIL_FLD}/include
LIBS = ${MTCP_LIB}
ifeq ($(PS),1)
INC += -I{PS_INC}
LIBS += -lmtcp -L${PS_DIR}/lib -lps -lpthread -lnuma -lrt
endif

ifeq ($(NETMAP),1)
LIBS += -lmtcp -lpthread -lnuma -lrt
endif

# CFLAGS for DPDK-related compilation
INC += ${MTCP_INC}
ifeq ($(DPDK),1)
DPDK_MACHINE_FLAGS = $(shell cat /users/jingliu/datacenter-OS/libos/libmtcp/mtcp/dpdk/include/cflags.txt)
INC += ${DPDK_MACHINE_FLAGS} -I${DPDK_INC} -include $(DPDK_INC)/rte_config.h
endif

ifeq ($(shell uname -m),x86_64)
LIBS += -m64
endif

ifeq ($(DPDK),1)
DPDK_LIB_FLAGS = $(shell cat /users/jingliu/datacenter-OS/libos/libmtcp/mtcp/dpdk/lib/ldflags.txt) -lgmp
#LIBS += -m64 -g -O3 -pthread -lrt -march=native -Wl,-export-dynamic ${MTCP_FLD}/lib/libmtcp.a -L../../dpdk/lib -Wl,-lnuma -Wl,-lmtcp -Wl,-lpthread -Wl,-lrt -Wl,-ldl -Wl,${DPDK_LIB_FLAGS}
LIBS += -g -O3 -pthread -lrt -march=native -export-dynamic ${MTCP_FLD}/lib/libmtcp.a -L../../dpdk/lib -lnuma -lmtcp -lpthread -lrt -ldl ${DPDK_LIB_FLAGS}
else
#LIBS += -m64 -g -O3 -pthread -lrt -march=native -Wl,-export-dynamic ${MTCP_FLD}/lib/libmtcp.a -L../../dpdk/lib -Wl,-lnuma -Wl,-lmtcp -Wl,-lpthread -Wl,-lrt -Wl,-ldl -Wl,${DPDK_LIB_FLAGS}
LIBS += -g -O3 -pthread -lrt -march=native -export-dynamic ${MTCP_FLD}/lib/libmtcp.a -L../../dpdk/lib -lnuma -lmtcp -lpthread -lrt -ldl ${DPDK_LIB_FLAGS}
endif

ifeq ($(ONVM),1)
ifeq ($(RTE_TARGET),)
$(error "Please define RTE_TARGET environment variable")
endif

INC += -I/onvm_nflib
INC += -DENABLE_ONVM
LIBS += /onvm_nflib/$(RTE_TARGET)/libonvm.a
endif

ifeq ($V,) # no echo
	export MSG=@echo
	export HIDE=@
else
	export MSG=@\#
	export HIDE=
endif

ZEUS_MTCP_CFLAGS=-I/users/jingliu/datacenter-OS/include/
ZEUS_MTCP_CFLAGS += ${CFLAGS}
ZEUS_SRC_DIR=/users/jingliu/datacenter-OS/
ZEUS_CFLAGS := -I$(ZEUS_SRC_DIR)
ZEUS_LDFLAGS := -L$(ZEUS_SRC_DIR) -lhoard -Wl,-rpath,$(ZEUS_SRC_DIR)
ZEUS_MTCP_CFLAGS += ${ZEUS_CFLAGS}
ZEUS_MTCP_LDFLAGS := ${ZEUS_LDFLAGS}

all: mtcp_queue

mtcp_queue: mtcp_queue.o ${MTCP_FLD}/lib/libmtcp.a
	gcc mtcp_queue.o -shared ${ZEUS_MTCP_LDFLAGS} ${LIBS} ${UTIL_OBJ} -o libmtcp_queue.so

mtcp_queue.o: mtcp_queue.c
	gcc -c -fPIC mtcp_queue.c ${ZEUS_MTCP_CFLAGS} ${INC} -o $@

#epserver.o: epserver.c
#	$(MSG) "   CC $<"
#	$(HIDE) ${CC} -c $< ${CFLAGS} ${INC}

#epserver: epserver.o ${MTCP_FLD}/lib/libmtcp.a
#	$(MSG) "   LD $<"
#	$(HIDE) ${CC} $< ${LIBS} ${UTIL_OBJ} -o $@

test_so: test_so.c
	gcc -Wall -g test_so.c ${ZEUS_MTCP_CFLAGS} ${INC} ${LIBS}  -lmtcp_queue -L./ -o test_so

#epwget.o: epwget.c
#	$(MSG) "   CC $<"
#	$(HIDE) ${CC} -c $< ${CFLAGS} ${INC}

#epwget: epwget.o ${MTCP_FLD}/lib/libmtcp.a
#	$(MSG) "   LD $<"
#	$(HIDE) ${CC} $< ${LIBS} ${UTIL_OBJ} -o $@

clean:
	rm -f *~ *.o ${TARGETS} log_*

distclean: clean
	rm -rf Makefile
