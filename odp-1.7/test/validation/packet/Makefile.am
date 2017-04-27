include ../Makefile.inc

noinst_LTLIBRARIES = libtestpacket.la
libtestpacket_la_SOURCES = packet.c

test_PROGRAMS = packet_main$(EXEEXT)
dist_packet_main_SOURCES = packet_main.c
packet_main_LDADD = libtestpacket.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = packet.h
