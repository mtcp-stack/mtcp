include $(top_srcdir)/test/Makefile.inc

bin_PROGRAMS = odp_ring$(EXEEXT)

odp_ring_CFLAGS = $(AM_CFLAGS)

odp_ring_LDFLAGS = $(AM_LDFLAGS) -static

noinst_HEADERS = \
		  $(top_srcdir)/test/api_test/odp_common.h \
		  $(top_srcdir)/test/test_debug.h

dist_odp_ring_SOURCES = odp_ring_test.c odp_common.c

#The tests will need to retain the deprecated test implementation
AM_CFLAGS += -Wno-deprecated-declarations