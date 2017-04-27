include ../Makefile.inc

noinst_LTLIBRARIES = libtestshmem.la
libtestshmem_la_SOURCES = shmem.c

test_PROGRAMS = shmem_main$(EXEEXT)
dist_shmem_main_SOURCES = shmem_main.c
shmem_main_LDADD = libtestshmem.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = shmem.h
