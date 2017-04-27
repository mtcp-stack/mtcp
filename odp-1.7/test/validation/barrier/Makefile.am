include ../Makefile.inc

noinst_LTLIBRARIES = libtestbarrier.la
libtestbarrier_la_SOURCES = barrier.c

test_PROGRAMS = barrier_main$(EXEEXT)
dist_barrier_main_SOURCES = barrier_main.c
barrier_main_LDADD = libtestbarrier.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = barrier.h
