include ../Makefile.inc

noinst_LTLIBRARIES = libtestcpumask.la
libtestcpumask_la_SOURCES = cpumask.c
libtestcpumask_la_LIBADD = $(LIBCPUMASK_COMMON)

test_PROGRAMS = cpumask_main$(EXEEXT)
dist_cpumask_main_SOURCES = cpumask_main.c
cpumask_main_LDADD = libtestcpumask.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = cpumask.h
