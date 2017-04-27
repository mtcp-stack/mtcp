include ../Makefile.inc

noinst_LTLIBRARIES = libteststd_clib.la
libteststd_clib_la_SOURCES = std_clib.c

test_PROGRAMS = std_clib_main$(EXEEXT)
dist_std_clib_main_SOURCES = std_clib_main.c
std_clib_main_LDADD = libteststd_clib.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = std_clib.h
