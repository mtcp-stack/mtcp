include ../Makefile.inc

noinst_LTLIBRARIES = libtesterrno.la
libtesterrno_la_SOURCES = errno.c

test_PROGRAMS = errno_main$(EXEEXT)
dist_errno_main_SOURCES = errno_main.c
errno_main_LDADD = libtesterrno.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = errno.h
