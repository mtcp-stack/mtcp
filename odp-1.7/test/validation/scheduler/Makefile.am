include ../Makefile.inc

noinst_LTLIBRARIES = libtestscheduler.la
libtestscheduler_la_SOURCES = scheduler.c

test_PROGRAMS = scheduler_main$(EXEEXT)
dist_scheduler_main_SOURCES = scheduler_main.c
scheduler_main_LDADD = libtestscheduler.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = scheduler.h
