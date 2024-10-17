#-------------------------------------------------------------------------#
#ifeq ($(RTE_SDK),)
#$(error "Please define RTE_SDK environment variable")
#endif
#
#ifeq ($(RTE_TARGET),)
#$(error "Please define RTE_TARGET environment variable")
#endif
#-------------------------------------------------------------------------#
#include $(RTE_SDK)/mk/rte.vars.mk
CC=gcc
obj-m=dpdk_iface.o
DPDK_MACHINE_LINKER_FLAGS=$(shell pkg-config --libs libdpdk)
DPDK_MACHINE_LDFLAGS=$(shell pkg-config --libs libdpdk)
DPDK_INC=$(shell pkg-config --cflags libdpdk)
DPDK_LIB=
appname=dpdk_iface_main
#-------------------------------------------------------------------------#
ifeq ($V,) # no echo
	export MSG=@echo
	export HIDE=@
else
	export MSG=@\#
	export HIDE=
endif
#-------------------------------------------------------------------------#
all: dpdk_iface.c $(appname) $(appname).c
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

$(appname): $(appname).c
	$(MSG) "  CC $<"
	$(HIDE) $(CC) -g $(CFLAGS) $(appname).c -g -o $(appname) -g \
	${DPDK_INC} \
	$(DPDK_LIB) ${DPDK_MACHINE_LDFLAGS} -lpthread

clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	$(MSG) "  CLEAN  $(appname)"
	$(HIDE) rm -rf *~ *.o *.ko dpdk_iface_main

run: all
	sudo ./$(appname)
#-------------------------------------------------------------------------#
