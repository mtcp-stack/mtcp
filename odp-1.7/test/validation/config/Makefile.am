include ../Makefile.inc

noinst_LTLIBRARIES = libtestconfig.la
libtestconfig_la_SOURCES = config.c

test_PROGRAMS = config_main$(EXEEXT)
dist_config_main_SOURCES = config_main.c
config_main_LDADD = libtestconfig.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = config.h
