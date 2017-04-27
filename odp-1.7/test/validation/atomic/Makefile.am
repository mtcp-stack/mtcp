include ../Makefile.inc

noinst_LTLIBRARIES = libtestatomic.la
libtestatomic_la_SOURCES = atomic.c

test_PROGRAMS = atomic_main$(EXEEXT)
dist_atomic_main_SOURCES = atomic_main.c
atomic_main_LDADD = libtestatomic.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = atomic.h
