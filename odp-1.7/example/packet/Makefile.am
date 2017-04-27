include $(top_srcdir)/example/Makefile.inc

bin_PROGRAMS = odp_pktio$(EXEEXT)
odp_pktio_LDFLAGS = $(AM_LDFLAGS) -static
odp_pktio_CFLAGS = $(AM_CFLAGS) -I${top_srcdir}/example

noinst_HEADERS = \
		  $(top_srcdir)/example/example_debug.h

dist_odp_pktio_SOURCES = odp_pktio.c
