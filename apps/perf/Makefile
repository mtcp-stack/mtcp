TARGETS = client
CC=gcc -g -O3 -Wall -Werror -fgnu89-inline
DPDK=1
PS=0
NETMAP=0

# DPDK LIBRARY and HEADER
DPDK_INC=/home/frank/mtcp/dpdk/include
DPDK_LIB=/home/frank/mtcp/dpdk/lib/

# mtcp library and header 
MTCP_FLD    =../../mtcp/
MTCP_INC    =-I${MTCP_FLD}/include
MTCP_LIB    =-L${MTCP_FLD}/lib
MTCP_TARGET = ${MTCP_LIB}/libmtcp.a

UTIL_FLD = ../../util
UTIL_INC = -I${UTIL_FLD}/include
UTIL_OBJ = ${UTIL_FLD}/http_parsing.o ${UTIL_FLD}/tdate_parse.o


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
DPDK_MACHINE_FLAGS = $(shell cat /home/frank/mtcp/dpdk/include/cflags.txt)
INC += ${DPDK_MACHINE_FLAGS} -I${DPDK_INC} -include $(DPDK_INC)/rte_config.h
endif

ifeq ($V,) # no echo
	export MSG=@echo
	export HIDE=@
else
	export MSG=@\#
	export HIDE=
endif

ifeq ($(DPDK),1)
DPDK_LIB_FLAGS = $(shell cat /home/frank/mtcp/dpdk/lib/ldflags.txt) -lgmp
#LIBS += -m64 -g -O3 -pthread -lrt -march=native -Wl,-export-dynamic ${MTCP_FLD}/lib/libmtcp.a -L../../dpdk/lib -Wl,-lnuma -Wl,-lmtcp -Wl,-lpthread -Wl,-lrt -Wl,-ldl -Wl,${DPDK_LIB_FLAGS}
LIBS += -m64 -g -O3 -pthread -lrt -march=native -export-dynamic ${MTCP_FLD}/lib/libmtcp.a -L../../dpdk/lib -lnuma -lmtcp -lpthread -lrt -ldl ${DPDK_LIB_FLAGS}
else
#LIBS += -m64 -g -O3 -pthread -lrt -march=native -Wl,-export-dynamic ${MTCP_FLD}/lib/libmtcp.a -L../../dpdk/lib -Wl,-lnuma -Wl,-lmtcp -Wl,-lpthread -Wl,-lrt -Wl,-ldl -Wl,${DPDK_LIB_FLAGS}
LIBS += -m64 -g -O3 -pthread -lrt -march=native -export-dynamic ${MTCP_FLD}/lib/libmtcp.a -L../../dpdk/lib -lnuma -lmtcp -lpthread -lrt -ldl ${DPDK_LIB_FLAGS}
endif

# LIBCCP
#LIBCCP=/home/frank/mtcp/mtcp/src/libccp
#LIBS += -L$(LIBCCP) -lccp
#INC += -I$(LIBCCP)
# CCP_OBJ = $(LIBCCP)/ccp.o

all: client

client.o: client.c
	$(MSG) "   CC $<"
	$(HIDE) ${CC} -g -c $< ${CFLAGS} ${INC}
	echo ${INC}

client: client.o
	$(MSG) "   LD $<"
	$(HIDE) ${CC} $< ${INC} ${LIBS} ${UTIL_OBJ} -g -o $@

clean:
	rm -f *~ *.o ${TARGETS} log_*

distclean: clean
	rm -rf Makefile
