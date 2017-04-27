include ../Makefile.inc

noinst_LTLIBRARIES = libtestqueue.la
libtestqueue_la_SOURCES = queue.c

test_PROGRAMS = queue_main$(EXEEXT)
dist_queue_main_SOURCES = queue_main.c
queue_main_LDADD = libtestqueue.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = queue.h
