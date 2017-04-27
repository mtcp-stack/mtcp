include ../Makefile.inc

noinst_LTLIBRARIES = libtesttime.la
libtesttime_la_SOURCES = time.c

test_PROGRAMS = time_main$(EXEEXT)
dist_time_main_SOURCES = time_main.c
time_main_LDADD = libtesttime.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = time.h
