include ../Makefile.inc

noinst_LTLIBRARIES = libtestpktio.la
libtestpktio_la_SOURCES = pktio.c

test_PROGRAMS = pktio_main$(EXEEXT)
dist_pktio_main_SOURCES = pktio_main.c
pktio_main_LDADD = libtestpktio.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = pktio.h
