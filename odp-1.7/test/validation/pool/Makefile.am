include ../Makefile.inc

noinst_LTLIBRARIES = libtestpool.la
libtestpool_la_SOURCES = pool.c

test_PROGRAMS = pool_main$(EXEEXT)
dist_pool_main_SOURCES = pool_main.c
pool_main_LDADD = libtestpool.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = pool.h
