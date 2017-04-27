include $(top_srcdir)/platform/@with_platform@/Makefile.inc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(top_builddir)/pkgconfig/libodphelper.pc

LIB   = $(top_builddir)/lib
AM_CFLAGS  = -I$(srcdir)/include
AM_CFLAGS += -I$(top_srcdir)/platform/@with_platform@/include
AM_CFLAGS += -I$(top_srcdir)/include

helperincludedir = $(includedir)/odp/helper/
helperinclude_HEADERS = \
		  $(srcdir)/include/odp/helper/ring.h \
		  $(srcdir)/include/odp/helper/linux.h \
		  $(srcdir)/include/odp/helper/chksum.h\
		  $(srcdir)/include/odp/helper/eth.h\
		  $(srcdir)/include/odp/helper/icmp.h\
		  $(srcdir)/include/odp/helper/ip.h\
		  $(srcdir)/include/odp/helper/ipsec.h\
		  $(srcdir)/include/odp/helper/strong_types.h\
		  $(srcdir)/include/odp/helper/tcp.h\
		  $(srcdir)/include/odp/helper/table.h\
		  $(srcdir)/include/odp/helper/udp.h

noinst_HEADERS = \
		 $(srcdir)/odph_debug.h \
		 $(srcdir)/odph_hashtable.h \
		 $(srcdir)/odph_lineartable.h \
		 $(srcdir)/odph_list_internal.h

__LIB__libodphelper_la_SOURCES = \
					linux.c \
					ring.c \
					hashtable.c \
					lineartable.c

lib_LTLIBRARIES = $(LIB)/libodphelper.la
