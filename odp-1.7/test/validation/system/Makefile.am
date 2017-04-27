include ../Makefile.inc

noinst_LTLIBRARIES = libtestsystem.la
libtestsystem_la_SOURCES = system.c

test_PROGRAMS = system_main$(EXEEXT)
dist_system_main_SOURCES = system_main.c
system_main_LDADD = libtestsystem.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = system.h
