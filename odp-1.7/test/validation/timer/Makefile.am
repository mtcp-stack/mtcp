include ../Makefile.inc

noinst_LTLIBRARIES = libtesttimer.la
libtesttimer_la_SOURCES = timer.c

test_PROGRAMS = timer_main$(EXEEXT)
dist_timer_main_SOURCES = timer_main.c
timer_main_LDADD = libtesttimer.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = timer.h
