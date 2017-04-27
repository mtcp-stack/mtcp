include ../Makefile.inc

noinst_LTLIBRARIES = libtestrandom.la
libtestrandom_la_SOURCES = random.c

test_PROGRAMS = random_main$(EXEEXT)
dist_random_main_SOURCES = random_main.c
random_main_LDADD = libtestrandom.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = random.h
