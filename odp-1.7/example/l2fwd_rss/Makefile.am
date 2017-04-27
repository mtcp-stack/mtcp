include $(top_srcdir)/example/Makefile.inc

bin_PROGRAMS = odp_l2fwd_rss
odp_l2fwd_LDFLAGS = $(AM_LDFLAGS) -static
odp_l2fwd_CFLAGS = $(AM_CFLAGS) -I${top_srcdir}/example	-I${top_srcdir}/example/platform/linux-hisilicon/include

noinst_HEADERS = \
		  $(top_srcdir)/example/example_debug.h

dist_odp_l2fwd_SOURCES = odp_l2fwd_rss.c
