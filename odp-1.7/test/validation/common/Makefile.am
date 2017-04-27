AUTOMAKE_OPTIONS = foreign
include $(top_srcdir)/test/Makefile.inc

noinst_LTLIBRARIES = libcunit_common.la libcpumask_common.la libthrmask_common.la

libcunit_common_la_SOURCES = odp_cunit_common.c

libcpumask_common_la_SOURCES = mask_common.c

libthrmask_common_la_SOURCES = mask_common.c
libthrmask_common_la_CFLAGS = $(AM_CFLAGS) -DTEST_THRMASK

EXTRA_DIST = mask_common.h odp_cunit_common.h
