include $(top_srcdir)/example/Makefile.inc

bin_PROGRAMS = odp_l2fwd
odp_l2fwd_LDFLAGS = $(AM_LDFLAGS) -static
odp_l2fwd_CFLAGS = $(AM_CFLAGS) -I${top_srcdir}/example

noinst_HEADERS = \
		  $(top_srcdir)/example/example_debug.h

dist_odp_l2fwd_SOURCES = odp_l2fwd.c
