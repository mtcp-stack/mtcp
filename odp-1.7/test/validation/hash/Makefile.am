include ../Makefile.inc

noinst_LTLIBRARIES = libtesthash.la
libtesthash_la_SOURCES = hash.c

test_PROGRAMS = hash_main$(EXEEXT)
dist_hash_main_SOURCES = hash_main.c
hash_main_LDADD = libtesthash.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = hash.h
