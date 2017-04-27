include ../Makefile.inc

noinst_LTLIBRARIES = libtestthread.la
libtestthread_la_SOURCES = thread.c
libtestthread_la_CFLAGS = $(AM_CFLAGS) -DTEST_THRMASK
libtestthread_la_LIBADD = $(LIBTHRMASK_COMMON)

test_PROGRAMS = thread_main$(EXEEXT)
dist_thread_main_SOURCES = thread_main.c
thread_main_LDADD = libtestthread.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = thread.h
