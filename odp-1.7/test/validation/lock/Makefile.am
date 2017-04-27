include ../Makefile.inc

noinst_LTLIBRARIES = libtestlock.la
libtestlock_la_SOURCES = lock.c

test_PROGRAMS = lock_main$(EXEEXT)
dist_lock_main_SOURCES = lock_main.c
lock_main_LDADD = libtestlock.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = lock.h
